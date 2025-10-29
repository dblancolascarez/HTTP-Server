#ifndef METRICS_H
#define METRICS_H

#include <pthread.h>
#include <sys/time.h>
#include <stdbool.h>

// ============================================================================
// COMMAND METRICS - Métricas por comando
// ============================================================================

typedef struct {
    char command_name[64];          // Nombre del comando (ej: "isprime")
    
    // Métricas de tiempo (en microsegundos)
    unsigned long total_wait_time_us;     // Tiempo total en cola
    unsigned long total_exec_time_us;     // Tiempo total de ejecución
    unsigned long count;                   // Número de ejecuciones
    
    // Arrays para calcular desviación estándar (últimas N mediciones)
    double *wait_times_ms;          // Circular buffer de tiempos de espera
    double *exec_times_ms;          // Circular buffer de tiempos de ejecución
    int buffer_size;                // Tamaño del buffer (ej: 1000)
    int buffer_index;               // Índice actual en buffer circular
    int buffer_count;               // Cuántos elementos hay (max = buffer_size)
    
    // Métricas de cola
    int current_queue_size;         // Tamaño actual de la cola
    int max_queue_size;             // Máximo histórico
    
    // Métricas de workers
    int total_workers;              // Total de workers para este comando
    int busy_workers;               // Workers actualmente ocupados
    
    // Thread safety
    pthread_mutex_t mutex;
    
} command_metrics_t;

// ============================================================================
// GLOBAL METRICS MANAGER
// ============================================================================

#define MAX_COMMANDS 32

typedef struct {
    command_metrics_t commands[MAX_COMMANDS];
    int num_commands;
    pthread_mutex_t mutex;
    
    // Métricas globales del servidor
    struct timeval start_time;
    unsigned long total_requests;
    unsigned long total_errors;
    
} metrics_manager_t;

// ============================================================================
// FUNCIONES DE INICIALIZACIÓN
// ============================================================================

/**
 * Inicializar sistema de métricas global
 */
void metrics_init();

/**
 * Destruir sistema de métricas
 */
void metrics_destroy();

/**
 * Registrar un nuevo comando para tracking
 * 
 * @param command_name Nombre del comando (ej: "isprime")
 * @param num_workers Número de workers para este comando
 * @param queue_capacity Capacidad máxima de la cola
 * @param buffer_size Tamaño del buffer para cálculo de std dev (ej: 1000)
 * @return Índice del comando o -1 si error
 */
int metrics_register_command(const char *command_name, int num_workers, 
                             int queue_capacity, int buffer_size);

// ============================================================================
// FUNCIONES DE REGISTRO DE MÉTRICAS
// ============================================================================

/**
 * Registrar tiempo de espera en cola
 * 
 * @param command_name Nombre del comando
 * @param wait_time_us Tiempo de espera en microsegundos
 */
void metrics_record_wait_time(const char *command_name, unsigned long wait_time_us);

/**
 * Registrar tiempo de ejecución
 * 
 * @param command_name Nombre del comando
 * @param exec_time_us Tiempo de ejecución en microsegundos
 */
void metrics_record_exec_time(const char *command_name, unsigned long exec_time_us);

/**
 * Actualizar tamaño de cola
 * 
 * @param command_name Nombre del comando
 * @param queue_size Tamaño actual de la cola
 */
void metrics_update_queue_size(const char *command_name, int queue_size);

/**
 * Actualizar workers ocupados
 * 
 * @param command_name Nombre del comando
 * @param busy_count Número de workers ocupados
 */
void metrics_update_workers(const char *command_name, int busy_count);

/**
 * Incrementar contador de requests totales
 */
void metrics_increment_requests();

/**
 * Incrementar contador de errores
 */
void metrics_increment_errors();

// ============================================================================
// FUNCIONES DE CONSULTA
// ============================================================================

/**
 * Obtener métricas de un comando específico
 * 
 * @param command_name Nombre del comando
 * @param metrics Puntero donde copiar las métricas (thread-safe)
 * @return 0 si éxito, -1 si no encontrado
 */
int metrics_get_command(const char *command_name, command_metrics_t *metrics);

/**
 * Calcular tiempo promedio de espera (en ms)
 * 
 * @param command_name Nombre del comando
 * @return Tiempo promedio en ms, o -1.0 si error
 */
double metrics_get_avg_wait_time_ms(const char *command_name);

/**
 * Calcular tiempo promedio de ejecución (en ms)
 * 
 * @param command_name Nombre del comando
 * @return Tiempo promedio en ms, o -1.0 si error
 */
double metrics_get_avg_exec_time_ms(const char *command_name);

/**
 * Calcular desviación estándar de tiempo de espera
 * 
 * @param command_name Nombre del comando
 * @return Desviación estándar en ms, o -1.0 si error
 */
double metrics_get_stddev_wait_time_ms(const char *command_name);

/**
 * Calcular desviación estándar de tiempo de ejecución
 * 
 * @param command_name Nombre del comando
 * @return Desviación estándar en ms, o -1.0 si error
 */
double metrics_get_stddev_exec_time_ms(const char *command_name);

/**
 * Obtener todas las métricas en formato JSON
 * 
 * @param buffer Buffer donde escribir el JSON
 * @param buffer_size Tamaño del buffer
 * @return Número de bytes escritos, o -1 si error
 */
int metrics_get_json(char *buffer, size_t buffer_size);

// ============================================================================
// HELPER - Calcular desviación estándar
// ============================================================================

/**
 * Calcular desviación estándar de un array
 * 
 * @param values Array de valores
 * @param count Número de valores
 * @param mean Media de los valores
 * @return Desviación estándar
 */
double calculate_stddev(double *values, int count, double mean);

#endif // METRICS_H