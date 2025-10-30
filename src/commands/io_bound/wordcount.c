// Contar líneas/palabras
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include "../../utils/utils.h"

// Structure to hold word count results
typedef struct {
    unsigned long lines;
    unsigned long words;
    unsigned long bytes;
    unsigned long chars;
} wc_result_t;

// Function declarations
char* handle_wordcount(const char* name_str);
static int count_file(const char* filepath, wc_result_t* result);
static int is_word_char(int c);

// Check if character is a word constituent
static int is_word_char(int c) {
    return !isspace(c);
}

// Count lines, words, bytes, and characters in a file
static int count_file(const char* filepath, wc_result_t* result) {
    FILE* file = fopen(filepath, "rb");  // Open in binary mode for accurate byte count
    if (!file) return -1;
    
    // Initialize counters
    result->lines = 0;
    result->words = 0;
    result->bytes = 0;
    result->chars = 0;
    
    int c;
    int in_word = 0;
    int prev_c = '\n';
    
    // Use buffered reading for better performance with large files
    #define BUFFER_SIZE 8192
    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            c = buffer[i];
            result->bytes++;
            result->chars++;  // For ASCII, chars == bytes
            
            // Count lines
            if (c == '\n') {
                result->lines++;
            }
            
            // Count words
            if (is_word_char(c)) {
                if (!in_word) {
                    result->words++;
                    in_word = 1;
                }
            } else {
                in_word = 0;
            }
            
            prev_c = c;
        }
    }
    
    // If file doesn't end with newline, but has content, count as a line
    if (result->bytes > 0 && prev_c != '\n') {
        result->lines++;
    }
    
    fclose(file);
    return 0;
}

// Main handler function
char* handle_wordcount(const char* name_str) {
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
    
    // Check if file exists and get size
    struct stat st;
    if (stat(filepath, &st) != 0) {
        timer_stop(&timer);
        long elapsed = timer_elapsed_ms(&timer);
        
        size_t json_len = 256;
        char* json = malloc(json_len);
        if (json) {
            snprintf(json, json_len,
                "{\"success\":false,\"error\":\"File not found\",\"elapsed_ms\":%ld}",
                elapsed);
        }
        
        free(decoded_name);
        free(filepath);
        return json;
    }
    
    long file_size = (long)st.st_size;
    
    // Count lines, words, and bytes
    wc_result_t result;
    int count_result = count_file(filepath, &result);
    
    // Stop timing
    timer_stop(&timer);
    long elapsed = timer_elapsed_ms(&timer);
    
    if (count_result != 0) {
        size_t json_len = 256;
        char* json = malloc(json_len);
        if (json) {
            snprintf(json, json_len,
                "{\"success\":false,\"error\":\"Failed to read file\",\"elapsed_ms\":%ld}",
                elapsed);
        }
        
        free(decoded_name);
        free(filepath);
        return json;
    }
    
    // Build success JSON response
    // Format: {"success":true,"file":"name","lines":X,"words":Y,"bytes":Z,"chars":Z,"elapsed_ms":T}
    size_t json_len = strlen(decoded_name) + 512;
    char* json = malloc(json_len);
    if (!json) {
        free(decoded_name);
        free(filepath);
        return NULL;
    }
    
    snprintf(json, json_len,
        "{\"success\":true,\"file\":\"%s\",\"lines\":%lu,\"words\":%lu,\"bytes\":%lu,\"chars\":%lu,\"elapsed_ms\":%ld}",
        decoded_name, result.lines, result.words, result.bytes, result.chars, elapsed);
    
    // Clean up
    free(decoded_name);
    free(filepath);
    
    return json;
}
