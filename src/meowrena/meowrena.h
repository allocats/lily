#ifndef MEOWRENA_H
#define MEOWRENA_H

#include <stdint.h>
#include <stddef.h>

#define null        NULL

typedef int8_t      i8;
typedef int16_t     i16;
typedef int32_t     i32;
typedef int64_t     i64;

typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;

typedef uint8_t     b8;
typedef uint32_t    b32;

typedef float       f32;
typedef double      f64;
typedef long double f128;

typedef size_t      usize;
typedef ptrdiff_t   isize;

typedef struct Arena      Arena;
typedef struct ArenaBlock ArenaBlock;

typedef enum {
    ALIGN_NONE = 1,
    ALIGN_2    = 2,
    ALIGN_8    = 8,
    ALIGN_16   = 16,
    ALIGN_32   = 32,
    ALIGN_64   = 64,
} Alignment;

typedef struct Arena {
    u64 default_capacity;
    Alignment byte_alignment;
    ArenaBlock* current;
    ArenaBlock* start;
    ArenaBlock* end;
} Arena;

typedef struct ArenaBlock {
    ArenaBlock* next;
    u64 used;
    u64 capacity;
    u8 __padding[8];
    u8 data[]; 
} ArenaBlock;

#define ARENA_KB(n) (n * 1024)
#define ARENA_MB(n) (n * 1024 * 1024)
#define ARENA_GB(n) (n * 1024 * 1024 * 1024)

#define arena_alloc_array(arena, type, n) \
    arena_alloc(arena, sizeof(type) * n)

#define ARENA_ALIGN_UP(a, n) ((n + a - 1) & ~(a - 1))

void  arena_init(Arena* arena, u64 default_capacity, Alignment arena_alignment);
void* arena_alloc(Arena* arena, u64 size);
void* arena_calloc(Arena* arena, u64 size);
void* arena_realloc(Arena* arena, void* ptr, u64 old_size, u64 new_size);
void* arena_memcpy(void* dest, void* src, u64 size);
void* arena_memset(void* ptr, u8 value, u64 size);
void  arena_reset(Arena* arena);
void  arena_destroy(Arena* arena);

void  arena_print_stats(Arena* arena, char* label);
u64   arena_total_usage(Arena* arena);
u64   arena_total_capacity(Arena* arena);

#endif // !MEOWRENA_H

#ifdef MEOWRENA_IMPL

#include <assert.h>
#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define arena_likely(x)   __builtin_expect(!!(x), 1)
#define arena_unlikely(x) __builtin_expect(!!(x), 0)

#define AVX2_CHUNK(p, n) (p + (32 * n))
#define SSE2_CHUNK(p, n) (p + (16 * n))

// Internal functions
static ArenaBlock* __arena_new_block(Alignment align, u64 default_capacity, u64 size);

static void  __arena_dispatch(void); 
static void* __arena_realloc_avx2(Arena* arena, void* ptr, u64 old_size, u64 new_size);
static void* __arena_memcpy_avx2(void* dest, void* src, u64 len);
static void* __arena_memset_avx2(void* ptr, u8 value, u64 len);

static void* __arena_realloc_sse2(Arena* arena, void* ptr, u64 old_size, u64 new_size);
static void* __arena_memcpy_sse2(void* dest, void* src, u64 len);
static void* __arena_memset_sse2(void* ptr, u8 value, u64 len);

static void* __arena_realloc_generic(Arena* arena, void* ptr, u64 old_size, u64 new_size);
static void* __arena_memcpy_generic(void* dest, void* src, u64 len);
static void* __arena_memset_generic(void* ptr, u8 value, u64 len);

static void* (*__arena_realloc_impl)(Arena* arena, void* ptr, u64 old_size, u64 new_size);
static void* (*__arena_memcpy_impl)(void* dest, void* src, u64 len);
static void* (*__arena_memset_impl)(void* ptr, u8 value, u64 len);

__attribute__ ((constructor)) static void __arena_dispatch(void) {
    __builtin_cpu_init();

    if (__builtin_cpu_supports("avx2")) {
        __arena_realloc_impl = __arena_realloc_avx2;
        __arena_memcpy_impl  = __arena_memcpy_avx2;
        __arena_memset_impl  = __arena_memset_avx2;
    } else if (__builtin_cpu_supports("sse2")) {
        __arena_realloc_impl = __arena_realloc_sse2;
        __arena_memcpy_impl  = __arena_memcpy_sse2;
        __arena_memset_impl  = __arena_memset_sse2;
    } else {
        __arena_realloc_impl = __arena_realloc_generic;
        __arena_memcpy_impl  = __arena_memcpy_generic;
        __arena_memset_impl  = __arena_memset_generic;
    }
}

void arena_init(Arena* arena, u64 default_capacity, Alignment arena_alignment) {
    assert(arena && "Arena is null in arena_init(arena, capacity, align)\n");
    
    arena -> default_capacity = ARENA_ALIGN_UP(arena_alignment, default_capacity);
    arena -> byte_alignment = arena_alignment;
    arena -> current = null;
    arena -> start = null;
    arena -> end = null;
}

void* arena_alloc(Arena* arena, u64 size) {
    assert(size != 0 && "Passed in 0 size request to arena_alloc(arena, size)\n");

    u64 aligned_size = ARENA_ALIGN_UP(arena -> byte_alignment, size);

    ArenaBlock* block = arena -> current;

    if (arena_unlikely(!block)) {
        block = __arena_new_block(arena -> byte_alignment, arena -> default_capacity, aligned_size);

        arena -> current = block;
        arena -> start = block;
        arena -> end = block;
    }

    assert(block -> used <= block -> capacity);

    if (aligned_size > block -> capacity - block -> used) {
        ArenaBlock* new_block = __arena_new_block(arena -> byte_alignment, arena -> default_capacity, aligned_size);

        arena -> end = new_block;
        arena -> current = new_block;

        block -> next = new_block;
        block = new_block;
    }

    void* ptr = (u8*) block -> data + block -> used; 
    block -> used += aligned_size;
    return ptr;
}

void* arena_calloc(Arena* arena, u64 size) {
    assert(size != 0 && "Passed in 0 size request to arena_calloc(arena, size)\n");

    void* ptr = arena_alloc(arena, size);
    arena_memset(ptr, 0, size);
    return ptr;
}

void* arena_realloc(Arena* arena, void* ptr, u64 old_size, u64 new_size) {
    return __arena_realloc_impl(arena, ptr, old_size, new_size);
}

void* arena_memcpy(void* dest, void* src, u64 len) {
    return __arena_memcpy_impl(dest, src, len);
}

void* arena_memset(void* ptr, u8 value, u64 len) {
    return __arena_memset_impl(ptr, value, len);
}

void  arena_reset(Arena* arena) {
    for (ArenaBlock* current = arena -> start; current != null; current = current -> next) {
        current -> used = 0;
    }

    arena -> current = arena -> start;
}

void arena_destroy(Arena* arena) {
    ArenaBlock* current = arena -> start;

    arena -> current = null;
    arena -> start = null;
    arena -> end = null;

    while (current != null) {
        ArenaBlock* previous = current;
        current = current -> next;
        free(previous);
    }
}

void arena_print_stats(Arena* arena, char* label) {
    i64 block_count = 0;

    for (ArenaBlock* block = arena -> start; block != null; block = block -> next) {
        block_count++;
    }

    printf("%s Stats:\n", label);
    printf("  Blocks: %ld\n", block_count);
    printf("  Total Usage: %ld\n", arena_total_usage(arena));
    printf("  Total capacity: %ld\n", arena_total_capacity(arena));
}

u64 arena_total_usage(Arena* arena) {
    u64 total = 0;

    for (ArenaBlock* block = arena -> start; block != null; block = block -> next) {
        total += block -> used;
    }

    return total;
}

u64 arena_total_capacity(Arena* arena) {
    u64 total = 0;

    for (ArenaBlock* block = arena -> start; block != null; block = block -> next) {
        total += block -> capacity;
    }

    return total;
}

// Internal

static ArenaBlock* __arena_new_block(Alignment arena_alignment, u64 default_capacity, u64 size) {
    u64 capacity = default_capacity;

    while (size > capacity) capacity *= 2;

    u64 aligned_capacity = ARENA_ALIGN_UP(arena_alignment, capacity);
    u64 total_size       = ARENA_ALIGN_UP(arena_alignment, sizeof(ArenaBlock) + aligned_capacity);

    ArenaBlock* block = (ArenaBlock*) aligned_alloc(arena_alignment, total_size);
    assert(block != null && "aligned_alloc() failed! Buy more ram silly :3!\n");

    arena_memset(block, 0, total_size);

    block -> next = null;
    block -> used = 0;
    block -> capacity = aligned_capacity;

    return block;
}

static void* __arena_realloc_avx2(Arena* arena, void* ptr, u64 old_size, u64 new_size) {
    if (arena_unlikely(new_size <= old_size)) {
        return ptr;
    }

    void* result = arena_alloc(arena, new_size);
    u8* new_ptr = (u8*) result;
    u8* old_ptr = (u8*) ptr;
    u64 copy_size = old_size;

    while (copy_size >= 128) {
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(new_ptr, 0), _mm256_loadu_si256((const __m256i*) AVX2_CHUNK(old_ptr, 0)));
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(new_ptr, 1), _mm256_loadu_si256((const __m256i*) AVX2_CHUNK(old_ptr, 1)));
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(new_ptr, 2), _mm256_loadu_si256((const __m256i*) AVX2_CHUNK(old_ptr, 2)));
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(new_ptr, 3), _mm256_loadu_si256((const __m256i*) AVX2_CHUNK(old_ptr, 3)));

        new_ptr += 128;
        old_ptr += 128;
        copy_size -= 128;
    }

    while (copy_size >= 96) {
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(new_ptr, 0), _mm256_loadu_si256((const __m256i*) AVX2_CHUNK(old_ptr, 0)));
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(new_ptr, 1), _mm256_loadu_si256((const __m256i*) AVX2_CHUNK(old_ptr, 1)));
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(new_ptr, 2), _mm256_loadu_si256((const __m256i*) AVX2_CHUNK(old_ptr, 2)));

        new_ptr += 96;
        old_ptr += 96;
        copy_size -= 96;
    }

    while (copy_size >= 64) {
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(new_ptr, 0), _mm256_loadu_si256((const __m256i*) AVX2_CHUNK(old_ptr, 0)));
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(new_ptr, 1), _mm256_loadu_si256((const __m256i*) AVX2_CHUNK(old_ptr, 1)));

        new_ptr += 64;
        old_ptr += 64;
        copy_size -= 64;
    }

    while (copy_size >= 32) {
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(new_ptr, 0), _mm256_loadu_si256((const __m256i*) AVX2_CHUNK(old_ptr, 0)));

        new_ptr += 32;
        old_ptr += 32;
        copy_size -= 32;
    }

    while (copy_size--) {
        *new_ptr++ = *old_ptr++;
    }

    return result;
}

static void* __arena_memset_avx2(void* ptr, u8 value, u64 len) {
    u8* p = (u8*) ptr;

    __m256i byte_value = _mm256_set1_epi8(value);

    while (len >= 128) {
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(p, 0), byte_value);
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(p, 1), byte_value);
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(p, 2), byte_value);
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(p, 3), byte_value);

        p += 128;
        len -= 128;
    }

    while (len >= 96) {
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(p, 0), byte_value);
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(p, 1), byte_value);
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(p, 2), byte_value);

        p += 96;
        len -= 96;
    }

    while (len >= 64) {
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(p, 0), byte_value);
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(p, 1), byte_value);

        p += 64;
        len -= 64;
    }

    while (len >= 32) {
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(p, 0), byte_value);

        p += 32;
        len -= 32;
    }

    while (len > 0) {
        *p++ = value;
        len--;
    }

    return ptr;
}

static void* __arena_memcpy_avx2(void* dest, void* src, u64 len) {
    u8* d = (u8*) dest;
    u8* s = (u8*) src;


    while (len >= 128) {
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(d, 0), _mm256_loadu_si256((const __m256i*) AVX2_CHUNK(s, 0)));
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(d, 1), _mm256_loadu_si256((const __m256i*) AVX2_CHUNK(s, 1)));
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(d, 2), _mm256_loadu_si256((const __m256i*) AVX2_CHUNK(s, 2)));
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(d, 3), _mm256_loadu_si256((const __m256i*) AVX2_CHUNK(s, 3)));

        len -= 128;
        d += 128;
        s += 128;
    }

    while (len >= 96) {
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(d, 0), _mm256_loadu_si256((const __m256i*) AVX2_CHUNK(s, 0)));
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(d, 1), _mm256_loadu_si256((const __m256i*) AVX2_CHUNK(s, 1)));
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(d, 2), _mm256_loadu_si256((const __m256i*) AVX2_CHUNK(s, 2)));

        len -= 96;
        d += 96;
        s += 96;
    }

    while (len >= 64) {
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(d, 0), _mm256_loadu_si256((const __m256i*) AVX2_CHUNK(s, 0)));
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(d, 1), _mm256_loadu_si256((const __m256i*) AVX2_CHUNK(s, 1)));

        len -= 64;
        d += 64;
        s += 64;
    }

    while (len >= 32) {
        _mm256_storeu_si256((__m256i*) AVX2_CHUNK(d, 0), _mm256_loadu_si256((const __m256i*) AVX2_CHUNK(s, 0)));

        len -= 32;
        d += 32;
        s += 32;
    }

    while (len--) {
        *d++ = *s++;
    }

    return dest;
}

void* __arena_realloc_sse2(Arena* arena, void* ptr, u64 old_size, u64 new_size) {
    if (arena_unlikely(new_size <= old_size)) {
        return ptr;
    }

    void* result = arena_alloc(arena, new_size);
    u8* new_ptr = (u8*) result;
    u8* old_ptr = (u8*) ptr;
    u64 copy_size = old_size;

    while (copy_size >= 64) {
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(new_ptr, 0), _mm_loadu_si128((const __m128i*) SSE2_CHUNK(old_ptr, 0)));
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(new_ptr, 1), _mm_loadu_si128((const __m128i*) SSE2_CHUNK(old_ptr, 1)));
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(new_ptr, 2), _mm_loadu_si128((const __m128i*) SSE2_CHUNK(old_ptr, 2)));
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(new_ptr, 3), _mm_loadu_si128((const __m128i*) SSE2_CHUNK(old_ptr, 3)));

        new_ptr += 64;
        old_ptr += 64;
        copy_size -= 64;
    }

    while (copy_size >= 48) {
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(new_ptr, 0), _mm_loadu_si128((const __m128i*) SSE2_CHUNK(old_ptr, 0)));
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(new_ptr, 1), _mm_loadu_si128((const __m128i*) SSE2_CHUNK(old_ptr, 1)));
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(new_ptr, 2), _mm_loadu_si128((const __m128i*) SSE2_CHUNK(old_ptr, 2)));

        new_ptr += 48;
        old_ptr += 48;
        copy_size -= 48;
    }

    while (copy_size >= 32) {
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(new_ptr, 0), _mm_loadu_si128((const __m128i*) SSE2_CHUNK(old_ptr, 0)));
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(new_ptr, 1), _mm_loadu_si128((const __m128i*) SSE2_CHUNK(old_ptr, 1)));

        new_ptr += 32;
        old_ptr += 32;
        copy_size -= 32;
    }

    while (copy_size >= 16) {
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(new_ptr, 0), _mm_loadu_si128((const __m128i*) SSE2_CHUNK(old_ptr, 0)));

        new_ptr += 16;
        old_ptr += 16;
        copy_size -= 16;
    }

    while (copy_size--) {
        *new_ptr++ = *old_ptr++;
    }

    return result;
}

static void* __arena_memset_sse2(void* ptr, u8 value, u64 len) {
    u8* p = (u8*) ptr;

    __m128i byte_value = _mm_set1_epi8(value);

    while (len >= 64) {
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(p, 0), byte_value);
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(p, 1), byte_value);
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(p, 2), byte_value);
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(p, 3), byte_value);

        p += 64;
        len -= 64;
    }

    while (len >= 48) {
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(p, 0), byte_value);
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(p, 1), byte_value);
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(p, 2), byte_value);

        p += 48;
        len -= 48;
    }

    while (len >= 32) {
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(p, 0), byte_value);
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(p, 1), byte_value);

        p += 32;
        len -= 32;
    }

    while (len >= 16) {
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(p, 0), byte_value);

        p += 16;
        len -= 16;
    }

    while (len > 0) {
        *p++ = value;
        len--;
    }

    return ptr;
}

static void* __arena_memcpy_sse2(void* dest, void* src, u64 len) {
    u8* d = (u8*) dest;
    u8* s = (u8*) src;


    while (len >= 64) {
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(d, 0), _mm_loadu_si128((const __m128i*) SSE2_CHUNK(s, 0)));
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(d, 1), _mm_loadu_si128((const __m128i*) SSE2_CHUNK(s, 1)));
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(d, 2), _mm_loadu_si128((const __m128i*) SSE2_CHUNK(s, 2)));
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(d, 3), _mm_loadu_si128((const __m128i*) SSE2_CHUNK(s, 3)));

        len -= 64;
        d += 64;
        s += 64;
    }

    while (len >= 48) {
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(d, 0), _mm_loadu_si128((const __m128i*) SSE2_CHUNK(s, 0)));
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(d, 1), _mm_loadu_si128((const __m128i*) SSE2_CHUNK(s, 1)));
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(d, 2), _mm_loadu_si128((const __m128i*) SSE2_CHUNK(s, 2)));

        len -= 48;
        d += 48;
        s += 48;
    }

    while (len >= 32) {
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(d, 0), _mm_loadu_si128((const __m128i*) SSE2_CHUNK(s, 0)));
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(d, 1), _mm_loadu_si128((const __m128i*) SSE2_CHUNK(s, 1)));

        len -= 32;
        d += 32;
        s += 32;
    }

    while (len >= 16) {
        _mm_storeu_si128((__m128i*) SSE2_CHUNK(d, 0), _mm_loadu_si128((const __m128i*) SSE2_CHUNK(s, 0)));

        len -= 16;
        d += 16;
        s += 16;
    }

    while (len--) {
        *d++ = *s++;
    }

    return dest;
}

void* __arena_realloc_generic(Arena* arena, void* ptr, u64 old_size, u64 new_size) {
    if (arena_unlikely(new_size <= old_size)) return ptr;

    void* result = arena_alloc(arena, new_size);
    u8* new_ptr = (u8*) result;
    u8* old_ptr = (u8*) ptr;

    memcpy(new_ptr, old_ptr, old_size);

    return result;
}

void* __arena_memset_generic(void* ptr, u8 value, u64 len) {
    memset(ptr, value, len);
    return ptr;
} 

static void* __arena_memcpy_generic(void* dest, void* src, u64 len) {
    memcpy(dest, src, len);
    return dest;
}

#endif // MEOWRENA_IMPL
