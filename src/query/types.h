#ifndef LILY_QUERY_TYPES_H
#define LILY_QUERY_TYPES_H

#include "ids.h"

#define QUERY_STACK_MAX 256

typedef enum {
    RESOLVE_UNRESOLVED, 
    RESOLVE_RESOLVING, 
    RESOLVE_RESOLVED, 
    RESOLVE_ERROR, 
} ResolveState;

typedef enum {
    QUERT_CONST_EVAL,
    QUERY_SYMBOL,
    QUERY_TYPE
} QueryKind;

typedef struct {
    QueryKind kind;
    ModuleId  module_id;

    union {
        // constant evaluation
        AstNodeId node_id;

        // symbol
        SymbolId  symbol_id;

        // type
        TypeId    type_id;
    } as;
} Query;

typedef struct {
    Query items[QUERY_STACK_MAX]; 
    u32 top;
} QueryStack;

#endif // !LILY_QUERY_TYPES_H
