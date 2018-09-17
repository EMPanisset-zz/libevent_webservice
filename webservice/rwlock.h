#ifndef _TIGERA_RWLOCK__H_
#define _TIGERA_RWLOCK__H_

typedef struct rwlock rwlock_t;

rwlock_t *
rwlock_new(void);

void
rwlock_free(rwlock_t *rwlock);

int
rwlock_rdlock(rwlock_t *rwlock);

int
rwlock_wrlock(rwlock_t *rwlock);

int
rwlock_unlock(rwlock_t *rwlock);

#endif /* _TIGERA_RWLOCK__H_ */
