#include "thread.h"
#include "includes.h"

typedef struct thread_id thread_id_t;

struct thread_id {
    pthread_t id;
};

struct thread {
    pthread_t id;
    thread_fn_t fn;
};

struct thread_key {
    pthread_key_t key;
};

typedef struct thread_arg thread_arg_t;

struct thread_arg {
    thread_t *thread;
    void *user_arg;
};

thread_arg_t *
thread_arg_new(void)
{
    thread_arg_t *thread_arg = calloc(1, sizeof(thread_arg_t));
    return thread_arg;
}

void
thread_arg_free(thread_arg_t *thread_arg)
{
    if (NULL != thread_arg) {
        thread_arg->thread = NULL;
        thread_arg->user_arg = NULL;
        free(thread_arg);
    }
}    

thread_t *
thread_new(thread_fn_t fn)
{
    thread_t *thread = calloc(1, sizeof(thread_t));
    if (NULL != thread) {
        thread->fn = fn;
    }
    return thread;
}

void
thread_free(thread_t *thread)
{
    if (NULL != thread) {
        memset(thread, 0, sizeof(thread_t));
        free(thread);
    }
}

static void *
thread_routine(void *arg)
{
    thread_arg_t *thread_arg = arg;
    thread_t *thread = thread_arg->thread;
    void *user_arg = thread_arg->user_arg;
    thread_arg_free(thread_arg);
    thread->fn(user_arg);
    pthread_exit(NULL);
}

int
thread_start(thread_t *thread, void *arg)
{
    int ret = 0;
    thread_arg_t *thread_arg = thread_arg_new();
    if (NULL == thread_arg) {
        goto error;
    }
    thread_arg->thread = thread;
    thread_arg->user_arg = arg;
    ret = pthread_create(&thread->id, NULL, thread_routine, thread_arg);
    if (0 != ret) {
        goto error;
    }
    return 0;

error:
    thread_arg_free(thread_arg);
    return -1; 
}

int
thread_join(thread_t *thread)
{
    if (0 == pthread_join(thread->id, NULL)) {
        return 0;
    }
    return -1;
}

thread_id_t *
thread_self(void)
{
    thread_id_t *thread_id = calloc(1, sizeof(thread_id_t));
    if (NULL != thread_id) {
        thread_id->id = pthread_self();
    }
    return thread_id;
} 

void
thread_id_free(thread_id_t *thread_id)
{
    free(thread_id);
}

thread_key_t *
thread_key_new(void)
{
    thread_key_t *thread_key = calloc(1, sizeof(thread_key_t));
    if (NULL != thread_key) {
        if (pthread_key_create(&thread_key->key, NULL) != 0) {
            thread_key_free(thread_key);
            thread_key = NULL;
        }
    }
    return thread_key;    
}

void
thread_key_free(thread_key_t *thread_key)
{
    if (NULL != thread_key) {
        pthread_key_delete(thread_key->key);
        memset(thread_key, 0, sizeof(thread_key_t));
        free(thread_key);
    }
}

int
thread_key_set(thread_key_t *thread_key, void *ptr)
{
    if (pthread_setspecific(thread_key->key, ptr) != 0) {
        return -1;
    }
    return 0;
}

void *
thread_key_get(thread_key_t *thread_key)
{
    return pthread_getspecific(thread_key->key);
}

void
thread_signal(thread_id_t *thread_id, thread_signal_t sig)
{
    switch (sig) {

        case THREAD_SIGTERM:
            pthread_kill(thread_id->id, SIGTERM);
            break;

        default:
            break;
    }
}
