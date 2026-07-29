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

static inline f64 time_elapsed_in_ms(Timer* timer) {
    f64 start = timer -> start.tv_sec * 1000.0 + timer -> start.tv_nsec / 1000000.0;
    f64 end = timer -> end.tv_sec * 1000.0 + timer -> end.tv_nsec / 1000000.0;
    return end - start;
}

#endif // !LILY_UTILS_TIMER_H
