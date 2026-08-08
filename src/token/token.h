#ifndef LILY_TOKEN_H
#define LILY_TOKEN_H

#include "meowrena/meowrena.h"
#include "token/types.h"

#define TOKEN_IS_BUILTIN_TYPE(n) ((n) >= TOK_I8 && (n) <= TOK_USIZE)

void tokens_init(TokenArray* tokens) ;

Token* tokens_get_new_tok(TokenArray* tokens);

bool token_is_assignment(TokenKind kind);

void token_print(Token* token);
void tokens_print(TokenArray* tokens);

#endif // !LILY_TOKEN_H
