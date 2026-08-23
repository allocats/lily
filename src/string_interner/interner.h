#ifndef LILY_STRING_INTERNER_H
#define LILY_STRING_INTERNER_H

#include "ids.h"
#include "token/types.h"
#include "utils/types.h"

#define STRING_ID_LOOKUP(index)         (driver.string_interner.entries[index])
#define STRING_ID_LOOKUP_REF(index)     (&driver.string_interner.entries[index])

#define STR8_PRINT(id) STRING_ID_LOOKUP(id).str.len, STRING_ID_LOOKUP(id).str.ptr

void string_interner_init(void);

StringId string_intern_str8(str8 str);
StringId string_lookup_str8(str8 str);
StringId string_intern_cstr(const char* str);
StringId string_lookup_cstr(const char* str);
StringId string_intern_token(FileId file_id, Token token);

#endif // !LILY_STRING_INTERNER_H
