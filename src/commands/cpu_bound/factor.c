#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "../../utils/utils.h"

// Structure to hold a prime factor and its count
typedef struct {
    uint64_t factor;
    int count;
} PrimeFactor;

// Function declarations
char* handle_factor(const char* n_str);
static PrimeFactor* factorize(uint64_t n, int* num_factors);

// Main handler function
char* handle_factor(const char* n_str) {
    if (!n_str) return NULL;

    // Start timing
    http_timer_t timer;
    timer_start(&timer);
    
    // Decode the URL-encoded parameter
    char* decoded = url_decode(n_str);
    if (!decoded) return NULL;

    // Convert string to integer
    char* endptr;
    uint64_t n = strtoull(decoded, &endptr, 10);
    
    // Validate the input
    if (*endptr != '\0' || n < 2) {
        free(decoded);
        return NULL;
    }

    // Factorize the number
    int num_factors = 0;
    PrimeFactor* factors = factorize(n, &num_factors);
    
    if (!factors) {
        free(decoded);
        return NULL;
    }

    // Stop timing
    timer_stop(&timer);
    long elapsed = timer_elapsed_ms(&timer);

    // Build JSON response
    // Format: {"input":"n","factors":[{"prime":2,"count":3},{"prime":5,"count":1}]}
    size_t json_len = strlen(decoded) + (num_factors * 50) + 256;
    char* json = malloc(json_len);
    if (!json) {
        free(decoded);
        free(factors);
        return NULL;
    }


    // Start building JSON
    int offset = snprintf(json, json_len, "{\"input\":\"%s\",\"factors\":[, \"elapsed_ms\":%ld", decoded, elapsed);

    // Add each factor with its count
    for (int i = 0; i < num_factors; i++) {
        if (i > 0) {
            offset += snprintf(json + offset, json_len - offset, ",");
        }
        offset += snprintf(json + offset, json_len - offset, 
            "{\"prime\":%lu,\"count\":%d}", 
            (unsigned long)factors[i].factor, factors[i].count);
    }

    // Close JSON
    snprintf(json + offset, json_len - offset, "]}");

    // Clean up
    free(decoded);
    free(factors);

    return json;
}

// Prime factorization using trial division
// Time complexity: O(√n)
static PrimeFactor* factorize(uint64_t n, int* num_factors) {
    if (n < 2) return NULL;
    
    // Allocate initial array for factors
    int capacity = 10;
    PrimeFactor* factors = malloc(capacity * sizeof(PrimeFactor));
    if (!factors) return NULL;
    
    *num_factors = 0;

    // Handle factor 2 separately
    if (n % 2 == 0) {
        factors[*num_factors].factor = 2;
        factors[*num_factors].count = 0;
        
        while (n % 2 == 0) {
            factors[*num_factors].count++;
            n /= 2;
        }
        (*num_factors)++;
    }

    // Check odd factors from 3 to √n
    for (uint64_t i = 3; i * i <= n; i += 2) {
        if (n % i == 0) {
            // Expand array if needed
            if (*num_factors >= capacity) {
                capacity *= 2;
                PrimeFactor* temp = realloc(factors, capacity * sizeof(PrimeFactor));
                if (!temp) {
                    free(factors);
                    return NULL;
                }
                factors = temp;
            }
            
            factors[*num_factors].factor = i;
            factors[*num_factors].count = 0;
            
            while (n % i == 0) {
                factors[*num_factors].count++;
                n /= i;
            }
            (*num_factors)++;
        }
    }

    // If n is still greater than 1, then it's a prime factor
    if (n > 1) {
        // Expand array if needed
        if (*num_factors >= capacity) {
            capacity *= 2;
            PrimeFactor* temp = realloc(factors, capacity * sizeof(PrimeFactor));
            if (!temp) {
                free(factors);
                return NULL;
            }
            factors = temp;
        }
        
        factors[*num_factors].factor = n;
        factors[*num_factors].count = 1;
        (*num_factors)++;
    }

    return factors;
}
