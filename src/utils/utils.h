// ¿Qué hace?
// - Define las INTERFACES (headers) de todas las funciones utilitarias
// - Es como un "menú" que dice "estas funciones existen"
// - Otros archivos lo incluyen para usar las funciones

// ¿Por qué es importante?
// - Evita duplicar declaraciones en cada archivo
// - Permite que cualquier parte del código use las utilidades
// - Facilita el mantenimiento (cambias el prototipo una sola vez)

#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>

// ============================================================================
// LOGGER - Sistema de logging thread-safe
// ============================================================================

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} log_level_t;

void logger_init(log_level_t level, const char *log_file);
void logger_shutdown();
void log_message(log_level_t level, const char *file, int line, const char *fmt, ...);

// Macros para logging conveniente
#define LOG_DEBUG(...) log_message(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  log_message(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  log_message(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) log_message(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)

// ============================================================================
// STRING UTILS - Parsing y manipulación de strings
// ============================================================================

// Parsear query string: "n=97&max=100" -> { "n": "97", "max": "100" }
typedef struct {
    char *key;
    char *value;
} query_param_t;

typedef struct {
    query_param_t *params;
    int count;
} query_params_t;

// Parsear query string completo
query_params_t* parse_query_string(const char *query);

// Obtener valor de un parámetro (retorna NULL si no existe)
const char* get_query_param(query_params_t *params, const char *key);

// Obtener valor como entero (retorna default_val si no existe o es inválido)
int get_query_param_int(query_params_t *params, const char *key, int default_val);

// Obtener valor como long (para números grandes)
long get_query_param_long(query_params_t *params, const char *key, long default_val);

// Liberar estructura de parámetros
void free_query_params(query_params_t *params);

// URL decode: "hello%20world" -> "hello world"
char* url_decode(const char *str);

// Trim whitespace (modifica in-place)
char* trim(char *str);

// String split por delimitador
char** str_split(const char *str, char delimiter, int *count);

// Liberar array de strings
void free_str_array(char **arr, int count);

// ============================================================================
// JSON BUILDER - Construcción simple de JSON
// ============================================================================

typedef struct json_builder json_builder_t;

json_builder_t* json_create();
void json_destroy(json_builder_t *json);

// Iniciar/terminar objeto
void json_object_begin(json_builder_t *json);
void json_object_end(json_builder_t *json);

// Iniciar/terminar array
void json_array_begin(json_builder_t *json, const char *key);
void json_array_end(json_builder_t *json);

// Agregar campos
void json_add_string(json_builder_t *json, const char *key, const char *value);
void json_add_int(json_builder_t *json, const char *key, int value);
void json_add_long(json_builder_t *json, const char *key, long value);
void json_add_bool(json_builder_t *json, const char *key, bool value);
void json_add_double(json_builder_t *json, const char *key, double value);

// Obtener string final (no liberar manualmente, json_destroy lo hace)
const char* json_get_string(json_builder_t *json);

// ============================================================================
// TIMER - Medición de tiempo
// ============================================================================

typedef struct {
    struct timeval start;
    struct timeval end;
} http_timer_t;

void timer_start(http_timer_t *timer);
void timer_stop(http_timer_t *timer);
long timer_elapsed_ms(http_timer_t *timer);
long timer_elapsed_us(http_timer_t *timer);

// ============================================================================
// UUID - Generación de IDs únicos
// ============================================================================

void generate_request_id(char *buffer, size_t size);

#endif // UTILS_H