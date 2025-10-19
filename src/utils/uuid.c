// Generación de request IDs
#include "utils.h"
#include <unistd.h>

// Generador simple de IDs únicos (no es UUID RFC4122, pero es suficiente)
void generate_request_id(char *buffer, size_t size) {
    if (!buffer || size < 32) return;
    
    static unsigned long counter = 0;
    static pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    pthread_mutex_lock(&counter_mutex);
    unsigned long id = counter++;
    pthread_mutex_unlock(&counter_mutex);
    
    // Formato: timestamp-pid-counter
    snprintf(buffer, size, "%ld%06ld-%d-%lu", 
             tv.tv_sec, tv.tv_usec, getpid(), id);
}