#ifndef LILY_SYMBOLS_TYPES_H
#define LILY_SYMBOLS_TYPES_H

#include "ids.h"
#include "resolver_stack/types.h"

typedef enum {
    SYMBOL_ENUM,
    SYMBOL_FIELD,
    SYMBOL_FUNCTION,
    SYMBOL_IMPORT,
    SYMBOL_PARAMETER,
    SYMBOL_STRUCT,
    SYMBOL_UNION,
    SYMBOL_VARIABLE,
    SYMBOL_VARIANT,
} SymbolKind;

typedef struct {
    SymbolId id;
    SymbolKind kind;
    u16 flags;

    FileId file_id;
    StringId name_id;
    AstNodeId ast_node_id;

    ResolveState state;

    union {
        struct {
            ModuleId module_id;
        } SymbolImport;

        struct {
            SymbolId* parameters;
            u32 parameter_count;

            TypeId return_type_id;
        } SymbolFunction;

        struct {
            TypeId type_id;
        } SymbolParameter;

        struct {
            SymbolId* variants;
            u32 variant_count;

            TypeId resolved_type_id;
        } SymbolEnum;

        struct {
            TypeId type_id;
        } SymbolVariant;

        struct {
            SymbolId* fields;
            u32 field_count;

            TypeId resolved_type_id;
        } SymbolUnion, SymbolStruct;

        struct {
            TypeId type_id;
        } SymbolField;

        struct {
            TypeId type_id;
        } SymbolVariable;
    } as;
} Symbol;

#endif // !LILY_SYMBOLS_TYPES_H
