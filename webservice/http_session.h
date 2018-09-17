#ifndef _TIGERA_HTTP_SESSION__H__
#define _TIGERA_HTTP_SESSION__H__

#include "http_request.h"

typedef struct http_session http_session_t;

typedef void (*http_session_cb_t)(http_session_t *);

typedef struct http_callbacks http_callbacks_t;

struct http_callbacks {
    http_session_cb_t message_complete;
    http_session_cb_t connected;
    http_session_cb_t ready_to_close;
};

struct session;

http_session_t *
http_session_new(struct session *master, io_channel_t *channel, int http_parser_type);

void
http_session_free(http_session_t *session);

struct session *
http_session_master(http_session_t *session);

struct evbuffer;
struct evbuffer *
http_session_body(http_session_t *session);

void
http_session_callbacks_set(http_session_t *session, http_callbacks_t *callbacks);

int
http_request_write(http_session_t *session, http_request_t *request);

int
http_response_write(http_session_t *session, char *body);

#endif /* _TIGERA_HTTP_SESSION__H__ */
