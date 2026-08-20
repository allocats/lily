#ifndef LILY_TOKEN_H
#define LILY_TOKEN_H

#include "ids.h"
#include "token/types.h"

void tokens_array_init(TokenArray* arr);
Token* tokens_get_new_token(TokenArray* arr);

i64 token_get_int_literal(FileId id, Token token);
f64 token_get_float_literal(FileId id, Token token);
i64 token_get_char_literal(FileId id, Token token);

void token_print(FileId id, Token token);
void tokens_print(FileId id);

#endif // !LILY_TOKEN_H
