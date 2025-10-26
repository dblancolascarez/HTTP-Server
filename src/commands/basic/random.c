#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../../utils/utils.h"


// Function declaration that will be used by other modules
char* handle_random(const char* count_str, const char* min_str, const char* max_str);

// Generates random numbers in a range and returns the result in JSON format
// The caller is responsible for freeing the returned string
char* handle_random(const char* count_str, const char* min_str, const char* max_str) {
    if (!count_str || !min_str || !max_str) return NULL;

    // Decode URL-encoded parameters
    char* decoded_count = url_decode(count_str);
    char* decoded_min = url_decode(min_str);
    char* decoded_max = url_decode(max_str);
    
    if (!decoded_count || !decoded_min || !decoded_max) {
        free(decoded_count);
        free(decoded_min);
        free(decoded_max);
        return NULL;
    }

    // Convert strings to integers
    char* endptr;
    long count = strtol(decoded_count, &endptr, 10);
    if(*endptr != '\0' || count<=0 || count>1000){
        free(decoded_count);
        free(decoded_min);
        free(decoded_max);
        return NULL;
    }

    long min = strtol(decoded_min, &endptr, 10);
    if (*endptr != '\0') {
        free(decoded_count);
        free(decoded_min);
        free(decoded_max);
        return NULL;
    }
    
    long max = strtol(decoded_max, &endptr, 10);
    if (*endptr != '\0' || max < min) {
        free(decoded_count);
        free(decoded_min);
        free(decoded_max);
        return NULL;
    }

    // Seed the random number generator (only once per process)
    static int seeded = 0;
    if (!seeded) {
        srand(time(NULL));
        seeded = 1;
    }

        // Generate random numbers
    long* numbers = malloc(count * sizeof(long));
    if (!numbers) {
        free(decoded_count);
        free(decoded_min);
        free(decoded_max);
        return NULL;
    }

    for (long i = 0; i < count; i++) {
        numbers[i] = (rand() % (max - min + 1)) + min;
    }

    // Build JSON array string
    // Format: {"count":"n","min":"a","max":"b","output":[x,y,z]}
    size_t json_len = 128 + (count * 12); // Estimate size
    char* json = malloc(json_len);
    if (!json) {
        free(decoded_count);
        free(decoded_min);
        free(decoded_max);
        free(numbers);
        return NULL;
    }

    // Start building JSON
    int offset = snprintf(json, json_len, 
        "{\"count\":\"%s\",\"min\":\"%s\",\"max\":\"%s\",\"output\":[",
        decoded_count, decoded_min, decoded_max);

     // Add numbers to array
    for (long i = 0; i < count; i++) {
        if (i > 0) {
            offset += snprintf(json + offset, json_len - offset, ",");
        }
        offset += snprintf(json + offset, json_len - offset, "%ld", numbers[i]);
    }
    
    // Close JSON
    snprintf(json + offset, json_len - offset, "]}");

    // Clean up
    free(decoded_count);
    free(decoded_min);
    free(decoded_max);
    free(numbers);

    return json;

}