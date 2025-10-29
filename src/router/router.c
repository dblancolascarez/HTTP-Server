// Mapeo paths -> handlers + dispatch
#include "router.h"
#include "../core/job_manager.h"
#include "../core/job_executor.h"
#include "../core/metrics.h"
#include "../utils/utils.h"
#include "../commands/basic/basic_commands.h"
#include "../commands/cpu_bound/cpu_bound_commands.h"
#include "../commands/io_bound/io_bound_commands.h"
#include "../commands/files/files_commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* json_escape_append removed — router builds JSON with manual escaping where needed.
   The helper was unused and generated a compiler warning; removing it keeps the
   code simpler until a shared JSON builder is added. */
char* handle_pi(const char* digits_str);
char* handle_mandelbrot(const char* width_str, const char* height_str, const char* max_iter_str);
char* handle_matrixmul(const char* size_str, const char* seed_str);

// Construir un JSON sencillo a partir de query_params_t (todos strings)
static char* build_json_from_params(query_params_t *qp) {
    size_t cap = 1024;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    buf[0] = '{';
    len = 1;

    for (int i = 0; qp && i < qp->count; i++) {
        const char *k = qp->params[i].key;
        const char *v = qp->params[i].value;
        // agregar "," si no es el primer campo
        if (len > 1) {
            if (len + 2 >= cap) { cap *= 2; buf = realloc(buf, cap); }
            buf[len++] = ',';
        }
        // agregar "key":"value"
        if (len + strlen(k) + strlen(v) + 10 >= cap) { cap = cap + strlen(k) + strlen(v) + 1024; buf = realloc(buf, cap); }
        buf[len++] = '"';
        // escape key
        for (const char *p = k; *p; p++) {
            if (*p == '"' || *p == '\\') { buf[len++] = '\\'; }
            buf[len++] = *p;
        }
        buf[len++] = '"';
        buf[len++] = ':';
        buf[len++] = '"';
        for (const char *p = v; *p; p++) {
            if (*p == '"' || *p == '\\') { buf[len++] = '\\'; }
            buf[len++] = *p;
        }
        buf[len++] = '"';
        buf[len] = '\0';
    }

    if (len + 2 >= cap) { cap += 16; buf = realloc(buf, cap); }
    buf[len++] = '}';
    buf[len] = '\0';
    return buf;
}

static const char* job_status_to_string(job_status_t s) {
    switch (s) {
        case JOB_STATUS_QUEUED: return "queued";
        case JOB_STATUS_RUNNING: return "running";
        case JOB_STATUS_DONE: return "done";
        case JOB_STATUS_ERROR: return "error";
        case JOB_STATUS_CANCELED: return "canceled";
        default: return "unknown";
    }
}

// Implementación de los endpoints de jobs

static ssize_t handle_jobs_submit(int client_fd, const char *request_id, query_params_t *qp) {
    const char *task = get_query_param(qp, "task");
    if (!task) {
        return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'task' parameter", request_id);
    }

    // Construir payload JSON con todos los parámetros (guardado en job manager)
    char *payload = build_json_from_params(qp);
    if (!payload) {
        return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to build job payload", request_id);
    }

    // Submit job (lo guarda en job manager y retorna job_id)
    char *job_id = job_submit(task, payload, 0); // TODO: priority
    free(payload);

    if (!job_id) {
        return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to submit job", request_id);
    }

    // Construir query string para pasar al executor: key=value&key2=value2
    size_t qcap = 256;
    size_t qlen = 0;
    char *qbuf = malloc(qcap);
    if (!qbuf) {
        free(job_id);
        return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Memory allocation failed", request_id);
    }
    qbuf[0] = '\0';

    for (int i = 0; qp && i < qp->count; i++) {
        const char *k = qp->params[i].key;
        const char *v = qp->params[i].value ? qp->params[i].value : "";
        size_t need = strlen(k) + 1 + strlen(v) + (qlen ? 1 : 0) + 1;
        if (qlen + need >= qcap) {
            qcap = qcap + need + 256;
            char *tmp = realloc(qbuf, qcap);
            if (!tmp) { free(qbuf); free(job_id); return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Memory allocation failed", request_id); }
            qbuf = tmp;
        }
        if (qlen > 0) { qbuf[qlen++] = '&'; }
        strcpy(qbuf + qlen, k);
        qlen += strlen(k);
        qbuf[qlen++] = '=';
        strcpy(qbuf + qlen, v);
        qlen += strlen(v);
        qbuf[qlen] = '\0';
    }

    // Preparar task para el executor
    char pathbuf[128];
    snprintf(pathbuf, sizeof(pathbuf), "/%s", task);

    task_t *t = task_create(-1, pathbuf, qbuf, request_id);
    if (!t) {
        free(qbuf);
        free(job_id);
        return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to create task", request_id);
    }
    // task_create duplica path/query internamente; guardamos job_id en la tarea
    t->job_id = strdup(job_id);

    // Encolar para ejecución asíncrona (job_executor toma ownership de task en caso de éxito)
    int rc = job_executor_enqueue(t);
    if (rc != 0) {
        // Si falla el enqueue, liberar la tarea y continuar (el job queda en estado queued en job_manager)
        task_free(t);
        // Notificar cliente igualmente con el job_id; el job queda en queued y se puede reintentar más tarde.
        free(qbuf);
    } else {
        // enqueue hizo copia del query/path internamente (task_create usó strdup), podemos liberar qbuf
        free(qbuf);
    }

    // Responder con job_id
    char json[256];
    snprintf(json, sizeof(json), "{\"job_id\":\"%s\",\"status\":\"queued\"}", job_id);
    free(job_id);

    return http_send_json(client_fd, HTTP_OK, json, request_id);
}

static ssize_t handle_jobs_status(int client_fd, const char *request_id, query_params_t *qp) {
    const char *id = get_query_param(qp, "id");
    if (!id) {
        return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'id' parameter", request_id);
    }

    // Get job status
    job_status_info_t info;
    if (job_get_status(id, &info) != 0) {
        return http_send_error(client_fd, HTTP_NOT_FOUND, "Job not found", request_id);
    }

    // Response
    char json[256];
    snprintf(json, sizeof(json), 
        "{\"status\":\"%s\",\"progress\":%d,\"eta_ms\":%ld}",
        job_status_to_string(info.status), info.progress, info.eta_ms);

    return http_send_json(client_fd, HTTP_OK, json, request_id);
}

static ssize_t handle_jobs_result(int client_fd, const char *request_id, query_params_t *qp) {
    const char *id = get_query_param(qp, "id");
    if (!id) {
        return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'id' parameter", request_id);
    }

    // Get result
    char *result = job_get_result(id);
    if (!result) {
        return http_send_error(client_fd, HTTP_NOT_FOUND, "Job not found or not completed", request_id);
    }

    ssize_t sent = http_send_json(client_fd, HTTP_OK, result, request_id);
    free(result);
    return sent;
}

static ssize_t handle_jobs_cancel(int client_fd, const char *request_id, query_params_t *qp) {
    const char *id = get_query_param(qp, "id");
    if (!id) {
        return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'id' parameter", request_id);
    }

    // Try to cancel
    int rc = job_cancel(id);
    if (rc < 0) {
        return http_send_error(client_fd, HTTP_NOT_FOUND, "Job not found", request_id);
    }

    // Response
    const char *status = (rc == 0) ? "canceled" : "not_cancelable";
    char json[256];
    snprintf(json, sizeof(json), "{\"status\":\"%s\"}", status);

    return http_send_json(client_fd, HTTP_OK, json, request_id);
}

// Main jobs router
ssize_t handle_jobs_request(const char *subpath, int client_fd, const char *request_id, query_params_t *qp) {
    if (strcmp(subpath, "submit") == 0) {
        return handle_jobs_submit(client_fd, request_id, qp);
    }
    
    if (strcmp(subpath, "status") == 0) {
        return handle_jobs_status(client_fd, request_id, qp);
    }
    
    if (strcmp(subpath, "result") == 0) {
        return handle_jobs_result(client_fd, request_id, qp);
    }
    
    if (strcmp(subpath, "cancel") == 0) {
        return handle_jobs_cancel(client_fd, request_id, qp);
    }

    return http_send_error(client_fd, HTTP_NOT_FOUND, "Invalid jobs endpoint", request_id);
}

ssize_t router_handle_request(const http_request_t *req, int client_fd,
                              const char *request_id,
                              server_state_t *server,
                              size_t bytes_received) {
    if (!req || client_fd < 0) return -1;
    (void)bytes_received; // parámetro no usado en este router, evitar warning

    // Parsear query params
    query_params_t *qp = parse_query_string(req->query);

    //*************************************** */
    // Basic Commands 
    //*************************************** */

    if (strcmp(req->path, "/fibonacci") == 0) {
        const char *num = get_query_param(qp, "num");
        if (!num) { 
            free_query_params(qp); 
            return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'num' parameter", request_id); 
        }
        
        char *json = handle_fibonacci(num);
        if (!json) {
            free_query_params(qp);
            return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to process request", request_id);
        }
        
        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free(json);
        free_query_params(qp);
        return sent;
    }

    if (strcmp(req->path, "/createfile") == 0) {
        const char *name = get_query_param(qp, "name");
        const char *content = get_query_param(qp, "content");
        const char *repeat = get_query_param(qp, "repeat");
        
        if (!name || !content || !repeat) { 
            free_query_params(qp); 
            return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'name', 'content', or 'repeat' parameter", request_id); 
        }
        
        char *json = handle_createfile(name, content, repeat);
        if (!json) {
            free_query_params(qp);
            return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to process request", request_id);
        }
        
        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free(json);
        free_query_params(qp);
        return sent;
    }

    if (strcmp(req->path, "/deletefile") == 0) {
        const char *name = get_query_param(qp, "name");
        
        if (!name) { 
            free_query_params(qp); 
            return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'name' parameter", request_id); 
        }
        
        char *json = handle_deletefile(name);
        if (!json) {
            free_query_params(qp);
            return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to process request", request_id);
        }
        
        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free(json);
        free_query_params(qp);
        return sent;
    }

    if (strcmp(req->path, "/status") == 0) {
        // Construir status JSON (básico)
        char json[512];
        long uptime = server_get_uptime(server);
        server_stats_t stats;
        server_get_stats(server, &stats);
        int n = snprintf(json, sizeof(json),
            "{\"status\":\"running\",\"pid\":%d,\"uptime_seconds\":%ld,\"connections_served\":%lu,\"requests_ok\":%lu,\"requests_error\":%lu}",
            getpid(), uptime, stats.connections_served, stats.requests_ok, stats.requests_error);
        (void)n;
        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free_query_params(qp);
        return sent;
    }

    if (strcmp(req->path, "/reverse") == 0) {
        const char *text = get_query_param(qp, "text");
        if (!text) { 
            free_query_params(qp); 
            return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'text' parameter", request_id); 
        }
        
        char *json = handle_reverse(text);
        if (!json) {
            free_query_params(qp);
            return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to process request", request_id);
        }
        
        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free(json);
        free_query_params(qp);
        return sent;
    }

    if (strcmp(req->path, "/toupper") == 0) {
        const char *text = get_query_param(qp, "text");
        if (!text) { free_query_params(qp); return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'text' parameter", request_id); }
        char *decoded = url_decode(text);
        if (decoded) {
            for (char *p = decoded; *p; p++) *p = toupper((unsigned char)*p);
        }
        char json[1024];
        snprintf(json, sizeof(json), "{\"input\":\"%s\",\"output\":\"%s\"}", text, decoded ? decoded : "");
        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free(decoded); free_query_params(qp);
        return sent;
    }

    if (strcmp(req->path, "/random")==0){
        const char *count = get_query_param(qp, "count");
        const char *min = get_query_param(qp, "min");
        const char *max = get_query_param(qp, "max");

        if (!count || !min || !max){
            free_query_params(qp);
            return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'count', 'min', or 'max' parameter", request_id);
        }

        char *json = handle_random(count, min, max);
        if (!json){
            free_query_params(qp);
            return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to process request", request_id);
        }

        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free(json);
        free_query_params(qp);
        return sent;
    }

    if (strcmp(req->path, "/timestamp") == 0) {
        time_t now = time(NULL);
        char json[256];
        snprintf(json, sizeof(json), "{\"timestamp\":%ld}", (long)now);
        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free_query_params(qp);
        return sent;
    }

    if (strcmp(req->path, "/hash") == 0) {
        const char *text = get_query_param(qp, "text");
        if (!text) { 
            free_query_params(qp); 
            return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'text' parameter", request_id); 
        }
        
        char *json = handle_hash(text);
        if (!json) {
            free_query_params(qp);
            return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to process request", request_id);
        }
        
        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free(json);
        free_query_params(qp);
        return sent;
    }

    if (strcmp(req->path, "/simulate") == 0) {
        const char *seconds = get_query_param(qp, "seconds");
        const char *task = get_query_param(qp, "task");
        if (!seconds || !task) {
            free_query_params(qp);
            return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'seconds' or 'task' parameter", request_id);
        }
        char *json = handle_simulate(seconds, task);
        if (!json) {
            free_query_params(qp);
            return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to process request", request_id);
        }
        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free(json);
        free_query_params(qp);
        return sent;
    }    

    if (strcmp(req->path, "/sleep") == 0) {
        const char *seconds = get_query_param(qp, "seconds");
        if (!seconds) {
            free_query_params(qp);
            return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'seconds' parameter", request_id);
        }
        char *json = handle_sleep(seconds);
        if (!json) {
            free_query_params(qp);
            return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to process request", request_id);
        }
        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free(json);
        free_query_params(qp);
        return sent;
    }

    if (strcmp(req->path, "/loadtest") == 0) {
        const char *tasks = get_query_param(qp, "tasks");
        const char *sleep_ms = get_query_param(qp, "sleep");
        if (!tasks || !sleep_ms) {
            free_query_params(qp);
            return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'tasks' or 'sleep' parameter", request_id);
        }
        char *json = handle_loadtest(tasks, sleep_ms);
        if (!json) {
            free_query_params(qp);
            return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to process request", request_id);
        }
        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free(json);
        free_query_params(qp);
        return sent;
    }

    if (strcmp(req->path, "/help") == 0) {
        char *json = handle_help();
        if (!json) {
            free_query_params(qp);
            return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to build help", request_id);
        }
        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free(json);
        free_query_params(qp);
        return sent;
    }

    //*************************************** */
    // CPU Bound Commands
    //*************************************** */

    if (strcmp(req->path, "/isprime") == 0) {
        const char *n = get_query_param(qp, "n");
        if (!n) { 
            free_query_params(qp); 
            return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'n' parameter", request_id); 
        }
        
        char *json = handle_isprime(n);
        if (!json) {
            free_query_params(qp);
            return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to process request", request_id);
        }
        
        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free(json);
        free_query_params(qp);
        return sent;
    }

    if (strcmp(req->path, "/factor") == 0) {
        const char *n = get_query_param(qp, "n");
        if (!n) { 
            free_query_params(qp); 
            return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'n' parameter", request_id); 
        }
        
        char *json = handle_factor(n);
        if (!json) {
            free_query_params(qp);
            return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to process request", request_id);
        }
        
        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free(json);
        free_query_params(qp);
        return sent;
    }
  
    if (strcmp(req->path, "/pi") == 0) {
        const char *digits = get_query_param(qp, "digits");
        if (!digits) { free_query_params(qp); return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'digits' parameter", request_id); }
        char *json = handle_pi(digits);
        if (!json) { free_query_params(qp); return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to process request", request_id); }
        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free(json); free_query_params(qp); return sent;
    }

    if (strcmp(req->path, "/mandelbrot") == 0) {
        const char *width = get_query_param(qp, "width");
        const char *height = get_query_param(qp, "height");
        const char *max_iter = get_query_param(qp, "max_iter");
        if (!width || !height || !max_iter) { free_query_params(qp); return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'width','height' or 'max_iter' parameter", request_id); }
        char *json = handle_mandelbrot(width, height, max_iter);
        if (!json) { free_query_params(qp); return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to process request", request_id); }
        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free(json); free_query_params(qp); return sent;
    }

    if (strcmp(req->path, "/matrixmul") == 0) {
        const char *size = get_query_param(qp, "size");
        const char *seed = get_query_param(qp, "seed");
        if (!size || !seed) { free_query_params(qp); return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'size' or 'seed' parameter", request_id); }
        char *json = handle_matrixmul(size, seed);
        if (!json) { free_query_params(qp); return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to process request", request_id); }
        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free(json); free_query_params(qp); return sent;
    }
    
    //*************************************** */
    // IO Bound Commands
    //*************************************** */

    if (strcmp(req->path, "/sortfile") == 0) {
        const char *name = get_query_param(qp, "name");
        const char *algo = get_query_param(qp, "algo");
        
        if (!name || !algo) { 
            free_query_params(qp); 
            return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'name' or 'algo' parameter", request_id); 
        }
        
        char *json = handle_sortfile(name, algo);
        if (!json) {
            free_query_params(qp);
            return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to process request", request_id);
        }
        
        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free(json);
        free_query_params(qp);
        return sent;
    }

    if (strcmp(req->path, "/wordcount") == 0) {
        const char *name = get_query_param(qp, "name");
        
        if (!name) { 
            free_query_params(qp); 
            return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'name' parameter", request_id); 
        }
        
        char *json = handle_wordcount(name);
        if (!json) {
            free_query_params(qp);
            return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to process request", request_id);
        }
        
        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free(json);
        free_query_params(qp);
        return sent;
    }

    if (strcmp(req->path, "/grep") == 0) {
        const char *name = get_query_param(qp, "name");
        const char *pattern = get_query_param(qp, "pattern");

        if (!name || !pattern) { 
            free_query_params(qp); 
            return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'name' or 'pattern' parameter", request_id); 
        }

        char *json = handle_grep(name, pattern);
        if (!json) {
            free_query_params(qp);
            return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to process request", request_id);
        }

        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free(json);
        free_query_params(qp);
        return sent;
    }

    if (strcmp(req->path, "/compress") == 0) {
        const char *name = get_query_param(qp, "name");
        const char *codec = get_query_param(qp, "codec");

        if (!name || !codec) { 
            free_query_params(qp); 
            return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'name' or 'codec' parameter", request_id); 
        }

        char *json = handle_compress(name, codec);
        if (!json) {
            free_query_params(qp);
            return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to process request", request_id);
        }

        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free(json);
        free_query_params(qp);
        return sent;
    }

    if (strcmp(req->path, "/hashfile") == 0) {
        const char *name = get_query_param(qp, "name");
        const char *algo = get_query_param(qp, "algo");
        
        if (!name || !algo) { 
            free_query_params(qp); 
            return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'name' or 'algo' parameter", request_id); 
        }
        
        char *json = handle_hashfile(name, algo);
        if (!json) {
            free_query_params(qp);
            return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to process request", request_id);
        }
        
        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free(json);
        free_query_params(qp);
        return sent;
    }
    char json_response[8192]; 

    if (strcmp(req->path, "/metrics") == 0) {
        char *json = malloc(8192); // Buffer suficiente para las métricas
        if (!json) {
            free_query_params(qp);
            return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Memory allocation failed", request_id);
        }
        
        int written = metrics_get_json(json, 8192);
        if (written <= 0) {
            free(json);
            free_query_params(qp);
            return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to generate metrics", request_id);
        }
        
        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free(json);
        free_query_params(qp);
        return sent;
    }

    //*************************************** */
    // Jobs Endpoints
    //*************************************** */

    if (strncmp(req->path, "/jobs/", 6) == 0) {
        const char *sub = req->path + 6;
        return handle_jobs_request(sub, client_fd, request_id, qp);
    }
    // Ruta no encontrada
    free_query_params(qp);
    return http_send_error(client_fd, HTTP_NOT_FOUND, "Endpoint not found", request_id);
}

