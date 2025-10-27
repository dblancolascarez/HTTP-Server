// Mapeo paths -> handlers + dispatch
#include "router.h"
#include "../core/job_manager.h"
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

ssize_t router_handle_request(const http_request_t *req, int client_fd,
                              const char *request_id,
                              server_state_t *server,
                              size_t bytes_received) {
    if (!req || client_fd < 0) return -1;
    (void)bytes_received; // parámetro no usado en este router, evitar warning

    // Parsear query params
    query_params_t *qp = parse_query_string(req->query);

    // Rutas básicas
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

    if (strcmp(req->path, "/help") == 0) {
        const char *help_json = "{\"message\":\"HTTP Server v1.0\",\"note\":\"Router available\"}";
        ssize_t sent = http_send_json(client_fd, HTTP_OK, help_json, request_id);
        free_query_params(qp);
        return sent;
    }

    // Basic Commands: /fibonacci, /hash, /random,  /reverse, /timestamp/, /toupper
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

    if (strcmp(req->path, "/timestamp") == 0) {
        time_t now = time(NULL);
        char json[256];
        snprintf(json, sizeof(json), "{\"timestamp\":%ld}", (long)now);
        ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
        free_query_params(qp);
        return sent;
    }

    // Cpu Bound Commands: /is_prime, /factor 
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

    // IO Bound commands: /sortfile, /wordcount, /hashfile
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



    // Files commands: /createfile, /deletefile
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


    // JOBS endpoints: /jobs/submit, /jobs/status, /jobs/result, /jobs/cancel
    if (strncmp(req->path, "/jobs/", 6) == 0) {
        const char *sub = req->path + 6; // after /jobs/

        if (strcmp(sub, "submit") == 0) {
            const char *task = get_query_param(qp, "task");
            if (!task) { free_query_params(qp); return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'task' parameter", request_id); }
            // Construir payload JSON con los parámetros excepto 'task'
            // Para simplicidad incluimos todo el query como JSON
            char *payload = build_json_from_params(qp);
            // Encolar job
            char *job_id = job_submit(task, payload, 0);
            free(payload);
            if (!job_id) { free_query_params(qp); return http_send_error(client_fd, HTTP_INTERNAL_ERROR, "Failed to submit job", request_id); }
            char json[512];
            snprintf(json, sizeof(json), "{\"job_id\":\"%s\",\"status\":\"queued\"}", job_id);
            free(job_id);
            ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
            free_query_params(qp);
            return sent;
        }

        if (strcmp(sub, "status") == 0) {
            const char *id = get_query_param(qp, "id");
            if (!id) { free_query_params(qp); return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'id' parameter", request_id); }
            job_status_info_t info;
            int r = job_get_status(id, &info);
            if (r != 0) { free_query_params(qp); return http_send_error(client_fd, HTTP_NOT_FOUND, "Job not found", request_id); }
            char json[256];
            snprintf(json, sizeof(json), "{\"status\":\"%s\",\"progress\":%d,\"eta_ms\":%ld}", job_status_to_string(info.status), info.progress, info.eta_ms);
            ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
            free_query_params(qp);
            return sent;
        }

        if (strcmp(sub, "result") == 0) {
            const char *id = get_query_param(qp, "id");
            if (!id) { free_query_params(qp); return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'id' parameter", request_id); }
            char *res = job_get_result(id);
            if (!res) { free_query_params(qp); return http_send_error(client_fd, HTTP_NOT_FOUND, "Result not available", request_id); }
            // res puede ya ser JSON. Enviarlo tal cual.
            ssize_t sent = http_send_json(client_fd, HTTP_OK, res, request_id);
            free(res); free_query_params(qp);
            return sent;
        }

        if (strcmp(sub, "cancel") == 0) {
            const char *id = get_query_param(qp, "id");
            if (!id) { free_query_params(qp); return http_send_error(client_fd, HTTP_BAD_REQUEST, "Missing 'id' parameter", request_id); }
            int r = job_cancel(id);
            if (r == -1) { free_query_params(qp); return http_send_error(client_fd, HTTP_NOT_FOUND, "Job not found", request_id); }
            if (r == 1) {
                char json[128]; snprintf(json, sizeof(json), "{\"status\":\"not_cancelable\"}");
                ssize_t sent = http_send_json(client_fd, HTTP_CONFLICT, json, request_id);
                free_query_params(qp);
                return sent;
            }
            char json[128]; snprintf(json, sizeof(json), "{\"status\":\"canceled\"}");
            ssize_t sent = http_send_json(client_fd, HTTP_OK, json, request_id);
            free_query_params(qp);
            return sent;
        }
    }

    // Otras rutas no implementadas: devolver 404
    free_query_params(qp);
    return http_send_error(client_fd, HTTP_NOT_FOUND, "Endpoint not implemented", request_id);
}
