#ifndef LILY_UTILS_TIMER_H
#define LILY_UTILS_TIMER_H

#include "utils/types.h"

#define __USE_POSIX199309
#include <time.h>

typedef struct {
    struct timespec start;
    struct timespec end;
} Timer;

static inline void timer_start(Timer* timer) {
    clock_gettime(CLOCK_MONOTONIC, &timer -> start);
}

static inline void timer_end(Timer* timer) {
    clock_gettime(CLOCK_MONOTONIC, &timer -> end);
}

static inline f64 timer_elapsed_seconds(const Timer* timer) {
    time_t sec = timer -> end.tv_sec - timer -> start.tv_sec;
    long nsec = timer -> end.tv_nsec - timer -> start.tv_nsec;

    return (f64)sec + (f64)nsec / 1e9;
}

static inline f64 timer_elapsed_milliseconds(const Timer* timer) {
    return timer_elapsed_seconds(timer) * 1000.0;
}

#endif // !LILY_UTILS_TIMER_H
