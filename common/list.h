#ifndef _TIGERA_LIST__H__
#define _TIGERA_LIST__H__

#include "includes.h"

typedef struct node node_t;

struct node {
    node_t *next;
    node_t **previous;
    void *data;
};

typedef struct list list_t;

struct list {
    node_t *head;
    node_t **tail;
};

node_t *
node_new(void);

void
node_free(node_t *node);

static inline void *
node_data(node_t *node)
{
    return node->data;
}

#define list_foreach(list, node) \
    for ((node) = (list)->head; \
         (NULL != (node)); \
         (node) = (node)->next)

#define list_foreach_safe(list, node, next) \
    for ((node) = (list)->head; \
         (NULL != (node)) && ((next) = (node)->next, 1); \
         (node) = (next))

list_t *
list_new(void);

void *
list_remove(list_t *list, node_t *node);

void
list_push_front(list_t *list, void *data);

void
list_push_back(list_t *list, void *data);

void
list_push_before(list_t *list, node_t *next, void *data);

void *
list_pop_front(list_t *list);

void
list_free(list_t *list);

static inline node_t *
list_head(list_t *list)
{
    return list->head;
}

static inline node_t *
list_tail(list_t *list)
{
    if (list->tail == &list->head) {
        return NULL;
    }
    return downcast(list->tail, node_t, next);
}

static inline node_t *
list_next(list_t *list, node_t *node)
{
    return node->next;
}

static inline node_t *
list_previous(list_t *list, node_t *node)
{
    if (node->previous == &list->head) {
        return NULL;
    }
    return downcast(node->previous, node_t, next);
}

#endif /* _TIGERA_LIST__H__ */
