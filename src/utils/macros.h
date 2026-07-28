#ifndef LILY_UTILS_MACROS_H
#define LILY_UTILS_MACROS_H

#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#define MIN(a, b) (a) < (b) ? (a) : (b)
#define MAX(a, b) (a) > (b) ? (a) : (b)

#endif // !LILY_UTILS_MACROS_H
