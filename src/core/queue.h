#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stdbool.h>
#include <sys/time.h>

// ============================================================================
// TASK - Estructura de una tarea
// ============================================================================

typedef struct task {
    // Identificación
    char request_id[64];           // UUID único de la request
    
    // Información del cliente
    int client_fd;                 // File descriptor del socket del cliente
    
    // Información de la request HTTP
    char *path;                    // Ej: "/isprime"
    char *query;                   // Ej: "n=97"
    char *command;                 // Nombre del comando (redundante con path)
    
    // Parámetros parseados (opcional, puede ser NULL)
    void *params;                  // Puntero a estructura específica del comando
    
    // Metadata
    struct timeval enqueue_time;   // Cuándo se encoló
    int priority;                  // 0=low, 1=normal, 2=high
    
    // Para jobs asíncronos (opcional)
    char *job_id;                  // NULL si es ejecución directa
} task_t;

// ============================================================================
// QUEUE - Cola thread-safe con backpressure
// ============================================================================

// Nodo interno de la cola (lista enlazada)
typedef struct queue_node {
    task_t *task;
    struct queue_node *next;
} queue_node_t;

// Estructura principal de la cola
typedef struct queue {
    // Lista enlazada (FIFO)
    queue_node_t *head;            // Primer elemento (dequeue aquí)
    queue_node_t *tail;            // Último elemento (enqueue aquí)
    
    // Estado
    int size;                      // Número de tareas actuales
    int max_size;                  // Límite para backpressure (0 = ilimitado)
    bool shutdown;                 // Flag para terminar workers
    
    // Sincronización
    pthread_mutex_t mutex;         // Protege toda la estructura
    pthread_cond_t not_empty;      // Señal: hay tareas disponibles
    pthread_cond_t not_full;       // Señal: hay espacio disponible
    
    // Métricas (para /metrics endpoint)
    unsigned long total_enqueued;  // Total de tareas encoladas (histórico)
    unsigned long total_dequeued;  // Total de tareas desencoladas
    unsigned long total_dropped;   // Tareas rechazadas por cola llena
} queue_t;

// ============================================================================
// QUEUE - Funciones públicas
// ============================================================================

/**
 * Crear una nueva cola thread-safe
 * 
 * @param max_size Capacidad máxima (0 = ilimitado)
 * @return Puntero a la cola, o NULL si falla
 */
queue_t* queue_create(int max_size);

/**
 * Destruir cola y liberar todos los recursos
 * NOTA: Debe llamarse después de queue_shutdown() y join de todos los workers
 * 
 * @param queue Cola a destruir
 */
void queue_destroy(queue_t *queue);

/**
 * Encolar una tarea
 * 
 * Comportamiento:
 * - Si hay espacio, encola inmediatamente
 * - Si está llena y timeout_ms > 0, espera hasta timeout
 * - Si está llena y timeout_ms = 0, retorna error inmediatamente
 * - Si timeout_ms < 0, espera indefinidamente
 * 
 * @param queue Cola destino
 * @param task Tarea a encolar (la cola toma ownership)
 * @param timeout_ms Timeout en milisegundos (-1 = infinito, 0 = no esperar)
 * @return 0 = éxito, -1 = timeout/llena, -2 = shutdown activo
 */
int queue_enqueue(queue_t *queue, task_t *task, int timeout_ms);

/**
 * Desencolar una tarea (bloqueante)
 * 
 * Espera indefinidamente hasta que:
 * - Hay una tarea disponible (retorna la tarea)
 * - Se activa shutdown (retorna NULL)
 * 
 * @param queue Cola origen
 * @return Tarea desencolada, o NULL si shutdown
 */
task_t* queue_dequeue(queue_t *queue);

/**
 * Desencolar con timeout
 * 
 * @param queue Cola origen
 * @param timeout_ms Timeout en milisegundos
 * @return Tarea desencolada, o NULL si timeout o shutdown
 */
task_t* queue_dequeue_timeout(queue_t *queue, int timeout_ms);

/**
 * Obtener tamaño actual de la cola (thread-safe)
 * 
 * @param queue Cola a consultar
 * @return Número de tareas pendientes
 */
int queue_size(queue_t *queue);

/**
 * Verificar si la cola está llena (thread-safe)
 * 
 * @param queue Cola a consultar
 * @return true si está llena (size >= max_size)
 */
bool queue_is_full(queue_t *queue);

/**
 * Verificar si la cola está vacía (thread-safe)
 * 
 * @param queue Cola a consultar
 * @return true si está vacía (size == 0)
 */
bool queue_is_empty(queue_t *queue);

/**
 * Activar shutdown: despertar todos los workers
 * 
 * Después de llamar a esto:
 * - queue_dequeue() retornará NULL
 * - queue_enqueue() retornará -2
 * - Workers deben terminar su loop
 * 
 * @param queue Cola a cerrar
 */
void queue_shutdown(queue_t *queue);

/**
 * Obtener métricas de la cola (thread-safe)
 * 
 * @param queue Cola a consultar
 * @param enqueued Puntero donde guardar total encolado
 * @param dequeued Puntero donde guardar total desencolado
 * @param dropped Puntero donde guardar total rechazado
 */
void queue_get_stats(queue_t *queue, 
                     unsigned long *enqueued, 
                     unsigned long *dequeued,
                     unsigned long *dropped);

// ============================================================================
// TASK - Funciones de utilidad
// ============================================================================

/**
 * Crear una nueva tarea
 * 
 * @param client_fd File descriptor del cliente
 * @param path Path HTTP (se copia internamente)
 * @param query Query string (se copia internamente)
 * @param request_id Request ID único
 * @return Puntero a tarea, o NULL si falla
 */
task_t* task_create(int client_fd, const char *path, const char *query, const char *request_id);

/**
 * Liberar una tarea y todos sus recursos
 * 
 * @param task Tarea a liberar
 */
void task_free(task_t *task);

#endif // QUEUE_H