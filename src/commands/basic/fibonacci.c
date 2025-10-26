#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include "../../utils/utils.h"

// Function declaration that will be used by other modules
char* handle_fibonacci(const char* n_str);

// Calculates the nth Fibonacci number and returns the result in JSON format
// The caller is responsible for freeing the returned string
char* handle_fibonacci(const char* n_str) {
    if (!n_str) return NULL;
    
    // First decode the URL-encoded parameter
    char* decoded = url_decode(n_str);
    if (!decoded) return NULL;

    // Convert string to integer
    char* endptr;
    long n = strtol(decoded, &endptr, 10);
    
    // Validate the input
    if (*endptr != '\0' || n < 0 || n > 93) {
        free(decoded);
        return NULL;
    }

    // Calculate Fibonacci number iteratively
    unsigned long long fib_result;
    
    if (n == 0) {
        fib_result = 0;
    } else if (n == 1) {
        fib_result = 1;
    } else {
        unsigned long long a = 0, b = 1, temp;
        for (long i = 2; i <= n; i++) {
            temp = a + b;
            a = b;
            b = temp;
        }
        fib_result = b;
    }

    // Convert result to string
    char result_str[32];
    snprintf(result_str, sizeof(result_str), "%llu", fib_result);

    // Allocate memory for the JSON response
    // Format: {"input":"n","output":"fibonacci_result"}
    size_t json_len = strlen(decoded) + strlen(result_str) + 32;
    char* json = malloc(json_len);
    if (!json) {
        free(decoded);
        return NULL;
    }

    // Format the JSON response
    snprintf(json, json_len, "{\"input\":\"%s\",\"output\":\"%s\"}", decoded, result_str);

    // Clean up
    free(decoded);

    return json;
}
