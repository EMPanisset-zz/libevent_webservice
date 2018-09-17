#include "includes.h"
#include "libs.h"
#include "worker.h"
#include <assert.h>

#define NR_WORKERS  100

static int counter[NR_WORKERS];

static int 
prologue(void *ctx)
{
    int *i = ctx;
    ++(*i);
    return 0;
}

static int
epilogue(void *ctx)
{
    int *i = ctx;
    --(*i);

    return 0;
}


int
main(int argc, char **argv)
{
	worker_t *workers[NR_WORKERS];

    worker_init();

	for (int i = 0; i < countof(workers); ++i) {
		workers[i] = worker_new(&counter[i]);
        worker_set_prologue(workers[i], prologue);
        worker_set_epilogue(workers[i], epilogue);
		worker_start(workers[i]);
	}

	for (int i = countof(workers) - 1; i >= 0; --i) {
		worker_stop(workers[i]);
        assert(0 == counter[i]);
		worker_free(workers[i]);
	}

    worker_fini();
    return 0;
}
