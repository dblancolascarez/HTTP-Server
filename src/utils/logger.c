// ¿Qué hace?
// - Escribe mensajes de debug/info/warn/error a un archivo o consola
// - Agrega timestamps automáticamente
// - Es thread-safe (varios hilos pueden loggear simultáneamente)

// ¿Por qué lo necesitas?
// - Debuggear: ver qué está pasando cuando el servidor está corriendo
// - Producción: detectar errores sin detener el servidor
// - Pruebas: verificar el flujo de ejecución

#include "utils.h"
#include <stdarg.h>

// Estado global del logger
static struct {
    log_level_t level;
    FILE *file;
    pthread_mutex_t mutex;
    bool initialized;
} g_logger = {
    .level = LOG_INFO,
    .file = NULL,
    .initialized = false
};

void logger_init(log_level_t level, const char *log_file) {
    if (g_logger.initialized) {
        return;
    }
    
    g_logger.level = level;
    
    if (log_file) {
        g_logger.file = fopen(log_file, "a");
        if (!g_logger.file) {
            fprintf(stderr, "Failed to open log file: %s\n", log_file);
            g_logger.file = stderr;
        }
    } else {
        g_logger.file = stderr;
    }
    
    pthread_mutex_init(&g_logger.mutex, NULL);
    g_logger.initialized = true;
}

void logger_shutdown() {
    if (!g_logger.initialized) {
        return;
    }
    
    pthread_mutex_lock(&g_logger.mutex);
    
    if (g_logger.file && g_logger.file != stderr) {
        fclose(g_logger.file);
    }
    
    g_logger.initialized = false;
    pthread_mutex_unlock(&g_logger.mutex);
    pthread_mutex_destroy(&g_logger.mutex);
}

static const char* level_to_string(log_level_t level) {
    switch (level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO:  return "INFO ";
        case LOG_WARN:  return "WARN ";
        case LOG_ERROR: return "ERROR";
        default:        return "?????";
    }
}

void log_message(log_level_t level, const char *file, int line, const char *fmt, ...) {
    // Auto-inicializar si no se ha hecho
    if (!g_logger.initialized) {
        logger_init(LOG_INFO, NULL);
    }
    
    // Filtrar por nivel
    if (level < g_logger.level) {
        return;
    }
    
    pthread_mutex_lock(&g_logger.mutex);
    
    // Timestamp
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Extraer solo el nombre del archivo (sin path)
    const char *filename = strrchr(file, '/');
    filename = filename ? filename + 1 : file;
    
    // Imprimir header
    fprintf(g_logger.file, "[%s] [%s] [%s:%d] ", 
            timestamp, level_to_string(level), filename, line);
    
    // Imprimir mensaje
    va_list args;
    va_start(args, fmt);
    vfprintf(g_logger.file, fmt, args);
    va_end(args);
    
    fprintf(g_logger.file, "\n");
    fflush(g_logger.file);
    
    pthread_mutex_unlock(&g_logger.mutex);
}