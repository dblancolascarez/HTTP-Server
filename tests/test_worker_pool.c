#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "../src/core/queue.h"
#include "../src/core/worker_pool.h"

// Simple handler that sleeps a bit and increments a counter in context
typedef struct {
    pthread_mutex_t mutex;
    int processed;
} ctx_t;

int simple_handler(task_t *task, void *user_ctx) {
    ctx_t *ctx = (ctx_t*)user_ctx;
    // Simulate light work
    usleep(1000 * 10); // 10ms
    pthread_mutex_lock(&ctx->mutex);
    ctx->processed++;
    pthread_mutex_unlock(&ctx->mutex);
    return 0;
}

int main(void) {
    // Crear cola con capacidad suficiente
    queue_t *q = queue_create(0); // ilimitada
    if (!q) { fprintf(stderr, "queue_create failed\n"); return 1; }

    ctx_t ctx;
    pthread_mutex_init(&ctx.mutex, NULL);
    ctx.processed = 0;

    worker_pool_t *pool = worker_pool_create(4, q, simple_handler, &ctx);
    assert(pool != NULL);

    int rc = worker_pool_start(pool);
    assert(rc == 0);

    const int N = 20;
    for (int i = 0; i < N; ++i) {
        char rid[32];
        snprintf(rid, sizeof(rid), "req-%d", i);
        task_t *t = task_create(-1, "/test", NULL, rid);
        int enq = queue_enqueue(q, t, -1);
        if (enq != 0) {
            fprintf(stderr, "enqueue failed: %d\n", enq);
            task_free(t);
        }
    }

    // Esperar a que se procesen al menos N
    int waited = 0;
    while (1) {
        pthread_mutex_lock(&ctx.mutex);
        int p = ctx.processed;
        pthread_mutex_unlock(&ctx.mutex);
        if (p >= N) break;
        usleep(1000 * 50);
        waited += 50;
        if (waited > 5000) break; // 5s timeout
    }

    pthread_mutex_lock(&ctx.mutex);
    int processed = ctx.processed;
    pthread_mutex_unlock(&ctx.mutex);

    if (processed != N) {
        fprintf(stderr, "processed %d != %d\n", processed, N);
        return 2;
    }

    // Stop and destroy
    worker_pool_destroy(pool);
    queue_destroy(q);

    printf("All tasks processed: %d\n", processed);
    return 0;
}
