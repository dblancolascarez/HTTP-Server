// Medición de tiempos (gettimeofday)
#include "utils.h"

void timer_start(http_timer_t *timer) {
    if (!timer) return;
    gettimeofday(&timer->start, NULL);
}

void timer_stop(http_timer_t *timer) {
    if (!timer) return;
    gettimeofday(&timer->end, NULL);
}

long timer_elapsed_ms(http_timer_t *timer) {
    if (!timer) return 0;
    
    long seconds = timer->end.tv_sec - timer->start.tv_sec;
    long microseconds = timer->end.tv_usec - timer->start.tv_usec;
    
    return seconds * 1000 + microseconds / 1000;
}

long timer_elapsed_us(http_timer_t *timer) {
    if (!timer) return 0;
    
    long seconds = timer->end.tv_sec - timer->start.tv_sec;
    long microseconds = timer->end.tv_usec - timer->start.tv_usec;
    
    return seconds * 1000000 + microseconds;
}

