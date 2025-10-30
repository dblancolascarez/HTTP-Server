#include "io_bound_commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

//-----------------------------------------------------
// /compress?name=FILE&codec=gzip|xz
//-----------------------------------------------------
char* handle_compress(const char *filename, const char *codec) {
    char filepath[512];
    char outname[512];
    
    // Construir rutas completas
    snprintf(filepath, sizeof(filepath), "files/%s", filename);
    
    // Construye nombre de salida (.gz o .xz)
    if (strcmp(codec, "gzip") == 0)
        snprintf(outname, sizeof(outname), "files/%s.gz", filename);
    else
        snprintf(outname, sizeof(outname), "files/%s.xz", filename);

    // Comando del sistema (usa gzip o xz)
    char command[1024];
    snprintf(command, sizeof(command),
             "%s -c \"%s\" > \"%s\"",
             strcmp(codec, "gzip") == 0 ? "gzip" : "xz",
             filepath, outname);

    // Medir tiempo
    clock_t start = clock();
    int ret = system(command);
    clock_t end = clock();

    if (ret != 0) {
        char *err = malloc(256);
        snprintf(err, 256, "{\"error\":\"Compression failed for %s\"}", filename);
        return err;
    }

    // Obtener tamaño del archivo comprimido
    struct stat st;
    if (stat(outname, &st) != 0) {
        char *err = malloc(256);
        snprintf(err, 256, "{\"error\":\"Failed to stat output file\"}");
        return err;
    }

    double elapsed_ms = (double)(end - start) * 1000.0 / CLOCKS_PER_SEC;

    // Calcular tamaño necesario del buffer JSON
    size_t needed = strlen(filename) + strlen(codec) + strlen(outname) + 128;
    char *json = malloc(needed);
    if (!json) {
        char *err = malloc(128);
        snprintf(err, 128, "{\"error\":\"Memory allocation failed\"}");
        return err;
    }

    snprintf(json, needed,
             "{\"file\":\"%s\",\"codec\":\"%s\","
             "\"output\":\"%s\",\"size_bytes\":%ld,"
             "\"elapsed_ms\":%.2f}",
             filename, codec, outname, (long)st.st_size, elapsed_ms);

    return json;
}