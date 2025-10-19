// Tests de cola thread-safe
#include "test_utils.h"
#include "../src/core/queue.h"
#include <pthread.h>
#include <unistd.h>

// ============================================================================
// TESTS BÁSICOS - CREAR Y DESTRUIR
// ============================================================================

TEST(test_queue_create) {
    queue_t *queue = queue_create(10);
    
    ASSERT_NOT_NULL(queue);
    ASSERT_EQ(queue_size(queue), 0);
    ASSERT_TRUE(queue_is_empty(queue));
    ASSERT_FALSE(queue_is_full(queue));
    
    queue_destroy(queue);
}

TEST(test_queue_create_unlimited) {
    queue_t *queue = queue_create(0); // 0 = ilimitado
    
    ASSERT_NOT_NULL(queue);
    ASSERT_EQ(queue_size(queue), 0);
    ASSERT_FALSE(queue_is_full(queue)); // Nunca está llena si es ilimitada
    
    queue_destroy(queue);
}

// ============================================================================
// TESTS DE TASK
// ============================================================================

TEST(test_task_create) {
    task_t *task = task_create(42, "/isprime", "n=97", "req-123");
    
    ASSERT_NOT_NULL(task);
    ASSERT_EQ(task->client_fd, 42);
    ASSERT_STR_EQ(task->path, "/isprime");
    ASSERT_STR_EQ(task->query, "n=97");
    ASSERT_STR_EQ(task->request_id, "req-123");
    ASSERT_EQ(task->priority, 1); // Normal por defecto
    
    task_free(task);
}

TEST(test_task_create_null_query) {
    task_t *task = task_create(10, "/status", NULL, "req-456");
    
    ASSERT_NOT_NULL(task);
    ASSERT_NULL(task->query);
    
    task_free(task);
}

// ============================================================================
// TESTS DE ENQUEUE/DEQUEUE BÁSICO
// ============================================================================

TEST(test_enqueue_dequeue_single) {
    queue_t *queue = queue_create(10);
    task_t *task = task_create(1, "/test", "a=1", "req-1");
    
    // Encolar
    int result = queue_enqueue(queue, task, 0);
    ASSERT_EQ(result, 0);
    ASSERT_EQ(queue_size(queue), 1);
    ASSERT_FALSE(queue_is_empty(queue));
    
    // Desencolar
    task_t *dequeued = queue_dequeue_timeout(queue, 100);
    ASSERT_NOT_NULL(dequeued);
    ASSERT_STR_EQ(dequeued->path, "/test");
    ASSERT_EQ(queue_size(queue), 0);
    ASSERT_TRUE(queue_is_empty(queue));
    
    task_free(dequeued);
    queue_destroy(queue);
}

TEST(test_enqueue_dequeue_multiple) {
    queue_t *queue = queue_create(10);
    
    // Encolar 3 tareas
    task_t *t1 = task_create(1, "/test1", "a=1", "req-1");
    task_t *t2 = task_create(2, "/test2", "b=2", "req-2");
    task_t *t3 = task_create(3, "/test3", "c=3", "req-3");
    
    queue_enqueue(queue, t1, 0);
    queue_enqueue(queue, t2, 0);
    queue_enqueue(queue, t3, 0);
    
    ASSERT_EQ(queue_size(queue), 3);
    
    // Desencolar en orden FIFO
    task_t *d1 = queue_dequeue_timeout(queue, 100);
    ASSERT_STR_EQ(d1->path, "/test1");
    
    task_t *d2 = queue_dequeue_timeout(queue, 100);
    ASSERT_STR_EQ(d2->path, "/test2");
    
    task_t *d3 = queue_dequeue_timeout(queue, 100);
    ASSERT_STR_EQ(d3->path, "/test3");
    
    ASSERT_EQ(queue_size(queue), 0);
    
    task_free(d1);
    task_free(d2);
    task_free(d3);
    queue_destroy(queue);
}

// ============================================================================
// TESTS DE BACKPRESSURE (COLA LLENA)
// ============================================================================

TEST(test_enqueue_full_no_wait) {
    queue_t *queue = queue_create(2); // Máximo 2 elementos
    
    task_t *t1 = task_create(1, "/test1", NULL, "req-1");
    task_t *t2 = task_create(2, "/test2", NULL, "req-2");
    task_t *t3 = task_create(3, "/test3", NULL, "req-3");
    
    // Encolar hasta llenar
    ASSERT_EQ(queue_enqueue(queue, t1, 0), 0);
    ASSERT_EQ(queue_enqueue(queue, t2, 0), 0);
    ASSERT_TRUE(queue_is_full(queue));
    
    // Intentar encolar sin esperar (timeout=0)
    int result = queue_enqueue(queue, t3, 0);
    ASSERT_EQ(result, -1); // Debe fallar
    
    // Verificar métricas
    unsigned long dropped;
    queue_get_stats(queue, NULL, NULL, &dropped);
    ASSERT_EQ(dropped, 1);
    
    // Limpiar
    task_free(t3);
    task_free(queue_dequeue_timeout(queue, 100));
    task_free(queue_dequeue_timeout(queue, 100));
    queue_destroy(queue);
}

TEST(test_enqueue_full_with_timeout) {
    queue_t *queue = queue_create(1);
    
    task_t *t1 = task_create(1, "/test1", NULL, "req-1");
    task_t *t2 = task_create(2, "/test2", NULL, "req-2");
    
    // Llenar cola
    queue_enqueue(queue, t1, 0);
    
    // Intentar encolar con timeout de 100ms (debe fallar por timeout)
    int result = queue_enqueue(queue, t2, 100);
    ASSERT_EQ(result, -1);
    
    task_free(t2);
    task_free(queue_dequeue_timeout(queue, 100));
    queue_destroy(queue);
}

// ============================================================================
// TESTS DE DEQUEUE CON COLA VACÍA
// ============================================================================

TEST(test_dequeue_empty_timeout) {
    queue_t *queue = queue_create(10);
    
    // Intentar desencolar de cola vacía con timeout
    task_t *task = queue_dequeue_timeout(queue, 100);
    
    ASSERT_NULL(task); // Debe retornar NULL por timeout
    
    queue_destroy(queue);
}

// ============================================================================
// TESTS DE SHUTDOWN
// ============================================================================

TEST(test_shutdown_dequeue_returns_null) {
    queue_t *queue = queue_create(10);
    
    // Activar shutdown
    queue_shutdown(queue);
    
    // Intentar desencolar debe retornar NULL inmediatamente
    task_t *task = queue_dequeue(queue);
    ASSERT_NULL(task);
    
    queue_destroy(queue);
}

TEST(test_shutdown_enqueue_fails) {
    queue_t *queue = queue_create(10);
    
    queue_shutdown(queue);
    
    task_t *task = task_create(1, "/test", NULL, "req-1");
    int result = queue_enqueue(queue, task, 0);
    
    ASSERT_EQ(result, -2); // -2 indica shutdown
    
    task_free(task);
    queue_destroy(queue);
}

// ============================================================================
// TESTS DE MÉTRICAS
// ============================================================================

TEST(test_queue_stats) {
    queue_t *queue = queue_create(10);
    
    // Encolar y desencolar
    for (int i = 0; i < 5; i++) {
        task_t *task = task_create(i, "/test", NULL, "req");
        queue_enqueue(queue, task, 0);
    }
    
    for (int i = 0; i < 3; i++) {
        task_t *task = queue_dequeue_timeout(queue, 100);
        task_free(task);
    }
    
    // Verificar stats
    unsigned long enqueued, dequeued, dropped;
    queue_get_stats(queue, &enqueued, &dequeued, &dropped);
    
    ASSERT_EQ(enqueued, 5);
    ASSERT_EQ(dequeued, 3);
    ASSERT_EQ(dropped, 0);
    ASSERT_EQ(queue_size(queue), 2); // 5 - 3 = 2 pendientes
    
    // Limpiar
    while (!queue_is_empty(queue)) {
        task_free(queue_dequeue_timeout(queue, 100));
    }
    queue_destroy(queue);
}

// ============================================================================
// TESTS DE CONCURRENCIA
// ============================================================================

// Estructura para pasar datos a threads
typedef struct {
    queue_t *queue;
    int thread_id;
    int num_tasks;
    int *counter; // Contador compartido
    pthread_mutex_t *counter_mutex;
} thread_data_t;

// Thread productor: encola tareas
void* producer_thread(void *arg) {
    thread_data_t *data = (thread_data_t*)arg;
    
    for (int i = 0; i < data->num_tasks; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/task-%d-%d", data->thread_id, i);
        
        task_t *task = task_create(data->thread_id, path, NULL, "req");
        
        // Encolar con timeout
        int result = queue_enqueue(data->queue, task, 5000); // 5s timeout
        if (result != 0) {
            task_free(task);
        }
        
        // Simular trabajo
        usleep(1000); // 1ms
    }
    
    return NULL;
}

// Thread consumidor: desencola tareas
void* consumer_thread(void *arg) {
    thread_data_t *data = (thread_data_t*)arg;
    
    while (1) {
        task_t *task = queue_dequeue_timeout(data->queue, 2000); // 2s timeout
        
        if (!task) {
            break; // Timeout o shutdown
        }
        
        // Incrementar contador (thread-safe)
        pthread_mutex_lock(data->counter_mutex);
        (*data->counter)++;
        pthread_mutex_unlock(data->counter_mutex);
        
        task_free(task);
        
        // Simular procesamiento
        usleep(500); // 0.5ms
    }
    
    return NULL;
}

TEST(test_concurrent_multiple_producers) {
    queue_t *queue = queue_create(100);
    
    int num_producers = 3;
    int tasks_per_producer = 10;
    pthread_t producers[3];
    thread_data_t data[3];
    
    // Crear productores
    for (int i = 0; i < num_producers; i++) {
        data[i].queue = queue;
        data[i].thread_id = i;
        data[i].num_tasks = tasks_per_producer;
        pthread_create(&producers[i], NULL, producer_thread, &data[i]);
    }
    
    // Esperar a que terminen
    for (int i = 0; i < num_producers; i++) {
        pthread_join(producers[i], NULL);
    }
    
    // Verificar que se encolaron todas las tareas
    unsigned long enqueued;
    queue_get_stats(queue, &enqueued, NULL, NULL);
    ASSERT_EQ(enqueued, (size_t)(num_producers * tasks_per_producer));
    
    // Limpiar
    while (!queue_is_empty(queue)) {
        task_free(queue_dequeue_timeout(queue, 100));
    }
    queue_destroy(queue);
}

TEST(test_concurrent_producer_consumer) {
    queue_t *queue = queue_create(50);
    
    int num_producers = 2;
    int num_consumers = 3;
    int tasks_per_producer = 20;
    
    int processed_count = 0;
    pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    pthread_t producers[2];
    pthread_t consumers[3];
    thread_data_t prod_data[2];
    thread_data_t cons_data[3];
    
    // Crear consumidores primero
    for (int i = 0; i < num_consumers; i++) {
        cons_data[i].queue = queue;
        cons_data[i].thread_id = i;
        cons_data[i].counter = &processed_count;
        cons_data[i].counter_mutex = &counter_mutex;
        pthread_create(&consumers[i], NULL, consumer_thread, &cons_data[i]);
    }
    
    // Crear productores
    for (int i = 0; i < num_producers; i++) {
        prod_data[i].queue = queue;
        prod_data[i].thread_id = i;
        prod_data[i].num_tasks = tasks_per_producer;
        pthread_create(&producers[i], NULL, producer_thread, &prod_data[i]);
    }
    
    // Esperar productores
    for (int i = 0; i < num_producers; i++) {
        pthread_join(producers[i], NULL);
    }
    
    // Dar tiempo a que consuman todo
    sleep(1);
    
    // Activar shutdown para que consumidores terminen
    queue_shutdown(queue);
    
    // Esperar consumidores
    for (int i = 0; i < num_consumers; i++) {
        pthread_join(consumers[i], NULL);
    }
    
    // Verificar que se procesaron todas las tareas
    int expected = num_producers * tasks_per_producer;
    ASSERT_EQ(processed_count, expected);
    
    pthread_mutex_destroy(&counter_mutex);
    queue_destroy(queue);
}

TEST(test_concurrent_backpressure) {
    queue_t *queue = queue_create(5); // Cola pequeña
    
    int num_producers = 5;
    int tasks_per_producer = 10;
    pthread_t producers[5];
    thread_data_t data[5];
    
    // Crear productores (intentarán llenar cola rápidamente)
    for (int i = 0; i < num_producers; i++) {
        data[i].queue = queue;
        data[i].thread_id = i;
        data[i].num_tasks = tasks_per_producer;
        pthread_create(&producers[i], NULL, producer_thread, &data[i]);
    }
    
    // Esperar
    for (int i = 0; i < num_producers; i++) {
        pthread_join(producers[i], NULL);
    }
    
    // Verificar que NO todas se encolaron (algunas fueron rechazadas)
    unsigned long enqueued, dropped;
    queue_get_stats(queue, &enqueued, NULL, &dropped);
    
    // Con backpressure, algunas tareas deben haberse rechazado
    ASSERT_TRUE(dropped > 0);
    
    // Limpiar
    while (!queue_is_empty(queue)) {
        task_free(queue_dequeue_timeout(queue, 100));
    }
    queue_destroy(queue);
}

// ============================================================================
// TEST DE STRESS - MUCHAS OPERACIONES CONCURRENTES
// ============================================================================

TEST(test_stress_high_concurrency) {
    queue_t *queue = queue_create(200);
    
    int num_producers = 10;
    int num_consumers = 10;
    int tasks_per_producer = 50;
    
    int processed_count = 0;
    pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    pthread_t producers[10];
    pthread_t consumers[10];
    thread_data_t prod_data[10];
    thread_data_t cons_data[10];
    
    // Lanzar consumidores
    for (int i = 0; i < num_consumers; i++) {
        cons_data[i].queue = queue;
        cons_data[i].thread_id = i;
        cons_data[i].counter = &processed_count;
        cons_data[i].counter_mutex = &counter_mutex;
        pthread_create(&consumers[i], NULL, consumer_thread, &cons_data[i]);
    }
    
    // Lanzar productores
    for (int i = 0; i < num_producers; i++) {
        prod_data[i].queue = queue;
        prod_data[i].thread_id = i;
        prod_data[i].num_tasks = tasks_per_producer;
        pthread_create(&producers[i], NULL, producer_thread, &prod_data[i]);
    }
    
    // Esperar productores
    for (int i = 0; i < num_producers; i++) {
        pthread_join(producers[i], NULL);
    }
    
    // Esperar a que se procesen todas
    sleep(2);
    queue_shutdown(queue);
    
    // Esperar consumidores
    for (int i = 0; i < num_consumers; i++) {
        pthread_join(consumers[i], NULL);
    }
    
    // Verificar
    int expected = num_producers * tasks_per_producer;
    ASSERT_EQ(processed_count, expected);
    ASSERT_TRUE(queue_is_empty(queue));
    
    pthread_mutex_destroy(&counter_mutex);
    queue_destroy(queue);
}

// ============================================================================
// TEST SUITE PRINCIPAL
// ============================================================================

void test_queue_all() {
    printf("\n📦 Test Suite: Queue (Thread-Safe)\n");
    printf("==========================================\n");
    
    // Tests básicos
    RUN_TEST(test_queue_create);
    RUN_TEST(test_queue_create_unlimited);
    RUN_TEST(test_task_create);
    RUN_TEST(test_task_create_null_query);
    
    // Tests de enqueue/dequeue
    RUN_TEST(test_enqueue_dequeue_single);
    RUN_TEST(test_enqueue_dequeue_multiple);
    
    // Tests de backpressure
    RUN_TEST(test_enqueue_full_no_wait);
    RUN_TEST(test_enqueue_full_with_timeout);
    
    // Tests de cola vacía
    RUN_TEST(test_dequeue_empty_timeout);
    
    // Tests de shutdown
    RUN_TEST(test_shutdown_dequeue_returns_null);
    RUN_TEST(test_shutdown_enqueue_fails);
    
    // Tests de métricas
    RUN_TEST(test_queue_stats);
    
    // Tests de concurrencia
    RUN_TEST(test_concurrent_multiple_producers);
    RUN_TEST(test_concurrent_producer_consumer);
    RUN_TEST(test_concurrent_backpressure);
    
    // Test de stress
    RUN_TEST(test_stress_high_concurrency);
    
    printf("\n");
}

int main(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║         HTTP Server - Test Suite: Queue                   ║\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("\n");
    
    test_queue_all();
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                 ✅ ALL TESTS PASSED                        ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    return 0;
}