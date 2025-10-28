#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../../utils/utils.h"

char* handle_loadtest(const char* tasks_str, const char* sleep_str) {
    if (!tasks_str || !sleep_str) return NULL;
    
    // Parse parameters
    int tasks = atoi(tasks_str);
    int sleep_time = atoi(sleep_str);
    
    if (tasks <= 0 || tasks > 1000) {
        return strdup("{\"error\":\"Invalid tasks parameter. Must be between 1 and 1000.\"}");
    }
    
    if (sleep_time < 0 || sleep_time > 60) {
        return strdup("{\"error\":\"Invalid sleep parameter. Must be between 0 and 60 seconds.\"}");
    }

    http_timer_t timer;
    timer_start(&timer);

    // Simulate multiple tasks with sleep
    for (int i = 0; i < tasks; i++) {
        // Simulate some work
        for (int j = 0; j < 1000000; j++) {
            // Simple CPU work
            __asm__ volatile("" : : : "memory");
        }
        if (sleep_time > 0) {
            usleep(sleep_time * 1000); // Convert to microseconds
        }
    }

    timer_stop(&timer);
    long elapsed = timer_elapsed_ms(&timer);
    
    // Calculate metrics
    double avg_task_time = (double)elapsed / tasks;
    
    // Format response
    char* json = malloc(512);
    if (json) {
        snprintf(json, 512,
                "{\"tasks\":%d,\"sleep_ms\":%d,\"total_time_ms\":%ld,"
                "\"avg_task_time_ms\":%.2f,\"throughput_tasks_sec\":%.2f}",
                tasks, sleep_time, elapsed, avg_task_time,
                (tasks * 1000.0) / elapsed);
    }
    return json;
}