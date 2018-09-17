#include "rwlock.h"
#include "includes.h"

struct rwlock {
    pthread_rwlock_t lock;
};

rwlock_t *
rwlock_new(void)
{
    rwlock_t *rwlock = calloc(1, sizeof(rwlock_t));
    if (NULL != rwlock) {
        int ret = pthread_rwlock_init(&rwlock->lock, NULL);
        if (0 != ret) {
            free(rwlock);
            rwlock = NULL;
        }
    }
	return rwlock;
}

void
rwlock_free(rwlock_t *rwlock)
{
    if (NULL != rwlock) {
        pthread_rwlock_destroy(&rwlock->lock);
        free(rwlock);
    }
}

int
rwlock_rdlock(rwlock_t *rwlock)
{
    int ret = pthread_rwlock_rdlock(&rwlock->lock);
    if (0 != ret) {
        return -1;
    }
    return 0;
}

int
rwlock_wrlock(rwlock_t *rwlock)
{
    int ret = pthread_rwlock_wrlock(&rwlock->lock);
    if (0 != ret) {
        return -1;
    }
    return 0;
}

int
rwlock_unlock(rwlock_t *rwlock)
{
    int ret = pthread_rwlock_unlock(&rwlock->lock);
    if (0 != ret) {
        return -1;
    }
    return 0;
}
