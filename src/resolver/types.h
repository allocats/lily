#ifndef LILY_RESOLVER_TYPES_H
#define LILY_RESOLVER_TYPES_H

#include "ids.h"
#include "symbols/types.h"

#define RESOLVE_STACK_MAX 256

typedef struct {
    ResolveKind kind;
    ModuleId module_id;

    union {
        SymbolId symbol;
        TypeId type;
    } as;
} ResolveItem;

typedef struct {
    ResolveItem items[RESOLVE_STACK_MAX];
    i32 top;
} ResolveStack;

typedef struct {
    NamespaceId current_namespace_id;
    ModuleId current_module_id;
    ScopeId current_scope_id;

    SymbolTable* table; // points to current symbol table
    SymbolTable* builtins; // points to builtin symbol table from driver_ctx
} Resolver;

#endif // !LILY_RESOLVER_TYPES_H
