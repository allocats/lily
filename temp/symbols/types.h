// TODO: Add types

#ifndef LILY_SYMBOLS_TYPES_H
#define LILY_SYMBOLS_TYPES_H

#include "ast/nodes/types.h"
#include "meowrena/meowrena.h"
#include "string_interner/types.h"
#include "utils/types.h"

#define SYMBOL_ID_NONE U32_MAX
#define SCOPE_ID_NONE  U32_MAX

typedef u32 SymbolId;
typedef u32 ScopeId;

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

//
// structs and unions
//
typedef struct {
    SymbolId* fields;
    u32 count;
} SymbolStruct, SymbolUnion;

//
// enums
//
typedef struct {
    SymbolId* variants;
    u32 count;
} SymbolEnum;

//
// function
//
typedef struct {
    SymbolId* params;
    u32 count;

    SymbolId return_type;
} SymbolFunction, SymbolMacro;

//
// parameter 
//
typedef struct {
} SymbolParameter;

//
// constants
//
typedef struct {
    AstNodeId value;
} SymbolConstant;

//
// variable
//
typedef struct {
    AstNodeId value;
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
    u32 current_module_id;
    u32 current_scope_id;
    u32 current_file_id;

    SymbolTable* table;
} Resolver;

#endif // !LILY_SYMBOLS_TYPES_H
