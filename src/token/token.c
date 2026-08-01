#include "token/token.h"

#include "utils/debug.h"
#include "utils/macros.h"

#include <stdio.h>

#define TOKENS_INIT_CAPACITY 64

void tokens_init(TokenArray* tokens) {
    arena_init(&tokens -> arena, ARENA_KB(1), ALIGN_16);
    debug_printf("Tokens: Allocated tokens arena with 1KB\n");

    tokens -> items = arena_alloc_array(&tokens -> arena, Token, TOKENS_INIT_CAPACITY);
    tokens -> count = 0;
    tokens -> capacity = TOKENS_INIT_CAPACITY;

    debug_printf("Tokens: Allocated tokens array with %zu bytes\n", sizeof(Token) * TOKENS_INIT_CAPACITY);
}

Token* tokens_get_new_tok(TokenArray* tokens) {
    if (UNLIKELY(tokens -> count >= tokens -> capacity)) {
        u64 size = sizeof(Token) * tokens -> capacity;

        tokens -> items = arena_realloc(&tokens -> arena, tokens -> items, size, size * 2);
        tokens -> capacity *= 2;

        debug_printf("Tokens realloc from %ld -> %ld bytes\n", size, size * 2);
    }

    return &tokens -> items[tokens -> count++];
}

void token_print(Token* token) {
    printf(
        "Token {\n  Kind: %s\n  Lexeme: %.*s\n Length: %d\n}\n\n",
        TOKEN_KIND_STRS[token -> kind],
        token -> lexeme.length,
        token -> lexeme.pointer,
        token -> lexeme.length
    );
}

void tokens_print(TokenArray* tokens) {
    for (u32 i = 0; i < tokens -> count; i++) {
        Token token = tokens -> items[i];

        printf(
            "Token %d {\n  Kind: %s\n  Lexeme: %.*s\n  Length: %d\n}",
            i,
            TOKEN_KIND_STRS[token.kind],
            token.lexeme.length,
            token.lexeme.pointer,
            token.lexeme.length
        );
        
        printf("\n\n");
    }
}
