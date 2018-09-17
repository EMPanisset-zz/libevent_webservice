#ifndef _TIGERA_MUTEX__H__
#define _TIGERA_MUTEX__H__

typedef struct mutex mutex_t;

mutex_t *
mutex_new(void);

void
mutex_free(mutex_t *mutex);

void
mutex_lock(mutex_t *mutex);

void
mutex_unlock(mutex_t *mutex);

#endif /* _TIGERA_MUTEX__H__ */
