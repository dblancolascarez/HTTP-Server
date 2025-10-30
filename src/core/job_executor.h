#ifndef JOB_EXECUTOR_H

#define JOB_EXECUTOR_H

#include "queue.h"
#include "worker_pool.h"

/**
 * Inicializar el sistema de ejecución de jobs
 * 
 * Crea el worker pool global que procesará los jobs encolados
 * 
 * @param num_workers Número de workers para procesar jobs
 * @param queue_depth Profundidad máxima de la cola de jobs
 * @return 0 on success, -1 on error
 */
int job_executor_init(int num_workers, int queue_depth);

/**
 * Shutdown del executor: detiene workers y libera recursos
 */
void job_executor_shutdown();

/**
 * Encolar un job para ejecución asíncrona
 * 
 * @param task Tarea a ejecutar (toma ownership)
 * @return 0 on success, -1 si cola llena, -2 si shutdown
 */
int job_executor_enqueue(task_t *task);

/**
 * Ejecutar un comando directamente (síncrono)
 * Útil para comandos rápidos que no necesitan job
 * 
 * @param task Tarea a ejecutar
 * @return 0 on success, -1 on error
 */
int job_executor_execute_direct(task_t *task);

/**
 * Obtener métricas del executor
 */
void job_executor_get_metrics(int *queue_size, int *workers_busy, int *workers_total);

#endif // JOB_EXECUTOR_H
