#include "metrics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Singleton global
static metrics_manager_t g_metrics_manager;
static bool g_metrics_initialized = false;

// ============================================================================
// INICIALIZACIÓN
// ============================================================================

void metrics_init() {
    if (g_metrics_initialized) {
        return;
    }
    
    memset(&g_metrics_manager, 0, sizeof(metrics_manager_t));
    pthread_mutex_init(&g_metrics_manager.mutex, NULL);
    gettimeofday(&g_metrics_manager.start_time, NULL);
    
    g_metrics_initialized = true;
}

void metrics_destroy() {
    if (!g_metrics_initialized) {
        return;
    }
    
    pthread_mutex_lock(&g_metrics_manager.mutex);
    
    // Liberar buffers de cada comando
    for (int i = 0; i < g_metrics_manager.num_commands; i++) {
        command_metrics_t *cmd = &g_metrics_manager.commands[i];
        free(cmd->wait_times_ms);
        free(cmd->exec_times_ms);
        pthread_mutex_destroy(&cmd->mutex);
    }
    
    pthread_mutex_unlock(&g_metrics_manager.mutex);
    pthread_mutex_destroy(&g_metrics_manager.mutex);
    
    g_metrics_initialized = false;
}

int metrics_register_command(const char *command_name, int num_workers,
                             int queue_capacity, int buffer_size) {
    if (!g_metrics_initialized) {
        metrics_init();
    }
    
    pthread_mutex_lock(&g_metrics_manager.mutex);
    
    // Verificar si ya existe
    for (int i = 0; i < g_metrics_manager.num_commands; i++) {
        if (strcmp(g_metrics_manager.commands[i].command_name, command_name) == 0) {
            pthread_mutex_unlock(&g_metrics_manager.mutex);
            return i; // Ya registrado
        }
    }
    
    // Verificar límite
    if (g_metrics_manager.num_commands >= MAX_COMMANDS) {
        pthread_mutex_unlock(&g_metrics_manager.mutex);
        return -1;
    }
    
    int idx = g_metrics_manager.num_commands++;
    command_metrics_t *cmd = &g_metrics_manager.commands[idx];
    
    // Inicializar
    strncpy(cmd->command_name, command_name, sizeof(cmd->command_name) - 1);
    cmd->total_wait_time_us = 0;
    cmd->total_exec_time_us = 0;
    cmd->count = 0;
    
    cmd->buffer_size = buffer_size;
    cmd->buffer_index = 0;
    cmd->buffer_count = 0;
    cmd->wait_times_ms = (double*)calloc(buffer_size, sizeof(double));
    cmd->exec_times_ms = (double*)calloc(buffer_size, sizeof(double));
    
    cmd->current_queue_size = 0;
    cmd->max_queue_size = queue_capacity;
    
    cmd->total_workers = num_workers;
    cmd->busy_workers = 0;
    
    pthread_mutex_init(&cmd->mutex, NULL);
    
    pthread_mutex_unlock(&g_metrics_manager.mutex);
    
    return idx;
}

// ============================================================================
// HELPERS INTERNOS
// ============================================================================

static command_metrics_t* find_command(const char *command_name) {
    for (int i = 0; i < g_metrics_manager.num_commands; i++) {
        if (strcmp(g_metrics_manager.commands[i].command_name, command_name) == 0) {
            return &g_metrics_manager.commands[i];
        }
    }
    return NULL;
}

// ============================================================================
// REGISTRO DE MÉTRICAS
// ============================================================================

void metrics_record_wait_time(const char *command_name, unsigned long wait_time_us) {
    command_metrics_t *cmd = find_command(command_name);
    if (!cmd) return;
    
    pthread_mutex_lock(&cmd->mutex);
    
    cmd->total_wait_time_us += wait_time_us;
    cmd->count++;
    
    // Agregar al buffer circular
    double wait_ms = wait_time_us / 1000.0;
    cmd->wait_times_ms[cmd->buffer_index] = wait_ms;
    
    cmd->buffer_index = (cmd->buffer_index + 1) % cmd->buffer_size;
    if (cmd->buffer_count < cmd->buffer_size) {
        cmd->buffer_count++;
    }
    
    pthread_mutex_unlock(&cmd->mutex);
}

void metrics_record_exec_time(const char *command_name, unsigned long exec_time_us) {
    command_metrics_t *cmd = find_command(command_name);
    if (!cmd) return;
    
    pthread_mutex_lock(&cmd->mutex);
    
    cmd->total_exec_time_us += exec_time_us;
    
    // Agregar al buffer circular
    double exec_ms = exec_time_us / 1000.0;
    int idx = (cmd->buffer_index - 1 + cmd->buffer_size) % cmd->buffer_size;
    cmd->exec_times_ms[idx] = exec_ms;
    
    pthread_mutex_unlock(&cmd->mutex);
}

void metrics_update_queue_size(const char *command_name, int queue_size) {
    command_metrics_t *cmd = find_command(command_name);
    if (!cmd) return;
    
    pthread_mutex_lock(&cmd->mutex);
    cmd->current_queue_size = queue_size;
    pthread_mutex_unlock(&cmd->mutex);
}

void metrics_update_workers(const char *command_name, int busy_count) {
    command_metrics_t *cmd = find_command(command_name);
    if (!cmd) return;
    
    pthread_mutex_lock(&cmd->mutex);
    cmd->busy_workers = busy_count;
    pthread_mutex_unlock(&cmd->mutex);
}

void metrics_increment_requests() {
    pthread_mutex_lock(&g_metrics_manager.mutex);
    g_metrics_manager.total_requests++;
    pthread_mutex_unlock(&g_metrics_manager.mutex);
}

void metrics_increment_errors() {
    pthread_mutex_lock(&g_metrics_manager.mutex);
    g_metrics_manager.total_errors++;
    pthread_mutex_unlock(&g_metrics_manager.mutex);
}

// ============================================================================
// CONSULTAS
// ============================================================================

int metrics_get_command(const char *command_name, command_metrics_t *metrics) {
    command_metrics_t *cmd = find_command(command_name);
    if (!cmd) return -1;
    
    pthread_mutex_lock(&cmd->mutex);
    memcpy(metrics, cmd, sizeof(command_metrics_t));
    pthread_mutex_unlock(&cmd->mutex);
    
    return 0;
}

double metrics_get_avg_wait_time_ms(const char *command_name) {
    command_metrics_t *cmd = find_command(command_name);
    if (!cmd) return -1.0;
    
    pthread_mutex_lock(&cmd->mutex);
    
    if (cmd->count == 0) {
        pthread_mutex_unlock(&cmd->mutex);
        return 0.0;
    }
    
    double avg = (double)cmd->total_wait_time_us / (double)cmd->count / 1000.0;
    
    pthread_mutex_unlock(&cmd->mutex);
    return avg;
}

double metrics_get_avg_exec_time_ms(const char *command_name) {
    command_metrics_t *cmd = find_command(command_name);
    if (!cmd) return -1.0;
    
    pthread_mutex_lock(&cmd->mutex);
    
    if (cmd->count == 0) {
        pthread_mutex_unlock(&cmd->mutex);
        return 0.0;
    }
    
    double avg = (double)cmd->total_exec_time_us / (double)cmd->count / 1000.0;
    
    pthread_mutex_unlock(&cmd->mutex);
    return avg;
}

double calculate_stddev(double *values, int count, double mean) {
    if (count == 0) return 0.0;
    
    double sum_sq_diff = 0.0;
    for (int i = 0; i < count; i++) {
        double diff = values[i] - mean;
        sum_sq_diff += diff * diff;
    }
    
    return sqrt(sum_sq_diff / count);
}

double metrics_get_stddev_wait_time_ms(const char *command_name) {
    command_metrics_t *cmd = find_command(command_name);
    if (!cmd) return -1.0;
    
    pthread_mutex_lock(&cmd->mutex);
    
    if (cmd->buffer_count == 0) {
        pthread_mutex_unlock(&cmd->mutex);
        return 0.0;
    }
    
    // Calcular media
    double sum = 0.0;
    for (int i = 0; i < cmd->buffer_count; i++) {
        sum += cmd->wait_times_ms[i];
    }
    double mean = sum / cmd->buffer_count;
    
    // Calcular desviación estándar
    double stddev = calculate_stddev(cmd->wait_times_ms, cmd->buffer_count, mean);
    
    pthread_mutex_unlock(&cmd->mutex);
    return stddev;
}

double metrics_get_stddev_exec_time_ms(const char *command_name) {
    command_metrics_t *cmd = find_command(command_name);
    if (!cmd) return -1.0;
    
    pthread_mutex_lock(&cmd->mutex);
    
    if (cmd->buffer_count == 0) {
        pthread_mutex_unlock(&cmd->mutex);
        return 0.0;
    }
    
    // Calcular media
    double sum = 0.0;
    for (int i = 0; i < cmd->buffer_count; i++) {
        sum += cmd->exec_times_ms[i];
    }
    double mean = sum / cmd->buffer_count;
    
    // Calcular desviación estándar
    double stddev = calculate_stddev(cmd->exec_times_ms, cmd->buffer_count, mean);
    
    pthread_mutex_unlock(&cmd->mutex);
    return stddev;
}

// ============================================================================
// GENERACIÓN DE JSON
// ============================================================================

int metrics_get_json(char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return -1;
    
    int offset = 0;
    
    // Inicio del JSON
    offset += snprintf(buffer + offset, buffer_size - offset, "{\n");
    
    // Métricas globales
    pthread_mutex_lock(&g_metrics_manager.mutex);
    
    struct timeval now;
    gettimeofday(&now, NULL);
    long uptime = now.tv_sec - g_metrics_manager.start_time.tv_sec;
    
    offset += snprintf(buffer + offset, buffer_size - offset,
                      "  \"uptime_seconds\": %ld,\n"
                      "  \"total_requests\": %lu,\n"
                      "  \"total_errors\": %lu,\n",
                      uptime,
                      g_metrics_manager.total_requests,
                      g_metrics_manager.total_errors);
    
    // Métricas por comando
    offset += snprintf(buffer + offset, buffer_size - offset, "  \"commands\": {\n");
    
    for (int i = 0; i < g_metrics_manager.num_commands; i++) {
        command_metrics_t *cmd = &g_metrics_manager.commands[i];
        
        pthread_mutex_lock(&cmd->mutex);
        
        double avg_wait = cmd->count > 0 ? 
                         (double)cmd->total_wait_time_us / cmd->count / 1000.0 : 0.0;
        double avg_exec = cmd->count > 0 ?
                         (double)cmd->total_exec_time_us / cmd->count / 1000.0 : 0.0;
        
        // Calcular stddev
        double stddev_wait = 0.0, stddev_exec = 0.0;
        if (cmd->buffer_count > 0) {
            double sum_wait = 0.0, sum_exec = 0.0;
            for (int j = 0; j < cmd->buffer_count; j++) {
                sum_wait += cmd->wait_times_ms[j];
                sum_exec += cmd->exec_times_ms[j];
            }
            double mean_wait = sum_wait / cmd->buffer_count;
            double mean_exec = sum_exec / cmd->buffer_count;
            
            stddev_wait = calculate_stddev(cmd->wait_times_ms, cmd->buffer_count, mean_wait);
            stddev_exec = calculate_stddev(cmd->exec_times_ms, cmd->buffer_count, mean_exec);
        }
        
        offset += snprintf(buffer + offset, buffer_size - offset,
                          "    \"%s\": {\n"
                          "      \"count\": %lu,\n"
                          "      \"avg_wait_ms\": %.2f,\n"
                          "      \"stddev_wait_ms\": %.2f,\n"
                          "      \"avg_exec_ms\": %.2f,\n"
                          "      \"stddev_exec_ms\": %.2f,\n"
                          "      \"queue_size\": %d,\n"
                          "      \"queue_capacity\": %d,\n"
                          "      \"workers\": {\n"
                          "        \"total\": %d,\n"
                          "        \"busy\": %d,\n"
                          "        \"idle\": %d\n"
                          "      }\n"
                          "    }%s\n",
                          cmd->command_name,
                          cmd->count,
                          avg_wait,
                          stddev_wait,
                          avg_exec,
                          stddev_exec,
                          cmd->current_queue_size,
                          cmd->max_queue_size,
                          cmd->total_workers,
                          cmd->busy_workers,
                          cmd->total_workers - cmd->busy_workers,
                          (i < g_metrics_manager.num_commands - 1) ? "," : "");
        
        pthread_mutex_unlock(&cmd->mutex);
    }
    
    offset += snprintf(buffer + offset, buffer_size - offset, "  }\n");
    
    pthread_mutex_unlock(&g_metrics_manager.mutex);
    
    // Fin del JSON
    offset += snprintf(buffer + offset, buffer_size - offset, "}\n");
    
    return offset;
}