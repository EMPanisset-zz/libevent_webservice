#ifndef _TIGERA_ATOMIC__H__
#define _TIGERA_ATOMIC__H__

typedef long atomic_t;

static
inline
atomic_t
atomic_inc(atomic_t *atomic)
{
    return __sync_add_and_fetch(atomic, 1);
}

static
inline
atomic_t
atomic_dec(atomic_t *atomic)
{
    return __sync_sub_and_fetch(atomic, 1);
}

#endif /* __TIGERA_ATOMIC__H__ */
