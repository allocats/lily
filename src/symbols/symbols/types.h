#ifndef LILY_SYMBOLS_SYMBOLS_TYPES_H
#define LILY_SYMBOLS_SYMBOLS_TYPES_H

#include "ids.h"
#include "resolver_stack/types.h"

typedef enum {
    SYMBOL_ENUM,
    SYMBOL_FIELD,
    SYMBOL_FUNCTION,
    SYMBOL_IMPORT,
    SYMBOL_PARAMETER,
    SYMBOL_STRUCT,
    SYMBOL_TYPE,
    SYMBOL_UNION,
    SYMBOL_VARIABLE,
    SYMBOL_VARIANT,
    SYMBOL_ERROR,
}__attribute__((packed)) SymbolKind;

typedef struct {
    SymbolId id;
    SymbolKind kind;

    // used for catching circular symbols, for example:
    //
    // MyStruct :: struct {
    //     x: MyStruct; // this should error!
    // }
    ResolveState state;

    // copied from AstNode, just for easier access and better 
    // cache performance (don't have to touch AST to check if const)
    u16 flags;

    // file it is from. 27/08: might not actually need this since scope's will be attached 
    // to files so the FileId is present when accessing the symbols
    FileId file_id;

    // binding
    StringId name_id;

    // declaration within the AST
    AstNodeId ast_node_id;

    union {
        struct {
            // TODO: think about exports, thinking of just making a top level scope attached to each file
            // then that holds the exported symbols? then perhaps change this to ScopeId rather than ModuleId?
            ScopeId scope_id;
        } import_symbol;

        struct {
            SymbolId* parameters;
            u32 parameter_count;

            TypeId return_type_id;
        } function_symbol;

        struct {
            TypeId type_id;
        } parameter_symbol;

        struct {
            SymbolId* variants;
            u32 variant_count;

            TypeId resolved_type_id;
        } enum_symbol;

        struct {
            TypeId type_id;
        } variant_symbol;

        struct {
            SymbolId* fields;
            u32 field_count;

            TypeId resolved_type_id;
        } union_symbol, struct_symbol;

        struct {
            TypeId type_id;
        } field_symbol;

        struct {
            TypeId type_id;
        } variable_symbol;

        struct {
            TypeId type_id;
        } type_symbol;
    } as;
} Symbol;

#endif // !LILY_SYMBOLS_SYMBOLS_TYPES_H
