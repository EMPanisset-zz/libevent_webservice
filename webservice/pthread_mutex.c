#include "includes.h"
#include "mutex.h"

struct mutex {
    pthread_mutex_t lock;
};

mutex_t *
mutex_new(void)
{
    mutex_t *mutex = calloc(1, sizeof(mutex_t));
    if (NULL != mutex) {
        pthread_mutex_init(&mutex->lock, NULL);
    }
    return mutex;
}

void
mutex_free(mutex_t *mutex)
{
    if (NULL != mutex) {
        pthread_mutex_destroy(&mutex->lock);
        free(mutex);
    }
}

void
mutex_lock(mutex_t *mutex)
{
    pthread_mutex_lock(&mutex->lock);
}

void
mutex_unlock(mutex_t *mutex)
{
    pthread_mutex_unlock(&mutex->lock);
}
