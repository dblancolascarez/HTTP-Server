#ifndef SERVER_H
#define SERVER_H

#include <stdbool.h>
#include <sys/time.h>
#include <pthread.h>
#include <stddef.h>        // For size_t (C header)
#include <sys/socket.h>    // For socket functions (Linux)
#include <netinet/in.h>    // For sockaddr_in (Linux)
#include <unistd.h>        // For close() (Linux)



// ============================================================================
// SERVER CONFIGURATION
// ============================================================================

typedef struct {
    int port;                       // Puerto del servidor
    int max_connections;            // Máximo de conexiones simultáneas
    int request_timeout_sec;        // Timeout para leer request (segundos)
    int max_request_size;           // Tamaño máximo del request (bytes)
    bool running;                   // Flag de estado del servidor
} server_config_t;

// ============================================================================
// SERVER STATISTICS
// ============================================================================

typedef struct {
    unsigned long connections_served;    // Total de conexiones atendidas
    unsigned long requests_ok;           // Requests con status 2xx
    unsigned long requests_error;        // Requests con status 4xx/5xx
    unsigned long bytes_received;        // Bytes totales recibidos
    unsigned long bytes_sent;            // Bytes totales enviados
    struct timeval start_time;           // Timestamp de inicio del servidor
    pthread_mutex_t mutex;               // Mutex para actualizar stats
} server_stats_t;

// ============================================================================
// SERVER STATE
// ============================================================================

typedef struct {
    server_config_t config;
    server_stats_t stats;
    int server_fd;                       // File descriptor del socket listener
    bool shutdown_requested;             // Flag para graceful shutdown
    pthread_mutex_t shutdown_mutex;      // Mutex para shutdown
} server_state_t;

// ============================================================================
// SERVER FUNCTIONS
// ============================================================================

/**
 * Inicializar servidor TCP
 * 
 * Crea socket, hace bind() y listen()
 * 
 * @param config Configuración del servidor
 * @return Puntero al estado del servidor, o NULL si falla
 */
server_state_t* server_init(const server_config_t *config);

/**
 * Iniciar servidor (accept loop)
 * 
 * Loop principal que:
 * 1. Acepta nuevas conexiones
 * 2. Crea thread por conexión
 * 3. Delega procesamiento
 * 
 * Esta función bloquea hasta que se llama server_shutdown()
 * 
 * @param server Estado del servidor
 * @return 0 si terminó correctamente, -1 si error
 */
int server_start(server_state_t *server);

/**
 * Detener servidor gracefully
 * 
 * Señaliza al servidor que debe detenerse después de
 * procesar las conexiones actuales
 * 
 * @param server Estado del servidor
 */
void server_shutdown(server_state_t *server);

/**
 * Liberar recursos del servidor
 * 
 * Debe llamarse después de server_shutdown() y
 * después de que server_start() haya retornado
 * 
 * @param server Estado del servidor
 */
void server_destroy(server_state_t *server);

/**
 * Obtener estadísticas del servidor (thread-safe)
 * 
 * @param server Estado del servidor
 * @param stats Buffer donde copiar las estadísticas
 */
void server_get_stats(server_state_t *server, server_stats_t *stats);

/**
 * Obtener uptime del servidor en segundos
 * 
 * @param server Estado del servidor
 * @return Segundos desde que inició
 */
long server_get_uptime(server_state_t *server);

/**
 * Actualizar contadores de estadísticas (thread-safe)
 * 
 * @param server Estado del servidor
 * @param is_ok true si fue 2xx, false si fue error
 * @param bytes_received Bytes recibidos en esta request
 * @param bytes_sent Bytes enviados en esta response
 */
void server_update_stats(server_state_t *server, bool is_ok,
                        size_t bytes_received, size_t bytes_sent);

// ============================================================================
// CONNECTION HANDLING
// ============================================================================

/**
 * Información de una conexión individual
 */
typedef struct {
    int client_fd;                  // File descriptor del cliente
    struct sockaddr_in client_addr; // Dirección del cliente
    server_state_t *server;         // Referencia al servidor
} connection_info_t;

/**
 * Handler que procesa una conexión individual
 * 
 * Esta función se ejecuta en un thread separado por cada conexión
 * 
 * Workflow:
 * 1. Leer HTTP request del socket
 * 2. Parsear request
 * 3. Generar request_id
 * 4. Delegar a router (FASE 4)
 * 5. Enviar response
 * 6. Cerrar conexión
 * 7. Actualizar estadísticas
 * 
 * @param arg Puntero a connection_info_t
 * @return NULL
 */
void* connection_handler(void *arg);

/**
 * Leer HTTP request completo del socket
 * 
 * Lee hasta encontrar \r\n\r\n (fin de headers)
 * 
 * @param client_fd File descriptor del cliente
 * @param buffer Buffer donde guardar el request
 * @param buffer_size Tamaño del buffer
 * @param timeout_sec Timeout en segundos
 * @return Bytes leídos, 0 si timeout, -1 si error
 */
ssize_t server_read_request(int client_fd, char *buffer, size_t buffer_size,
                            int timeout_sec);

#endif // SERVER_H