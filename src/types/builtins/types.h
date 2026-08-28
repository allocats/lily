#ifndef LILY_TYPES_BUILTINS_TYPES_H
#define LILY_TYPES_BUILTINS_TYPES_H

#include "ids.h"
#include "types/builtins/builtins.h"
#include "utils/types.h"

#include <stdarg.h>

typedef struct {
    TypeId id;

    str8 name;
    StringId name_id;

    u16 align;
    u32 size;
} TypeBuiltin;

typedef struct {
    #define X(id, name, size, align) TypeId type_##id;
        BUILTIN_TYPES(X)
    #undef X
} TypeBuiltinIds;

static TypeBuiltin BUILTIN_NOMINAL_TYPES[] = {
#define X(id, str, sz, al)              \
    {                                   \
        .name = {                       \
            .ptr = str,                 \
            .len = sizeof(str) - 1,     \
        },                              \
        .size = sz,                     \
        .align = al,                    \
    },

    BUILTIN_TYPES(X)
#undef X
};

static constexpr u32 BUILTIN_NOMINAL_TYPES_COUNT = 17;

#endif // !LILY_TYPES_BUILTINS_TYPES_H
