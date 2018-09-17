#include "slist.h"

snode_t *
snode_new(void)
{
    snode_t *node = calloc(1, sizeof(snode_t));
    return node;
}

void
snode_free(snode_t *node)
{
    free(node);
}

slist_t *
slist_new(void)
{
    slist_t *list = calloc(1, sizeof(slist_t));
    return list;
}

snode_t *
slist_push_front(slist_t *list, void *data)
{
    snode_t *node = snode_new();
    if (NULL != node) {
        node->data = data;
        node->next = list->head;
        list->head = node;
    }
    return node;
}

void *
slist_pop_front(slist_t *list)
{
    void *data = NULL;
    snode_t *node = list->head;
    if (NULL != node) {
        data = node->data;
        list->head = node->next;
        snode_free(node);
    }
    return data;
}

void
slist_free(slist_t *list)
{
    while (NULL != slist_pop_front(list)) {
    }
}
