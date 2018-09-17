#ifndef _TIGERA_REFERENCE__H__
#define _TIGERA_REFERENCE__H__

#include "includes.h"

typedef void (*free_fn_t)(void *);

typedef struct reference reference_t;

struct reference {
    void *ptr;
    int refcnt;
    int weak_refcnt;
    free_fn_t free_fn;
};

static inline reference_t *
reference_new(void *ptr, free_fn_t free_fn)
{
    reference_t *ref = calloc(1, sizeof(*ref));
    if (NULL != ref) {
        ref->ptr = ptr;
        ref->refcnt = 1;
        ref->free_fn = free_fn;
    }
    return ref;
}

static inline int 
reference_dec(reference_t *ref)
{
    if (ref != NULL) {
        if (--ref->refcnt == 0) {
            if (ref->free_fn) {
                ref->free_fn(ref->ptr);
            }
            ref->ptr = NULL;
            if (ref->weak_refcnt == 0) {
                free(ref);
            }
            return 0;
        }
        return ref->refcnt;
    }
    return 0;
}

static inline int 
reference_weak_dec(reference_t *ref)
{
    if (ref != NULL) {
        if (--ref->weak_refcnt == 0) {
            if (ref->ptr == NULL) {
                free(ref);
            }
            return 0;
        }
        return ref->weak_refcnt;
    }
    return 0;
}

static inline int
reference_inc(reference_t *ref)
{
    if (ref != NULL) {
        return ++ref->refcnt;
    }
    return 0;
}

static inline int
reference_weak_inc(reference_t *ref)
{
    if (ref != NULL) {
        return ++ref->weak_refcnt;
    }
    return 0;
}

static inline void *
reference_get(reference_t *ref)
{
    if (ref != NULL) {
        return ref->ptr;
    }
    return NULL;
}

#endif /* _TIGERA_REFERENCE__H_ */
