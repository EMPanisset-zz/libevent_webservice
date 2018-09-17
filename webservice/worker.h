#ifndef _TIGERA_WORKER__H__
#define _TIGERA_WORKER__H__

typedef struct worker worker_t;

typedef int (*worker_prologue_t)(void *);
typedef int (*worker_epilogue_t)(void *);

int
worker_init(void);

void
worker_fini(void);

worker_t *
worker_new(void *ctx);

void
worker_free(worker_t *worker);

int
worker_start(worker_t *worker);

int
worker_stop(worker_t *worker);

int
worker_join(worker_t *worker);

void
worker_set_prologue(worker_t *worker, worker_prologue_t prologue);

void
worker_set_epilogue(worker_t *worker, worker_epilogue_t epilogue);

worker_t *
this_worker(void);

/**
 * Return event loop object per thread
 * for processing events on each thread.
 */
struct event_base;

struct event_base *
this_event_base(void);

/**
 * Return dnsbase per thread for
 * asynchronous dns resolution.
 */
struct evdns_base;

struct evdns_base *
this_dnsbase(void);


#endif /* _TIGERA_WORKER__H__ */
