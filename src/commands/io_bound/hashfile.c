// Hash SHA-256 de archivos
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <openssl/evp.h>
#include <errno.h>
#include <sys/stat.h>
#include "../../utils/utils.h"

// Function declarations
char* handle_hashfile(const char* name_str, const char* algo_str);
static char* compute_file_hash(const char* filepath, const char* algo);

// Compute hash of a file using OpenSSL EVP API
static char* compute_file_hash(const char* filepath, const char* algo) {
    FILE* file = fopen(filepath, "rb");
    if (!file) return NULL;
    
    // Create EVP context
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        fclose(file);
        return NULL;
    }
    
    // Initialize digest based on algorithm
    const EVP_MD* md = NULL;
    if (strcmp(algo, "sha256") == 0) {
        md = EVP_sha256();
    } else {
        EVP_MD_CTX_free(mdctx);
        fclose(file);
        return NULL;
    }
    
    if (1 != EVP_DigestInit_ex(mdctx, md, NULL)) {
        EVP_MD_CTX_free(mdctx);
        fclose(file);
        return NULL;
    }
    
    // Read file in chunks and update hash
    #define BUFFER_SIZE 8192
    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        if (1 != EVP_DigestUpdate(mdctx, buffer, bytes_read)) {
            EVP_MD_CTX_free(mdctx);
            fclose(file);
            return NULL;
        }
    }
    
    fclose(file);
    
    // Finalize hash
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    
    if (1 != EVP_DigestFinal_ex(mdctx, hash, &hash_len)) {
        EVP_MD_CTX_free(mdctx);
        return NULL;
    }
    
    EVP_MD_CTX_free(mdctx);
    
    // Convert hash to hexadecimal string
    char* hash_hex = malloc(hash_len * 2 + 1);
    if (!hash_hex) return NULL;
    
    for (unsigned int i = 0; i < hash_len; i++) {
        sprintf(hash_hex + (i * 2), "%02x", hash[i]);
    }
    hash_hex[hash_len * 2] = '\0';
    
    return hash_hex;
}

// Main handler function
char* handle_hashfile(const char* name_str, const char* algo_str) {
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
    if (strcmp(decoded_algo, "sha256") != 0) {
        timer_stop(&timer);
        long elapsed = timer_elapsed_ms(&timer);
        
        size_t json_len = 256;
        char* json = malloc(json_len);
        if (json) {
            snprintf(json, json_len,
                "{\"success\":false,\"error\":\"Unsupported algorithm. Use 'sha256'\",\"elapsed_ms\":%ld}",
                elapsed);
        }
        
        free(decoded_name);
        free(decoded_algo);
        return json;
    }
    
    // Build full file path
    size_t path_len = strlen("files/") + strlen(decoded_name) + 1;
    char* filepath = malloc(path_len);
    if (!filepath) {
        free(decoded_name);
        free(decoded_algo);
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
        free(decoded_algo);
        free(filepath);
        return json;
    }
    
    long file_size = (long)st.st_size;
    
    // Compute hash
    char* hash_hex = compute_file_hash(filepath, decoded_algo);
    
    // Stop timing
    timer_stop(&timer);
    long elapsed = timer_elapsed_ms(&timer);
    
    if (!hash_hex) {
        size_t json_len = 256;
        char* json = malloc(json_len);
        if (json) {
            snprintf(json, json_len,
                "{\"success\":false,\"error\":\"Failed to compute hash\",\"elapsed_ms\":%ld}",
                elapsed);
        }
        
        free(decoded_name);
        free(decoded_algo);
        free(filepath);
        return json;
    }
    
    // Build success JSON response
    // Format: {"success":true,"file":"name","algo":"sha256","hash":"hex","size":bytes,"elapsed_ms":time}
    size_t json_len = strlen(decoded_name) + strlen(decoded_algo) + strlen(hash_hex) + 512;
    char* json = malloc(json_len);
    if (!json) {
        free(decoded_name);
        free(decoded_algo);
        free(filepath);
        free(hash_hex);
        return NULL;
    }
    
    snprintf(json, json_len,
        "{\"success\":true,\"file\":\"%s\",\"algo\":\"%s\",\"hash\":\"%s\",\"size\":%ld,\"elapsed_ms\":%ld}",
        decoded_name, decoded_algo, hash_hex, file_size, elapsed);
    
    // Clean up
    free(decoded_name);
    free(decoded_algo);
    free(filepath);
    free(hash_hex);
    
    return json;
}
