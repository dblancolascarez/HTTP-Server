#include "job_executor.h"
#include "job_manager.h"
#include "../utils/utils.h"
#include "../server/http.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Importar handlers de comandos
#include "../commands/basic/basic_commands.h"
#include "../commands/cpu_bound/cpu_bound_commands.h"
#include "../commands/io_bound/io_bound_commands.h"
#include "../commands/files/files_commands.h"

// Estado global del executor
static queue_t *g_job_queue = NULL;
static worker_pool_t *g_worker_pool = NULL;

// Forward declarations
static int execute_command(task_t *task, char **result_json, char **error_msg);
static char* get_param(const char *query, const char *key);

/**
 * Handler que ejecuta cada job
 * Llamado por los workers del pool
 */
static int job_handler(task_t *task, void *user_ctx) {
    (void)user_ctx; // No usado por ahora
    
    if (!task) return -1;
    
    // Si el task tiene job_id, es un job asíncrono
    if (task->job_id) {
        // Marcar como running
        job_mark_running(task->job_id);
        
        // Ejecutar el comando
        char *result = NULL;
        char *error = NULL;
        int rc = execute_command(task, &result, &error);
        
        if (rc == 0 && result) {
            // Éxito: marcar como done
            job_mark_done(task->job_id, result);
            free(result);
        } else {
            // Error: marcar como error
            const char *err = error ? error : "Unknown error";
            job_mark_error(task->job_id, err);
            if (error) free(error);
        }
    } else {
        // Ejecución directa: enviar respuesta al cliente
        char *result = NULL;
        char *error = NULL;
        int rc = execute_command(task, &result, &error);
        
        if (rc == 0 && result) {
            http_send_json(task->client_fd, HTTP_OK, result, task->request_id);
            free(result);
        } else {
            const char *err = error ? error : "Command execution failed";
            http_send_error(task->client_fd, HTTP_INTERNAL_ERROR, err, task->request_id);
            if (error) free(error);
        }
    }
    
    return 0;
}

/**
 * Ejecuta un comando y retorna el resultado en JSON
 * 
 * @param task Tarea a ejecutar
 * @param result_json [out] JSON con el resultado (caller debe free)
 * @param error_msg [out] Mensaje de error si falla (caller debe free)
 * @return 0 on success, -1 on error
 */
static int execute_command(task_t *task, char **result_json, char **error_msg) {
    if (!task || !task->path) {
        if (error_msg) *error_msg = strdup("Invalid task");
        return -1;
    }
    
    const char *path = task->path;
    const char *query = task->query ? task->query : "";
    
    // Helper para extraer parámetros
    #define GET_PARAM(name) get_param(query, name)
    
    // ========================================
    // BASIC COMMANDS
    // ========================================
    
    if (strcmp(path, "/fibonacci") == 0) {
        const char *num = GET_PARAM("num");
        if (!num) {
            if (error_msg) *error_msg = strdup("Missing 'num' parameter");
            return -1;
        }
        *result_json = handle_fibonacci(num);
        return (*result_json) ? 0 : -1;
    }
    
    if (strcmp(path, "/reverse") == 0) {
        const char *text = GET_PARAM("text");
        if (!text) {
            if (error_msg) *error_msg = strdup("Missing 'text' parameter");
            return -1;
        }
        *result_json = handle_reverse(text);
        return (*result_json) ? 0 : -1;
    }
    
    if (strcmp(path, "/hash") == 0) {
        const char *text = GET_PARAM("text");
        if (!text) {
            if (error_msg) *error_msg = strdup("Missing 'text' parameter");
            return -1;
        }
        *result_json = handle_hash(text);
        return (*result_json) ? 0 : -1;
    }
    
    if (strcmp(path, "/random") == 0) {
        const char *count = GET_PARAM("count");
        const char *min = GET_PARAM("min");
        const char *max = GET_PARAM("max");
        if (!count || !min || !max) {
            if (error_msg) *error_msg = strdup("Missing parameters");
            return -1;
        }
        *result_json = handle_random(count, min, max);
        return (*result_json) ? 0 : -1;
    }
    
    if (strcmp(path, "/simulate") == 0) {
        const char *seconds = GET_PARAM("seconds");
        const char *task_name = GET_PARAM("task");
        if (!seconds || !task_name) {
            if (error_msg) *error_msg = strdup("Missing parameters");
            return -1;
        }
        *result_json = handle_simulate(seconds, task_name);
        return (*result_json) ? 0 : -1;
    }
    
    if (strcmp(path, "/sleep") == 0) {
        const char *seconds = GET_PARAM("seconds");
        if (!seconds) {
            if (error_msg) *error_msg = strdup("Missing 'seconds' parameter");
            return -1;
        }
        *result_json = handle_sleep(seconds);
        return (*result_json) ? 0 : -1;
    }
    
    if (strcmp(path, "/loadtest") == 0) {
        const char *tasks = GET_PARAM("tasks");
        const char *sleep_ms = GET_PARAM("sleep");
        if (!tasks || !sleep_ms) {
            if (error_msg) *error_msg = strdup("Missing parameters");
            return -1;
        }
        *result_json = handle_loadtest(tasks, sleep_ms);
        return (*result_json) ? 0 : -1;
    }
    
    // ========================================
    // CPU-BOUND COMMANDS
    // ========================================
    
    if (strcmp(path, "/isprime") == 0) {
        const char *n = GET_PARAM("n");
        if (!n) {
            if (error_msg) *error_msg = strdup("Missing 'n' parameter");
            return -1;
        }
        *result_json = handle_isprime(n);
        return (*result_json) ? 0 : -1;
    }
    
    if (strcmp(path, "/factor") == 0) {
        const char *n = GET_PARAM("n");
        if (!n) {
            if (error_msg) *error_msg = strdup("Missing 'n' parameter");
            return -1;
        }
        *result_json = handle_factor(n);
        return (*result_json) ? 0 : -1;
    }
    
    if (strcmp(path, "/pi") == 0) {
        const char *digits = GET_PARAM("digits");
        if (!digits) {
            if (error_msg) *error_msg = strdup("Missing 'digits' parameter");
            return -1;
        }
        *result_json = handle_pi(digits);
        return (*result_json) ? 0 : -1;
    }
    
    if (strcmp(path, "/mandelbrot") == 0) {
        const char *width = GET_PARAM("width");
        const char *height = GET_PARAM("height");
        const char *max_iter = GET_PARAM("max_iter");
        if (!width || !height || !max_iter) {
            if (error_msg) *error_msg = strdup("Missing parameters");
            return -1;
        }
        *result_json = handle_mandelbrot(width, height, max_iter);
        return (*result_json) ? 0 : -1;
    }
    
    if (strcmp(path, "/matrixmul") == 0) {
        const char *size = GET_PARAM("size");
        const char *seed = GET_PARAM("seed");
        if (!size || !seed) {
            if (error_msg) *error_msg = strdup("Missing parameters");
            return -1;
        }
        *result_json = handle_matrixmul(size, seed);
        return (*result_json) ? 0 : -1;
    }
    
    // ========================================
    // IO-BOUND COMMANDS
    // ========================================
    
    if (strcmp(path, "/sortfile") == 0) {
        const char *name = GET_PARAM("name");
        const char *algo = GET_PARAM("algo");
        if (!name || !algo) {
            if (error_msg) *error_msg = strdup("Missing parameters");
            return -1;
        }
        *result_json = handle_sortfile(name, algo);
        return (*result_json) ? 0 : -1;
    }
    
    if (strcmp(path, "/wordcount") == 0) {
        const char *name = GET_PARAM("name");
        if (!name) {
            if (error_msg) *error_msg = strdup("Missing 'name' parameter");
            return -1;
        }
        *result_json = handle_wordcount(name);
        return (*result_json) ? 0 : -1;
    }
    
    if (strcmp(path, "/grep") == 0) {
        const char *name = GET_PARAM("name");
        const char *pattern = GET_PARAM("pattern");
        if (!name || !pattern) {
            if (error_msg) *error_msg = strdup("Missing parameters");
            return -1;
        }
        *result_json = handle_grep(name, pattern);
        return (*result_json) ? 0 : -1;
    }
    
    if (strcmp(path, "/compress") == 0) {
        const char *name = GET_PARAM("name");
        const char *codec = GET_PARAM("codec");
        if (!name || !codec) {
            if (error_msg) *error_msg = strdup("Missing parameters");
            return -1;
        }
        *result_json = handle_compress(name, codec);
        return (*result_json) ? 0 : -1;
    }
    
    if (strcmp(path, "/hashfile") == 0) {
        const char *name = GET_PARAM("name");
        const char *algo = GET_PARAM("algo");
        if (!name || !algo) {
            if (error_msg) *error_msg = strdup("Missing parameters");
            return -1;
        }
        *result_json = handle_hashfile(name, algo);
        return (*result_json) ? 0 : -1;
    }
    
    // ========================================
    // FILE COMMANDS
    // ========================================
    
    if (strcmp(path, "/createfile") == 0) {
        const char *name = GET_PARAM("name");
        const char *content = GET_PARAM("content");
        const char *repeat = GET_PARAM("repeat");
        if (!name || !content || !repeat) {
            if (error_msg) *error_msg = strdup("Missing parameters");
            return -1;
        }
        *result_json = handle_createfile(name, content, repeat);
        return (*result_json) ? 0 : -1;
    }
    
    if (strcmp(path, "/deletefile") == 0) {
        const char *name = GET_PARAM("name");
        if (!name) {
            if (error_msg) *error_msg = strdup("Missing 'name' parameter");
            return -1;
        }
        *result_json = handle_deletefile(name);
        return (*result_json) ? 0 : -1;
    }
    
    // Comando no reconocido
    if (error_msg) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Unknown command: %s", path);
        *error_msg = strdup(buf);
    }
    return -1;
    
    #undef GET_PARAM
}

/**
 * Helper para extraer un parámetro de la query string
 */
static char* get_param(const char *query, const char *key) {
    if (!query || !key) return NULL;
    
    // Parsear query string manualmente
    // Formato: key1=value1&key2=value2
    
    size_t key_len = strlen(key);
    const char *p = query;
    
    while (*p) {
        // Buscar la key
        if (strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            // Encontrado: extraer el value
            p += key_len + 1; // Saltar "key="
            
            const char *end = strchr(p, '&');
            size_t value_len = end ? (size_t)(end - p) : strlen(p);
            
            char *value = malloc(value_len + 1);
            if (!value) return NULL;
            
            memcpy(value, p, value_len);
            value[value_len] = '\0';
            
            // URL decode
            char *decoded = url_decode(value);
            free(value);
            return decoded;
        }
        
        // Avanzar al siguiente parámetro
        p = strchr(p, '&');
        if (!p) break;
        p++; // Saltar '&'
    }
    
    return NULL;
}

// ============================================================================
// API PÚBLICA
// ============================================================================

int job_executor_init(int num_workers, int queue_depth) {
    if (num_workers <= 0 || queue_depth <= 0) return -1;
    
    // Crear cola
    g_job_queue = queue_create(queue_depth);
    if (!g_job_queue) return -1;
    
    // Crear worker pool
    g_worker_pool = worker_pool_create(num_workers, g_job_queue, job_handler, NULL);
    if (!g_worker_pool) {
        queue_destroy(g_job_queue);
        g_job_queue = NULL;
        return -1;
    }
    
    // Iniciar workers
    if (worker_pool_start(g_worker_pool) != 0) {
        worker_pool_destroy(g_worker_pool);
        queue_destroy(g_job_queue);
        g_worker_pool = NULL;
        g_job_queue = NULL;
        return -1;
    }
    
    return 0;
}

void job_executor_shutdown() {
    if (g_worker_pool) {
        worker_pool_stop(g_worker_pool);
        worker_pool_destroy(g_worker_pool);
        g_worker_pool = NULL;
    }
    
    if (g_job_queue) {
        queue_destroy(g_job_queue);
        g_job_queue = NULL;
    }
}

int job_executor_enqueue(task_t *task) {
    if (!g_job_queue || !task) return -1;
    
    // Encolar con timeout de 100ms
    return queue_enqueue(g_job_queue, task, 100);
}

int job_executor_execute_direct(task_t *task) {
    if (!task) return -1;
    
    // Ejecutar directamente usando el handler
    return job_handler(task, NULL);
}

void job_executor_get_metrics(int *out_queue_size, int *out_workers_busy, int *out_workers_total) {
    if (out_queue_size) {
        *out_queue_size = g_job_queue ? queue_size(g_job_queue) : 0;
    }
    
    if (out_workers_total) {
        *out_workers_total = g_worker_pool ? worker_pool_get_total(g_worker_pool) : 0;
    }
    
    if (out_workers_busy) {
        *out_workers_busy = g_worker_pool ? worker_pool_get_busy(g_worker_pool) : 0;
    }
}
