#ifndef WORKER_POOL_H
#define WORKER_POOL_H

#include <pthread.h>
#include <stdbool.h>
#include "queue.h"

// Handler callback invoked by a worker for each dequeued task.
// The function should process the task and return 0 on success, non-zero on error.
typedef int (*worker_handler_t)(task_t *task, void *user_ctx);

typedef struct worker_pool {
    // Pool configuration
    int total_workers;         // cantidad total de hilos
    queue_t *queue;            // cola de tareas asociada

    // Runtime state
    pthread_t *threads;        // array de hilos
    volatile int busy_workers; // número de workers ocupados (acceso atomico/inmutable bajo mutex)
    pthread_mutex_t mutex;     // protege estado interno y condvar
    pthread_cond_t cond;       // usado para signal de shutdown
    bool shutdown;             // flag para indicar terminación

    // Handler para procesar tareas
    worker_handler_t handler;
    void *handler_ctx;
} worker_pool_t;

// Crear pool. La función toma ownership de la cola pointer pero NO la destruye al destruir el pool.
// total_workers: número de hilos a lanzar
// queue: cola compartida donde se extraen tareas
// handler: callback que procesa cada tarea
// handler_ctx: contexto pasado al handler
worker_pool_t *worker_pool_create(int total_workers, queue_t *queue, worker_handler_t handler, void *handler_ctx);

// Iniciar (lanza los hilos). Retorna 0 ok, -1 error
int worker_pool_start(worker_pool_t *pool);

// Detener el pool: marcar shutdown y esperar join de hilos
void worker_pool_stop(worker_pool_t *pool);

// Destruir pool y liberar recursos. No destruye la cola.
void worker_pool_destroy(worker_pool_t *pool);

// Obtener métricas
int worker_pool_get_total(worker_pool_t *pool);
int worker_pool_get_busy(worker_pool_t *pool);

#endif // WORKER_POOL_H
