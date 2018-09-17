#ifndef _TIGERA_IO_CHANNEL__H__
#define _TIGERA_IO_CHANNEL__H__

#include "includes.h"

/**
 * Abstraction for different IO channels:
 * TCP sockets, UDP sockets, UNIX sockets, etc.
 *
 * TODO: Currently implements TCP sockets.
 *       Additional IO channels might be implemented.
 */

typedef enum io_channel_error io_channel_error_t;

enum io_channel_error {
    IO_CHANNEL_E_SUCCESS,
    IO_CHANNEL_E_AGAIN,
    IO_CHANNEL_E_ERROR
};

typedef enum io_channel_event io_channel_event_t;

enum io_channel_event {
     IO_CHANNEL_EVENT_NONE      = 0x00
    ,IO_CHANNEL_EVENT_READ      = 0x01
    ,IO_CHANNEL_EVENT_WRITE     = 0x02
    ,IO_CHANNEL_EVENT_CONNECTED = 0x04
    ,IO_CHANNEL_EVENT_EOF       = 0x08
    ,IO_CHANNEL_EVENT_ERROR     = 0x10
    ,IO_CHANNEL_EVENT_TIMEOUT   = 0x20
};

typedef union socket_addr socket_addr_t;

union socket_addr {
    struct sockaddr_in addr4;
    struct sockaddr_in6 addr6;
    struct sockaddr addr;
};

typedef struct io_channel_accept_param io_channel_accept_param_t;

struct io_channel_accept_param {
    socket_addr_t src;
    socket_addr_t dst;
    unsigned char proto;
    void *io_ctx; /**< opaque structure only known by underlying io channel */
};

typedef struct io_channel io_channel_t;

typedef io_channel_t* (*io_channel_accept_t)(io_channel_accept_param_t *param);
typedef io_channel_error_t (*io_channel_listen_t)(io_channel_t *, struct sockaddr_storage *sockaddr);
typedef io_channel_error_t (*io_channel_connect_t)(io_channel_t *, struct sockaddr_storage *sockaddr);
typedef io_channel_error_t (*io_channel_read_t)(io_channel_t *, unsigned char *buffer, size_t len);
typedef io_channel_error_t (*io_channel_write_t)(io_channel_t *, const unsigned char *buffer, size_t len);
typedef io_channel_error_t (*io_channel_shutdown_t)(io_channel_t *);
typedef void (*io_channel_free_t)(io_channel_t *);
typedef struct evbuffer* (*io_channel_get_input_t)(io_channel_t *);
typedef struct evbuffer* (*io_channel_get_output_t)(io_channel_t *);
typedef size_t (*io_channel_get_input_length_t)(io_channel_t *);
typedef size_t (*io_channel_get_output_length_t)(io_channel_t *);
typedef size_t (*io_channel_get_write_low_wm_t)(io_channel_t *);
typedef int (*io_channel_drain_input_t)(io_channel_t *, size_t len);

typedef struct io_channel_ops io_channel_ops_t;

struct io_channel_ops {
    const char *name;
    io_channel_accept_t accept;
    io_channel_listen_t listen;
    io_channel_connect_t connect;
    io_channel_read_t read;
    io_channel_write_t write;
    io_channel_shutdown_t shutdown;
    io_channel_free_t free;
    io_channel_get_input_t get_input;
    io_channel_get_output_t get_output;
    io_channel_get_input_length_t get_input_length;
    io_channel_get_output_length_t get_output_length;
    io_channel_get_write_low_wm_t get_write_low_wm;
    io_channel_drain_input_t drain_input;
};

struct io_service;

struct io_channel {
    io_channel_ops_t *ops;
    struct io_service *service;
    unsigned eof:1;
    void *ctx;
};

/* Helper Functions */
static inline io_channel_t *
channel_accept(io_channel_t *channel, io_channel_accept_param_t *param)
{
    return channel->ops->accept(param);
}

static inline io_channel_error_t
channel_listen(io_channel_t *channel, struct sockaddr_storage *sockaddr)
{
    return channel->ops->listen(channel, sockaddr);
}

static inline io_channel_error_t
channel_connect(io_channel_t *channel, struct sockaddr_storage *sockaddr)
{
    return channel->ops->connect(channel, sockaddr);
}

static inline io_channel_error_t
channel_read(io_channel_t *channel, unsigned char *buffer, size_t len)
{
    return channel->ops->read(channel, buffer, len);
}

static inline io_channel_error_t
channel_write(io_channel_t *channel, const unsigned char *buffer, size_t len)
{
    return channel->ops->write(channel, buffer, len);
}

static inline io_channel_error_t
channel_shutdown(io_channel_t * channel)
{
    return channel->ops->shutdown(channel);
}

static inline void 
channel_free(io_channel_t *channel)
{
    channel->ops->free(channel);
}

static inline struct evbuffer *
channel_get_input(io_channel_t *channel)
{
    return channel->ops->get_input(channel);
}

static inline struct evbuffer *
channel_get_output(io_channel_t *channel)
{
    return channel->ops->get_output(channel);
}

static inline size_t
channel_get_input_length(io_channel_t *channel)
{
    return channel->ops->get_input_length(channel);
}

static inline size_t
channel_get_output_length(io_channel_t *channel)
{
    return channel->ops->get_output_length(channel);
}

static inline size_t
channel_get_write_low_wm(io_channel_t *channel)
{
    return channel->ops->get_write_low_wm(channel);
}

static inline int
channel_drain_input(io_channel_t *channel, size_t len)
{
    return channel->ops->drain_input(channel, len);
}

#endif /* _TIGERA_IO_CHANNEL__H__ */
