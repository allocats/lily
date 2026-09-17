#include "token/token.h"
#include "files/files.h"
#include "files/types.h"
#include "ids.h"
#include "lexer/types.h"
#include "token/types.h"
#include "utils/debug.h"
#include "utils/macros.h"
#include "utils/types.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static constexpr u64 tokens_arena_init_size_kb = 1;
static constexpr u64 tokens_init_capacity = ARENA_KB(tokens_arena_init_size_kb) / sizeof(Token);
static constexpr u64 tokens_init_alloc_size = tokens_init_capacity * sizeof(Token);

static constexpr u64 locations_arena_init_size_kb = 1;
static constexpr u64 locations_init_capacity = ARENA_KB(locations_arena_init_size_kb) / sizeof(SourceLocation);
static constexpr u64 locations_init_alloc_size = locations_init_capacity * sizeof(SourceLocation);

// Always init the token array with something upfront
static_assert(tokens_init_capacity > 0);
static_assert(tokens_init_alloc_size > 0);

void tokens_array_init(TokenArray* arr) {
    assert(arr != null);

    arena_init(&arr -> arena, ARENA_KB(tokens_arena_init_size_kb), ALIGN_DEFAULT);
    debug_printf("Init tokens array arena with %luKB", arena_init_size_kb);

    arr -> items = arena_calloc(&arr -> arena, tokens_init_alloc_size);
    arr -> count = 0;
    arr -> capacity = tokens_init_capacity;

    debug_printf("Allocated TokenArray arr -> items with %lu bytes", tokens_init_alloc_size);
}

void source_locations_array_init(SourceLocationArray* arr) {
    assert(arr != null);

    arena_init(&arr -> arena, ARENA_KB(locations_arena_init_size_kb), ALIGN_DEFAULT);
    debug_printf("Init source locations array arena with %luKB", locations_arena_init_size_kb);

    arr -> items = arena_calloc(&arr -> arena, locations_init_alloc_size);
    arr -> count = 0;
    arr -> capacity = locations_init_capacity;

    debug_printf("Allocated SourceLocationArray arr -> items with %lu bytes", locations_init_alloc_size);
}

// asserts that tokens.count == locations.count
static void set_source_location(Lexer* lexer) {
    SourceLocationArray* arr = &lexer -> file -> source_locations;

    if (UNLIKELY(arr -> count >= arr -> capacity)) {
        u64 old_size = arr -> capacity * sizeof(SourceLocation);
        u64 new_size = old_size * 2;

        assert(new_size > old_size);

        arr -> items = arena_realloc(&arr -> arena, arr -> items, old_size, new_size);
        arr -> capacity *= 2;

        debug_printf("Reallocated SourceLocationArray arr -> items from %lu to %lu bytes", old_size, new_size);
    }

    arr -> items[arr -> count++] = (SourceLocation) {
        .line = lexer -> line,
        .col = lexer -> col
    };

    assert(lexer -> file -> tokens.count == arr -> count);
}

Token* tokens_get_new_token(Lexer* lexer) {
    TokenArray* arr = &lexer -> file -> tokens;

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

    Token* token = &arr -> items[arr -> count++];

    set_source_location(lexer);

    return token;
}

inline SourceLocation token_get_source_location(File* file, u32 index) {
    return file -> source_locations.items[index];
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
        "Token {\n  Kind: \"%s\"\n  Lexeme: %.*s\n  Length: %u\n}\n\n",
        TOKEN_KIND_STRS[token.kind],
        token.length,
        token_start,
        token.length
    );
}

void tokens_print(FileId id) {
    File* file = file_lookup_id(id);

    u32 count = file -> tokens.count;

    for (u32 i = 0; i < count; i++) {
        Token token = file -> tokens.items[i];
        SourceLocation location = file -> source_locations.items[i];

        const char* token_start = file -> buffer.ptr + token.start;

        printf(
            "%u :: Token {\n  Lexeme: \"%.*s\"\n  Kind: %s\n  Line: %d\n  Col: %d\n}\n\n",
            i,
            token.length,
            token_start,
            TOKEN_KIND_STRS[token.kind],
            location.line,
            location.col
        );
    }
}
