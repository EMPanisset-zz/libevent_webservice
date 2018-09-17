#include "libs.h"
#include "io_service.h"
#include "http_service.h"
#include "session.h"
#include "http_session.h"
#include "http_parser.h"

typedef enum http_state http_state_t;

enum http_state {
     HTTP_MESSAGE_BEGIN
    ,HTTP_MESSAGE_PARSE_BEGIN
    ,HTTP_HEADERS_COMPLETE
    ,HTTP_BODY_PARSE
    ,HTTP_MESSAGE_COMPLETE
};

struct http_session {
    session_t *master;
    http_state_t state;
    size_t len;
    struct http_parser http_parser;
    struct evbuffer *body;
    io_channel_t *channel;
    http_callbacks_t cbs;
};

/* http parser callbacks */

static int
message_begin(http_parser *parser)
{
    //int type = parser->type;
    http_session_t *session = parser->data;
    session->body = evbuffer_new();
    if (NULL == session->body) {
        return -1;
    }
    session->state = HTTP_MESSAGE_PARSE_BEGIN;
    http_parser_pause(parser, 1); /**< breaking parser loop */
    return 0;
}

static int
headers_parse_complete(struct http_parser *parser)
{
    //int type = parser->type;
    http_session_t *session = parser->data;
    session->state = HTTP_HEADERS_COMPLETE;
    http_parser_pause(parser, 1); /**< breaking parser loop */
    return 0;
}

static int
message_parse_complete(http_parser *parser)
{
    //int type = parser->type;
    http_session_t *session = parser->data;
    session->state = HTTP_MESSAGE_COMPLETE;
    http_parser_pause(parser, 1); /**< breaking parser loop */
    return 0;
}

static int
message_body(http_parser *parser, const char *at, size_t len)
{
    http_session_t *session = parser->data;
    if (evbuffer_add(session->body, at, len) < 0) {
        return -1;
    }
    fprintf(stderr, "%s: %d %*.s\n", __func__, (int)len, (int)len, at);
    return 0;
} 

struct http_parser_settings http_parser_hooks = {
    /* notification cb */.on_message_begin    = message_begin,
    /* data cb         */.on_url              = NULL,
    /* data cb         */.on_status           = NULL,
    /* data cb         */.on_header_field     = NULL,
    /* data cb         */.on_header_value     = NULL,
    /* notification cb */.on_headers_complete = headers_parse_complete,
    /* data cb         */.on_body             = message_body,
    /* notification cb */.on_message_complete = message_parse_complete,
    /* notification cb */.on_chunk_header     = NULL, /**< when this function is called, the current chunk length is stored in parser->content_length */
    /* notification cb */.on_chunk_complete   = NULL
};

int
http_request_write(http_session_t *session, http_request_t *request)
{
    struct evbuffer *output = channel_get_output(session->channel);

    /* Add Request Line */
    if (evbuffer_add(output, request->request_line, strlen(request->request_line)) < 0) {
        return -1;
    }

    /* Add Headers */
    int i;
    for (i = 0; i < request->headers_count; ++i) {
        size_t header_len = strlen(request->headers[i]);
        if (evbuffer_add(output, request->headers[i], header_len) < 0) {
            return -1;
        }
    }

    if (evbuffer_add(output, "\r\n", 2) < 0) {
        return -1;
    }

    return 0;
}

static int
http_get_datetime(char datetime[38])
{
    time_t now = time(NULL);
    struct tm tm;
    if (NULL == gmtime_r(&now, &tm)) {
        return -1;
    }
    size_t len = strftime(datetime, 38, "Date: %a, %d %b %Y %H:%M:%S %Z\r\n", &tm);
    if (len != 37) {
        return -1;
    }
    return 0;
}

static void
http_response_cleanup(const void *data, size_t len, void *extra)
{
    free(extra);
}

int
http_response_write(http_session_t *session, char *body)
{
    static const char HTTP_STATUS_LINE_OK[] = "HTTP/1.0 200 OK\r\n";
    static const char HTTP_CONTENT_TYPE[] = "Content-Type: text/plain\r\n";
    static const char HTTP_SERVER_HEADER[] = "Server: Tigera/WebServer/1.0.0\r\n";

    struct evbuffer *output = channel_get_output(session->channel);
    size_t body_length = strlen(body);

    char datetime[38];
    if (http_get_datetime(datetime) < 0) {
        return -1;
    }

    /* Add Status Line */
    if (evbuffer_add(output, HTTP_STATUS_LINE_OK, sizeof(HTTP_STATUS_LINE_OK) - 1) < 0) {
        return -1;
    }

    /* Add headers */
    if (evbuffer_add(output, HTTP_CONTENT_TYPE, sizeof(HTTP_CONTENT_TYPE) - 1) < 0) {
        return -1;
    }

    if (evbuffer_add_printf(output, "Content-Length: %zu\r\n", body_length) < 0) {
        return -1;
    }

    if (evbuffer_add(output, HTTP_SERVER_HEADER, sizeof(HTTP_SERVER_HEADER) - 1) < 0) {
        return -1;
    }

    if (evbuffer_add(output, datetime, 37) < 0) {
        return -1;
    }

    if (evbuffer_add(output, "\r\n", 2) < 0) {
        return -1;
    }
    
    /* Add body */

    /* Zero-Copy transference to libevent */
    if (evbuffer_add_reference(output, body, body_length, 
                                http_response_cleanup, body) < 0) {
        free(body);
        return -1;
    }

    return 0;
}
    
typedef enum http_parser_error http_parser_error_t;

enum http_parser_error {
    HTTP_PARSER_E_SUCCESS,
    HTTP_PARSER_E_PAUSED,
    HTTP_PARSER_E_ERROR
};

static http_parser_error_t 
http_request_parse(http_session_t *session, io_channel_t *channel)
{
    http_parser *parser = &session->http_parser;
    http_parser_error_t error = HTTP_PARSER_E_SUCCESS;
    struct evbuffer *input = channel_get_input(channel);
    struct evbuffer_iovec iovecs[5];
    struct evbuffer_iovec *vecs = iovecs;
    struct evbuffer_ptr start;
    int num_vecs;
    int i;

    if (evbuffer_ptr_set(input, &start, session->len, EVBUFFER_PTR_SET) < 0) {
        return HTTP_PARSER_E_ERROR;
    }
    num_vecs = evbuffer_peek(input, -1, &start, NULL, 0);

    if (num_vecs > countof(iovecs)) {
        vecs = malloc(num_vecs * sizeof(struct evbuffer_iovec));
        if (NULL == vecs) {
            return HTTP_PARSER_E_ERROR;
        }
    }

    if (evbuffer_peek(input, -1, &start, vecs, num_vecs) != num_vecs) {
        error = HTTP_PARSER_E_ERROR;
        goto done;
    }

    for (i = 0; i < num_vecs; ++i) {
        size_t parsed = http_parser_execute(parser,
                                            &http_parser_hooks,
                                            vecs[i].iov_base,
                                            vecs[i].iov_len);

        session->len += parsed;

        if (HPE_PAUSED == parser->http_errno) {

            http_parser_pause(parser, 0); /**< unpause parser */

			/* if http parser is paused from on_headers_complete
             * callback it does not consume last byte of header.
             * code below makes required adjustment.
			 */
            if (HTTP_HEADERS_COMPLETE == session->state &&
                parsed < vecs[i].iov_len) {
                parsed = http_parser_execute(parser,
                                            &http_parser_hooks,
                                            vecs[i].iov_base + parsed,
                                            1);
				if (1 != parsed) {
					error = HTTP_PARSER_E_ERROR;
					goto done;
				}

                session->len += parsed;
            }
            error = HTTP_PARSER_E_PAUSED;
            goto done;
        }

        if (HPE_OK != parser->http_errno) {
            error = HTTP_PARSER_E_ERROR;
            goto done;
        }

        if (parsed < vecs[i].iov_len) {
            error = HTTP_PARSER_E_ERROR;
            goto done;
        }
    }

done:

    if (vecs != iovecs) {
        free(vecs);
    }

    return error;
}

static bool 
http_session_channel_should_close(io_channel_t *channel)
{
    /**
     * If client closes connection whithout ever sending data or
     * client request was fully processed (no data sitting in
     * channel output buffer) then session is deallocated and
     * underlying connection is closed. 
     */
    http_session_t *session = channel->ctx;

    if ((channel->eof && (HTTP_MESSAGE_BEGIN == session->state)) ||
        ((HTTP_MESSAGE_COMPLETE == session->state) &&
         (0 == channel_get_output_length(channel)))) {
        if (NULL != session->cbs.ready_to_close) {
            session->cbs.ready_to_close(session);
        }
        return true;
    }
    return false;    
}

static void
http_session_event_cb(io_channel_t *channel, io_channel_event_t event)
{
    http_session_t *session = channel->ctx;

    if (event & IO_CHANNEL_EVENT_ERROR) {
        /* unrecoverable error */
        http_service_session_remove(session->master);

    }
    else if (event & IO_CHANNEL_EVENT_CONNECTED) {
        if (NULL != session->cbs.connected) {
            session->cbs.connected(session);
        }
    }
    else if (event & IO_CHANNEL_EVENT_EOF) {
        /* EOF received */

        if (http_session_channel_should_close(channel)) {
            return;
        }
    }
    else if ((event & IO_CHANNEL_EVENT_TIMEOUT) &&
             (event & IO_CHANNEL_EVENT_READ)) {
        /* long time no see from the client */
        http_service_session_remove(session->master);
    }
}

/**
 * Triggered whenever there is data to read in the input buffer.
 */
static void
http_session_read_cb(io_channel_t *channel)
{
    http_parser_error_t error = HTTP_PARSER_E_SUCCESS;
    http_session_t *session = channel->ctx;

    if (http_session_channel_should_close(channel)) {
        return;
    }

    if (0 == channel_get_input_length(channel)) {
        /* nothing to be done */
        return;
    }

    while (HTTP_PARSER_E_SUCCESS == error) {
        switch (session->state) {
        
            case HTTP_MESSAGE_BEGIN:
            case HTTP_MESSAGE_PARSE_BEGIN:
            case HTTP_BODY_PARSE:
            {
                http_state_t state = session->state;
                error = http_request_parse(session, channel);
                if (HTTP_PARSER_E_ERROR != error) {
                    if (state == session->state) {
                        return;
                    }
                    error = HTTP_PARSER_E_SUCCESS;
                }
            }
            break;

            case HTTP_HEADERS_COMPLETE: 
            {
                if (channel_drain_input(channel, session->len) < 0) {
                    error = HTTP_PARSER_E_ERROR;
                }
                else {
                    session->len = 0;
                    error = HTTP_PARSER_E_SUCCESS;
                    session->state = HTTP_BODY_PARSE;
                }
            }
            break;

            case HTTP_MESSAGE_COMPLETE:
            {
                if (channel_drain_input(channel, session->len) < 0) {
                    error = HTTP_PARSER_E_ERROR;
                }
                else {
                    session->len = 0;
                    if (session->cbs.message_complete) {
                        session->cbs.message_complete(session);
                    }
                    //session->state = HTTP_MESSAGE_BEGIN;
                    //error = HTTP_PARSER_E_SUCCESS;
                    return;
                }
            }
        }
    }

    if (HTTP_PARSER_E_ERROR == error) {
        http_service_session_remove(session->master);
        return;
    }
}

/**
 * Triggered whenever output buffer becomes empty.
 */
static void
http_session_write_cb(io_channel_t *channel)
{
    if (http_session_channel_should_close(channel)) {
        return;
    }
}

session_t *
http_session_master(http_session_t *session)
{
    return session->master;
}

struct evbuffer *
http_session_body(http_session_t *session)
{
    return session->body;
}

io_service_t
http_session_io_service = {
    .event_cb = http_session_event_cb,
    .read_cb = http_session_read_cb,
    .write_cb = http_session_write_cb
};

http_session_t *
http_session_new(session_t *master, io_channel_t *channel, int http_parser_type)
{
    http_session_t *session = calloc(1, sizeof(http_session_t));
    if (NULL != session) {
        http_parser_init(&session->http_parser, http_parser_type);
        session->http_parser.data = session;
        session->state = HTTP_MESSAGE_BEGIN;
        session->channel = channel;
        session->master = master;
        channel->service = &http_session_io_service;
        channel->ctx = session;
    }
    return session;
}

void
http_session_free(http_session_t *session)
{
    if (NULL != session) {
        if (NULL != session->body) {
            evbuffer_free(session->body);
            session->body = NULL;
        }
        channel_free(session->channel);
        session->channel = NULL;
        free(session);
    }
}

void
http_session_callbacks_set(http_session_t *session, http_callbacks_t *cbs)
{
    session->cbs = *cbs;
}
