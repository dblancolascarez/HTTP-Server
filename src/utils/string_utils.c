// ¿Qué hace?
// - Convierte "n=97&max=100" en una estructura de datos usable
// - Decodifica URL encoding (%20 -> espacio, + -> espacio)
// - Convierte strings a enteros de forma segura

// ¿Por qué lo necesitas?
// - TODOS los endpoints reciben parámetros en el query string
// - HTTP envía: /isprime?n=97
// - Tu código necesita: int n = 97;

#include "utils.h"
#include <ctype.h>

// ============================================================================
// URL DECODE
// ============================================================================

static int hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

char* url_decode(const char *str) {
    if (!str) return NULL;
    
    size_t len = strlen(str);
    char *decoded = malloc(len + 1);
    if (!decoded) return NULL;
    
    size_t i = 0, j = 0;
    while (i < len) {
        if (str[i] == '%' && i + 2 < len) {
            // Decodificar %XX
            decoded[j++] = (hex_to_int(str[i+1]) << 4) | hex_to_int(str[i+2]);
            i += 3;
        } else if (str[i] == '+') {
            // + se convierte en espacio
            decoded[j++] = ' ';
            i++;
        } else {
            decoded[j++] = str[i++];
        }
    }
    decoded[j] = '\0';
    
    return decoded;
}

// ============================================================================
// TRIM WHITESPACE
// ============================================================================

char* trim(char *str) {
    if (!str) return NULL;
    
    // Trim leading whitespace
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == '\0') return str;
    
    // Trim trailing whitespace
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    
    *(end + 1) = '\0';
    return str;
}

// ============================================================================
// STRING SPLIT
// ============================================================================

char** str_split(const char *str, char delimiter, int *count) {
    if (!str || !count) return NULL;
    
    // Contar delimitadores
    *count = 1;
    for (const char *p = str; *p; p++) {
        if (*p == delimiter) (*count)++;
    }
    
    // Alocar array
    char **result = malloc(sizeof(char*) * (*count));
    if (!result) {
        *count = 0;
        return NULL;
    }
    
    // Copiar y separar
    char *copy = strdup(str);
    // strtok expects a NUL-terminated string of delimiters
    char delim_str[2]; delim_str[0] = delimiter; delim_str[1] = '\0';
    char *token = strtok(copy, delim_str);
    int i = 0;
    
    while (token && i < *count) {
        result[i++] = strdup(token);
        token = strtok(NULL, delim_str);
    }
    
    free(copy);
    *count = i;
    return result;
}

void free_str_array(char **arr, int count) {
    if (!arr) return;
    for (int i = 0; i < count; i++) {
        free(arr[i]);
    }
    free(arr);
}

// ============================================================================
// QUERY STRING PARSING
// ============================================================================

query_params_t* parse_query_string(const char *query) {
    if (!query || strlen(query) == 0) {
        query_params_t *params = malloc(sizeof(query_params_t));
        params->params = NULL;
        params->count = 0;
        return params;
    }
    
    query_params_t *result = malloc(sizeof(query_params_t));
    if (!result) return NULL;
    
    // Separar por '&'
    int pair_count;
    char **pairs = str_split(query, '&', &pair_count);
    
    result->params = malloc(sizeof(query_param_t) * pair_count);
    result->count = 0;
    
    for (int i = 0; i < pair_count; i++) {
        // Separar por '='
        char *equal_sign = strchr(pairs[i], '=');
        
        if (equal_sign) {
            *equal_sign = '\0';
            char *key = pairs[i];
            char *value = equal_sign + 1;
            
            // Decodificar key y value
            result->params[result->count].key = url_decode(key);
            result->params[result->count].value = url_decode(value);
            result->count++;
        } else {
            // Parámetro sin valor: "key" -> key=""
            result->params[result->count].key = url_decode(pairs[i]);
            result->params[result->count].value = strdup("");
            result->count++;
        }
    }
    
    free_str_array(pairs, pair_count);
    return result;
}

const char* get_query_param(query_params_t *params, const char *key) {
    if (!params || !key) return NULL;
    
    for (int i = 0; i < params->count; i++) {
        if (strcmp(params->params[i].key, key) == 0) {
            return params->params[i].value;
        }
    }
    
    return NULL;
}

int get_query_param_int(query_params_t *params, const char *key, int default_val) {
    const char *value = get_query_param(params, key);
    if (!value) return default_val;
    
    char *endptr;
    long result = strtol(value, &endptr, 10);
    
    // Validar que se parseó correctamente
    if (*endptr != '\0' || endptr == value) {
        return default_val;
    }
    
    return (int)result;
}

long get_query_param_long(query_params_t *params, const char *key, long default_val) {
    const char *value = get_query_param(params, key);
    if (!value) return default_val;
    
    char *endptr;
    long result = strtol(value, &endptr, 10);
    
    if (*endptr != '\0' || endptr == value) {
        return default_val;
    }
    
    return result;
}

void free_query_params(query_params_t *params) {
    if (!params) return;
    
    for (int i = 0; i < params->count; i++) {
        free(params->params[i].key);
        free(params->params[i].value);
    }
    
    free(params->params);
    free(params);
}