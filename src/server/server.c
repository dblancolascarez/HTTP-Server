// Socket, accept, config, shutdown
#include "server.h"
#include "http.h"
#include "../utils/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>

// ============================================================================
// SERVER INITIALIZATION
// ============================================================================

server_state_t* server_init(const server_config_t *config) {
    if (!config) {
        LOG_ERROR("Config is NULL");
        return NULL;
    }
    
    server_state_t *server = (server_state_t*)malloc(sizeof(server_state_t));
    if (!server) {
        LOG_ERROR("Failed to allocate server state");
        return NULL;
    }
    
    // Copiar configuración
    server->config = *config;
    server->shutdown_requested = false;
    
    // Inicializar estadísticas
    memset(&server->stats, 0, sizeof(server_stats_t));
    gettimeofday(&server->stats.start_time, NULL);
    pthread_mutex_init(&server->stats.mutex, NULL);
    pthread_mutex_init(&server->shutdown_mutex, NULL);
    
    // Crear socket TCP
    server->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_fd < 0) {
        LOG_ERROR("Failed to create socket: %s", strerror(errno));
        free(server);
        return NULL;
    }
    
    // Permitir reutilizar dirección inmediatamente (SO_REUSEADDR)
    int opt = 1;
    if (setsockopt(server->server_fd, SOL_SOCKET, SO_REUSEADDR, 
                   &opt, sizeof(opt)) < 0) {
        LOG_WARN("Failed to set SO_REUSEADDR: %s", strerror(errno));
    }
    
    // Configurar dirección
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; // Escuchar en todas las interfaces
    addr.sin_port = htons(config->port);
    
    // Bind
    if (bind(server->server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("Failed to bind to port %d: %s", config->port, strerror(errno));
        close(server->server_fd);
        free(server);
        return NULL;
    }
    
    // Listen
    if (listen(server->server_fd, config->max_connections) < 0) {
        LOG_ERROR("Failed to listen: %s", strerror(errno));
        close(server->server_fd);
        free(server);
        return NULL;
    }
    
    LOG_INFO("Server initialized on port %d (max connections: %d)",
             config->port, config->max_connections);
    
    return server;
}

// ============================================================================
// SERVER START (ACCEPT LOOP)
// ============================================================================

int server_start(server_state_t *server) {
    if (!server) {
        return -1;
    }
    
    LOG_INFO("Server starting... PID: %d", getpid());
    
    server->config.running = true;
    
    while (!server->shutdown_requested) {
        // Accept nueva conexión
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server->server_fd, 
                              (struct sockaddr*)&client_addr, 
                              &client_len);
        
        if (client_fd < 0) {
            if (errno == EINTR) {
                // Interrupción por señal, continuar
                continue;
            }
            
            LOG_ERROR("Accept failed: %s", strerror(errno));
            
            // Si shutdown fue solicitado, salir del loop
            pthread_mutex_lock(&server->shutdown_mutex);
            bool should_exit = server->shutdown_requested;
            pthread_mutex_unlock(&server->shutdown_mutex);
            
            if (should_exit) {
                break;
            }
            
            continue;
        }
        
        // Log de nueva conexión
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        LOG_DEBUG("New connection from %s:%d", client_ip, ntohs(client_addr.sin_port));
        
        // Crear estructura de información de la conexión
        connection_info_t *conn_info = (connection_info_t*)malloc(sizeof(connection_info_t));
        if (!conn_info) {
            LOG_ERROR("Failed to allocate connection info");
            close(client_fd);
            continue;
        }
        
        conn_info->client_fd = client_fd;
        conn_info->client_addr = client_addr;
        conn_info->server = server;
        
        // Crear thread para manejar la conexión
        pthread_t thread;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        
        if (pthread_create(&thread, &attr, connection_handler, conn_info) != 0) {
            LOG_ERROR("Failed to create connection thread: %s", strerror(errno));
            free(conn_info);
            close(client_fd);
        }
        
        pthread_attr_destroy(&attr);
        
        // Incrementar contador de conexiones
        pthread_mutex_lock(&server->stats.mutex);
        server->stats.connections_served++;
        pthread_mutex_unlock(&server->stats.mutex);
    }
    
    LOG_INFO("Server stopped");
    server->config.running = false;
    
    return 0;
}

// ============================================================================
// CONNECTION HANDLER
// ============================================================================

void* connection_handler(void *arg) {
    connection_info_t *conn = (connection_info_t*)arg;
    
    if (!conn) {
        return NULL;
    }
    
    int client_fd = conn->client_fd;
    server_state_t *server = conn->server;
    
    // Buffer para el request
    char request_buffer[8192];
    memset(request_buffer, 0, sizeof(request_buffer));
    
    // Leer request HTTP
    ssize_t bytes_read = server_read_request(
        client_fd, 
        request_buffer, 
        sizeof(request_buffer) - 1,
        server->config.request_timeout_sec
    );
    
    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            LOG_DEBUG("Client closed connection before sending data");
        } else {
            LOG_ERROR("Failed to read request: %s", strerror(errno));
        }
        close(client_fd);
        free(conn);
        return NULL;
    }
    
    request_buffer[bytes_read] = '\0';
    
    // Generar request ID único
    char request_id[64];
    generate_request_id(request_id, sizeof(request_id));
    
    LOG_DEBUG("Request received (id=%s, size=%zd bytes)", request_id, bytes_read);
    
    // Parsear HTTP request
    http_request_t http_req;
    if (http_parse_request(request_buffer, &http_req) != 0) {
        LOG_WARN("Failed to parse HTTP request (id=%s)", request_id);
        http_send_error(client_fd, HTTP_BAD_REQUEST, 
                       "Malformed HTTP request", request_id);
        
        server_update_stats(server, false, bytes_read, 0);
        close(client_fd);
        free(conn);
        return NULL;
    }
    
    // Verificar método soportado
    if (!http_is_method_supported(http_req.method)) {
        LOG_WARN("Unsupported method: %s (id=%s)", http_req.method, request_id);
        http_send_error(client_fd, HTTP_BAD_REQUEST,
                       "Method not supported", request_id);
        
        server_update_stats(server, false, bytes_read, 0);
        close(client_fd);
        free(conn);
        return NULL;
    }
    
    // Verificar path seguro
    if (!http_is_path_safe(http_req.path)) {
        LOG_WARN("Unsafe path detected: %s (id=%s)", http_req.path, request_id);
        http_send_error(client_fd, HTTP_BAD_REQUEST,
                       "Invalid path", request_id);
        
        server_update_stats(server, false, bytes_read, 0);
        close(client_fd);
        free(conn);
        return NULL;
    }
    
    LOG_INFO("Processing: %s %s%s%s (id=%s)", 
             http_req.method, 
             http_req.path,
             http_req.query[0] ? "?" : "",
             http_req.query,
             request_id);
    
    // ========================================================================
    // FASE 4: AQUÍ SE DELEGARÁ AL ROUTER
    // Por ahora, respuesta simple para todos los paths
    // ========================================================================
    
    // Respuesta temporal (será reemplazada por el router)
    char json_response[512];
    
    if (strcmp(http_req.path, "/status") == 0) {
        // Endpoint de status básico
        long uptime = server_get_uptime(server);
        
        server_stats_t stats;
        server_get_stats(server, &stats);
        
        snprintf(json_response, sizeof(json_response),
                "{"
                "\"status\":\"running\","
                "\"pid\":%d,"
                "\"uptime_seconds\":%ld,"
                "\"connections_served\":%lu,"
                "\"requests_ok\":%lu,"
                "\"requests_error\":%lu"
                "}",
                getpid(),
                uptime,
                stats.connections_served,
                stats.requests_ok,
                stats.requests_error
        );
        
        http_send_json(client_fd, HTTP_OK, json_response, request_id);
        server_update_stats(server, true, bytes_read, strlen(json_response));
        
    } else if (strcmp(http_req.path, "/help") == 0) {
        // Endpoint de ayuda
        const char *help_json = 
            "{"
            "\"message\":\"HTTP Server v1.0\","
            "\"endpoints\":[\"/status\",\"/help\"],"
            "\"note\":\"More endpoints coming in Phase 4 (Router)\""
            "}";
        
        http_send_json(client_fd, HTTP_OK, help_json, request_id);
        server_update_stats(server, true, bytes_read, strlen(help_json));
        
    } else {
        // Endpoint no encontrado (temporal)
        http_send_error(client_fd, HTTP_NOT_FOUND,
                       "Endpoint not found. Try /status or /help", request_id);
        server_update_stats(server, false, bytes_read, 0);
    }
    
    // Cerrar conexión
    close(client_fd);
    free(conn);
    
    return NULL;
}

// ============================================================================
// SERVER READ REQUEST
// ============================================================================

ssize_t server_read_request(int client_fd, char *buffer, size_t buffer_size,
                            int timeout_sec) {
    if (client_fd < 0 || !buffer || buffer_size == 0) {
        return -1;
    }
    
    // Configurar timeout con select()
    fd_set read_fds;
    struct timeval timeout;
    
    FD_ZERO(&read_fds);
    FD_SET(client_fd, &read_fds);
    
    timeout.tv_sec = timeout_sec;
    timeout.tv_usec = 0;
    
    int ready = select(client_fd + 1, &read_fds, NULL, NULL, &timeout);
    
    if (ready < 0) {
        return -1; // Error en select
    }
    
    if (ready == 0) {
        return 0; // Timeout
    }
    
    // Leer datos
    ssize_t total_read = 0;
    ssize_t bytes_read;
    
    // Leer hasta encontrar "\r\n\r\n" (fin de headers HTTP)
    while (total_read < (ssize_t)buffer_size - 1) {
        bytes_read = read(client_fd, buffer + total_read, 
                         buffer_size - total_read - 1);
        
        if (bytes_read < 0) {
            return -1; // Error
        }
        
        if (bytes_read == 0) {
            break; // Cliente cerró conexión
        }
        
        total_read += bytes_read;
        buffer[total_read] = '\0';
        
        // Verificar si tenemos headers completos
        if (strstr(buffer, "\r\n\r\n") != NULL) {
            break;
        }
        
        // Evitar leer indefinidamente
        if (total_read >= (ssize_t)buffer_size - 1) {
            break;
        }
    }
    
    return total_read;
}

// ============================================================================
// SERVER SHUTDOWN & CLEANUP
// ============================================================================

void server_shutdown(server_state_t *server) {
    if (!server) return;
    
    pthread_mutex_lock(&server->shutdown_mutex);
    server->shutdown_requested = true;
    pthread_mutex_unlock(&server->shutdown_mutex);
    
    LOG_INFO("Shutdown requested");
    
    // Cerrar socket listener para desbloquear accept()
    if (server->server_fd >= 0) {
        shutdown(server->server_fd, SHUT_RDWR);
        close(server->server_fd);
        server->server_fd = -1;
    }
}

void server_destroy(server_state_t *server) {
    if (!server) return;
    
    if (server->server_fd >= 0) {
        close(server->server_fd);
    }
    
    pthread_mutex_destroy(&server->stats.mutex);
    pthread_mutex_destroy(&server->shutdown_mutex);
    
    free(server);
}

// ============================================================================
// SERVER STATISTICS
// ============================================================================

void server_get_stats(server_state_t *server, server_stats_t *stats) {
    if (!server || !stats) return;
    
    pthread_mutex_lock(&server->stats.mutex);
    *stats = server->stats;
    pthread_mutex_unlock(&server->stats.mutex);
}

long server_get_uptime(server_state_t *server) {
    if (!server) return 0;
    
    struct timeval now;
    gettimeofday(&now, NULL);
    
    pthread_mutex_lock(&server->stats.mutex);
    long uptime = now.tv_sec - server->stats.start_time.tv_sec;
    pthread_mutex_unlock(&server->stats.mutex);
    
    return uptime;
}

void server_update_stats(server_state_t *server, bool is_ok,
                        size_t bytes_received, size_t bytes_sent) {
    if (!server) return;
    
    pthread_mutex_lock(&server->stats.mutex);
    
    if (is_ok) {
        server->stats.requests_ok++;
    } else {
        server->stats.requests_error++;
    }
    
    server->stats.bytes_received += bytes_received;
    server->stats.bytes_sent += bytes_sent;
    
    pthread_mutex_unlock(&server->stats.mutex);
}