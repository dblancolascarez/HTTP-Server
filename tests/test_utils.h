// ¿Qué hace?
// - Proporciona macros para escribir tests unitarios fácilmente
// - Cuenta cuántos tests pasan y fallan
// - Muestra resultados bonitos con ✅ y ❌

// ¿Por qué lo necesitas?
// - El proyecto REQUIERE 90% de cobertura de pruebas
// - Sin esto, tendrías que verificar todo manualmente
// - Detecta bugs ANTES de que rompan el servidor

#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// ============================================================================
// VARIABLES GLOBALES DE TESTING
// ============================================================================

static int g_test_passes = 0;
static int g_test_failures = 0;
static int g_current_test_failures = 0;
static const char *g_current_test_name = NULL;

// ============================================================================
// MACROS DE ASSERTIONS
// ============================================================================

#define ASSERT(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "  ❌ FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_test_failures++; \
            g_current_test_failures++; \
        } else { \
            g_test_passes++; \
        } \
    } while(0)

#define ASSERT_TRUE(x) ASSERT((x) == true)
#define ASSERT_FALSE(x) ASSERT((x) == false)
#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NEQ(a, b) ASSERT((a) != (b))
#define ASSERT_NULL(x) ASSERT((x) == NULL)
#define ASSERT_NOT_NULL(x) ASSERT((x) != NULL)

#define ASSERT_STR_EQ(a, b) \
    do { \
        if ((a) == NULL || (b) == NULL || strcmp((a), (b)) != 0) { \
            fprintf(stderr, "  ❌ FAIL: %s:%d: Expected \"%s\", got \"%s\"\n", \
                    __FILE__, __LINE__, (b), (a) ? (a) : "NULL"); \
            g_test_failures++; \
            g_current_test_failures++; \
        } else { \
            g_test_passes++; \
        } \
    } while(0)

#define ASSERT_FLOAT_EQ(a, b, epsilon) \
    do { \
        double diff = (a) - (b); \
        if (diff < 0) diff = -diff; \
        if (diff > (epsilon)) { \
            fprintf(stderr, "  ❌ FAIL: %s:%d: Expected %.6f, got %.6f (diff: %.6f)\n", \
                    __FILE__, __LINE__, (double)(b), (double)(a), diff); \
            g_test_failures++; \
            g_current_test_failures++; \
        } else { \
            g_test_passes++; \
        } \
    } while(0)

// ============================================================================
// TEST EXECUTION MACROS
// ============================================================================

#define TEST(test_name) \
    static void test_name(); \
    static void run_##test_name() { \
        g_current_test_name = #test_name; \
        g_current_test_failures = 0; \
        printf("  Running: %s ... ", #test_name); \
        fflush(stdout); \
        test_name(); \
        if (g_current_test_failures == 0) { \
            printf("✅ PASS\n"); \
        } else { \
            printf("❌ FAIL (%d assertions failed)\n", g_current_test_failures); \
        } \
    } \
    static void test_name()

#define RUN_TEST(test_name) run_##test_name()

// ============================================================================
// TEST SUITE MACROS
// ============================================================================

#define TEST_SUITE(suite_name) \
    void suite_name() { \
        printf("\n📦 Test Suite: %s\n", #suite_name); \
        printf("==========================================\n");

#define END_TEST_SUITE() \
        printf("\n"); \
    }

// ============================================================================
// SUMMARY FUNCTIONS
// ============================================================================

static inline void print_test_summary() {
    int total = g_test_passes + g_test_failures;
    double pass_rate = total > 0 ? (100.0 * g_test_passes / total) : 0.0;
    
    printf("\n");
    printf("==========================================\n");
    printf("📊 TEST SUMMARY\n");
    printf("==========================================\n");
    printf("Total Assertions: %d\n", total);
    printf("✅ Passed: %d\n", g_test_passes);
    printf("❌ Failed: %d\n", g_test_failures);
    printf("Pass Rate: %.2f%%\n", pass_rate);
    printf("==========================================\n");
    
    if (g_test_failures == 0) {
        printf("🎉 ALL TESTS PASSED!\n");
    } else {
        printf("⚠️  SOME TESTS FAILED\n");
    }
    printf("\n");
}

static inline int get_test_failures() {
    return g_test_failures;
}

static inline void reset_test_counters() {
    g_test_passes = 0;
    g_test_failures = 0;
    g_current_test_failures = 0;
}

#endif // TEST_UTILS_H