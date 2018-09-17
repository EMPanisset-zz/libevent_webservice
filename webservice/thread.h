#ifndef _TIGERA_THREAD__H__
#define _TIGERA_THREAD__H__

typedef enum thread_signal thread_signal_t;

enum thread_signal {
    THREAD_SIGTERM
};

typedef struct thread_id thread_id_t;

typedef struct thread thread_t;

typedef struct thread_key thread_key_t;

typedef void (*thread_fn_t)(void *);

thread_t *
thread_new(thread_fn_t fn);

void
thread_free(thread_t *thread);

int
thread_start(thread_t *thread, void *arg);

int
thread_join(thread_t *thread);

thread_id_t *
thread_self(void);

void
thread_signal(thread_id_t *thread_id, thread_signal_t sig);

void
thread_id_free(thread_id_t *thread_id);

thread_key_t *
thread_key_new(void);

void
thread_key_free(thread_key_t *key);

int
thread_key_set(thread_key_t *key, void *ptr);

void *
thread_key_get(thread_key_t *key);

#endif /* _TIGERA_THREAD__H__ */
