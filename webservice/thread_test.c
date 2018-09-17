#include "includes.h"
#include "thread.h"
#include "rwlock.h"
#include <assert.h>

static thread_key_t *thread_key = NULL;

typedef struct user_arg user_arg_t;

struct user_arg {
    rwlock_t *rwlock;
    int counter;
};

user_arg_t *
user_arg_new(void)
{
    user_arg_t *user_arg = calloc(1, sizeof(user_arg_t));
    return user_arg;
}

void
user_arg_free(user_arg_t *user_arg)
{
    free(user_arg);
}

static void
thread_run(void *arg)
{
    user_arg_t *user_arg = arg;
    rwlock_t *rwlock = user_arg->rwlock;
    int counter = user_arg->counter;

    thread_key_set(thread_key, user_arg);

    if (counter % 2) {
        rwlock_rdlock(rwlock);
        printf("rd thread unlocked\n");
    }
    else {
        rwlock_wrlock(rwlock);
        printf("wr thread unlocked\n");
    }
    rwlock_unlock(rwlock);

    assert(thread_key_get(thread_key) == user_arg);
    user_arg_free(user_arg);
}

int
main(int argc, char **argv)
{
    user_arg_t *user_arg = NULL;
    thread_t *threads[100];
    rwlock_t *rwlock = rwlock_new();

    thread_key = thread_key_new();

    for (int i = 0; i < countof(threads); ++i) {
        threads[i] = thread_new(thread_run);
        user_arg = user_arg_new();
        user_arg->rwlock = rwlock;
        user_arg->counter = i;
        thread_start(threads[i], user_arg);
    }

    rwlock_wrlock(rwlock);
    printf("wr thread unlocked\n");
    rwlock_unlock(rwlock);

    for (int i = 0; i < countof(threads); ++i) {
        thread_join(threads[i]);
        thread_free(threads[i]);
    }

    rwlock_free(rwlock);

    thread_key_free(thread_key);

    return 0;
}
