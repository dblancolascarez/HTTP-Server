#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <time.h>

#define SERVER_PORT 8080
#define SERVER_HOST "127.0.0.1"
#define MAX_CLIENTS 100

// Estructura para cada cliente
typedef struct {
    int client_id;
    int num_requests;
    int success_count;
    int error_count;
    double total_time_ms;
} client_stats_t;

// Mutex para proteger estadísticas globales
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;
int total_requests = 0;
int total_successes = 0;
int total_errors = 0;

// ============================================================================
// FUNCIONES AUXILIARES
// ============================================================================

int connect_to_server(const char *host, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &server_addr.sin_addr);
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        return -1;
    }
    
    return sock;
}

int send_http_request(int sock, const char *endpoint) {
    char request[1024];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: localhost\r\n"
             "Connection: close\r\n"
             "\r\n",
             endpoint);
    
    ssize_t sent = send(sock, request, strlen(request), 0);
    if (sent < 0) {
        return -1;
    }
    
    // Leer respuesta
    char response[8192];
    ssize_t received = recv(sock, response, sizeof(response) - 1, 0);
    if (received < 0) {
        return -1;
    }
    
    response[received] = '\0';
    
    // Verificar que la respuesta contiene "200 OK"
    if (strstr(response, "200 OK") != NULL) {
        return 0;
    }
    
    return -1;
}

// ============================================================================
// THREAD WORKER - Simula un cliente
// ============================================================================

void* client_worker(void *arg) {
    client_stats_t *stats = (client_stats_t*)arg;
    
    const char *endpoints[] = {
        "/random",
        "/timestamp",
        "/reverse?text=hello",
        "/toupper?text=world",
        "/isprime?n=17",
        "/factor?n=42",
        "/hashfile?name=test.txt&algo=md5",
        "/metrics"
    };
    int num_endpoints = sizeof(endpoints) / sizeof(endpoints[0]);
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < stats->num_requests; i++) {
        // Conectar al servidor
        int sock = connect_to_server(SERVER_HOST, SERVER_PORT);
        if (sock < 0) {
            stats->error_count++;
            usleep(10000); // 10ms de espera antes de reintentar
            continue;
        }
        
        // Seleccionar endpoint aleatorio
        int endpoint_idx = rand() % num_endpoints;
        
        // Enviar request
        if (send_http_request(sock, endpoints[endpoint_idx]) == 0) {
            stats->success_count++;
        } else {
            stats->error_count++;
        }
        
        close(sock);
        
        // Pequeña pausa aleatoria entre requests (0-5ms)
        usleep(rand() % 5000);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    stats->total_time_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                           (end.tv_nsec - start.tv_nsec) / 1000000.0;
    
    // Actualizar estadísticas globales
    pthread_mutex_lock(&stats_mutex);
    total_requests += stats->num_requests;
    total_successes += stats->success_count;
    total_errors += stats->error_count;
    pthread_mutex_unlock(&stats_mutex);
    
    return NULL;
}

// ============================================================================
// TESTS DE CONCURRENCIA
// ============================================================================

void test_concurrent_clients(int num_clients, int requests_per_client) {
    printf("\n========================================\n");
    printf("TEST: %d clientes concurrentes, %d requests c/u\n", 
           num_clients, requests_per_client);
    printf("========================================\n");
    
    pthread_t *threads = malloc(num_clients * sizeof(pthread_t));
    client_stats_t *stats = malloc(num_clients * sizeof(client_stats_t));
    
    // Resetear estadísticas globales
    total_requests = 0;
    total_successes = 0;
    total_errors = 0;
    
    // Inicializar estadísticas de cada cliente
    for (int i = 0; i < num_clients; i++) {
        stats[i].client_id = i;
        stats[i].num_requests = requests_per_client;
        stats[i].success_count = 0;
        stats[i].error_count = 0;
        stats[i].total_time_ms = 0.0;
    }
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Crear threads
    printf("Creando %d threads...\n", num_clients);
    for (int i = 0; i < num_clients; i++) {
        if (pthread_create(&threads[i], NULL, client_worker, &stats[i]) != 0) {
            fprintf(stderr, "❌ Error creando thread %d\n", i);
            exit(1);
        }
    }
    
    // Esperar a que terminen todos
    printf("Esperando a que terminen todos los clientes...\n");
    for (int i = 0; i < num_clients; i++) {
        pthread_join(threads[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double total_time = (end.tv_sec - start.tv_sec) * 1000.0 +
                        (end.tv_nsec - start.tv_nsec) / 1000000.0;
    
    // Resultados
    printf("\n--- RESULTADOS ---\n");
    printf("Total requests:  %d\n", total_requests);
    printf("Exitosos:        %d (%.2f%%)\n", 
           total_successes, 
           100.0 * total_successes / total_requests);
    printf("Errores:         %d (%.2f%%)\n", 
           total_errors,
           100.0 * total_errors / total_requests);
    printf("Tiempo total:    %.2f ms\n", total_time);
    printf("Throughput:      %.2f req/s\n", 
           total_requests / (total_time / 1000.0));
    
    // Verificar que no hubo muchos errores
    double error_rate = 100.0 * total_errors / total_requests;
    if (error_rate > 5.0) {
        printf("\n⚠️  ADVERTENCIA: Tasa de error alta (%.2f%%).\n", error_rate);
        printf("   Posible race condition o deadlock.\n");
    } else {
        printf("\n✅ Test PASSED - Tasa de error aceptable (%.2f%%)\n", error_rate);
    }
    
    free(threads);
    free(stats);
}

void test_stress_burst() {
    printf("\n========================================\n");
    printf("TEST: Burst de conexiones (sin pausa)\n");
    printf("========================================\n");
    
    int num_clients = 50;
    pthread_t *threads = malloc(num_clients * sizeof(pthread_t));
    client_stats_t *stats = malloc(num_clients * sizeof(client_stats_t));
    
    // Resetear
    total_requests = 0;
    total_successes = 0;
    total_errors = 0;
    
    for (int i = 0; i < num_clients; i++) {
        stats[i].client_id = i;
        stats[i].num_requests = 10;
        stats[i].success_count = 0;
        stats[i].error_count = 0;
    }
    
    // Crear todos los threads SIN PAUSAS (burst)
    printf("Creando %d threads simultáneamente...\n", num_clients);
    for (int i = 0; i < num_clients; i++) {
        pthread_create(&threads[i], NULL, client_worker, &stats[i]);
    }
    
    // Join
    for (int i = 0; i < num_clients; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("\n--- RESULTADOS BURST ---\n");
    printf("Total requests:  %d\n", total_requests);
    printf("Exitosos:        %d\n", total_successes);
    printf("Errores:         %d\n", total_errors);
    
    if (total_errors > total_requests * 0.1) {
        printf("\n⚠️  Alta tasa de error en burst - posible problema de concurrencia\n");
    } else {
        printf("\n✅ Burst test PASSED\n");
    }
    
    free(threads);
    free(stats);
}

void test_metrics_consistency() {
    printf("\n========================================\n");
    printf("TEST: Consistencia de /metrics bajo carga\n");
    printf("========================================\n");
    
    int num_clients = 20;
    pthread_t *threads = malloc(num_clients * sizeof(pthread_t));
    client_stats_t *stats = malloc(num_clients * sizeof(client_stats_t));
    
    for (int i = 0; i < num_clients; i++) {
        stats[i].client_id = i;
        stats[i].num_requests = 5;
        stats[i].success_count = 0;
        stats[i].error_count = 0;
    }
    
    // Todos los clientes consultan /metrics repetidamente
    printf("Consultando /metrics desde %d clientes simultáneos...\n", num_clients);
    
    for (int i = 0; i < num_clients; i++) {
        pthread_create(&threads[i], NULL, client_worker, &stats[i]);
    }
    
    for (int i = 0; i < num_clients; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Verificar al final consultando metrics
    int sock = connect_to_server(SERVER_HOST, SERVER_PORT);
    if (sock >= 0) {
        if (send_http_request(sock, "/metrics") == 0) {
            printf("✅ /metrics respondió correctamente después de carga concurrente\n");
        } else {
            printf("❌ /metrics falló - posible corrupción de datos\n");
        }
        close(sock);
    }
    
    free(threads);
    free(stats);
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char **argv) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║     TEST DE CONCURRENCIA Y RACE CONDITIONS                 ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("⚠️  IMPORTANTE: Debe estar corriendo el servidor en puerto 8080\n");
    printf("   Ejecutar antes: ./build/http_server 8080\n");
    printf("\n");
    
    // Verificar que el servidor está corriendo
    printf("Verificando conexión al servidor...\n");
    int test_sock = connect_to_server(SERVER_HOST, SERVER_PORT);
    if (test_sock < 0) {
        fprintf(stderr, "\n❌ ERROR: No se puede conectar al servidor.\n");
        fprintf(stderr, "   Asegúrate de que el servidor esté corriendo:\n");
        fprintf(stderr, "   ./build/http_server 8080\n\n");
        return 1;
    }
    close(test_sock);
    printf("✅ Servidor detectado\n");
    
    // Seed para números aleatorios
    srand(time(NULL));
    
    // TESTS
    test_concurrent_clients(10, 10);   // 10 clientes, 10 requests c/u
    sleep(1);
    
    test_concurrent_clients(50, 5);    // 50 clientes, 5 requests c/u
    sleep(1);
    
    test_concurrent_clients(100, 3);   // 100 clientes, 3 requests c/u
    sleep(1);
    
    test_stress_burst();               // Burst sin pausas
    sleep(1);
    
    test_metrics_consistency();        // Test de /metrics
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║              TESTS DE CONCURRENCIA COMPLETADOS             ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("📝 Para detectar race conditions más profundas, ejecutar:\n");
    printf("   valgrind --tool=helgrind ./build/http_server 8080\n");
    printf("   valgrind --tool=drd ./build/http_server 8080\n");
    printf("\n");
    
    return 0;
}
