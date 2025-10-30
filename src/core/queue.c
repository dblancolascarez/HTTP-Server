#include "queue.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

// ============================================================================
// FUNCIONES PRIVADAS (HELPERS)
// ============================================================================

/**
 * Calcular timespec para pthread_cond_timedwait
 * Convierte timeout relativo (ms) a timespec absoluto
 */
static void calculate_timeout_ts(struct timespec *ts, int timeout_ms) {
    clock_gettime(CLOCK_REALTIME, ts);
    
    // Agregar milisegundos
    long add_sec = timeout_ms / 1000;
    long add_nsec = (timeout_ms % 1000) * 1000000L;
    
    ts->tv_sec += add_sec;
    ts->tv_nsec += add_nsec;
    
    // Normalizar si nsec >= 1 segundo
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000L;
    }
}

// ============================================================================
// QUEUE - CREAR Y DESTRUIR
// ============================================================================

queue_t* queue_create(int max_size) {
    queue_t *queue = (queue_t*)malloc(sizeof(queue_t));
    if (!queue) {
        return NULL;
    }
    
    // Inicializar estructura
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    queue->max_size = max_size;
    queue->shutdown = false;
    queue->total_enqueued = 0;
    queue->total_dequeued = 0;
    queue->total_dropped = 0;
    
    // Inicializar primitivas de sincronización
    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        free(queue);
        return NULL;
    }
    
    if (pthread_cond_init(&queue->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&queue->mutex);
        free(queue);
        return NULL;
    }
    
    if (pthread_cond_init(&queue->not_full, NULL) != 0) {
        pthread_cond_destroy(&queue->not_empty);
        pthread_mutex_destroy(&queue->mutex);
        free(queue);
        return NULL;
    }
    
    return queue;
}

void queue_destroy(queue_t *queue) {
    if (!queue) return;
    
    pthread_mutex_lock(&queue->mutex);
    
    // Liberar todas las tareas pendientes
    queue_node_t *current = queue->head;
    while (current) {
        queue_node_t *next = current->next;
        task_free(current->task);
        free(current);
        current = next;
    }
    
    pthread_mutex_unlock(&queue->mutex);
    
    // Destruir primitivas
    pthread_cond_destroy(&queue->not_full);
    pthread_cond_destroy(&queue->not_empty);
    pthread_mutex_destroy(&queue->mutex);
    
    free(queue);
}

// ============================================================================
// QUEUE - ENQUEUE
// ============================================================================

int queue_enqueue(queue_t *queue, task_t *task, int timeout_ms) {
    if (!queue || !task) {
        return -1;
    }
    
    pthread_mutex_lock(&queue->mutex);
    
    // Verificar shutdown
    if (queue->shutdown) {
        pthread_mutex_unlock(&queue->mutex);
        return -2;
    }
    
    // Backpressure: esperar si está llena (solo si max_size > 0)
    if (queue->max_size > 0) {
        if (timeout_ms > 0) {
            // Esperar con timeout
            struct timespec ts;
            calculate_timeout_ts(&ts, timeout_ms);
            
            while (queue->size >= queue->max_size && !queue->shutdown) {
                int ret = pthread_cond_timedwait(&queue->not_full, &queue->mutex, &ts);
                if (ret == ETIMEDOUT) {
                    queue->total_dropped++;
                    pthread_mutex_unlock(&queue->mutex);
                    return -1; // Timeout: cola sigue llena
                }
            }
        } else if (timeout_ms == 0) {
            // No esperar: verificar inmediatamente
            if (queue->size >= queue->max_size) {
                queue->total_dropped++;
                pthread_mutex_unlock(&queue->mutex);
                return -1; // Cola llena
            }
        } else {
            // timeout_ms < 0: esperar indefinidamente
            while (queue->size >= queue->max_size && !queue->shutdown) {
                pthread_cond_wait(&queue->not_full, &queue->mutex);
            }
        }
        
        // Verificar shutdown después de esperar
        if (queue->shutdown) {
            pthread_mutex_unlock(&queue->mutex);
            return -2;
        }
    }
    
    // Crear nodo
    queue_node_t *node = (queue_node_t*)malloc(sizeof(queue_node_t));
    if (!node) {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }
    
    node->task = task;
    node->next = NULL;
    
    // Agregar timestamp de enqueue
    gettimeofday(&task->enqueue_time, NULL);
    
    // Insertar al final de la cola (FIFO)
    if (queue->tail) {
        queue->tail->next = node;
        queue->tail = node;
    } else {
        // Cola vacía
        queue->head = node;
        queue->tail = node;
    }
    
    queue->size++;
    queue->total_enqueued++;
    
    // Señalar que hay una tarea disponible
    pthread_cond_signal(&queue->not_empty);
    
    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

// ============================================================================
// QUEUE - DEQUEUE
// ============================================================================

task_t* queue_dequeue(queue_t *queue) {
    if (!queue) return NULL;
    
    pthread_mutex_lock(&queue->mutex);
    
    // Esperar hasta que haya una tarea o se active shutdown
    while (queue->size == 0 && !queue->shutdown) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }
    
    // Si shutdown y cola vacía, terminar
    if (queue->shutdown && queue->size == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    
    // Extraer primer nodo (FIFO)
    queue_node_t *node = queue->head;
    task_t *task = node->task;
    
    queue->head = node->next;
    if (!queue->head) {
        // Era el último elemento
        queue->tail = NULL;
    }
    
    free(node);
    queue->size--;
    queue->total_dequeued++;
    
    // Señalar que hay espacio disponible
    if (queue->max_size > 0) {
        pthread_cond_signal(&queue->not_full);
    }
    
    pthread_mutex_unlock(&queue->mutex);
    return task;
}

task_t* queue_dequeue_timeout(queue_t *queue, int timeout_ms) {
    if (!queue) return NULL;
    
    pthread_mutex_lock(&queue->mutex);
    
    // Calcular timeout absoluto
    struct timespec ts;
    calculate_timeout_ts(&ts, timeout_ms);
    
    // Esperar hasta que haya una tarea, timeout, o shutdown
    while (queue->size == 0 && !queue->shutdown) {
        int ret = pthread_cond_timedwait(&queue->not_empty, &queue->mutex, &ts);
        if (ret == ETIMEDOUT) {
            pthread_mutex_unlock(&queue->mutex);
            return NULL; // Timeout
        }
    }
    
    // Si shutdown y cola vacía, terminar
    if (queue->shutdown && queue->size == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    
    // Extraer tarea
    queue_node_t *node = queue->head;
    task_t *task = node->task;
    
    queue->head = node->next;
    if (!queue->head) {
        queue->tail = NULL;
    }
    
    free(node);
    queue->size--;
    queue->total_dequeued++;
    
    if (queue->max_size > 0) {
        pthread_cond_signal(&queue->not_full);
    }
    
    pthread_mutex_unlock(&queue->mutex);
    return task;
}

// ============================================================================
// QUEUE - CONSULTAS
// ============================================================================

int queue_size(queue_t *queue) {
    if (!queue) return 0;
    
    pthread_mutex_lock(&queue->mutex);
    int size = queue->size;
    pthread_mutex_unlock(&queue->mutex);
    
    return size;
}

bool queue_is_full(queue_t *queue) {
    if (!queue || queue->max_size <= 0) return false;
    
    pthread_mutex_lock(&queue->mutex);
    bool full = (queue->size >= queue->max_size);
    pthread_mutex_unlock(&queue->mutex);
    
    return full;
}

bool queue_is_empty(queue_t *queue) {
    if (!queue) return true;
    
    pthread_mutex_lock(&queue->mutex);
    bool empty = (queue->size == 0);
    pthread_mutex_unlock(&queue->mutex);
    
    return empty;
}

void queue_get_stats(queue_t *queue, 
                     unsigned long *enqueued, 
                     unsigned long *dequeued,
                     unsigned long *dropped) {
    if (!queue) return;
    
    pthread_mutex_lock(&queue->mutex);
    
    if (enqueued) *enqueued = queue->total_enqueued;
    if (dequeued) *dequeued = queue->total_dequeued;
    if (dropped) *dropped = queue->total_dropped;
    
    pthread_mutex_unlock(&queue->mutex);
}

// ============================================================================
// QUEUE - SHUTDOWN
// ============================================================================

void queue_shutdown(queue_t *queue) {
    if (!queue) return;
    
    pthread_mutex_lock(&queue->mutex);
    queue->shutdown = true;
    
    // Despertar TODOS los threads esperando
    pthread_cond_broadcast(&queue->not_empty);  // Workers en dequeue
    pthread_cond_broadcast(&queue->not_full);   // Productores en enqueue
    
    pthread_mutex_unlock(&queue->mutex);
}

// ============================================================================
// TASK - FUNCIONES DE UTILIDAD
// ============================================================================

task_t* task_create(int client_fd, const char *path, const char *query, const char *request_id) {
    task_t *task = (task_t*)malloc(sizeof(task_t));
    if (!task) return NULL;
    
    // Copiar request_id
    strncpy(task->request_id, request_id, sizeof(task->request_id) - 1);
    task->request_id[sizeof(task->request_id) - 1] = '\0';
    
    // Asignar valores
    task->client_fd = client_fd;
    
    // Copiar path y query
    task->path = path ? strdup(path) : NULL;
    task->query = query ? strdup(query) : NULL;
    task->command = NULL; // Se puede asignar después
    
    // Inicializar opcionales
    task->params = NULL;
    task->priority = 1; // Normal por defecto
    task->job_id = NULL;
    
    // Timestamp se asigna en queue_enqueue
    memset(&task->enqueue_time, 0, sizeof(task->enqueue_time));
    
    return task;
}

void task_free(task_t *task) {
    if (!task) return;
    
    free(task->path);
    free(task->query);
    free(task->command);
    free(task->job_id);
    
    // params debe ser liberado por quien lo creó
    // (cada comando sabe cómo liberar sus params)
    if (task->params) {
        free(task->params);
    }
    
    free(task);
}