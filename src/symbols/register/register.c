#include "ast/nodes/types.h"
#include "ast/parser/parser.h"
#include "diagnostics/diagnostics.h"
#include "hash/hash.h"
#include "ids.h"
#include "modules/modules.h"
#include "symbols/symbols.h"
#include "symbols/register/register.h"
#include "types/ty.h"

void register_symbol(Resolver* r, AstNode* node, AstNodeId node_id) {
    switch (node -> kind) {
        case AST_FUNCTION:
            sym_register_function(r, node, node_id);
            break;

        case AST_MACRO:
            sym_register_macro(r, node, node_id);
            break;

        case AST_STRUCT:
            sym_register_struct(r, node, node_id);
            break;

        case AST_UNION:
            sym_register_union(r, node, node_id);
            break;

        case AST_CONST:
            sym_register_constant(r, node, node_id);
            break;

        case AST_ENUM:
            sym_register_enum(r, node, node_id);
            break;

        default:
            break;
    }
}

void sym_register_constant(Resolver* r, AstNode* node, AstNodeId node_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);

    StringId constant_name = node -> as.const_decl.name_id;
    SymbolId symbol_id = scope_get_sym(r, constant_name, hash_fnv1a_u32(constant_name));

    if (symbol_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            symbol_id,
            node_id
        );

        return;
    }

    symbol_id = scope_add_sym(r, node_id, constant_name, SYM_CONSTANT);
}

void sym_register_enum(Resolver* r, AstNode* node, AstNodeId node_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);

    StringId enum_name = node -> as.enum_decl.name_id;
    SymbolId symbol_id = scope_get_sym(r, enum_name, hash_fnv1a_u32(enum_name));

    if (symbol_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            symbol_id,
            node_id
        );

        return;
    }

    symbol_id = scope_add_sym(r, node_id, enum_name, SYM_ENUM);

    Symbol* symbol = &r -> table -> symbols[symbol_id]; 

    u32 variant_count = node -> as.enum_decl.variant_count;
    if (variant_count == 0) {
        symbol -> as.enums.count = 0;
    } else {
        symbol -> as.enums.variants = arena_alloc(&r -> table -> arena, variant_count * sizeof(SymbolId));
        symbol -> as.enums.count = variant_count;
    }

    TypeId type_id = type_table_register_enum(module, node, node_id, symbol_id);

    if (type_id == TYPE_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            symbol_id,
            node_id
        );
    }

    symbol -> as.enums.type = type_id;
}

void sym_register_function(Resolver* r, AstNode* node, AstNodeId node_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);

    StringId function_name = node -> as.func_decl.name_id;
    u32 hash = hash_fnv1a_u32(function_name);

    SymbolId symbol_id = builtins_get_sym(r, function_name, hash);

    if (symbol_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_is_builtin(
            &driver_ctx.diagnostics,
            module,
            symbol_id,
            node_id
        );

        return;
    }

    symbol_id = scope_get_sym(r, function_name, hash);

    if (symbol_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            symbol_id,
            node_id
        );

        return;
    }

    symbol_id = scope_add_sym(r, node_id, function_name, SYM_FUNCTION);

    Symbol* symbol = &r -> table -> symbols[symbol_id]; 

    u32 param_count = node -> as.func_decl.param_count;
    if (param_count == 0) {
        symbol -> as.function.count = 0;
    } else {
        symbol -> as.function.params = arena_alloc(&r -> table -> arena, param_count * sizeof(SymbolId));
        symbol -> as.function.count = param_count;
    }

    if (node -> flags & AST_FLAGS_IS_VARIADIC) {
        symbol -> as.function.is_variadic = true;
    }
}

void sym_register_macro(Resolver* r, AstNode* node, AstNodeId node_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);

    StringId macro_name = node -> as.macro_decl.name_id;
    SymbolId symbol_id = scope_get_sym(r, macro_name, hash_fnv1a_u32(macro_name));

    if (symbol_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            symbol_id,
            node_id
        );

        return;
    }

    symbol_id = scope_add_sym(r, node_id, macro_name, SYM_MACRO);

    Symbol* symbol = &r -> table -> symbols[symbol_id]; 

    u32 param_count = node -> as.macro_decl.param_count;
    if (param_count == 0) {
        symbol -> as.macro.count = 0;
        return;
    }

    symbol -> as.macro.params = arena_alloc(&r -> table -> arena, param_count * sizeof(SymbolId));
    symbol -> as.macro.count = param_count;
}

void sym_register_struct(Resolver* r, AstNode* node, AstNodeId node_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);

    StringId struct_name = node -> as.struct_decl.name_id;
    SymbolId symbol_id = scope_get_sym(r, struct_name, hash_fnv1a_u32(struct_name));

    if (symbol_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            symbol_id,
            node_id
        );

        return;
    }

    symbol_id = scope_add_sym(r, node_id, struct_name, SYM_STRUCT);

    Symbol* symbol = &r -> table -> symbols[symbol_id]; 

    u32 field_count = node -> as.struct_decl.field_count;
    if (field_count == 0) {
        symbol -> as.structs.count = 0;
    } else {
        symbol -> as.structs.fields = arena_alloc(&r -> table -> arena, field_count * sizeof(SymbolId));
        symbol -> as.structs.count = field_count;
    }

    TypeId type_id = type_table_register_struct(module, node, node_id, symbol_id);

    if (type_id == TYPE_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            symbol_id,
            node_id
        );
    }

    symbol -> as.structs.type = type_id;
}

void sym_register_union(Resolver* r, AstNode* node, AstNodeId node_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);

    StringId union_name = node -> as.union_decl.name_id;
    SymbolId symbol_id = scope_get_sym(r, union_name, hash_fnv1a_u32(union_name));

    if (symbol_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            symbol_id,
            node_id
        );

        return;
    }

    symbol_id = scope_add_sym(r, node_id, union_name, SYM_UNION);

    Symbol* symbol = &r -> table -> symbols[symbol_id]; 

    u32 field_count = node -> as.union_decl.field_count;
    if (field_count == 0) {
        symbol -> as.unions.count = 0;
    } else {
        symbol -> as.unions.fields = arena_alloc(&r -> table -> arena, field_count * sizeof(SymbolId));
        symbol -> as.unions.count = field_count;
    }

    TypeId type_id = type_table_register_union(module, node, node_id, symbol_id);

    if (type_id == TYPE_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            symbol_id,
            node_id
        );
    }

    symbol -> as.unions.type = type_id;
}

void sym_register_variable(Resolver* r, AstNode* node, AstNodeId node_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);

    StringId variable_name = node -> as.var_decl.name_id;
    SymbolId symbol_id = table_get_sym(r, variable_name);

    if (symbol_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            symbol_id,
            node_id
        );

        return;
    }

    symbol_id = scope_add_sym(r, node_id, variable_name, SYM_VARIABLE);
}
