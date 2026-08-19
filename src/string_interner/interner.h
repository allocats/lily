#ifndef LILY_STRING_INTERNER_H
#define LILY_STRING_INTERNER_H

#include "ids.h"
#include "utils/types.h"
#include "token/types.h"

#define STRING_ID_LOOKUP(index)         (driver_ctx.string_interner.entries[index])
#define STRING_ID_LOOKUP_REF(index)     (&driver_ctx.string_interner.entries[index])

#define STR8_PRINT(id) STRING_ID_LOOKUP(id).str.length, STRING_ID_LOOKUP(id).str.pointer

void string_interner_init(void);

StringId string_intern_str8(str8 str);
StringId string_lookup_str8(str8 str);
StringId string_intern_cstr(const char* str);
StringId string_lookup_cstr(const char* str);
StringId string_intern_token(FileId file_id, Token token);

#endif // !LILY_STRING_INTERNER_H
