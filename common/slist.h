#ifndef _TIGERA_SLIST__H__
#define _TIGERA_SLIST__H__

#include "includes.h"

typedef struct snode snode_t;

struct snode {
    snode_t *next;
    void *data;
};

typedef struct slist slist_t;

struct slist {
    snode_t *head;
};

snode_t *
snode_new(void);

void
snode_free(snode_t *node);

static inline void *
snode_data(snode_t *node)
{
    return node->data;
}

#define slist_foreach(list, node) \
    for ((node) = (list)->head; \
         (NULL != (node)); \
         (node) = (node)->next)

slist_t *
slist_new(void);

snode_t *
slist_push_front(slist_t *list, void *data);

void *
slist_pop_front(slist_t *list);

void
slist_free(slist_t *list);

static inline snode_t *
slist_head(slist_t *list)
{
    return list->head;
}

static inline snode_t *
slist_next(slist_t *list, snode_t *node)
{
    return node->next;
}

#endif /* _TIGERA_SLIST__H__ */
