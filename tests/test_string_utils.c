// ¿Qué hace?
// - Verifica que string_utils.c funcione correctamente
// - Prueba casos normales, casos límite y casos de error
// - Asegura que el parser no tiene bugs

// ¿Por qué lo necesitas?
// - Garantiza que tu parser funciona ANTES de usarlo en producción
// - Si cambias el código, los tests detectan si rompiste algo
// - Documentación viva: los tests muestran cómo usar las funciones
#include "test_utils.h"
#include "../src/utils/utils.h"

// ============================================================================
// TESTS DE URL_DECODE
// ============================================================================

TEST(test_url_decode_simple) {
    char *result = url_decode("hello");
    ASSERT_STR_EQ(result, "hello");
    free(result);
}

TEST(test_url_decode_spaces) {
    char *result = url_decode("hello+world");
    ASSERT_STR_EQ(result, "hello world");
    free(result);
}

TEST(test_url_decode_percent) {
    char *result = url_decode("hello%20world");
    ASSERT_STR_EQ(result, "hello world");
    free(result);
}

TEST(test_url_decode_special_chars) {
    char *result = url_decode("a%2Bb%3Dc");
    ASSERT_STR_EQ(result, "a+b=c");
    free(result);
}

TEST(test_url_decode_empty) {
    char *result = url_decode("");
    ASSERT_STR_EQ(result, "");
    free(result);
}

TEST(test_url_decode_null) {
    char *result = url_decode(NULL);
    ASSERT_NULL(result);
}

// ============================================================================
// TESTS DE TRIM
// ============================================================================

TEST(test_trim_no_whitespace) {
    char str[] = "hello";
    char *result = trim(str);
    ASSERT_STR_EQ(result, "hello");
}

TEST(test_trim_leading) {
    char str[] = "   hello";
    char *result = trim(str);
    ASSERT_STR_EQ(result, "hello");
}

TEST(test_trim_trailing) {
    char str[] = "hello   ";
    char *result = trim(str);
    ASSERT_STR_EQ(result, "hello");
}

TEST(test_trim_both) {
    char str[] = "   hello   ";
    char *result = trim(str);
    ASSERT_STR_EQ(result, "hello");
}

TEST(test_trim_only_whitespace) {
    char str[] = "     ";
    char *result = trim(str);
    ASSERT_STR_EQ(result, "");
}

// ============================================================================
// TESTS DE STR_SPLIT
// ============================================================================

TEST(test_str_split_simple) {
    int count;
    char **parts = str_split("a,b,c", ',', &count);
    
    ASSERT_EQ(count, 3);
    ASSERT_STR_EQ(parts[0], "a");
    ASSERT_STR_EQ(parts[1], "b");
    ASSERT_STR_EQ(parts[2], "c");
    
    free_str_array(parts, count);
}

TEST(test_str_split_single) {
    int count;
    char **parts = str_split("hello", ',', &count);
    
    ASSERT_EQ(count, 1);
    ASSERT_STR_EQ(parts[0], "hello");
    
    free_str_array(parts, count);
}

TEST(test_str_split_empty) {
    int count;
    char **parts = str_split("", ',', &count);
    
    ASSERT_EQ(count, 1);
    ASSERT_STR_EQ(parts[0], "");
    
    free_str_array(parts, count);
}

// ============================================================================
// TESTS DE PARSE_QUERY_STRING
// ============================================================================

TEST(test_parse_query_simple) {
    query_params_t *params = parse_query_string("n=97");
    
    ASSERT_NOT_NULL(params);
    ASSERT_EQ(params->count, 1);
    ASSERT_STR_EQ(get_query_param(params, "n"), "97");
    
    free_query_params(params);
}

TEST(test_parse_query_multiple) {
    query_params_t *params = parse_query_string("n=97&max=100&min=1");
    
    ASSERT_NOT_NULL(params);
    ASSERT_EQ(params->count, 3);
    ASSERT_STR_EQ(get_query_param(params, "n"), "97");
    ASSERT_STR_EQ(get_query_param(params, "max"), "100");
    ASSERT_STR_EQ(get_query_param(params, "min"), "1");
    
    free_query_params(params);
}

TEST(test_parse_query_url_encoded) {
    query_params_t *params = parse_query_string("text=hello+world&name=John%20Doe");
    
    ASSERT_NOT_NULL(params);
    ASSERT_EQ(params->count, 2);
    ASSERT_STR_EQ(get_query_param(params, "text"), "hello world");
    ASSERT_STR_EQ(get_query_param(params, "name"), "John Doe");
    
    free_query_params(params);
}

TEST(test_parse_query_empty) {
    query_params_t *params = parse_query_string("");
    
    ASSERT_NOT_NULL(params);
    ASSERT_EQ(params->count, 0);
    
    free_query_params(params);
}

TEST(test_parse_query_null) {
    query_params_t *params = parse_query_string(NULL);
    
    ASSERT_NOT_NULL(params);
    ASSERT_EQ(params->count, 0);
    
    free_query_params(params);
}

TEST(test_parse_query_no_value) {
    query_params_t *params = parse_query_string("flag");
    
    ASSERT_NOT_NULL(params);
    ASSERT_EQ(params->count, 1);
    ASSERT_STR_EQ(get_query_param(params, "flag"), "");
    
    free_query_params(params);
}

// ============================================================================
// TESTS DE GET_QUERY_PARAM
// ============================================================================

TEST(test_get_query_param_exists) {
    query_params_t *params = parse_query_string("a=1&b=2&c=3");
    
    ASSERT_STR_EQ(get_query_param(params, "a"), "1");
    ASSERT_STR_EQ(get_query_param(params, "b"), "2");
    ASSERT_STR_EQ(get_query_param(params, "c"), "3");
    
    free_query_params(params);
}

TEST(test_get_query_param_not_exists) {
    query_params_t *params = parse_query_string("a=1&b=2");
    
    ASSERT_NULL(get_query_param(params, "c"));
    ASSERT_NULL(get_query_param(params, "xyz"));
    
    free_query_params(params);
}

// ============================================================================
// TESTS DE GET_QUERY_PARAM_INT
// ============================================================================

TEST(test_get_query_param_int_valid) {
    query_params_t *params = parse_query_string("n=97&max=1000");
    
    ASSERT_EQ(get_query_param_int(params, "n", -1), 97);
    ASSERT_EQ(get_query_param_int(params, "max", -1), 1000);
    
    free_query_params(params);
}

TEST(test_get_query_param_int_negative) {
    query_params_t *params = parse_query_string("n=-42");
    
    ASSERT_EQ(get_query_param_int(params, "n", 0), -42);
    
    free_query_params(params);
}

TEST(test_get_query_param_int_invalid) {
    query_params_t *params = parse_query_string("n=abc");
    
    ASSERT_EQ(get_query_param_int(params, "n", 999), 999);
    
    free_query_params(params);
}

TEST(test_get_query_param_int_not_exists) {
    query_params_t *params = parse_query_string("a=1");
}
int main(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║         HTTP Server - Test Suite: String                   ║\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("\n");
    
    test_string_utils_all();
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                 ✅ ALL TESTS PASSED                        ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    return 0;
}