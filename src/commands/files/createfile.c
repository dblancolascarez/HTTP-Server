#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include "../../utils/utils.h"

// Function declaration
char* handle_createfile(const char* name_str, const char* content_str, const char* repeat_str);

// Creates a file with repeated content
// The caller is responsible for freeing the returned string
char* handle_createfile(const char* name_str, const char* content_str, const char* repeat_str) {
    if (!name_str || !content_str || !repeat_str) return NULL;
    
    // Start timing
    http_timer_t timer;
    timer_start(&timer);
    
    // Decode URL-encoded parameters
    char* decoded_name = url_decode(name_str);
    char* decoded_content = url_decode(content_str);
    char* decoded_repeat = url_decode(repeat_str);
    
    if (!decoded_name || !decoded_content || !decoded_repeat) {
        free(decoded_name);
        free(decoded_content);
        free(decoded_repeat);
        return NULL;
    }

    // Convert repeat string to integer
    char* endptr;
    long repeat = strtol(decoded_repeat, &endptr, 10);
    if (*endptr != '\0' || repeat <= 0 || repeat > 10000) {
        free(decoded_name);
        free(decoded_content);
        free(decoded_repeat);
        return NULL;
    }

    // Create directory if needed (files subdirectory)
    mkdir("files", 0755);
    
    // Build full file path
    size_t path_len = strlen("files/") + strlen(decoded_name) + 1;
    char* filepath = malloc(path_len);
    if (!filepath) {
        free(decoded_name);
        free(decoded_content);
        free(decoded_repeat);
        return NULL;
    }
    snprintf(filepath, path_len, "files/%s", decoded_name);

    // Open file for writing
    FILE* file = fopen(filepath, "w");
    if (!file) {
        timer_stop(&timer);
        long elapsed = timer_elapsed_ms(&timer);
        
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to create file: %s", strerror(errno));
        
        // Build error JSON
        size_t json_len = strlen(error_msg) + 128;
        char* json = malloc(json_len);
        if (json) {
            snprintf(json, json_len, 
                "{\"success\":false,\"error\":\"%s\",\"elapsed_ms\":%ld}", 
                error_msg, elapsed);
        }
        
        free(decoded_name);
        free(decoded_content);
        free(decoded_repeat);
        free(filepath);
        return json;
    }

    // Write content to file repeat times
    size_t content_len = strlen(decoded_content);
    size_t total_written = 0;
    
    for (long i = 0; i < repeat; i++) {
        size_t written = fwrite(decoded_content, 1, content_len, file);
        total_written += written;
        
        if (written != content_len) {
            fclose(file);
            timer_stop(&timer);
            long elapsed = timer_elapsed_ms(&timer);
            
            // Build error JSON
            size_t json_len = 256;
            char* json = malloc(json_len);
            if (json) {
                snprintf(json, json_len, 
                    "{\"success\":false,\"error\":\"Write error after %zu bytes\",\"elapsed_ms\":%ld}", 
                    total_written, elapsed);
            }
            
            free(decoded_name);
            free(decoded_content);
            free(decoded_repeat);
            free(filepath);
            return json;
        }
    }

    // Close the file
    fclose(file);

    // Stop timing
    timer_stop(&timer);
    long elapsed = timer_elapsed_ms(&timer);

    // Get file size
    struct stat st;
    if (stat(filepath, &st) != 0) {
        st.st_size = 0;
    }

    // Build success JSON response
    // Format: {"success":true,"filename":"name","path":"files/name","size":bytes,"repeat":x,"elapsed_ms":time}
    size_t json_len = strlen(decoded_name) + strlen(filepath) + 256;
    char* json = malloc(json_len);
    if (!json) {
        free(decoded_name);
        free(decoded_content);
        free(decoded_repeat);
        free(filepath);
        return NULL;
    }

    snprintf(json, json_len, 
        "{\"success\":true,\"filename\":\"%s\",\"path\":\"%s\",\"size\":%ld,\"repeat\":%ld,\"elapsed_ms\":%ld}", 
        decoded_name, filepath, (long)st.st_size, repeat, elapsed);

    // Clean up
    free(decoded_name);
    free(decoded_content);
    free(decoded_repeat);
    free(filepath);

    return json;
}
