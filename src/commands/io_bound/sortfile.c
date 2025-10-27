// Ordenar archivos grandes
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include "../../utils/utils.h"

// Function declarations
char* handle_sortfile(const char* name_str, const char* algo_str);
static int compare_int(const void* a, const void* b);
static void merge_sort(int* arr, int left, int right);
static void merge(int* arr, int left, int mid, int right);
static int* read_integers_from_file(const char* filepath, size_t* count);
static int write_integers_to_file(const char* filepath, int* arr, size_t count);

// Comparator for qsort
static int compare_int(const void* a, const void* b) {
    return (*(int*)a - *(int*)b);
}

// Merge function for merge sort
static void merge(int* arr, int left, int mid, int right) {
    int n1 = mid - left + 1;
    int n2 = right - mid;
    
    int* L = malloc(n1 * sizeof(int));
    int* R = malloc(n2 * sizeof(int));
    
    if (!L || !R) {
        free(L);
        free(R);
        return;
    }
    
    for (int i = 0; i < n1; i++)
        L[i] = arr[left + i];
    for (int j = 0; j < n2; j++)
        R[j] = arr[mid + 1 + j];
    
    int i = 0, j = 0, k = left;
    
    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) {
            arr[k++] = L[i++];
        } else {
            arr[k++] = R[j++];
        }
    }
    
    while (i < n1)
        arr[k++] = L[i++];
    while (j < n2)
        arr[k++] = R[j++];
    
    free(L);
    free(R);
}

// Merge sort implementation
static void merge_sort(int* arr, int left, int right) {
    if (left < right) {
        int mid = left + (right - left) / 2;
        merge_sort(arr, left, mid);
        merge_sort(arr, mid + 1, right);
        merge(arr, left, mid, right);
    }
}

// Read integers from file (one per line)
static int* read_integers_from_file(const char* filepath, size_t* count) {
    FILE* file = fopen(filepath, "r");
    if (!file) return NULL;
    
    // Count lines first
    size_t lines = 0;
    int temp;
    while (fscanf(file, "%d", &temp) == 1) {
        lines++;
    }
    
    if (lines == 0) {
        fclose(file);
        return NULL;
    }
    
    // Allocate array
    int* arr = malloc(lines * sizeof(int));
    if (!arr) {
        fclose(file);
        return NULL;
    }
    
    // Read integers
    rewind(file);
    size_t i = 0;
    while (i < lines && fscanf(file, "%d", &arr[i]) == 1) {
        i++;
    }
    
    fclose(file);
    *count = i;
    return arr;
}

// Write integers to file (one per line)
static int write_integers_to_file(const char* filepath, int* arr, size_t count) {
    FILE* file = fopen(filepath, "w");
    if (!file) return -1;
    
    for (size_t i = 0; i < count; i++) {
        fprintf(file, "%d\n", arr[i]);
    }
    
    fclose(file);
    return 0;
}

// Main handler function
char* handle_sortfile(const char* name_str, const char* algo_str) {
    if (!name_str || !algo_str) return NULL;
    
    // Start timing
    http_timer_t timer;
    timer_start(&timer);
    
    // Decode URL-encoded parameters
    char* decoded_name = url_decode(name_str);
    char* decoded_algo = url_decode(algo_str);
    
    if (!decoded_name || !decoded_algo) {
        free(decoded_name);
        free(decoded_algo);
        return NULL;
    }
    
    // Validate algorithm
    if (strcmp(decoded_algo, "merge") != 0 && strcmp(decoded_algo, "quick") != 0) {
        free(decoded_name);
        free(decoded_algo);
        return NULL;
    }
    
    // Build input file path
    size_t input_path_len = strlen("files/") + strlen(decoded_name) + 1;
    char* input_path = malloc(input_path_len);
    if (!input_path) {
        free(decoded_name);
        free(decoded_algo);
        return NULL;
    }
    snprintf(input_path, input_path_len, "files/%s", decoded_name);
    
    // Build output file path
    size_t output_path_len = strlen("files/") + strlen(decoded_name) + 10;
    char* output_path = malloc(output_path_len);
    if (!output_path) {
        free(decoded_name);
        free(decoded_algo);
        free(input_path);
        return NULL;
    }
    snprintf(output_path, output_path_len, "files/%s.sorted", decoded_name);
    
    // Read integers from file
    size_t count = 0;
    int* arr = read_integers_from_file(input_path, &count);
    
    if (!arr) {
        timer_stop(&timer);
        long elapsed = timer_elapsed_ms(&timer);
        
        size_t json_len = 256;
        char* json = malloc(json_len);
        if (json) {
            snprintf(json, json_len,
                "{\"success\":false,\"error\":\"Failed to read file\",\"elapsed_ms\":%ld}",
                elapsed);
        }
        
        free(decoded_name);
        free(decoded_algo);
        free(input_path);
        free(output_path);
        return json;
    }
    
    // Sort the array using selected algorithm
    if (strcmp(decoded_algo, "merge") == 0) {
        merge_sort(arr, 0, count - 1);
    } else { // quick
        qsort(arr, count, sizeof(int), compare_int);
    }
    
    // Write sorted array to output file
    int write_result = write_integers_to_file(output_path, arr, count);
    free(arr);
    
    if (write_result != 0) {
        timer_stop(&timer);
        long elapsed = timer_elapsed_ms(&timer);
        
        size_t json_len = 256;
        char* json = malloc(json_len);
        if (json) {
            snprintf(json, json_len,
                "{\"success\":false,\"error\":\"Failed to write sorted file\",\"elapsed_ms\":%ld}",
                elapsed);
        }
        
        free(decoded_name);
        free(decoded_algo);
        free(input_path);
        free(output_path);
        return json;
    }
    
    // Stop timing
    timer_stop(&timer);
    long elapsed = timer_elapsed_ms(&timer);
    
    // Get file sizes
    struct stat st_input, st_output;
    long input_size = 0, output_size = 0;
    if (stat(input_path, &st_input) == 0) {
        input_size = (long)st_input.st_size;
    }
    if (stat(output_path, &st_output) == 0) {
        output_size = (long)st_output.st_size;
    }
    
    // Build output filename (without path)
    char output_name[256];
    snprintf(output_name, sizeof(output_name), "%s.sorted", decoded_name);
    
    // Build success JSON response
    size_t json_len = strlen(decoded_name) + strlen(decoded_algo) + strlen(output_name) + 512;
    char* json = malloc(json_len);
    if (!json) {
        free(decoded_name);
        free(decoded_algo);
        free(input_path);
        free(output_path);
        return NULL;
    }
    
    snprintf(json, json_len,
        "{\"success\":true,\"file\":\"%s\",\"algo\":\"%s\",\"sorted_file\":\"%s\",\"count\":%zu,\"input_size\":%ld,\"output_size\":%ld,\"elapsed_ms\":%ld}",
        decoded_name, decoded_algo, output_name, count, input_size, output_size, elapsed);
    
    // Clean up
    free(decoded_name);
    free(decoded_algo);
    free(input_path);
    free(output_path);
    
    return json;
}
