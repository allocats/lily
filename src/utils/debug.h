#ifndef LILY_UTILS_DEBUG_H
#define LILY_UTILS_DEBUG_H

#ifdef DEBUG_MODE
#include <assert.h>
#include <stdio.h>

#define debug_printf(fmt, ...) do { printf("%s:%d: " fmt, __FILE__, __LINE__, ##__VA_ARGS__); } while(0)
#define debug_hex_dump(d, a, n) do { debug_hex_dump_impl(d, a, n); } while(0)
#define debug_assert(n) do { assert(n); } while(0)
#else
#define debug_printf(...) do {} while(0)
#define debug_hex_dump(d, a, n) do {} while(0)
#define debug_assert(n) do {} while(0)
#endif // DEBUG_MODE

void debug_hex_dump_impl(char* desc, void* addr, int len);

#endif // !LILY_UTILS_DEBUG_H
