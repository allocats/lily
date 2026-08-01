// TODO: Add types

#ifndef LILY_SYMBOLS_TYPES_H
#define LILY_SYMBOLS_TYPES_H

#include "ids.h"
#include "meowrena/meowrena.h"
#include "utils/types.h"

#define SYMBOLS(X)      \
    X(SYM_UNION)        \
    X(SYM_STRUCT)       \
    X(SYM_FIELD)        \
    X(SYM_ENUM)         \
    X(SYM_VARIANT)      \
    X(SYM_MACRO)        \
    X(SYM_FUNCTION)     \
    X(SYM_PARAMETER)    \
    X(SYM_CONSTANT)     \
    X(SYM_VARIABLE)     \

typedef enum {
    SYMBOLS(GENERATE_ENUM)
} SymbolKind;

static const char* SYMBOL_KIND_STRINGS[] = {
    SYMBOLS(GENERATE_STRING)
};

#undef SYMBOLS

typedef struct {
    ModuleId module_id;
    SymbolId symbol_id;
} SymbolRef;

//
// fields, parameters
//
typedef struct {
    SymbolRef type;
} SymbolField, SymbolParameter;

// structs and unions
//
typedef struct {
    SymbolId* fields;
    SymbolRef* types;
    u32 count;
} SymbolStruct, SymbolUnion;

//
// enums
//
typedef struct {
    SymbolRef type;
    SymbolId* variants;
    u32 count;
} SymbolEnum;

//
// function
//
typedef struct {
    SymbolId* params;
    u32 count;

    SymbolRef return_type;
} SymbolFunction, SymbolMacro;

//
// constants
//
typedef struct {
    AstNodeId value;
    SymbolRef type;
} SymbolConstant;

//
// variable
//
typedef struct {
    AstNodeId value;
    SymbolRef type;
} SymbolVariable;

typedef struct {
    SymbolId id;
    StringId name;
    ScopeId  scope;

    AstNodeId declaration;

    SymbolKind kind;

    union {
        SymbolParameter parameter;
        SymbolFunction  function;
        SymbolConstant  constant;
        SymbolVariable  variable;
        SymbolStruct    structs;
        SymbolField     field;
        SymbolMacro     macro;
        SymbolUnion     unions;
        SymbolEnum      enums;
    } as;
} Symbol;

typedef struct {
    // small arena for hash table, start off with like 1kb 
    Arena arena; 

    // AstNodeId owner; // could be useful for attaching the scope to the block it is declared by 
    ScopeId parent;

    StringId* str_ids;
    SymbolId* ids;
    u32 count;
    u32 capacity;
} Scope;

typedef struct {
    Arena arena;

    Scope* scopes;
    u32 scope_count; // acts like a 'top' variable, 0 == module level, lowest it can go is 0
    u32 scope_capacity;

    Symbol* symbols;
    u32 symbol_count;
    u32 symbol_capacity;
} SymbolTable;

typedef struct {
    NamespaceId current_namespace_id;
    ModuleId current_module_id;
    ScopeId current_scope_id;

    SymbolTable* table;
} Resolver;

#endif // !LILY_SYMBOLS_TYPES_H
