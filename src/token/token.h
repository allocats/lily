#ifndef LILY_TOKEN_H
#define LILY_TOKEN_H

#include "ids.h"
#include "token/types.h"

void tokens_array_init(TokenArray* arr);
Token* tokens_get_new_token(TokenArray* arr);

void tokens_print(FileId id);

#endif // !LILY_TOKEN_H
