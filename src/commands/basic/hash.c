#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <openssl/evp.h>
#include "../../utils/utils.h"

// Function declaration that will be used by other modules
char* handle_hash(const char* text);

// Generates SHA-256 hash of input text and returns the result in JSON format
// The caller is responsible for freeing the returned string
char* handle_hash(const char* text) {
    if (!text) return NULL;
    
    // First decode the URL-encoded text
    char* decoded = url_decode(text);
    if (!decoded) return NULL;

    // Create EVP context
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        free(decoded);
        return NULL;
    }

    // Calculate SHA-256 hash using EVP API
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;

    if (1 != EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL)) {
        EVP_MD_CTX_free(mdctx);
        free(decoded);
        return NULL;
    }

    if (1 != EVP_DigestUpdate(mdctx, decoded, strlen(decoded))) {
        EVP_MD_CTX_free(mdctx);
        free(decoded);
        return NULL;
    }

    if (1 != EVP_DigestFinal_ex(mdctx, hash, &hash_len)) {
        EVP_MD_CTX_free(mdctx);
        free(decoded);
        return NULL;
    }

    EVP_MD_CTX_free(mdctx);


    // Convert hash to hexadecimal string
    char hash_hex[hash_len * 2 + 1];
    for (unsigned int i = 0; i < hash_len; i++) {
        sprintf(hash_hex + (i * 2), "%02x", hash[i]);
    }
    hash_hex[hash_len * 2] = '\0';

    // Allocate memory for the JSON response
    // Format: {"input":"original","output":"hash"}
    size_t json_len = strlen(decoded) + strlen(hash_hex) + 32;
    char* json = malloc(json_len);
    if (!json) {
        free(decoded);
        return NULL;
    }

    // Format the JSON response
    snprintf(json, json_len, "{\"input\":\"%s\",\"output\":\"%s\"}", decoded, hash_hex);

    // Clean up
    free(decoded);

    return json;
}
