#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include "../../utils/utils.h"

// Function declaration
char* handle_deletefile(const char* name_str);

// Deletes a file from the files directory
// The caller is responsible for freeing the returned string
char* handle_deletefile(const char* name_str) {
    if (!name_str) return NULL;
    
    // Start timing
    http_timer_t timer;
    timer_start(&timer);
    
    // Decode the URL-encoded parameter
    char* decoded_name = url_decode(name_str);
    if (!decoded_name) return NULL;

    // Build full file path
    size_t path_len = strlen("files/") + strlen(decoded_name) + 1;
    char* filepath = malloc(path_len);
    if (!filepath) {
        free(decoded_name);
        return NULL;
    }
    snprintf(filepath, path_len, "files/%s", decoded_name);

    // Get file size before deleting (optional, for response info)
    struct stat st;
    long file_size = 0;
    if (stat(filepath, &st) == 0) {
        file_size = (long)st.st_size;
    }

    // Attempt to delete the file using remove()
    int result = remove(filepath);
    
    // Stop timing
    timer_stop(&timer);
    long elapsed = timer_elapsed_ms(&timer);

    // Allocate memory for the JSON response
    size_t json_len = strlen(decoded_name) + strlen(filepath) + 256;
    char* json = malloc(json_len);
    if (!json) {
        free(decoded_name);
        free(filepath);
        return NULL;
    }

    // Check if deletion was successful
    if (result == 0) {
        // Success
        snprintf(json, json_len, 
            "{\"success\":true,\"filename\":\"%s\",\"path\":\"%s\",\"size\":%ld,\"elapsed_ms\":%ld}", 
            decoded_name, filepath, file_size, elapsed);
    } else {
        // Failure - provide error details
        const char* error_msg;
        
        switch (errno) {
            case ENOENT:
                error_msg = "File not found";
                break;
            case EACCES:
            case EPERM:
                error_msg = "Permission denied";
                break;
            case EBUSY:
                error_msg = "File is in use";
                break;
            default:
                error_msg = strerror(errno);
                break;
        }
        
        snprintf(json, json_len, 
            "{\"success\":false,\"filename\":\"%s\",\"path\":\"%s\",\"error\":\"%s\",\"elapsed_ms\":%ld}", 
            decoded_name, filepath, error_msg, elapsed);
    }

    // Clean up
    free(decoded_name);
    free(filepath);

    return json;
}
