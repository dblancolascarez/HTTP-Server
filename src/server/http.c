// Parser + Response builder (combinado)
#include "http.h"
#include "../utils/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

// Helper: ensure all bytes are written to the socket (handle partial writes)
static ssize_t write_all(int fd, const void *buf, size_t len) {
    const char *p = buf;
    size_t remaining = len;
    while (remaining > 0) {
        ssize_t w = write(fd, p, remaining);
        if (w < 0) {
            return -1;
        }
        if (w == 0) {
            return -1;
        }
        p += w;
        remaining -= (size_t)w;
    }
    return (ssize_t)len;
}

// ============================================================================
// HTTP PARSING
// ============================================================================

int http_parse_request(const char *raw_request, http_request_t *request) {
    if (!raw_request || !request) {
        return -1;
    }
    
    // Inicializar estructura
    memset(request, 0, sizeof(http_request_t));
    request->connection_close = true; // HTTP/1.0 por defecto
    
    // Buscar fin de la primera línea
    const char *line_end = strstr(raw_request, "\r\n");
    if (!line_end) {
        return -1;
    }
    
    // Copiar primera línea (request line)
    char request_line[2048];
    size_t line_len = line_end - raw_request;
    if (line_len >= sizeof(request_line)) {
        return -1;
    }
    
    strncpy(request_line, raw_request, line_len);
    request_line[line_len] = '\0';
    
    // Parsear: METHOD URI VERSION
    char uri[2304]; // path + query
    if (sscanf(request_line, "%15s %2303s %15s", 
               request->method, uri, request->version) != 3) {
        return -1;
    }
    
    // Verificar versión HTTP
    if (strcmp(request->version, "HTTP/1.0") != 0 && 
        strcmp(request->version, "HTTP/1.1") != 0) {
        return -1;
    }
    
    // Separar path y query
    http_split_uri(uri, request->path, request->query);
    
    // Parsear headers (simplificado)
    const char *headers_start = line_end + 2; // Saltar \r\n
    const char *header_line = headers_start;
    
    while (*header_line != '\0') {
        // Buscar fin de línea
        const char *header_end = strstr(header_line, "\r\n");
        if (!header_end) break;
        
        // Línea vacía indica fin de headers
        if (header_end == header_line) {
            break;
        }
        
        // Copiar línea del header
        char header[512];
        size_t header_len = header_end - header_line;
        if (header_len >= sizeof(header)) {
            header_line = header_end + 2;
            continue;
        }
        
        strncpy(header, header_line, header_len);
        header[header_len] = '\0';
        
        // Parsear "Key: Value"
        char *colon = strchr(header, ':');
        if (colon) {
            *colon = '\0';
            char *key = header;
            char *value = colon + 1;
            
            // Trim whitespace del value
            while (*value == ' ' || *value == '\t') value++;
            
            // Parsear headers conocidos
            if (strcasecmp(key, "Host") == 0) {
                strncpy(request->host, value, sizeof(request->host) - 1);
            } else if (strcasecmp(key, "Content-Length") == 0) {
                request->content_length = atoi(value);
            } else if (strcasecmp(key, "Connection") == 0) {
                if (strcasecmp(value, "keep-alive") == 0) {
                    request->connection_close = false;
                }
            }
        }
        
        header_line = header_end + 2;
    }
    
    return 0;
}

void http_split_uri(const char *uri, char *path, char *query) {
    if (!uri) return;
    
    // Buscar '?'
    const char *question = strchr(uri, '?');
    
    if (question) {
        // Hay query string
        size_t path_len = question - uri;
        strncpy(path, uri, path_len);
        path[path_len] = '\0';
        
        strcpy(query, question + 1);
    } else {
        // No hay query string
        strcpy(path, uri);
        query[0] = '\0';
    }
}

bool http_is_method_supported(const char *method) {
    return (strcmp(method, "GET") == 0 || strcmp(method, "HEAD") == 0);
}

// ============================================================================
// HTTP RESPONSE BUILDING
// ============================================================================

const char* http_status_text(int status_code) {
    switch (status_code) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 409: return "Conflict";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 503: return "Service Unavailable";
        default:  return "Unknown";
    }
}

int http_send_response(int client_fd, const http_response_t *response) {
    if (client_fd < 0 || !response) {
        return -1;
    }
    
    char header_buffer[4096];
    int header_len = 0;
    
    // Status line
    header_len += snprintf(header_buffer + header_len, 
                          sizeof(header_buffer) - header_len,
                          "HTTP/1.0 %d %s\r\n",
                          response->status_code,
                          http_status_text(response->status_code));
    
    // Content-Type
    if (response->content_type) {
        header_len += snprintf(header_buffer + header_len,
                              sizeof(header_buffer) - header_len,
                              "Content-Type: %s\r\n",
                              response->content_type);
    }
    
    // Content-Length
    size_t body_len = response->body_length > 0 ? 
                      response->body_length : 
                      (response->body ? strlen(response->body) : 0);
    
    header_len += snprintf(header_buffer + header_len,
                          sizeof(header_buffer) - header_len,
                          "Content-Length: %zu\r\n",
                          body_len);
    
    // X-Request-Id
    if (response->request_id) {
        header_len += snprintf(header_buffer + header_len,
                              sizeof(header_buffer) - header_len,
                              "X-Request-Id: %s\r\n",
                              response->request_id);
    }
    
    // X-Worker-Pid
    header_len += snprintf(header_buffer + header_len,
                          sizeof(header_buffer) - header_len,
                          "X-Worker-Pid: %d\r\n",
                          response->worker_pid);
    
    // Connection: close (HTTP/1.0)
    header_len += snprintf(header_buffer + header_len,
                          sizeof(header_buffer) - header_len,
                          "Connection: close\r\n");
    
    // Headers extra (si existen)
    if (response->extra_headers) {
        header_len += snprintf(header_buffer + header_len,
                              sizeof(header_buffer) - header_len,
                              "%s",
                              response->extra_headers);
    }
    
    // Línea vacía para separar headers del body
    header_len += snprintf(header_buffer + header_len,
                          sizeof(header_buffer) - header_len,
                          "\r\n");
    
    // Enviar headers (asegurando escrituras completas)
    ssize_t sent = write_all(client_fd, header_buffer, (size_t)header_len);
    if (sent < 0) {
        return -1;
    }

    // Enviar body si existe
    if (response->body && body_len > 0) {
        ssize_t body_sent = write_all(client_fd, response->body, body_len);
        if (body_sent < 0) {
            return -1;
        }
        return header_len + body_sent;
    }

    return header_len;
}

int http_send_json(int client_fd, int status_code, const char *json_body,
                   const char *request_id) {
    http_response_t response = {
        .status_code = status_code,
        .content_type = "application/json",
        .body = json_body,
        .body_length = json_body ? strlen(json_body) : 0,
        .request_id = request_id,
        .worker_pid = getpid(),
        .extra_headers = NULL
    };
    
    return http_send_response(client_fd, &response);
}

int http_send_error(int client_fd, int status_code, const char *error_message,
                    const char *request_id) {
    char json_buffer[1024];
    
    // Escapar comillas en el mensaje de error
    char escaped_msg[512];
    const char *src = error_message;
    char *dst = escaped_msg;
    size_t remaining = sizeof(escaped_msg) - 1;
    
    while (*src && remaining > 1) {
        if (*src == '"' || *src == '\\') {
            if (remaining < 2) break;
            *dst++ = '\\';
            remaining--;
        }
        *dst++ = *src++;
        remaining--;
    }
    *dst = '\0';
    
    // Construir JSON
    snprintf(json_buffer, sizeof(json_buffer),
             "{\"error\": \"%s\"}",
             escaped_msg);
    
    return http_send_json(client_fd, status_code, json_buffer, request_id);
}

int http_send_503_backpressure(int client_fd, int retry_after_ms,
                                const char *request_id) {
    char json_buffer[256];
    char extra_headers[128];
    
    snprintf(json_buffer, sizeof(json_buffer),
             "{\"error\": \"Service overloaded\", \"retry_after_ms\": %d}",
             retry_after_ms);
    
    snprintf(extra_headers, sizeof(extra_headers),
             "Retry-After: %d\r\n",
             (retry_after_ms + 999) / 1000); // Convertir a segundos (redondear arriba)
    
    http_response_t response = {
        .status_code = 503,
        .content_type = "application/json",
        .body = json_buffer,
        .body_length = strlen(json_buffer),
        .request_id = request_id,
        .worker_pid = getpid(),
        .extra_headers = extra_headers
    };
    
    return http_send_response(client_fd, &response);
}

// ============================================================================
// HTTP UTILITIES
// ============================================================================

const char* http_content_type_from_file(const char *filename) {
    if (!filename) return "application/octet-stream";
    
    const char *ext = strrchr(filename, '.');
    if (!ext) return "application/octet-stream";
    
    ext++; // Saltar el punto
    
    if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0) {
        return "text/html";
    } else if (strcmp(ext, "txt") == 0) {
        return "text/plain";
    } else if (strcmp(ext, "json") == 0) {
        return "application/json";
    } else if (strcmp(ext, "css") == 0) {
        return "text/css";
    } else if (strcmp(ext, "js") == 0) {
        return "application/javascript";
    } else if (strcmp(ext, "png") == 0) {
        return "image/png";
    } else if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0) {
        return "image/jpeg";
    } else if (strcmp(ext, "gif") == 0) {
        return "image/gif";
    }
    
    return "application/octet-stream";
}

bool http_is_path_safe(const char *path) {
    if (!path) return false;
    
    // Verificar que no contenga ../
    if (strstr(path, "../") != NULL) {
        return false;
    }
    
    // Verificar que no contenga /..
    if (strstr(path, "/..") != NULL) {
        return false;
    }
    
    // Verificar que no comience con ..
    if (strncmp(path, "..", 2) == 0) {
        return false;
    }
    
    return true;
}