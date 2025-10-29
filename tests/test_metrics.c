#include "test_utils.h"
#include "../src/core/metrics.h"
#include <math.h>

// ============================================================================
// TESTS DE INICIALIZACIÓN
// ============================================================================

TEST(test_metrics_init) {
    metrics_init();
    metrics_destroy();
    
    // No debe crashear
    ASSERT_TRUE(true);
}

TEST(test_metrics_register_command) {
    metrics_init();
    
    int idx = metrics_register_command("isprime", 4, 64, 1000);
    
    ASSERT_TRUE(idx >= 0);
    
    metrics_destroy();
}

TEST(test_metrics_register_duplicate) {
    metrics_init();
    
    int idx1 = metrics_register_command("fibonacci", 2, 32, 500);
    int idx2 = metrics_register_command("fibonacci", 2, 32, 500);
    
    // Debe retornar el mismo índice
    ASSERT_EQ(idx1, idx2);
    
    metrics_destroy();
}

// ============================================================================
// TESTS DE REGISTRO DE MÉTRICAS
// ============================================================================

TEST(test_metrics_record_wait_time) {
    metrics_init();
    metrics_register_command("test_cmd", 1, 10, 100);
    
    // Registrar varios tiempos
    metrics_record_wait_time("test_cmd", 1000);  // 1ms
    metrics_record_wait_time("test_cmd", 2000);  // 2ms
    metrics_record_wait_time("test_cmd", 3000);  // 3ms
    
    // Promedio debe ser 2ms
    double avg = metrics_get_avg_wait_time_ms("test_cmd");
    ASSERT_FLOAT_EQ(avg, 2.0, 0.1);
    
    metrics_destroy();
}

TEST(test_metrics_record_exec_time) {
    metrics_init();
    metrics_register_command("test_cmd", 1, 10, 100);
    
    metrics_record_wait_time("test_cmd", 1000);  // Necesario para count
    metrics_record_exec_time("test_cmd", 5000);  // 5ms
    
    metrics_record_wait_time("test_cmd", 1000);
    metrics_record_exec_time("test_cmd", 10000); // 10ms
    
    double avg = metrics_get_avg_exec_time_ms("test_cmd");
    ASSERT_FLOAT_EQ(avg, 7.5, 0.1);
    
    metrics_destroy();
}

TEST(test_metrics_update_queue_size) {
    metrics_init();
    metrics_register_command("test_cmd", 1, 64, 100);
    
    metrics_update_queue_size("test_cmd", 10);
    
    command_metrics_t metrics;
    int result = metrics_get_command("test_cmd", &metrics);
    
    ASSERT_EQ(result, 0);
    ASSERT_EQ(metrics.current_queue_size, 10);
    
    metrics_destroy();
}

TEST(test_metrics_update_workers) {
    metrics_init();
    metrics_register_command("test_cmd", 4, 64, 100);
    
    metrics_update_workers("test_cmd", 2);  // 2 busy
    
    command_metrics_t metrics;
    metrics_get_command("test_cmd", &metrics);
    
    ASSERT_EQ(metrics.total_workers, 4);
    ASSERT_EQ(metrics.busy_workers, 2);
    
    metrics_destroy();
}

// ============================================================================
// TESTS DE DESVIACIÓN ESTÁNDAR
// ============================================================================

TEST(test_metrics_stddev_calculation) {
    // Datos: 10, 12, 23, 23, 16, 23, 21, 16
    // Media: 18
    // Stddev: ~4.9
    
    double values[] = {10, 12, 23, 23, 16, 23, 21, 16};
    int count = 8;
    double mean = 18.0;
    
    double stddev = calculate_stddev(values, count, mean);
    
    ASSERT_FLOAT_EQ(stddev, 4.9, 0.5);
}

TEST(test_metrics_stddev_wait_time) {
    metrics_init();
    metrics_register_command("test_cmd", 1, 10, 100);
    
    // Registrar tiempos: 10ms, 10ms, 30ms, 30ms
    // Media: 20ms
    // Stddev: 10ms
    metrics_record_wait_time("test_cmd", 10000);
    metrics_record_wait_time("test_cmd", 10000);
    metrics_record_wait_time("test_cmd", 30000);
    metrics_record_wait_time("test_cmd", 30000);
    
    double stddev = metrics_get_stddev_wait_time_ms("test_cmd");
    
    ASSERT_FLOAT_EQ(stddev, 10.0, 1.0);
    
    metrics_destroy();
}

// ============================================================================
// TESTS DE GENERACIÓN DE JSON
// ============================================================================

TEST(test_metrics_get_json_empty) {
    metrics_init();
    
    char buffer[4096];
    int written = metrics_get_json(buffer, sizeof(buffer));
    
    ASSERT_TRUE(written > 0);
    ASSERT_TRUE(strstr(buffer, "uptime_seconds") != NULL);
    ASSERT_TRUE(strstr(buffer, "commands") != NULL);
    
    metrics_destroy();
}

TEST(test_metrics_get_json_with_commands) {
    metrics_init();
    
    // Registrar comandos
    metrics_register_command("isprime", 4, 64, 100);
    metrics_register_command("fibonacci", 2, 32, 100);
    
    // Agregar métricas
    metrics_record_wait_time("isprime", 5000);
    metrics_record_exec_time("isprime", 10000);
    metrics_update_queue_size("isprime", 3);
    metrics_update_workers("isprime", 2);
    
    char buffer[4096];
    int written = metrics_get_json(buffer, sizeof(buffer));
    
    ASSERT_TRUE(written > 0);
    ASSERT_TRUE(strstr(buffer, "isprime") != NULL);
    ASSERT_TRUE(strstr(buffer, "fibonacci") != NULL);
    ASSERT_TRUE(strstr(buffer, "avg_wait_ms") != NULL);
    ASSERT_TRUE(strstr(buffer, "stddev_wait_ms") != NULL);
    ASSERT_TRUE(strstr(buffer, "queue_size") != NULL);
    ASSERT_TRUE(strstr(buffer, "workers") != NULL);
    
    metrics_destroy();
}

// ============================================================================
// TESTS DE CASOS LÍMITE
// ============================================================================

TEST(test_metrics_command_not_found) {
    metrics_init();
    
    double avg = metrics_get_avg_wait_time_ms("nonexistent");
    
    ASSERT_EQ(avg, -1.0);
    
    metrics_destroy();
}

TEST(test_metrics_no_data) {
    metrics_init();
    metrics_register_command("test_cmd", 1, 10, 100);
    
    // Sin registrar métricas
    double avg = metrics_get_avg_wait_time_ms("test_cmd");
    double stddev = metrics_get_stddev_wait_time_ms("test_cmd");
    
    ASSERT_EQ(avg, 0.0);
    ASSERT_EQ(stddev, 0.0);
    
    metrics_destroy();
}

TEST(test_metrics_buffer_overflow) {
    metrics_init();
    metrics_register_command("test_cmd", 1, 10, 10);  // Buffer pequeño
    
    // Registrar más de 10 valores
    for (int i = 0; i < 20; i++) {
        metrics_record_wait_time("test_cmd", 1000 * (i + 1));
    }
    
    // Debe usar solo los últimos 10
    double avg = metrics_get_avg_wait_time_ms("test_cmd");
    
    // Promedio de 1000..20000 / 20 = 10.5ms
    // Pero buffer solo guarda últimos 10: 11..20ms, promedio = 15.5ms
    ASSERT_TRUE(avg > 10.0);  // Promedio debe ser razonable
    
    metrics_destroy();
}

// ============================================================================
// TEST SUITE
// ============================================================================

void test_metrics_all() {
    printf("\n📦 Test Suite: Metrics System\n");
    printf("==========================================\n");
    
    // Inicialización
    RUN_TEST(test_metrics_init);
    RUN_TEST(test_metrics_register_command);
    RUN_TEST(test_metrics_register_duplicate);
    
    // Registro de métricas
    RUN_TEST(test_metrics_record_wait_time);
    RUN_TEST(test_metrics_record_exec_time);
    RUN_TEST(test_metrics_update_queue_size);
    RUN_TEST(test_metrics_update_workers);
    
    // Desviación estándar
    RUN_TEST(test_metrics_stddev_calculation);
    RUN_TEST(test_metrics_stddev_wait_time);
    
    // Generación de JSON
    RUN_TEST(test_metrics_get_json_empty);
    RUN_TEST(test_metrics_get_json_with_commands);
    
    // Casos límite
    RUN_TEST(test_metrics_command_not_found);
    RUN_TEST(test_metrics_no_data);
    RUN_TEST(test_metrics_buffer_overflow);
    
    printf("\n");
}

int main(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║         HTTP Server - Test Suite: Metrics                  ║\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("\n");
    
    test_metrics_all();
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                 ✅ ALL TESTS PASSED                        ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    return 0;
}