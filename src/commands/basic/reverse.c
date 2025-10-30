#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../../utils/utils.h"

// Function declaration that will be used by other modules
char* handle_reverse(const char* text);

// Reverses a string and returns the result in a JSON format
// The caller is responsible for freeing the returned string
char* handle_reverse(const char* text) {
    if (!text) return NULL;
    
    // First decode the URL-encoded text
    char* decoded = url_decode(text);
    if (!decoded) return NULL;

    // Get the length of the decoded text
    size_t len = strlen(decoded);
    
    // Allocate memory for the reversed string
    char* reversed = malloc(len + 1);
    if (!reversed) {
        free(decoded);
        return NULL;
    }

    // Reverse the string
    for (size_t i = 0; i < len; i++) {
        reversed[i] = decoded[len - 1 - i];
    }
    reversed[len] = '\0';

    // Allocate memory for the JSON response
    // Format: {"input":"original","output":"reversed"}
    size_t json_len = strlen(decoded) + strlen(reversed) + 32;
    char* json = malloc(json_len);
    if (!json) {
        free(decoded);
        free(reversed);
        return NULL;
    }

    // Format the JSON response
    snprintf(json, json_len, "{\"input\":\"%s\",\"output\":\"%s\"}", decoded, reversed);

    // Clean up temporary buffers
    free(decoded);
    free(reversed);

    return json;
}
