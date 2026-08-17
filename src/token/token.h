#ifndef LILY_TOKEN_H
#define LILY_TOKEN_H

#include "token/types.h"

void tokens_array_init(TokenArray* arr);
Token* tokens_get_new_token(TokenArray* arr);

#endif // !LILY_TOKEN_H
