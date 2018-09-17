#include "includes.h"
#include "libs.h"
#include "worker.h"
#include "io_service.h"
#include "tcp_socket.h"
#include "session.h"
#include "http_service.h"
#include "mutex.h"

typedef struct http_service http_service_t;

struct http_service {
    int nworkers;
    struct sockaddr_storage sockaddr;
    struct event_base *ebase;
    struct event *ev_sigterm;
    struct event *ev_sigint;
    const char *resolver;
    worker_t **workers;
    io_channel_t **listeners;
    list_t *sessions;
    mutex_t *session_mutex;
};

static http_service_t http_service;

static void
http_service_stop_request(evutil_socket_t fd, short events, void *arg);

static void
http_service_session_add(session_t *session)
{
	mutex_lock(http_service.session_mutex);
	list_push_back(http_service.sessions, session);
	mutex_unlock(http_service.session_mutex);
}

void
http_service_session_remove(session_t *session)
{
    bool found = false;
    node_t *node = NULL;
	mutex_lock(http_service.session_mutex);
    list_foreach(http_service.sessions, node) {
        if ((node)->data == session) {
            list_remove(http_service.sessions, node);
            session_free(session);
            found = true;
            break;
        }
    } 
	mutex_unlock(http_service.session_mutex);
    if (!found) {
        session_free(session);
    }
}

static void
http_service_accept_cb(io_channel_t *listener, io_channel_accept_param_t *param)
{
    io_channel_t *channel = channel_accept(listener, param);
    if (NULL != channel) {
        session_t *session = session_new(channel);
        if (NULL == session) {
            channel_free(channel);
			return;
        }
		http_service_session_add(session);
    }
}

io_service_t
http_io_service = {
    .accept_cb = http_service_accept_cb
};

/**
 * Start listeners on their own thread event loop,
 * waiting for incoming connection requests.
 */
static int 
http_service_listener_start(void *ctx)
{
    io_channel_t *listener = ctx;
    if (channel_listen(listener, &http_service.sockaddr) != IO_CHANNEL_E_SUCCESS) {
        return -1;
    }
    return 0;
}

int
http_service_init(int nworkers, struct sockaddr_storage *sockaddr, const char *resolver)
{
    worker_init();

    http_service.resolver = resolver;

	http_service.sessions = list_new();
	
	if (NULL == http_service.sessions) {
		goto error;
	}

    http_service.session_mutex = mutex_new();

    if (NULL == http_service.session_mutex) {
        goto error;
    }

    http_service.ebase = event_base_new();

    if (NULL == http_service.ebase) {
        goto error;
    }

    http_service.ev_sigterm = evsignal_new(http_service.ebase, SIGTERM, http_service_stop_request, NULL);
    if (NULL == http_service.ev_sigterm) {
        goto error;
    }

    if (event_add(http_service.ev_sigterm, NULL) < 0) {
        goto error;
    }

    http_service.ev_sigint = evsignal_new(http_service.ebase, SIGINT, http_service_stop_request, NULL);
    if (NULL == http_service.ev_sigint) {
        goto error;
    }

    if (event_add(http_service.ev_sigint, NULL) < 0) {
        goto error;
    }

    io_channel_t **listeners = calloc(nworkers, sizeof(io_channel_t *));
    if (NULL == listeners) {
        goto error;
    }
    http_service.listeners = listeners;

    for (int i = 0; i < nworkers; ++i) {
        listeners[i] = tcp_socket_new();
        if (NULL == listeners[i]) {
            goto error;
        }
        listeners[i]->service = &http_io_service;
    }

    worker_t **workers = calloc(nworkers, sizeof(worker_t *));
    if (NULL == workers) {
        return -1;
    }
    http_service.nworkers = nworkers;
    http_service.workers = workers;

    for (int i = 0; i < nworkers; ++i) {
        workers[i] = worker_new(listeners[i], resolver);
        if (NULL == workers[i]) {
            goto error;
        }
        worker_set_prologue(workers[i], http_service_listener_start);
    }


    http_service.sockaddr = *sockaddr;

    return 0;

error:

    http_service_fini();
    return -1;
}

int
http_service_start(void)
{
    for (int i = 0; i < http_service.nworkers; ++i) {
        if (worker_start(http_service.workers[i]) < 0) {
            return -1;
        }
    }
    event_base_dispatch(http_service.ebase);
    return 0;
}

static void
http_service_stop(void)
{
    for (int i = 0; i < http_service.nworkers; ++i) {
        worker_stop(http_service.workers[i]);
    }

    event_del(http_service.ev_sigterm);
    event_del(http_service.ev_sigint);
}

static void
http_service_stop_request(evutil_socket_t fd, short events, void *arg)
{
    http_service_stop();
}

void
http_service_fini(void)
{
    node_t *node = NULL, *next = NULL;;
    list_foreach_safe(http_service.sessions, node, next) {
        session_t *session = node->data;
        session_free(session);
    }
    list_free(http_service.sessions);
    http_service.sessions = NULL;

    if (NULL != http_service.listeners) {
        for (int i = 0; i < http_service.nworkers; ++i) {
            io_channel_t *listener = http_service.listeners[i];
            if (NULL != listener) {
                channel_free(listener);
                http_service.listeners[i] = NULL;
            }
        }
        free(http_service.listeners);
        http_service.listeners = NULL;
    }

    if (NULL != http_service.workers) {
        for (int i = 0; i < http_service.nworkers; ++i) {
            worker_t *worker = http_service.workers[i];
            if (NULL != worker) {
                worker_free(worker);
                http_service.workers[i] = NULL;
            }
        }
        free(http_service.workers);
        http_service.workers = NULL;
    }

    if (NULL != http_service.ev_sigterm) {
        event_free(http_service.ev_sigterm);
        http_service.ev_sigterm = NULL;
    }

    if (NULL != http_service.ev_sigint) {
        event_free(http_service.ev_sigint);
        http_service.ev_sigint = NULL;
    }

    if (NULL != http_service.ebase) {
        event_base_free(http_service.ebase);
        http_service.ebase = NULL;
    }

    worker_fini();
}
