#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "../../utils/utils.h"

char* handle_sleep(const char* seconds_str) {
    if (!seconds_str) return NULL;
    
    // Parse seconds parameter
    long seconds = strtol(seconds_str, NULL, 10);
    if (seconds <= 0) {
        return strdup("{\"error\":\"Invalid seconds parameter. Must be a positive integer.\"}");
    }

    // Get start time
    http_timer_t timer;
    timer_start(&timer);
    
    // Sleep for the specified duration
    sleep(seconds);
    
    // Get end time and calculate elapsed
    timer_stop(&timer);
    long elapsed = timer_elapsed_ms(&timer);
    
    // Format response
    char* json = malloc(128);
    if (json) {
        snprintf(json, 128, "{\"seconds\":%ld,\"slept_ms\":%ld}", seconds, elapsed);
    }
    return json;
}
