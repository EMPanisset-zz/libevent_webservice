#include "libs.h"
#include "tcp_socket.h"
#include "io_service.h"
#include "worker.h"
#include "atomic.h"

#define tcp_socket_cast(p)  downcast(p, tcp_socket_t, parent)
#define TCP_SOCKET_READ_HIGH_WM (16*1024*1024) /**< memory limit for input buffer in bytes */
#define TCP_SOCKET_BACKLOG  100

typedef struct tcp_socket tcp_socket_t;

struct tcp_socket {
    io_channel_t parent;
    union { /**< tcp socket is either: listener or non-listener socket */
        struct bufferevent *bev;
        struct evconnlistener *listener;
    };
    bool listen;
};

static int
tcp_socket_config(tcp_socket_t *tcp_socket);

io_channel_t *
tcp_socket_new(void);

static void
tcp_socket_free(io_channel_t *channel)
{
    if (NULL != channel) {
        tcp_socket_t *tcp_socket = tcp_socket_cast(channel);
        if (tcp_socket->listen) {
            if (NULL != tcp_socket->listener) {
                evconnlistener_free(tcp_socket->listener);
                tcp_socket->listener = NULL;
            }
        }
        else {
            if (NULL != tcp_socket->bev) {
                bufferevent_free(tcp_socket->bev);
                tcp_socket->bev = NULL;
            }
        }
        free(tcp_socket);
    }
}

static io_channel_t *
tcp_socket_accept(io_channel_accept_param_t *param)
{
    struct event_base *ebase = this_event_base();
    if (NULL == ebase) {
        return NULL;
    }
    io_channel_t *channel = tcp_socket_new();
    if (NULL != channel) {
        tcp_socket_t *tcp_socket = tcp_socket_cast(channel);
        tcp_socket->bev = param->io_ctx;
        if (tcp_socket_config(tcp_socket) == 0) {
            return channel; 
        }
        tcp_socket_free(channel);
        channel = NULL;
    }
    return channel;
}

static struct evbuffer *
tcp_socket_get_input(io_channel_t *channel)
{
    tcp_socket_t *tcp_socket = tcp_socket_cast(channel);
    return bufferevent_get_input(tcp_socket->bev);
}

size_t
tcp_socket_get_input_length(io_channel_t *channel)
{
    struct evbuffer *input = tcp_socket_get_input(channel);
    return evbuffer_get_length(input);
}

static struct evbuffer *
tcp_socket_get_output(io_channel_t *channel)
{
    tcp_socket_t *tcp_socket = tcp_socket_cast(channel);
    return bufferevent_get_output(tcp_socket->bev);
}

size_t
tcp_socket_get_output_length(io_channel_t *channel)
{
    struct evbuffer *output = tcp_socket_get_output(channel);
    return evbuffer_get_length(output);
}

size_t
tcp_socket_get_write_low_wm(io_channel_t *channel)
{
    size_t write_low_wm = 0;
    tcp_socket_t *tcp_socket = tcp_socket_cast(channel);
    bufferevent_getwatermark(tcp_socket->bev, EV_WRITE, &write_low_wm, NULL);
    return write_low_wm;     
}

static int 
tcp_socket_config(tcp_socket_t *tcp_socket);

io_channel_error_t
tcp_socket_connect(io_channel_t *channel, struct sockaddr_storage *sockaddr)
{
    struct event_base *ebase;
    evutil_socket_t fd = -1;
    struct bufferevent *bev = NULL;
    tcp_socket_t *tcp_socket = tcp_socket_cast(channel);

    if (tcp_socket->bev != NULL) {
        return IO_CHANNEL_E_ERROR;
    }

    fd = socket(sockaddr->ss_family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd == -1) {
        goto error;
    }

    /* returns event loop object associated to this thread */
    ebase = this_event_base();
    if (NULL == ebase) {
        goto error;
    }

    bev =
        bufferevent_socket_new(ebase,
                                fd,
                                BEV_OPT_CLOSE_ON_FREE |
                                BEV_OPT_THREADSAFE |
                                BEV_OPT_UNLOCK_CALLBACKS |
                                BEV_OPT_DEFER_CALLBACKS);

    if (bev == NULL) {
        goto error;
    }
    fd = -1; /*< bufferevent bev owns fd from now on */

    tcp_socket->bev = bev;
    if (tcp_socket_config(tcp_socket) < 0) {
        goto error;
    }

    int ret = bufferevent_socket_connect(tcp_socket->bev,
                                         (struct sockaddr *)sockaddr,
                                         sizeof(*sockaddr));
    if (ret < 0) {
        int err = evutil_socket_geterror(fd);
        fprintf(stderr, "%s: %s\n", __func__, evutil_socket_error_to_string(err));
        goto error;
    }

    fprintf(stderr, "%s: connecting to peer\n", __func__);

    return IO_CHANNEL_E_AGAIN;

error:
    if (fd != -1) {
        evutil_closesocket(fd);
    }
    if (tcp_socket->bev != NULL) {
        bufferevent_free(tcp_socket->bev);
    }
    return IO_CHANNEL_E_ERROR;
}

io_channel_error_t
tcp_socket_read(io_channel_t *channel, unsigned char *buffer, size_t len)
{
    tcp_socket_t *tcp_socket = tcp_socket_cast(channel);
    size_t ret = bufferevent_read(tcp_socket->bev, buffer, len);
    if (ret != len) {
        return IO_CHANNEL_E_ERROR;
    }
    return IO_CHANNEL_E_SUCCESS;
}

io_channel_error_t
tcp_socket_write(io_channel_t *channel, const unsigned char *buffer, size_t len)
{
    tcp_socket_t *tcp_socket = tcp_socket_cast(channel);
    int ret = bufferevent_write(tcp_socket->bev, buffer, len);
    if (ret < 0) {
        return IO_CHANNEL_E_ERROR;
    }
    return IO_CHANNEL_E_SUCCESS;
}

io_channel_error_t
tcp_socket_shutdown(io_channel_t *channel)
{
    tcp_socket_t *tcp_socket = tcp_socket_cast(channel);
    evutil_socket_t fd;
    int ret = bufferevent_flush(tcp_socket->bev, EV_WRITE, BEV_FINISHED);
    if (ret < 0) {
        return IO_CHANNEL_E_ERROR;
    }
    fd = bufferevent_getfd(tcp_socket->bev);
    if (fd == -1) {
        return IO_CHANNEL_E_ERROR;
    }
    if (shutdown(fd, SHUT_WR) < 0) {
        return IO_CHANNEL_E_ERROR;
    }
    return IO_CHANNEL_E_SUCCESS; 
}

static int
tcp_socket_drain_input(io_channel_t *channel, size_t len)
{
    tcp_socket_t *tcp_socket = tcp_socket_cast(channel);
    struct evbuffer *input = bufferevent_get_input(tcp_socket->bev);
    if (evbuffer_drain(input, len) < 0) {
        return -1;
    }
    return 0;
}

/* calls back registered io service */
static void
tcp_socket_write_cb(struct bufferevent *bev, void *arg)
{
    io_channel_t *channel = arg;

    if (NULL != channel->service && NULL != channel->service->write_cb) {
        channel->service->write_cb(arg);
    }
}

static void
tcp_socket_read_cb(struct bufferevent *bev, void *arg)
{
    io_channel_t *channel = arg;

    if (NULL != channel->service && NULL != channel->service->read_cb) {
        channel->service->read_cb(arg);
    }
}

static void
tcp_socket_event_cb(struct bufferevent *bev, short events, void *arg)
{
    io_channel_t *channel = arg;

    if (NULL == channel->service && NULL == channel->service->event_cb) {
        return;
    }

    io_channel_event_t io_event = IO_CHANNEL_EVENT_NONE;
    if (events & BEV_EVENT_ERROR) {
        /* unrecoverable error */
        io_event |= IO_CHANNEL_EVENT_ERROR;
        //IO_CHANNEL_LOG(ERROR, "%s", evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
    }
    else {
        if ((events & BEV_EVENT_READING) && (events & BEV_EVENT_EOF)) {
            io_event |= IO_CHANNEL_EVENT_EOF;
            channel->eof = 1;
        }
        if (events & (BEV_EVENT_CONNECTED)) {
            fprintf(stderr, "%s: connected to peer\n", __func__);
            io_event |= IO_CHANNEL_EVENT_CONNECTED;
        }
        if (events & BEV_EVENT_TIMEOUT) {
            io_event |= IO_CHANNEL_EVENT_TIMEOUT;
        }
    }
    if (events & BEV_EVENT_READING) {
        io_event |= IO_CHANNEL_EVENT_READ;
    }
    if (events & BEV_EVENT_WRITING) {
        io_event |= IO_CHANNEL_EVENT_WRITE;
    }
    channel->service->event_cb(channel, io_event);
}

static int 
tcp_socket_config(tcp_socket_t *tcp_socket)
{
    bufferevent_setcb(tcp_socket->bev,
                      tcp_socket_read_cb,
                      tcp_socket_write_cb,
                      tcp_socket_event_cb,
                      &tcp_socket->parent);

    if (bufferevent_enable(tcp_socket->bev, EV_READ|EV_WRITE) < 0) {
        bufferevent_free(tcp_socket->bev);
        tcp_socket->bev = NULL;
        return -1;
    }
    bufferevent_setwatermark(tcp_socket->bev, EV_READ, 0, TCP_SOCKET_READ_HIGH_WM);
    return 0;
}

static void
tcp_socket_accept_error_cb(struct evconnlistener *listener, void *arg)
{
    //io_channel_t *channel = arg;
    //IO_CHANNEL_ERROR("Error accepting connection: %s",
    //    evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
}

static void
tcp_socket_accept_cb(struct evconnlistener *listener,
                        evutil_socket_t fd,
                        struct sockaddr *sockaddr,
                        int socklen, void *arg)
{
    struct event_base *ebase = NULL;
    io_channel_accept_param_t param;
    io_channel_t *channel = arg;
    socklen_t addrlen = socklen;

    if (sockaddr->sa_family != AF_INET && sockaddr->sa_family != AF_INET6) {
        goto error;
    }

    memcpy(&param.dst, sockaddr, socklen);

    if (getsockname(fd, &param.src.addr, &addrlen) < 0) {
        goto error;
    }

    param.proto = IPPROTO_TCP;

    ebase = evconnlistener_get_base(listener);

    if (NULL == ebase) {
        goto error;
    }

    struct bufferevent *bev =
        bufferevent_socket_new(ebase,
                                fd,
                                BEV_OPT_CLOSE_ON_FREE |
                                BEV_OPT_THREADSAFE |
                                BEV_OPT_UNLOCK_CALLBACKS |
                                BEV_OPT_DEFER_CALLBACKS);
    
    if (NULL == bev) {
        goto error;
    }
    param.io_ctx = bev;
    channel->service->accept_cb(channel, &param);

    return;

error:
    evutil_closesocket(fd); 
}

static io_channel_error_t
tcp_socket_listen(io_channel_t *channel, struct sockaddr_storage *sockaddr)
{
    evutil_socket_t fd = -1;
    struct event_base *ebase = NULL;
    int addrlen = sizeof(struct sockaddr_storage);
    struct evconnlistener *listener = NULL;
    tcp_socket_t *tcp_socket = tcp_socket_cast(channel);

    if (AF_INET  != sockaddr->ss_family &&
        AF_INET6 != sockaddr->ss_family) {
        return IO_CHANNEL_E_ERROR;
    }

    fd = socket(sockaddr->ss_family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        goto error;
    }

    /**
     * Linux kernel should take care of efficient load balance among listening sockets:
     * https://lwn.net/Articles/542629/
     */
    int optval = 1;
    if (0 != setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval))) {
        evutil_closesocket(fd);
        goto error;
    }

    if (bind(fd, (struct sockaddr *)sockaddr, addrlen) != 0) {
        int err = evutil_socket_geterror(fd);
        fprintf(stderr, "%s: %s\n", __func__, evutil_socket_error_to_string(err));
        evutil_closesocket(fd);
        goto error;
    }

    /* returns event loop object associated to this thread */
    ebase = this_event_base();
        
    listener = evconnlistener_new(ebase,
                                  tcp_socket_accept_cb,
                                  channel,
                                  LEV_OPT_CLOSE_ON_FREE |
                                  //LEV_OPT_THREADSAFE |
                                  LEV_OPT_CLOSE_ON_EXEC |
                                  LEV_OPT_REUSEABLE,
                                  TCP_SOCKET_BACKLOG,
                                  fd);

    if (NULL == listener) {
        evutil_closesocket(fd);
        goto error;
    }

    tcp_socket->listener = listener;
    tcp_socket->listen = true;
    evconnlistener_set_error_cb(listener, tcp_socket_accept_error_cb);

    return IO_CHANNEL_E_SUCCESS;

error:
    if (NULL != listener) {
        evconnlistener_free(listener);
    }
    return IO_CHANNEL_E_ERROR;
}

io_channel_ops_t
tcp_socket_ops = {
    .name = "tcp_socket",
    .accept = tcp_socket_accept,
    .listen = tcp_socket_listen,
    .connect = tcp_socket_connect,
    .read = tcp_socket_read,
    .write = tcp_socket_write,
    .shutdown = tcp_socket_shutdown,
    .free = tcp_socket_free,
    .get_input = tcp_socket_get_input,
    .get_output = tcp_socket_get_output,
    .get_input_length = tcp_socket_get_input_length,
    .get_output_length = tcp_socket_get_output_length,
    .get_write_low_wm = tcp_socket_get_write_low_wm,
    .drain_input = tcp_socket_drain_input
};

io_channel_t *
tcp_socket_new(void)
{
    tcp_socket_t *tcp_socket = calloc(1, sizeof(tcp_socket_t));
    if (NULL != tcp_socket) {
        tcp_socket->parent.ops = &tcp_socket_ops;
        return &tcp_socket->parent;
    }
    return NULL;
}
