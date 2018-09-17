#ifndef _TIGERA_HTTP_REQUEST_H__
#define _TIGERA_HTTP_REQUEST_H__

typedef struct http_request http_request_t;

struct http_request {
    const char *request_line;
    const char **headers;
    size_t headers_count;
};

#endif /* _TIGERA_HTTP_REQUEST_H__ */
