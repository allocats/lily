#ifndef MEOWRENA_H
#define MEOWRENA_H

#include <stdint.h>
#include <stddef.h>

#include "utils/debug.h"
#include "utils/types.h"

typedef struct Arena      Arena;
typedef struct ArenaBlock ArenaBlock;

typedef enum {
    ALIGN_DEFAULT = alignof(max_align_t),
    ALIGN_2       = 2,
    ALIGN_8       = 8,
    ALIGN_16      = 16,
    ALIGN_32      = 32,
    ALIGN_64      = 64,
} Alignment;

typedef struct Arena {
    u64 default_capacity;
    Alignment byte_alignment;
    ArenaBlock* current;
    ArenaBlock* start;
    ArenaBlock* end;

    // last allocation, to create a growing array in realloc
    void* cached_allocation;

    u64 total_capacity;
    u64 total_used;
} Arena;

typedef struct ArenaBlock {
    ArenaBlock* next;
    u64 used;
    u64 capacity;
    u8 __padding[40];
    u8 data[]; 
} ArenaBlock;

#define ARENA_KB(n) (n * 1024)
#define ARENA_MB(n) (n * 1024 * 1024)
#define ARENA_GB(n) (n * 1024 * 1024 * 1024)

#define arena_alloc_array(arena, type, n) \
    arena_alloc(arena, sizeof(type) * (n))

#define ARENA_ALIGN_UP(a, n) (((u64)(n) + (u64)(a) - 1) & ~((u64)(a) - 1))

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
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define arena_likely(x)   __builtin_expect(!!(x), 1)
#define arena_unlikely(x) __builtin_expect(!!(x), 0)

// Internal functions
static ArenaBlock* __arena_new_block(Arena* arena, u64 size);

void arena_init(Arena* arena, u64 default_capacity, Alignment arena_alignment) {
    assert(arena && "Arena is null in arena_init(arena, capacity, align)\n");
    
    arena -> default_capacity = ARENA_ALIGN_UP(arena_alignment, default_capacity);
    arena -> byte_alignment = arena_alignment;
    arena -> current = null;
    arena -> start = null;
    arena -> end = null;
    arena -> cached_allocation = null;
    arena -> total_capacity = 0;
    arena -> total_used = 0;
}

void* arena_alloc(Arena* arena, u64 size) {
    assert(size != 0 && "Passed in 0 size request to arena_alloc(arena, size)\n");

    u64 aligned_size = ARENA_ALIGN_UP(arena -> byte_alignment, size);

    ArenaBlock* block = arena -> current;

    if (arena_unlikely(!block)) {
        block = __arena_new_block(arena, aligned_size);

        arena -> current = block;
        arena -> start = block;
        arena -> end = block;
    }

    assert(block -> used <= block -> capacity);

    if (aligned_size > block -> capacity - block -> used) {
        ArenaBlock* new_block = __arena_new_block(arena, aligned_size);

        arena -> end = new_block;
        arena -> current = new_block;

        block -> next = new_block;
        block = new_block;
    }

    void* ptr = (u8*) block -> data + block -> used; 
    block -> used += aligned_size;

    arena -> total_used += aligned_size;
    arena -> cached_allocation = ptr;

    debug_printf("arena_alloc() allocated %lu bytes onto arena, %p", aligned_size, arena);

    return ptr;
}

void* arena_calloc(Arena* arena, u64 size) {
    assert(size != 0 && "Passed in 0 size request to arena_calloc(arena, size)\n");

    void* ptr = arena_alloc(arena, size);
    arena_memset(ptr, 0, size);
    return ptr;
}

void* arena_realloc(Arena* arena, void* ptr, u64 old_size, u64 new_size) {
    if (arena_unlikely(new_size <= old_size)) {
        return ptr;
    }

    void* cached = arena -> cached_allocation;

    u64 aligned_new_size = ARENA_ALIGN_UP(arena -> byte_alignment, new_size);

    if (cached == ptr) {
        ArenaBlock* block = arena -> current;

        u64 offset = (u64) ((u8*) ptr - block -> data);
        u64 unused = block -> capacity - offset;

        if (aligned_new_size <= unused) {
            u64 old_used = block -> used;

            block -> used = offset + aligned_new_size;

            arena -> total_used += block -> used - old_used;

            return ptr;
        }
    }


    void* result = arena_alloc(arena, new_size);
    memcpy(result, ptr, old_size);
    return result;
}

void* arena_memcpy(void* dest, void* src, u64 len) {
    return memcpy(dest, src, len);
}

void* arena_memset(void* ptr, u8 value, u64 len) {
    return memset(ptr, value, len);
}

void  arena_reset(Arena* arena) {
    for (ArenaBlock* current = arena -> start; current != null; current = current -> next) {
        current -> used = 0;
    }

    arena -> current = arena -> start;
    arena -> cached_allocation = null;
    arena -> total_used = 0;

    debug_printf("arena_reset() reset the arena, %p", arena);
}

void arena_destroy(Arena* arena) {
    ArenaBlock* current = arena -> start;

    arena -> current = null;
    arena -> start = null;
    arena -> end = null;
    arena -> total_used = 0;
    arena -> total_capacity = 0;
    arena -> cached_allocation = null;

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
    printf("  Total capacity: %ld\n\n", arena_total_capacity(arena));
}

u64 arena_total_usage(Arena* arena) {
    return arena -> total_used;
}

u64 arena_total_capacity(Arena* arena) {
    return arena -> total_capacity;
}

// Internal

static ArenaBlock* __arena_new_block(Arena* arena, u64 size) {
    u64 capacity = arena -> total_capacity > arena -> default_capacity 
                 ? arena -> total_capacity 
                 : arena -> default_capacity;

    while (size > capacity) capacity *= 2;

    u64 aligned_capacity = ARENA_ALIGN_UP(arena -> byte_alignment, capacity);
    u64 total_size       = ARENA_ALIGN_UP(arena -> byte_alignment, sizeof(ArenaBlock) + aligned_capacity);

    ArenaBlock* block = (ArenaBlock*) aligned_alloc(arena -> byte_alignment, total_size);
    assert(block != null && "aligned_alloc() failed! Buy more ram silly :3!\n");

    debug_printf("new_block() allocated through aligned alloc %lu bytes for arena, %p", total_size, arena);

    arena_memset(block, 0, sizeof(ArenaBlock));

    block -> next = null;
    block -> used = 0;
    block -> capacity = aligned_capacity;

    arena -> total_capacity += aligned_capacity;

    return block;
}

#endif // MEOWRENA_IMPL
