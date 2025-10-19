// Runner principal de tests
#include "test_utils.h"

// Declarar las funciones de test suites
void test_queue_all();
// void test_string_utils_all();  // Descomentar cuando esté listo

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║   HTTP SERVER - UNIT TESTS            ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    // Resetear contadores
    reset_test_counters();
    
    // Ejecutar test suites
    test_queue_all();
    // test_string_utils_all();  // Descomentar cuando esté listo
    
    // Imprimir resumen
    print_test_summary();
    
    // Retornar exit code apropiado
    return get_test_failures() > 0 ? 1 : 0;
}