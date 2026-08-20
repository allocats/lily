#include "token/token.h"
#include "files/files.h"
#include "ids.h"
#include "token/types.h"
#include "utils/debug.h"
#include "utils/macros.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static constexpr u64 arena_init_size_kb = 1;
static constexpr u64 tokens_init_capacity = ARENA_KB(arena_init_size_kb) / sizeof(Token);
static constexpr u64 tokens_init_alloc_size = tokens_init_capacity * sizeof(Token);

// Always init the token array with something upfront
static_assert(tokens_init_capacity > 0);
static_assert(tokens_init_alloc_size > 0);

void tokens_array_init(TokenArray* arr) {
    assert(arr != null);

    arena_init(&arr -> arena, ARENA_KB(arena_init_size_kb), ALIGN_DEFAULT);
    debug_printf("Init tokens array arena with %luKB", arena_init_size_kb);

    arr -> items = arena_calloc(&arr -> arena, tokens_init_alloc_size);
    arr -> count = 0;
    arr -> capacity = tokens_init_capacity;

    debug_printf("Allocated TokenArray arr -> items with %lu bytes", tokens_init_alloc_size);
}

Token* tokens_get_new_token(TokenArray* arr) {
    // TODO: Profile these asserts to find out whether or not to make them debug asserts,
    // but keep as is, IN CASE we run into memory errors during development and can easily
    // catch scuffed/broken allocations
    debug_assert(arr != null);
    debug_assert(arr -> items != null);
    debug_assert(arr -> capacity > 0);
    debug_assert(arr -> count <= arr -> capacity);

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

i64 token_get_int_literal(FileId id, Token token) {
    assert(id < AST_NODE_ID_NONE);

    File* file = file_lookup_id(id);

    const char* start = file -> buffer.ptr + token.start;

    char* end = ((char*) start) + token.length;

    char putback = *end;

    *end = 0;

    i64 value = strtoll(start, null, 10);

    *end = putback;

    return value;
}

f64 token_get_float_literal(FileId id, Token token) {
    assert(id < AST_NODE_ID_NONE);

    File* file = file_lookup_id(id);

    const char* start = file -> buffer.ptr + token.start;

    char* end = ((char*) start) + token.length;

    char putback = *end;

    *end = 0;

    f64 value = strtod(start, null);

    *end = putback;

    return value;
}

i64 token_get_char_literal(FileId id, Token token) {
    assert(id < AST_NODE_ID_NONE);

    File* file = file_lookup_id(id);

    const char* start = file -> buffer.ptr + token.start;
    const char* character = start + 1;

    return *character;
}

void token_print(FileId id, Token token) {
    File* file = file_lookup_id(id);

    const char* token_start = file -> buffer.ptr + token.start;

    printf(
        "Token {\n  Lexeme: \"%.*s\"\n  Kind: %s\n}\n\n",
        token.length,
        token_start,
        TOKEN_KIND_STRS[token.kind]
    );
}

void tokens_print(FileId id) {
    File* file = file_lookup_id(id);

    u32 count = file -> tokens.count;

    for (u32 i = 0; i < count; i++) {
        Token token = file -> tokens.items[i];

        const char* token_start = file -> buffer.ptr + token.start;

        printf(
            "%u :: Token {\n  Lexeme: \"%.*s\"\n  Kind: %s\n}\n\n",
            i,
            token.length,
            token_start,
            TOKEN_KIND_STRS[token.kind]
        );
    }
}
