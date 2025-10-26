#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../utils/utils.h"

// Configuration: set to 1 for Miller-Rabin, 0 for trial division
#define USE_MILLER_RABIN 1
#define MILLER_RABIN_ITERATIONS 5

// Function declarations
char* handle_isprime(const char* n_str);
static bool is_prime_trial_division(uint64_t n);
static bool is_prime_miller_rabin(uint64_t n);
static uint64_t mod_pow(uint64_t base, uint64_t exp, uint64_t mod);

// Main handler function
char* handle_isprime(const char* n_str) {
    if (!n_str) return NULL;
    
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

    // Check primality using the configured method
    bool is_prime;
    const char* method;
    
#if USE_MILLER_RABIN
    is_prime = is_prime_miller_rabin(n);
    method = "miller-rabin";
#else
    is_prime = is_prime_trial_division(n);
    method = "trial-division";
#endif

    // Allocate memory for the JSON response
    // Format: {"input":"n","output":"true/false","method":"algorithm"}
    size_t json_len = strlen(decoded) + 128;
    char* json = malloc(json_len);
    if (!json) {
        free(decoded);
        return NULL;
    }

    // Format the JSON response
    snprintf(json, json_len, 
        "{\"input\":\"%s\",\"output\":%s,\"method\":\"%s\"}", 
        decoded, is_prime ? "true" : "false", method);

    free(decoded);
    return json;
}

// Trial division method: O(√n)
static bool is_prime_trial_division(uint64_t n) {
    if (n < 2) return false;
    if (n == 2 || n == 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    
    // Check divisibility by numbers of form 6k ± 1 up to √n
    uint64_t sqrt_n = (uint64_t)sqrt((double)n);
    for (uint64_t i = 5; i <= sqrt_n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) {
            return false;
        }
    }
    return true;
}

// Modular exponentiation: (base^exp) % mod
static uint64_t mod_pow(uint64_t base, uint64_t exp, uint64_t mod) {
    uint64_t result = 1;
    base %= mod;
    
    while (exp > 0) {
        if (exp & 1) {
            result = (__uint128_t)result * base % mod;
        }
        base = (__uint128_t)base * base % mod;
        exp >>= 1;
    }
    return result;
}

// Miller-Rabin primality test
static bool is_prime_miller_rabin(uint64_t n) {
    if (n < 2) return false;
    if (n == 2 || n == 3) return true;
    if (n % 2 == 0) return false;
    
    // Write n-1 as d * 2^r
    uint64_t d = n - 1;
    int r = 0;
    while (d % 2 == 0) {
        d /= 2;
        r++;
    }
    
    // Deterministic witnesses for n < 3,317,044,064,679,887,385,961,981
    uint64_t witnesses[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};
    int num_witnesses = (n < 341550071728321ULL) ? 7 : 12;
    
    for (int i = 0; i < num_witnesses && witnesses[i] < n; i++) {
        uint64_t a = witnesses[i];
        uint64_t x = mod_pow(a, d, n);
        
        if (x == 1 || x == n - 1) {
            continue;
        }
        
        bool composite = true;
        for (int j = 0; j < r - 1; j++) {
            x = (__uint128_t)x * x % n;
            if (x == n - 1) {
                composite = false;
                break;
            }
        }
        
        if (composite) {
            return false;
        }
    }
    
    return true;
}
