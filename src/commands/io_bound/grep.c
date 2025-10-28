#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <time.h>

//-----------------------------------------------------
// /grep?name=FILE&pattern=REGEX
//-----------------------------------------------------
char* handle_grep(const char *filename, const char *pattern) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        char *err = malloc(256);
        snprintf(err, 256, "{\"error\":\"Cannot open file %s\"}", filename);
        return err;
    }

    regex_t regex;
    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        fclose(fp);
        char *err = malloc(256);
        snprintf(err, 256, "{\"error\":\"Invalid regex pattern\"}");
        return err;
    }

    char buffer[4096];
    char *matched_lines[10];
    int match_count = 0;
    int captured = 0;

    clock_t start = clock();

    while (fgets(buffer, sizeof(buffer), fp)) {
        if (regexec(&regex, buffer, 0, NULL, 0) == 0) {
            match_count++;
            if (captured < 10) {
                buffer[strcspn(buffer, "\n")] = '\0'; // quitar salto
                matched_lines[captured++] = strdup(buffer);
            }
        }
    }

    regfree(&regex);
    fclose(fp);

    clock_t end = clock();
    double elapsed_ms = (double)(end - start) * 1000.0 / CLOCKS_PER_SEC;

    // Calcular tamaño dinámico del JSON
    size_t size_needed = 512;
    for (int i = 0; i < captured; i++)
        size_needed += strlen(matched_lines[i]) + 8;

    char *json = malloc(size_needed);
    if (!json) {
        for (int i = 0; i < captured; i++) free(matched_lines[i]);
        char *err = malloc(128);
        snprintf(err, 128, "{\"error\":\"Memory allocation failed\"}");
        return err;
    }

    // Construir JSON seguro con snprintf
    int offset = snprintf(json, size_needed,
        "{\"file\":\"%s\",\"pattern\":\"%s\","
        "\"matches\":%d,\"elapsed_ms\":%.2f,\"lines\":[",
        filename, pattern, match_count, elapsed_ms);

    for (int i = 0; i < captured; i++) {
        offset += snprintf(json + offset, size_needed - offset,
            "\"%s\"%s", matched_lines[i], (i < captured - 1) ? "," : "");
        free(matched_lines[i]);
    }

    snprintf(json + offset, size_needed - offset, "]}");
    return json;
}
