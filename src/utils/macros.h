#ifndef LILY_UTILS_MACROS_H
#define LILY_UTILS_MACROS_H

#include <stdio.h>

#define NEXT_POWER_OF_TWO(x) ((x) == 1 ? 1 : 1 << (64 - __builtin_clzl((x) - 1)))

#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#define MIN(a, b) (a) < (b) ? (a) : (b)
#define MAX(a, b) (a) > (b) ? (a) : (b)

#define STR8_FMT(s) (s).len, (s).ptr

#define UNREACHABLE(msg)                            \
    do {                                            \
        fprintf(stderr, "UNREACHABLE: %s\n", msg);  \
        abort();                                    \
    } while(0);

#define DA_RESIZE(da, arena)                                                        \
    do {                                                                            \
        if (UNLIKELY((da).count >= (da).capacity)) {                                \
            u64 old_size = (da).capacity * sizeof((da).items[0]);                   \
            u64 new_size = old_size * 2;                                            \
                                                                                    \
            (da).items = arena_realloc((arena), (da).items, old_size, new_size);    \
            (da).capacity *= 2;                                                     \
        }                                                                           \
    } while(0);

#endif // !LILY_UTILS_MACROS_H
