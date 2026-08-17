#include "token/token.h"
#include "token/types.h"
#include "utils/debug.h"
#include "utils/macros.h"

#include <assert.h>

static constexpr u32 arena_init_size_kb = 1;
static constexpr u32 tokens_init_capacity = ARENA_KB(arena_init_size_kb) / sizeof(Token);
static constexpr u32 tokens_init_alloc_size = tokens_init_capacity * sizeof(Token);

// Always init the token array with something upfront
static_assert(tokens_init_capacity > 0);
static_assert(tokens_init_alloc_size > 0);

void tokens_array_init(TokenArray* arr) {
    assert(arr != null);

    arena_init(&arr -> arena, ARENA_KB(arena_init_size_kb), ALIGN_DEFAULT);
    debug_printf("Init tokens array arena with %uKB", arena_init_size_kb);

    arr -> items = arena_calloc(&arr -> arena, tokens_init_alloc_size);
    arr -> count = 0;
    arr -> capacity = tokens_init_capacity;

    debug_printf("Allocated TokenArray arr -> items with %u bytes", tokens_init_alloc_size);
}

Token* tokens_get_new_token(TokenArray* arr) {
    assert(arr != null);
    assert(arr -> items != null);
    assert(arr -> capacity > 0);
    assert(arr -> count <= arr -> capacity);

    if (UNLIKELY(arr -> count >= arr -> capacity)) {
        u64 old_size = arr -> capacity * sizeof(Token);
        u64 new_size = old_size * 2;

        assert(new_size > old_size);

        arr -> items = arena_realloc(&arr -> arena, arr -> items, old_size, new_size);
        arr -> capacity *= 2;

        debug_printf("Reallocated TokenArray arr -> items from %lu to %lu bytes", old_size, new_size);
    }

    return &arr -> items[arr -> count++];
}
