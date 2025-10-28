#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "../../utils/utils.h"

// Simulates CPU or IO work for testing purposes
char* handle_simulate(const char* seconds_str, const char* task) {
    if (!seconds_str || !task) return NULL;
    
    // Parse seconds parameter
    long seconds = strtol(seconds_str, NULL, 10);
    if (seconds <= 0) {
        return strdup("{\"error\":\"Invalid seconds parameter. Must be a positive integer.\"}");
    }

    // Validate task type
    if (strcmp(task, "cpu") != 0 && strcmp(task, "io") != 0) {
        return strdup("{\"error\":\"Invalid task parameter. Must be 'cpu' or 'io'.\"}");
    }

    http_timer_t timer;
    timer_start(&timer);

    // Simulate work based on task type
    if (strcmp(task, "cpu") == 0) {
        // CPU-intensive work - calculate prime numbers and count them (used to avoid unused-variable warnings)
        long num = 2;
        long primes_found = 0;
        time_t end_time = time(NULL) + seconds;
        while (time(NULL) < end_time) {
            int is_prime = 1;
            for (long i = 2; i * i <= num; i++) {
                if (num % i == 0) {
                    is_prime = 0;
                    break;
                }
            }
            if (is_prime) primes_found++;
            num++;
        }
        (void)primes_found; // keep compiler happy; optional metric
    } else {
        // IO-intensive work - read/write operations
        char temp_file[] = "/tmp/simulate_io_XXXXXX";
        int fd = mkstemp(temp_file);
        if (fd != -1) {
            time_t end_time = time(NULL) + seconds;
            char buffer[4096];
            memset(buffer, 'A', sizeof(buffer));
            
            while (time(NULL) < end_time) {
                ssize_t w = write(fd, buffer, sizeof(buffer));
                (void)w; // intentionally ignore but store result to satisfy compiler
                lseek(fd, 0, SEEK_SET);
                ssize_t r = read(fd, buffer, sizeof(buffer));
                (void)r; // intentionally ignore but store result to satisfy compiler
            }
            
            close(fd);
            unlink(temp_file);
        }
    }

    timer_stop(&timer);
    long elapsed = timer_elapsed_ms(&timer);

    // Format response
    char* json = malloc(256);
    if (json) {
        snprintf(json, 256, 
                "{\"task\":\"%s\",\"requested_seconds\":%ld,\"actual_ms\":%ld}", 
                task, seconds, elapsed);
    }
    return json;
}
