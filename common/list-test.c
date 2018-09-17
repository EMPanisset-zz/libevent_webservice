#include "list.h"

typedef struct item item_t;

struct item {
    int value;
};

item_t *
item_new(void)
{
    item_t *item = calloc(1, sizeof(item_t));
    return item;
}

void
item_free(item_t *item)
{
    free(item);
}

int main(int argc, char **argv)
{
    node_t *node, *next;
    item_t *item = NULL;
    list_t *list = list_new();

    if (NULL == list) {
        return -1;
    }

    int i;
    for (i = 0; i < 10; ++i) {
        item = item_new();
        item->value = i;
        list_push_back(list, item);
    }

    list_foreach_safe(list, node, next) {
        item = list_remove(list, node);
        item->value += 1;
        list_push_before(list, next, item);
    }

    while (NULL != (item = list_pop_front(list))) {
        item_free(item);
    }

    for (i = 0; i < 1000; ++i) {
        item = item_new();
        item->value = i;
        list_push_front(list, item);
    }

    while (NULL != (item = list_pop_front(list))) {
        fprintf(stdout, "%d\n", item->value);
        item_free(item);
    }

    list_free(list);

    return 0;
}
