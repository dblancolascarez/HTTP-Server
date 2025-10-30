#include "worker_pool.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

static void *worker_thread_fn(void *arg) {
    worker_pool_t *pool = (worker_pool_t*)arg;

    while (1) {
        // Dequeue a task (bloqueante)
        task_t *task = queue_dequeue(pool->queue);

        // Si shutdown y cola vacía, queue_dequeue puede retornar NULL
        if (!task) {
            // Revisar shutdown y salir si solicitado
            pthread_mutex_lock(&pool->mutex);
            bool should_shutdown = pool->shutdown;
            pthread_mutex_unlock(&pool->mutex);
            if (should_shutdown) break;
            // Si no está shutdown, continue (defensive)
            continue;
        }

        // Mark busy
        pthread_mutex_lock(&pool->mutex);
        pool->busy_workers++;
        pthread_mutex_unlock(&pool->mutex);

        // Ejecutar handler
        if (pool->handler) {
            int rc = pool->handler(task, pool->handler_ctx);
            (void)rc; // handler decide qué hacer con errores
        }

        // Liberar task si corresponde (asumimos ownership de la cola sobre task)
        task_free(task);

        // Mark not busy
        pthread_mutex_lock(&pool->mutex);
        pool->busy_workers--;
        // Notify anyone waiting
        if (pool->busy_workers == 0) pthread_cond_broadcast(&pool->cond);
        pthread_mutex_unlock(&pool->mutex);

        // Verificar shutdown
        pthread_mutex_lock(&pool->mutex);
        bool sd = pool->shutdown;
        pthread_mutex_unlock(&pool->mutex);
        if (sd && queue_is_empty(pool->queue)) break;
    }

    return NULL;
}

worker_pool_t *worker_pool_create(int total_workers, queue_t *queue, worker_handler_t handler, void *handler_ctx) {
    if (total_workers <= 0 || !queue || !handler) return NULL;

    worker_pool_t *pool = (worker_pool_t*)malloc(sizeof(worker_pool_t));
    if (!pool) return NULL;

    pool->total_workers = total_workers;
    pool->queue = queue;
    pool->handler = handler;
    pool->handler_ctx = handler_ctx;
    pool->shutdown = false;
    pool->busy_workers = 0;
    pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * total_workers);
    if (!pool->threads) { free(pool); return NULL; }

    if (pthread_mutex_init(&pool->mutex, NULL) != 0) {
        free(pool->threads);
        free(pool);
        return NULL;
    }
    if (pthread_cond_init(&pool->cond, NULL) != 0) {
        pthread_mutex_destroy(&pool->mutex);
        free(pool->threads);
        free(pool);
        return NULL;
    }

    return pool;
}

int worker_pool_start(worker_pool_t *pool) {
    if (!pool) return -1;

    for (int i = 0; i < pool->total_workers; ++i) {
        int rc = pthread_create(&pool->threads[i], NULL, worker_thread_fn, pool);
        if (rc != 0) {
            // Mark shutdown and join created threads
            pthread_mutex_lock(&pool->mutex);
            pool->shutdown = true;
            pthread_mutex_unlock(&pool->mutex);
            for (int j = 0; j < i; ++j) pthread_join(pool->threads[j], NULL);
            return -1;
        }
    }
    return 0;
}

void worker_pool_stop(worker_pool_t *pool) {
    if (!pool) return;

    pthread_mutex_lock(&pool->mutex);
    pool->shutdown = true;
    pthread_mutex_unlock(&pool->mutex);

    // Wake up any blocked dequeues
    queue_shutdown(pool->queue);

    // Join threads
    for (int i = 0; i < pool->total_workers; ++i) {
        pthread_join(pool->threads[i], NULL);
    }
}

void worker_pool_destroy(worker_pool_t *pool) {
    if (!pool) return;

    // Pool should be stopped before destroy
    worker_pool_stop(pool);

    pthread_cond_destroy(&pool->cond);
    pthread_mutex_destroy(&pool->mutex);
    free(pool->threads);
    free(pool);
}

int worker_pool_get_total(worker_pool_t *pool) {
    if (!pool) return 0;
    return pool->total_workers;
}

int worker_pool_get_busy(worker_pool_t *pool) {
    if (!pool) return 0;
    pthread_mutex_lock(&pool->mutex);
    int b = pool->busy_workers;
    pthread_mutex_unlock(&pool->mutex);
    return b;
}
