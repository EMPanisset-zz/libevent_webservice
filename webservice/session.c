#include "libs.h"
#include "io_channel.h"
#include "tcp_socket.h"
#include "worker.h"
#include "session.h"
#include "http_session.h"
#include "http_service.h"
#include "reference.h"
#include "http_request.h"
#include "http_parser.h"

typedef enum session_state session_state_t;

enum session_state {
     START
    ,PARSING_CLIENT_REQUEST
    ,RESOLVING_WEBSERVER_DOMAINS
    ,CONNECTING_TO_WEBSERVERS
    ,REQUESTING_FROM_WEBSERVERS
    ,CLIENT_RESPONSE
    ,ERROR_RESOLVING_DOMAIN
    ,ERROR_CONNECTING_TO_WS
    ,ERROR_REQUESTING_FROM_WS
    ,ERROR_CLIENT_RESPONSE
};

enum {
     CLIENT
    ,NAME
    ,JOKE
    ,COUNT
};

typedef struct dns_request dns_request_t;

struct dns_request {
    int idx;
    struct evdns_getaddrinfo_request *evdns_req;
    reference_t *session; /* keeps weak reference to session object */
};

struct session {
    session_state_t state;
    http_session_t *http_sessions[COUNT]; /*< one client and two upstream http sessions */
    reference_t *me; /* keeps strong referene to myself */
    char *name;
    char *surname;
    char *joke;
    bool name_replied; /*< whether webserver replied name request */
    bool joke_replied; /*< whether webserver replied joke request */
    int pending_resolutions; /*< counter for # in-progress dns resolutions */
    int pending_connections; /*< counter for # in-progress connections to upstream servers */
    int pending_replies; /*< counter for # pending replies from upstream servers */
};

static void
session_state_set(session_t *session, session_state_t state)
{
    session->state = state;
}

static void
dns_request_free(dns_request_t *request)
{
    if (NULL != request) {
        reference_weak_dec(request->session);
        request->session = NULL;
       
        if (NULL != request->evdns_req) { 
            evdns_getaddrinfo_cancel(request->evdns_req);
            request->evdns_req = NULL;
        }
        
        free(request);
    }
}

static void
client_ready_to_close(http_session_t *http_session)
{
    session_t *session = http_session_master(http_session);
    if (session->state == CLIENT_RESPONSE) {
        fprintf(stderr, "client ready to close\n");
        http_service_session_remove(session);
    }
}

static char *
joke_create(char *sess_joke, int sess_joke_len, const char *token, int tlen, const char *sess_token, int sess_tlen)
{
    char *new_joke = NULL;
    char *n = sess_joke;

    int new_joke_len = sess_joke_len;
    while (*n != '\0' &&  (n = strstr(n, token))) {
        new_joke_len -= tlen;
        new_joke_len += sess_tlen;
        n += tlen;
    }
    new_joke_len++;

    new_joke = malloc(new_joke_len);
    if (NULL == new_joke) {
        return NULL;
    }

    int i = 0;
    char *p = sess_joke; 
    while (*p != '\0' && (n = strstr(p, token))) {
        int j;
   
        for (; p < n; ++p, ++i) {
            new_joke[i] = *p;
        }

        for (j = 0; j < sess_tlen; ++j, ++i) {
            new_joke[i] = sess_token[j];
        }

        p += tlen;
    }

    for (; *p != '\0'; ++p, ++i) {
        new_joke[i] = *p;
    }

    new_joke[i] = '\0';

    return new_joke;
}


static char *
client_response_create(session_t *session)
{
    static const char name[]    = "Eduardo";
    static const char surname[] = "Panisset";

    const int name_len = sizeof(name) - 1;
    const int surname_len = sizeof(surname) - 1;

    char *sess_name = session->name;
    char *sess_surname = session->surname;
    char *sess_joke = session->joke;

    int sess_name_len = strlen(sess_name);
    int sess_surname_len = strlen(sess_surname);
    int sess_joke_len = strlen(sess_joke);

    char *new_joke = NULL;

    new_joke = joke_create(sess_joke, sess_joke_len, name, name_len, sess_name, sess_name_len);

    free(sess_name);
    session->name = NULL;

    if (NULL == new_joke) {
        return NULL;
    }

    free(sess_joke);
    session->joke = NULL;

    sess_joke = new_joke;
    sess_joke_len = strlen(sess_joke);

    new_joke = joke_create(sess_joke, sess_joke_len, surname, surname_len, sess_surname, sess_surname_len);

    free(sess_surname);
    session->surname = NULL;

    free(sess_joke);

    return new_joke;
}

static void
http_client_response(session_t *session)
{
    if (!session->name_replied || !session->joke_replied) {
        return;
    }

    http_session_t *http_session = session->http_sessions[CLIENT];

    char *response = client_response_create(session);

    if (NULL == response) {
        session_state_set(session, ERROR_CLIENT_RESPONSE);
        http_service_session_remove(session);
        return;
    }

    fprintf(stderr, "%s: writing response to client: %s\n", __func__, response);

    if (http_response_write(http_session, response) < 0) {
        session_state_set(session, ERROR_CLIENT_RESPONSE);
        http_service_session_remove(session);
        return;
    }

    /* session successfully processed.
     * waiting for signal "ready-to-close" from
     * underlying channel. That will happen as
     * soon as all pending data is written to
     * socket and EOF is received from client.
     */
    session_state_set(session, CLIENT_RESPONSE);
}

static void
http_response_name(http_session_t *http_session)
{
    session_t *session = http_session_master(http_session);
    struct evbuffer *body = http_session_body(http_session);
    json_t *jresponse = NULL;
    char *name = NULL;
    char *surname = NULL;
    char *p = NULL;
    size_t body_len = 0;
    json_error_t jerror;

    session->pending_replies--;

    fprintf(stderr, "%s: name response received\n", __func__);

    body_len = evbuffer_get_length(body);
    
    p = (char *)evbuffer_pullup(body, body_len);
    if (NULL == p) {
        goto error;
    }
   
    jresponse = json_loadb(p, body_len, 0, &jerror);
    if (NULL == jresponse) {
        fprintf(stderr, "%s: error decoding json response: %s:%s\n", __func__, jerror.text, jerror.source);
        goto error;
    }

    if (0 != json_unpack(jresponse, "{s:s, s:s}", "name", &name, "surname", &surname)) {
        goto error;
    }

    session->name = strdup(name);
    if (NULL == session->name) {
        goto error;
    }

    session->surname = strdup(surname);
    if (NULL == session->surname) {
        goto error;
    }

    if (NULL != jresponse) {
        json_decref(jresponse);
    }

    session->name_replied = true;

    http_session_free(http_session);
    session->http_sessions[NAME] = NULL;

    http_client_response(session);

    return;

error:

    if (NULL != jresponse) {
        json_decref(jresponse);
    }

    session_state_set(session, ERROR_CLIENT_RESPONSE);
    http_service_session_remove(session);
}

static void
http_response_joke(http_session_t *http_session)
{
    session_t *session = http_session_master(http_session);
    struct evbuffer *body = http_session_body(http_session);
    json_t *jresponse = NULL;
    char *joke = NULL;
    char *p = NULL;
    size_t body_len = 0;
    json_error_t jerror;

    session->pending_replies--;

    fprintf(stderr, "%s: joke response received\n", __func__);

    body_len = evbuffer_get_length(body);
    
    p = (char *)evbuffer_pullup(body, body_len);
    if (NULL == p) {
        goto error;
    }
   
    jresponse = json_loadb(p, body_len, 0, &jerror);
    if (NULL == jresponse) {
        fprintf(stderr, "%s: error decoding json response: %s:%s\n", __func__, jerror.text, jerror.source);
        goto error;
    }

    if (0 != json_unpack_ex(jresponse, &jerror, 0, "{s:{s:s}}", "value", "joke", &joke)) {
        fprintf(stderr, "%s: error unpacking json response: %s:%s\n", __func__, jerror.text, jerror.source);
        goto error;
    }

    session->joke = strdup(joke);
    if (NULL == session->joke) {
        goto error;
    }

    if (NULL != jresponse) {
        json_decref(jresponse);
    }

    session->joke_replied = true;
    
    http_session_free(http_session);
    session->http_sessions[JOKE] = NULL;

    http_client_response(session);

    return;

error:

    if (NULL != jresponse) {
        json_decref(jresponse);
    }

    session_state_set(session, ERROR_CLIENT_RESPONSE);
    http_service_session_remove(session);
}

static void
http_request_name(http_session_t *http_session)
{
    session_t *session = NULL;
    http_request_t request;

    const char *request_line = "GET /api/ HTTP/1.1\r\n";
    const char *headers[] = {
        "Host: uinames.com\r\n",
        "User-Agent: Tigera/WebService/1.0.0\r\n",
        "Accept: */*\r\n"
    };

    session = http_session_master(http_session);
    session->pending_connections--;

    request.request_line = request_line;
    request.headers = headers;
    request.headers_count = countof(headers);

    if (http_request_write(http_session, &request) < 0) {
        session_state_set(session, ERROR_REQUESTING_FROM_WS);
        http_service_session_remove(session);
        return;
    }
    session->pending_replies++;
    fprintf(stderr, "%s: name request sent\n", __func__);
}

static void
http_request_joke(http_session_t *http_session)
{
    session_t *session = NULL;
    http_request_t request;

    const char *request_line = "GET /jokes/random?firstName=Eduardo&lastName=Panisset&limitTo=[nerdy] HTTP/1.1\r\n";
    const char *headers[] = {
        "Host: api.icndb.com\r\n",
        "User-Agent: Tigera/WebService/1.0.0\r\n",
        "Accept: */*\r\n"
    };

    session = http_session_master(http_session);
    session->pending_connections--;

    request.request_line = request_line;
    request.headers = headers;
    request.headers_count = countof(headers);

    if (http_request_write(http_session, &request) < 0) {
        session_state_set(session, ERROR_REQUESTING_FROM_WS);
        http_service_session_remove(session);
        return;
    }
    fprintf(stderr, "%s: joke request sent\n", __func__);
    session->pending_replies++;
}

static void
http_server_connect(session_t *session, struct sockaddr_storage *ss, int idx)
{
    http_callbacks_t callbacks;
    io_channel_error_t err;
    io_channel_t *channel = NULL;
    http_session_t *http_session = NULL;

    memset(&callbacks, 0, sizeof(callbacks));

    channel = tcp_socket_new();
    if (NULL == channel) {
        goto error;
    }

    http_session = http_session_new(session, channel, HTTP_RESPONSE);
    if (NULL == http_session) {
        channel_free(channel);
        goto error;
    }

    if (idx == NAME) {
        callbacks.connected = http_request_name;
        callbacks.message_complete = http_response_name;
    }
    else {
        callbacks.connected = http_request_joke;
        callbacks.message_complete = http_response_joke;
    }

    err = channel_connect(channel, ss);

    if (err == IO_CHANNEL_E_ERROR) {
        http_session_free(http_session);
        goto error;
    }

    session->http_sessions[idx] = http_session;
    session->pending_connections++;
    http_session_callbacks_set(http_session, &callbacks);

    return;

error:
    session_state_set(session, ERROR_CONNECTING_TO_WS);
    http_service_session_remove(session);
}


static void
dnsname_resolved_cb(int errcode, struct evutil_addrinfo *addr, void *ctx)
{
    dns_request_t *dns_request = ctx;
    session_t *session = reference_get(dns_request->session);
    int idx = dns_request->idx;
    
    dns_request->evdns_req = NULL;

    dns_request_free(dns_request);

    if (NULL == session) {
        /* session is gone */
        return;
    }
    
    if (errcode) {
        fprintf(stderr, "%s: dns resolution error: %s\n", __func__, evutil_gai_strerror(errcode));
    }
    else {
        char dst[INET6_ADDRSTRLEN];
        struct sockaddr_storage ss;
        struct evutil_addrinfo *ai;
        bool found = false;

        memset(&ss, 0, sizeof(ss));

        for (ai = addr; ai; ai = ai->ai_next) {
            //char buf[128];
            //const char *s = NULL;

            if (ai->ai_family == AF_INET) {
                struct sockaddr_in *sin = (struct sockaddr_in *)ai->ai_addr;
                memcpy(&ss, sin, sizeof(*sin));
                //s = evutil_inet_ntop(AF_INET, &sin->sin_addr, buf, 128);
                found = true;
                break;
            }
            else if (ai->ai_family == AF_INET6) {
                struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ai->ai_addr;
                memcpy(&ss, sin6, sizeof(*sin6));
                //s = evutil_inet_ntop(AF_INET6, &sin6->sin6_addr, buf, 128);
                found = true;
                break;
            }
        }
        evutil_freeaddrinfo(addr);
        if (found) {
            if (ss.ss_family == AF_INET) {
                struct sockaddr_in *s4 = (struct sockaddr_in *)&ss;
                s4->sin_port = htons(80);
                fprintf(stderr, "%s: domain resolved %d: %s\n", __func__, idx, evutil_inet_ntop(ss.ss_family, &s4->sin_addr, dst, INET6_ADDRSTRLEN));
            }
            else {
                struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)&ss;
                s6->sin6_port = htons(80);
                fprintf(stderr, "%s: domain resolved %d: %s\n", __func__, idx, evutil_inet_ntop(ss.ss_family, &s6->sin6_addr, dst, INET6_ADDRSTRLEN));
            }
            session->pending_resolutions--;
            http_server_connect(session, &ss, idx);
            return;
        }
        fprintf(stderr, "%s: no A nor AAAA records for domain name found\n", __func__);
    }
    session_state_set(session, ERROR_RESOLVING_DOMAIN);
    http_service_session_remove(session);
}

static void
dnsname_resolve(dns_request_t *dns_request, const char *domain, int idx)
{
    struct evutil_addrinfo hints;
    struct evdns_getaddrinfo_request *req = NULL;
    struct evdns_base *dnsbase = this_dnsbase();
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_flags = EVUTIL_AI_CANONNAME;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (NULL == dnsbase) {
        session_t *session = reference_get(dns_request->session);
        dns_request_free(dns_request);
        if (NULL != session) {
            session_state_set(session, ERROR_RESOLVING_DOMAIN);
            http_service_session_remove(session);
        }
        return;
    }

    req = evdns_getaddrinfo(dnsbase, domain, NULL,
                          &hints, dnsname_resolved_cb, dns_request);
    
    dns_request->evdns_req = req;
    dns_request->idx = idx;

    fprintf(stderr, "resolving domain %s\n", domain);
}

static dns_request_t *
dns_request_new(session_t *session, const char *domain, int idx)
{
    dns_request_t *dns_request = calloc(1, sizeof(dns_request_t));
    if (NULL != dns_request) {
        dns_request->session = session->me;
        reference_weak_inc(session->me);
        dnsname_resolve(dns_request, domain, idx);
        session->pending_resolutions++;

    }
    return dns_request;
}
    
static void
client_msg_complete(http_session_t *http_session)
{
    session_t *session = http_session_master(http_session);

    /* self strong reference */
    session->me = reference_new(session, NULL);
    if (NULL == session->me) {
        http_service_session_remove(session);
        return;
    }

    /* asynchronously resolve domain names for name and joke webservers */
    dns_request_t *request = dns_request_new(session, "uinames.com", NAME);
    if (NULL == request) {
        http_service_session_remove(session);
        return;
    }

    request = dns_request_new(session, "api.icndb.com", JOKE);
    if (NULL == request) {
        http_service_session_remove(session);
        return;
    }
    session_state_set(session, RESOLVING_WEBSERVER_DOMAINS);
}

void
session_free(session_t *session)
{
    if (NULL != session) {
        int i = 0;
        for (i = 0; i < countof(session->http_sessions); ++i) {
            http_session_free(session->http_sessions[i]);
            session->http_sessions[i] = NULL;
        }
        reference_dec(session->me);
        session->me = NULL;
        free(session->name);
        session->name = NULL;
        free(session->surname);
        session->surname = NULL;
        free(session->joke);
        session->joke = NULL;
        free(session);
    }
}

session_t *
session_new(io_channel_t *channel)
{
    session_t *session = calloc(1, sizeof(session_t));
    if (NULL != session) {
        http_session_t *http_session = http_session_new(session, channel, HTTP_REQUEST);
        if (NULL != http_session) {
            http_callbacks_t callbacks;
            memset(&callbacks, 0, sizeof(callbacks));
            session->http_sessions[CLIENT] = http_session;
            session->state = START;
            session->name_replied = false;
            session->joke_replied = false;
            session_state_set(session, PARSING_CLIENT_REQUEST);
            callbacks.message_complete = client_msg_complete;
            callbacks.ready_to_close = client_ready_to_close;
            http_session_callbacks_set(http_session, &callbacks);
            return session;
        }
        session_free(session);
        session = NULL; 
    }
    return session;
}
