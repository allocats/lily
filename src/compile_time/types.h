#ifndef LILY_COMPILE_TIME_TYPES_H
#define LILY_COMPILE_TIME_TYPES_H

#include "string_interner/types.h"
#include "utils/types.h"

typedef struct CompTimeValue CompTimeValue;

typedef enum {
    COMPILE_TIME_INTEGER,
    COMPILE_TIME_FLOATING,
    COMPILE_TIME_BOOLEAN,
    COMPILE_TIME_STRING,
    COMPILE_TIME_CHAR,
    COMPILE_TIME_TYPE,
    COMPILE_TIME_AGGREGATE,
} CompTimeKind;

typedef struct {
    TypeId type_id;

    u32 count;
    CompTimeValue* elements;
} CompTimeAggregate;

typedef struct CompTimeValue {
    CompTimeKind kind;
    
    union {
        i64 integer;
        f64 floating;
        bool boolean;
        StringId string;
        char character;
        TypeId type;
        CompTimeAggregate aggregate;
    } as;
} CompTimeValue;

#endif // !LILY_COMPILE_TIME_TYPES_H
