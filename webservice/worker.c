#include "includes.h"
#include "libs.h"
#include "thread.h"
#include "worker.h"

/* stores one worker per thread context */
static thread_key_t *thread_worker_key = NULL;

/* stores main thread id */
static thread_id_t *main_thread_id = NULL;

struct worker {
    struct event_base *ebase;
    struct evdns_base *dnsbase;
    thread_t *thread;
    worker_prologue_t prologue;
    worker_epilogue_t epilogue;
    void *ctx;
};

static void
worker_failure(void)
{
    thread_signal(main_thread_id, THREAD_SIGTERM);
}

static void
worker_loop(void *arg)
{
    //int ret;
    worker_t *worker = arg;

    thread_key_set(thread_worker_key, worker);

    if (NULL != worker->prologue) {
        if (worker->prologue(worker->ctx) < 0) {
            fprintf(stderr, "%s: error starting worker thread\n", __func__);
            worker_failure();
            return;
        }
    }

    /*ret = */event_base_dispatch(worker->ebase);

    if (NULL != worker->epilogue) {
        worker->epilogue(worker->ctx);
    }
}

int
worker_init(void)
{
    main_thread_id = thread_self();
    if (NULL == main_thread_id) {
        return -1;
    }

    if (evthread_use_pthreads() < 0) {
        return -1;
    }

    thread_worker_key = thread_key_new();
    if (NULL == thread_worker_key) {
        return -1;
    }
    return 0;
}

void
worker_fini(void)
{
    thread_key_free(thread_worker_key);
    thread_worker_key = NULL;
    thread_id_free(main_thread_id);
    main_thread_id = NULL;
}

worker_t *
worker_new(void *ctx)
{
    worker_t *worker = calloc(1, sizeof(worker_t));
    if (NULL != worker) {
        struct event_base *ebase = event_base_new();
        struct evdns_base *dnsbase = NULL;
        if (NULL == ebase) {
            goto error;
        }
        if (evthread_make_base_notifiable(ebase) != 0) {
            goto error;
        }
        worker->ebase = ebase;

        dnsbase = evdns_base_new(ebase, 0);
        if (NULL == dnsbase) {
            fprintf(stderr, "%s: error starting dns resolver\n", __func__);
            goto error;
        }
        worker->dnsbase = dnsbase;

        if (evdns_base_nameserver_ip_add(dnsbase, "8.8.8.8") < 0) {
            fprintf(stderr, "%s: error setting dnsserver to 8.8.8.8:53\n", __func__);
            goto error;
        }
        
        if (evdns_base_set_option(dnsbase, "timeout", "1.0") < 0) {
            fprintf(stderr, "%s: error setting dnsbase option timeout\n", __func__);
            goto error;
        }
        if (evdns_base_set_option(dnsbase, "attempts", "3") < 0) {
            fprintf(stderr, "%s: error setting dnsbase option attempts\n", __func__);
            goto error;
        }
        if (evdns_base_set_option(dnsbase, "max-timeouts", "3") < 0) {
            fprintf(stderr, "%s: error setting dnsbase option max-timeouts\n", __func__);
            goto error;
        }
        if (evdns_base_set_option(dnsbase, "randomize-case", "0") < 0) {
            fprintf(stderr, "%s: error setting dnsbase option randomize-case\n", __func__);
            goto error;
        }

        thread_t *thread = thread_new(worker_loop);
        if (NULL == thread) {
	        goto error;
        }
        worker->thread = thread;
        worker->ctx = ctx;
    }
    return worker;

error:
    worker_free(worker);
    return NULL;
}

void
worker_free(worker_t *worker)
{
    if (NULL != worker) {
        if (NULL != worker->dnsbase) {
            evdns_base_free(worker->dnsbase, 1);
            worker->dnsbase = NULL;
        }
        if (NULL != worker->ebase) {
            event_base_free(worker->ebase);
            worker->ebase = NULL;
        }
        thread_free(worker->thread);
        worker->thread = NULL;
        worker->prologue = NULL;
        worker->epilogue = NULL;
        worker->ctx = NULL;
        free(worker);
    }	
}

int
worker_start(worker_t *worker)
{
    int ret = thread_start(worker->thread, worker);
    return ret;
}

int
worker_stop(worker_t *worker)
{
    int ret = event_base_loopexit(worker->ebase, NULL);
    if (0 == ret) {
        thread_join(worker->thread);
    }
    return ret;
}

int
worker_join(worker_t *worker)
{
    return thread_join(worker->thread);
}

void
worker_set_prologue(worker_t *worker, worker_prologue_t prologue)
{
    worker->prologue = prologue;
}

void
worker_set_epilogue(worker_t *worker, worker_epilogue_t epilogue)
{
    worker->epilogue = epilogue;
}

worker_t *
this_worker(void)
{
    return thread_key_get(thread_worker_key);
}

struct event_base *
this_event_base(void)
{
    worker_t *worker = this_worker();
    if (NULL != worker) {
        return worker->ebase;
    }
    return NULL;
}

struct evdns_base *
this_dnsbase(void)
{
    worker_t *worker = this_worker();
    if (NULL != worker) {
        return worker->dnsbase;
    }
    return NULL;
}
