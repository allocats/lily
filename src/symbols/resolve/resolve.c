#include "diagnostics/diagnostics.h"
#include "driver/types.h"
#include "files/files.h"
#include "ids.h"
#include "resolver_stack/stack.h"
#include "resolver_stack/types.h"
#include "symbols/resolve/resolve.h"
#include "symbols/resolve/types.h"
#include "symbols/scope/scope.h"
#include "symbols/symbols/types.h"
#include "symbols/table/table.h"
#include "types/entries/entries.h"
#include "types/entries/types.h"
#include "types/resolve/resolve.h"
#include "types/table/table.h"
#include "utils/macros.h"

#include <assert.h>
#include <stdio.h>

extern DriverCtx driver;

static bool resolve_symbol_body(SymbolId id);
static bool resolve_struct(Resolver* r, SymbolId id);
static bool resolve_union(Resolver* r, SymbolId id);
static bool resolve_enum(Resolver* r, SymbolId id);
static bool resolve_function(Resolver* r, SymbolId id);

bool resolve_symbol(SymbolId id) {
    assert(id < driver.symbol_table.symbol_count);

    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(id);

    if (symbol -> state == RESOLVE_RESOLVED) return true;
    if (symbol -> state == RESOLVE_ERROR) return false;

    ResolveQuery query = {
        .kind = QUERY_SYMBOL,
        .as.symbol = id
    };

    if (symbol -> state == RESOLVE_RESOLVING) {
        i32 cycle_start = resolver_stack_find(query);

        if (cycle_start == -1) {
            UNREACHABLE("resolve_symbol()");
        } else {
            diagnostic_add_symbol_cycle(query);
        }

        symbol -> state = RESOLVE_ERROR;
        return false;
    }

    symbol -> state = RESOLVE_RESOLVING;

    if (!resolver_stack_push(query)) {
        diagnostic_add_generic(
            DIAG_ERROR,
            "reached recursion limit for symbol definition"
        );

        symbol -> state = RESOLVE_ERROR;
        return false;
    }

    bool result = resolve_symbol_body(id);

    resolver_stack_pop();

    symbol -> state = result ? RESOLVE_RESOLVED : RESOLVE_ERROR;

    return result;
}

static bool resolve_symbol_body(SymbolId id) {
    assert(id < driver.symbol_table.symbol_count);

    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(id);

    File* file = file_lookup_id(symbol -> file_id);

    bool result = false;

    Resolver r = {
        .file = file,
        .scope_id = file -> scope_id
    };

    switch (symbol -> kind) {
        case SYMBOL_STRUCT:
            result = resolve_struct(&r, id);
            break;

        case SYMBOL_UNION:
            result = resolve_union(&r, id);
            break;

        case SYMBOL_ENUM:
            result = resolve_enum(&r, id);
            break;

        case SYMBOL_FUNCTION:
            result = resolve_function(&r, id);
            break;

        // TODO: all the other symbols :p

        default:
            UNREACHABLE("resolve_symbol_body()");
    }

    return result;
}

static bool resolve_struct(Resolver* r, SymbolId id) {
    bool result = true;

    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(id);
    File* file = file_lookup_id(symbol -> file_id);
    AstNode* node = &file -> ast.nodes[symbol -> ast_node_id];

    u32 size = 0;
    u16 align = 0; 

    u32 field_count = node -> as.struct_decl.fields.count;

    scope_enter(r);

    for (u32 i = 0; i < field_count; i++) {
        AstNodeId field_id  = node -> as.struct_decl.fields.ids[i];
        AstNode* field_node = &file -> ast.nodes[field_id];

        StringId field_name_id = field_node -> as.field.name;

        SymbolId field_symbol_id = scope_lookup(r -> scope_id, field_name_id);
        
        if (field_symbol_id != SYMBOL_ID_NONE) {
            diagnostic_add_symbol_redefined(
                file -> id,
                field_id,
                field_symbol_id,
                field_name_id
            );

            result = false;

            continue;
        }

        field_symbol_id = scope_intern_from_node(r -> scope_id, file -> id, field_name_id, field_id);

        TypeId field_type_id = resolve_type_expr(file -> id, field_node -> as.field.type_expr);

        if (field_type_id == TYPE_ID_NONE) {
            // TODO: diagnostics, does not exist

            printf("a\n");

            result = false;

            continue;
        }

        if (is_type_void(field_type_id)) {
            // TODO: diagnostics, add indirection

            printf("b\n");

            result = false;

            continue;
        }

        TypeEntry* field_type_entry = TYPE_ID_LOOKUP_REF(field_type_id);

        size += field_type_entry -> size;
        align = MAX(align, field_type_entry -> alignment);
    }

    scope_exit(r);

    TypeEntry* entry = TYPE_ID_LOOKUP_REF(symbol -> as.struct_symbol.resolved_type_id);

    entry -> size = size;
    entry -> alignment = align;

    return result;
}

static bool resolve_union(Resolver* r, SymbolId id) {
    bool result = true;

    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(id);
    File* file = file_lookup_id(symbol -> file_id);
    AstNode* node = &file -> ast.nodes[symbol -> ast_node_id];

    u32 size = 0;
    u16 align = 0; 

    u32 field_count = node -> as.union_decl.fields.count;

    scope_enter(r);

    for (u32 i = 0; i < field_count; i++) {
        AstNodeId field_id  = node -> as.union_decl.fields.ids[i];
        AstNode* field_node = &file -> ast.nodes[field_id];

        StringId field_name_id = field_node -> as.field.name;

        SymbolId field_symbol_id = scope_lookup(r -> scope_id, field_name_id);
        
        if (field_symbol_id != SYMBOL_ID_NONE) {
            diagnostic_add_symbol_redefined(
                file -> id,
                field_id,
                field_symbol_id,
                field_name_id
            );

            result = false;

            continue;
        }

        field_symbol_id = scope_intern_from_node(r -> scope_id, file -> id, field_name_id, field_id);

        TypeId field_type_id = resolve_type_expr(file -> id, field_node -> as.field.type_expr);

        if (field_type_id == TYPE_ID_NONE) {
            // TODO: diagnostics, does not exist

            printf("a\n");

            result = false;

            continue;
        }

        if (is_type_void(field_type_id)) {
            // TODO: diagnostics, add indirection

            printf("b\n");

            result = false;

            continue;
        }

        TypeEntry* field_type_entry = TYPE_ID_LOOKUP_REF(field_type_id);

        size  = MAX(size, field_type_entry -> size);
        align = MAX(align, field_type_entry -> alignment);
    }

    scope_exit(r);

    TypeEntry* entry = TYPE_ID_LOOKUP_REF(symbol -> as.union_symbol.resolved_type_id);

    entry -> size = size;
    entry -> alignment = align;

    return result;
}

static bool resolve_enum(Resolver* r, SymbolId id) {
    bool result = true;

    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(id);
    File* file = file_lookup_id(symbol -> file_id);
    AstNode* node = &file -> ast.nodes[symbol -> ast_node_id];

    if (node -> as.enum_decl.type_expr != AST_NODE_ID_NONE) {
        TypeId type_id = resolve_type_expr(file -> id, node -> as.enum_decl.type_expr);

        if (type_id == TYPE_ID_NONE) {
            // todo: diagnostics

            result = false;
        }

        symbol -> as.enum_symbol.resolved_type_id = type_id;
    } else {
        symbol -> as.enum_symbol.resolved_type_id = driver.type_table.builtins.type_i32;
    }

    u32 variant_count = node -> as.enum_decl.variants.count;

    scope_enter(r);

    for (u32 i = 0; i < variant_count; i++) {
        AstNodeId variant_id  = node -> as.enum_decl.variants.ids[i];
        AstNode* variant_node = &file -> ast.nodes[variant_id];

        StringId variant_name_id = variant_node -> as.variant.name;

        SymbolId variant_symbol_id = scope_lookup(r -> scope_id, variant_name_id);
        
        if (variant_symbol_id != SYMBOL_ID_NONE) {
            diagnostic_add_symbol_redefined(
                file -> id,
                variant_id,
                variant_symbol_id,
                variant_name_id
            );

            result = false;

            continue;
        }


        Symbol* variant_symbol = SYMBOL_ID_LOOKUP_REF(variant_symbol_id);

        variant_symbol -> as.variant_symbol.type_id = symbol -> as.enum_symbol.resolved_type_id;

        if (variant_node -> as.variant.value_expr == AST_NODE_ID_NONE) {
            variant_symbol -> as.variant_symbol.value = i;
        } else {
            // TODO: compile time interpreter
            // variant_symbol -> as.variant_symbol.value = compute_value();
        }
    }

    scope_exit(r);

    return result;
}


static bool resolve_function(Resolver* r, SymbolId id) {
}
