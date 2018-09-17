#include "list.h"

node_t *
node_new(void)
{
    node_t *node = calloc(1, sizeof(node_t));
    return node;
};

void
node_free(node_t *node)
{
    if (NULL != node) {
        node->next = NULL;
        node->previous = NULL;
        node->data = NULL;
        free(node);
    }
}

list_t *
list_new(void)
{
    list_t *list = calloc(1, sizeof(list_t));
    if (NULL != list) {
        list->tail = &list->head;
    }
    return list;
}

void *
list_remove(list_t *list, node_t *node)
{
    void *data = NULL;
    if (NULL != node) {
        data = node->data;
        *node->previous = node->next;
        if (NULL != node->next) {
            node->next->previous = node->previous;
        }
        if (list->tail == &node->next) {
            list->tail = node->previous;
        }
        node_free(node); 
    }
    return data;
}

void
list_push_front(list_t *list, void *data)
{
    node_t *node = node_new();
    node->data = data;
    node->next = list->head;
    node->previous = &list->head;
    if (NULL != list->head) {
        list->head->previous = &node->next;
    }
    else {
        list->tail = &node->next;
    }
    list->head = node;
}

void
list_push_back(list_t *list, void *data)
{
    node_t *node = node_new();
    node->data = data;
    *list->tail = node;
    node->previous = list->tail;
    list->tail = &node->next;
}

void
list_push_before(list_t *list, node_t *next, void *data)
{
    if (NULL == next) {
        list_push_back(list, data);
        return;
    }
    node_t *node = node_new();
    node->data = data;
    node->next = next;
    node->previous = next->previous;
    *next->previous = node;
    next->previous = &node->next;
}

void *
list_pop_front(list_t *list)
{
    return list_remove(list, list->head);
}

void
list_free(list_t *list)
{
    if (NULL != list) {
        while (NULL != list_pop_front(list)) {
        }
        free(list);
    }
}
