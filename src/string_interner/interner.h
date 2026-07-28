#ifndef LILY_STRING_INTERNER_H
#define LILY_STRING_INTERNER_H

#include "string_interner/types.h"

#define STRING_ID_LOOKUP(index)         (driver_ctx.string_interner.entries[index])
#define STRING_ID_LOOKUP_REF(index)     (&driver_ctx.string_interner.entries[index])
#define STRING_INTERNER_LOOKUP_TOKEN(t) (string_lookup_str8((t) -> lexeme))

void string_intnerner_init(void);

StringId string_intern_str8(str8 str);
StringId string_lookup_str8(str8 str);

#endif // !LILY_STRING_INTERNER_H
