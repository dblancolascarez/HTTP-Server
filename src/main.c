// Punto de entrada, inicialización global
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "server/server.h"
#include "utils/utils.h"

// Variable global para el servidor (para signal handler)
static server_state_t *g_server = NULL;

// ============================================================================
// SIGNAL HANDLER - Para graceful shutdown
// ============================================================================

void signal_handler(int signum) {
    (void)signum;
    
    printf("\n");
    LOG_INFO("Received signal, shutting down gracefully...");
    
    if (g_server) {
        server_shutdown(g_server);
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char *argv[]) {
    // Parsear puerto desde argumentos (o usar default)
    int port = 8080;
    
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port: %s\n", argv[1]);
            fprintf(stderr, "Usage: %s [port]\n", argv[0]);
            return 1;
        }
    }
    
    // Inicializar logger
    logger_init(LOG_INFO, NULL); // NULL = stderr
    
    LOG_INFO("===========================================");
    LOG_INFO("  HTTP Server v1.0");
    LOG_INFO("===========================================");
    LOG_INFO("PID: %d", getpid());
    
    // Configurar servidor
    server_config_t config = {
        .port = port,
        .max_connections = 100,
        .request_timeout_sec = 30,
        .max_request_size = 8192,
        .running = false
    };
    
    // Inicializar servidor
    g_server = server_init(&config);
    if (!g_server) {
        LOG_ERROR("Failed to initialize server");
        logger_shutdown();
        return 1;
    }
    
    // Configurar signal handlers para graceful shutdown
    signal(SIGINT, signal_handler);   // Ctrl+C
    signal(SIGTERM, signal_handler);  // kill
    
    LOG_INFO("Server ready. Press Ctrl+C to stop.");
    LOG_INFO("Try: curl http://localhost:%d/status", port);
    LOG_INFO("     curl http://localhost:%d/help", port);
    
    // Iniciar servidor (bloquea hasta shutdown)
    int result = server_start(g_server);
    
    // Cleanup
    LOG_INFO("Cleaning up...");
    server_destroy(g_server);
    logger_shutdown();
    
    LOG_INFO("Server stopped successfully");
    
    return result == 0 ? 0 : 1;
}