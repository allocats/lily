#ifndef LILY_TOKEN_H
#define LILY_TOKEN_H

#include "meowrena/meowrena.h"
#include "token/types.h"

#define TOKEN_IS_BUILTIN_TYPE(n) ((n) >= TOK_I8 && (n) <= TOK_USIZE)

void tokens_init(TokenArray* tokens) ;

Token* tokens_get_new_tok(TokenArray* tokens);

void token_print(Token* token);
void tokens_print(TokenArray* tokens);

#endif // !LILY_TOKEN_H
