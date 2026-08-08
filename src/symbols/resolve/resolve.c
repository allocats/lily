#include "ast/nodes/types.h"
#include "diagnostics/diagnostics.h"
#include "driver/types.h"
#include "hash/hash.h"
#include "ids.h"
#include "modules/modules.h"
#include "modules/types.h"
#include "resolver/enums.h"
#include "resolver/resolver.h"
#include "semantics/semantics.h"
#include "symbols/symbols.h"
#include "symbols/resolve/resolve.h"
#include "symbols/types.h"
#include "types/ty.h"
#include "utils/debug.h"
#include "utils/macros.h"

#include <stdio.h>

extern LilyCtx driver_ctx;

#define ALIGN_TO_NEXT_MULTIPLE_OF_POINTER_SIZE(n) ((n + sizeof(void*) - 1) & (-sizeof(void*)))

typedef struct  {
    TypeId return_type;
    StringId name_id;
} FunctionContext;

static bool resolve_symbol_body(ModuleId module_id, SymbolId id);
static bool resolve_function(Resolver* r, Module* module, SymbolId symbol_id);
static bool resolve_struct(Resolver* r, Module* module, SymbolId symbol_id);
static bool resolve_union(Resolver* r, Module* module, SymbolId symbol_id);
static bool resolve_enum(Resolver* r, Module* module, SymbolId symbol_id);

static bool resolve_constant(Resolver* r, Module* module, SymbolId symbol_id);
static bool resolve_block_constant(Resolver* r, Module* module, AstNodeId stmt_id);

static bool resolve_block(
    Resolver* r,
    Module* module,
    AstNodeId block_id,
    FunctionContext* fn_ctx,
    bool* out_always_returns
);

static bool resolve_statement(
    Resolver* r,
    Module* module,
    AstNodeId stmt_id,
    FunctionContext* fn_ctx,
    bool* out_stmt_returns
);

static bool resolve_let_declaration(Resolver* r, Module* module, AstNodeId stmt_id);
static bool resolve_return_stmt(Resolver* r, Module* module, AstNodeId stmt_id, TypeId return_type, StringId name);

bool symbols_resolve_by_id(ModuleId module_id, SymbolId id) {
    Module* module = MODULE_ID_LOOKUP_REF(module_id);
    Symbol* symbol = &module -> symbol_table.symbols[id];

    if (symbol -> resolve_state == RESOLVE_RESOLVED) return true;
    if (symbol -> resolve_state == RESOLVE_ERROR) return false;

    ResolveItem resolve_item = {
        .kind = RESOLVE_SYMBOL,
        .module_id = module_id,
        .as.symbol = id
    };

    if (symbol -> resolve_state == RESOLVE_RESOLVING) {
        i32 cycle_start = resolver_stack_find(&driver_ctx.resolver_stack, resolve_item);

        diagnostic_add_resolver_symbol_cycle(
            &driver_ctx.diagnostics,
            cycle_start
        );

        symbol -> resolve_state = RESOLVE_ERROR;
        return false;
    }

    symbol -> resolve_state = RESOLVE_RESOLVING;

    if (!resolver_stack_push(&driver_ctx.resolver_stack, resolve_item)) {
        diagnostic_add_generic(
            &driver_ctx.diagnostics,
            DIAG_ERROR,
            "reached recursion limit"
        );

        symbol -> resolve_state = RESOLVE_ERROR;
        return null;
    }

    bool resolved_body = resolve_symbol_body(module_id, id);

    resolver_stack_pop(&driver_ctx.resolver_stack);
    
    module -> symbol_table.symbols[id].resolve_state = resolved_body ? RESOLVE_RESOLVED : RESOLVE_ERROR;

    return resolved_body;

}

bool resolve_symbol_body(ModuleId module_id, SymbolId id) {
    Module* module = MODULE_ID_LOOKUP_REF(module_id);
    Symbol* symbol = &module -> symbol_table.symbols[id];

    Resolver r = {
        .current_namespace_id = module -> namespace_id,
        .current_module_id = module_id,
        .current_scope_id = 0,
        .builtins = &driver_ctx.builtins,
        .table = &module -> symbol_table
    };

    switch (symbol -> kind) {
        case SYM_FUNCTION:
            return resolve_function(&r, module, id);

        case SYM_STRUCT:
            return resolve_struct(&r, module, id);

        case SYM_UNION:
            return resolve_union(&r, module, id);

        case SYM_ENUM:
            return resolve_enum(&r, module, id);

        case SYM_CONSTANT:
            return resolve_constant(&r, module, id); 

        default:
            // printf("Hit default case in symbols_resolve_by_id (id=%d)\n", id);
            return true;
    }

    return false;
}

bool resolve_struct(Resolver* r, Module* module, SymbolId symbol_id) {
    Symbol* symbol = &r -> table -> symbols[symbol_id];
    AstNode* node = &module -> ast.nodes[symbol -> declaration];

    u32 size = 0;
    u32 align = 0;

    u32 count = node -> as.struct_decl.field_count;
    
    scope_enter(r);

    for (u32 i = 0; i < count; i++) {
        AstNodeId field_id = node -> as.struct_decl.fields[i];
        AstNode* field = &module -> ast.nodes[field_id];

        u32 field_hash = hash_fnv1a_u32(field -> as.field_decl.name_id);
        SymbolId field_sym_id = scope_get_sym(r, field -> as.field_decl.name_id, field_hash);

        if (field_sym_id != SYMBOL_ID_NONE) {
            diagnostic_add_symbol_already_defined(
                &driver_ctx.diagnostics,
                module,
                field_sym_id,
                field_id
            );

            continue;
        }

        field_sym_id = scope_add_sym(r, field_id, field -> as.field_decl.name_id, SYM_FIELD);

        TypeId field_type = resolve_type(module -> id, field -> as.field_decl.type_expr);
        if (field_type == driver_ctx.type_table.builtins.type_void) {
            diagnostic_add_type_cannot_be_void(
                &driver_ctx.diagnostics,
                module,
                field -> as.field_decl.type_expr
            );
            scope_exit(r);
            return false;
        }

        TypeEntry* entry = resolve_type_entry(module -> id, field_type);
        if (entry == NULL) {
            scope_exit(r);
            return false;
        }

        size += entry -> size;

        Symbol* field_sym = &module -> symbol_table.symbols[field_sym_id];

        field_sym -> as.field.type = field_type;

        module -> symbol_table.symbols[symbol_id].as.structs.fields[i] = field_sym_id;
    }

    scope_exit(r);

    align = ALIGN_TO_NEXT_MULTIPLE_OF_POINTER_SIZE(size);

    TypeEntry* entry = &driver_ctx.type_table.entries[symbol -> as.structs.type];

    entry -> size = size;
    entry -> align = align;

    return true;
}

bool resolve_union(Resolver* r, Module* module, SymbolId symbol_id) {
    Symbol* symbol = &r -> table -> symbols[symbol_id];
    AstNode* node = &module -> ast.nodes[symbol -> declaration];

    u32 size = 0;
    u32 align = 0;

    u32 count = node -> as.union_decl.field_count;
    
    scope_enter(r);

    for (u32 i = 0; i < count; i++) {
        AstNodeId field_id = node -> as.union_decl.fields[i];
        AstNode* field = &module -> ast.nodes[field_id];

        u32 field_hash = hash_fnv1a_u32(field -> as.field_decl.name_id);
        SymbolId field_sym_id = scope_get_sym(r, field -> as.field_decl.name_id, field_hash);

        if (field_sym_id != SYMBOL_ID_NONE) {
            diagnostic_add_symbol_already_defined(
                &driver_ctx.diagnostics,
                module,
                field_sym_id,
                field_id
            );

            continue;
        }

        field_sym_id = scope_add_sym(r, field_id, field -> as.field_decl.name_id, SYM_FIELD);

        TypeId field_type = resolve_type(module -> id, field -> as.field_decl.type_expr);
        if (field_type == driver_ctx.type_table.builtins.type_void) {
            diagnostic_add_type_cannot_be_void(
                &driver_ctx.diagnostics,
                module,
                field -> as.field_decl.type_expr
            );
            scope_exit(r);
            return false;
        }

        TypeEntry* entry = resolve_type_entry(module -> id, field_type);
        if (entry == NULL) {
            scope_exit(r);
            return false;
        }

        size = MAX(size, entry -> size);

        Symbol* field_sym = &module -> symbol_table.symbols[field_sym_id];

        field_sym -> as.field.type = field_type;

        module -> symbol_table.symbols[symbol_id].as.unions.fields[i] = field_sym_id;
    }

    scope_exit(r);

    align = ALIGN_TO_NEXT_MULTIPLE_OF_POINTER_SIZE(size);

    TypeEntry* entry = &driver_ctx.type_table.entries[symbol -> as.unions.type];

    entry -> size = size;
    entry -> align = align;

    return true;
}

bool resolve_enum(Resolver* r, Module* module, SymbolId symbol_id) {
    Symbol* symbol = &r -> table -> symbols[symbol_id];
    AstNode* node = &module -> ast.nodes[symbol -> declaration];

    TypeTable* table = &driver_ctx.type_table;

    TypeId underlying_type = TYPE_ID_NONE;

    if (node -> as.enum_decl.type_expr == AST_NODE_ID_NONE) {
        underlying_type = table -> builtins.type_i32;
    } else {
        underlying_type = resolve_type(module -> id, node -> as.enum_decl.type_expr);

        if (underlying_type == TYPE_ID_NONE) {
            diagnostic_add_type_does_not_exist(
                &driver_ctx.diagnostics,
                module,
                node -> as.enum_decl.type_expr
            );

            return false;
        }

        if (!types_is_integer(underlying_type)) {
            diagnostic_add_type_is_not_an_integer(
                &driver_ctx.diagnostics,
                module,
                node -> as.enum_decl.type_expr
            );

            return false;
        }
    }

    scope_enter(r);

    u32 count = node -> as.enum_decl.variant_count;

    for (u32 i = 0; i < count; i++) {
        AstNodeId variant_node_id = node -> as.enum_decl.variants[i];
        AstNode* variant_node = &module -> ast.nodes[variant_node_id];

        u32 variant_hash = hash_fnv1a_u32(variant_node -> as.variant_decl.name_id);
        SymbolId variant_sym_id = scope_get_sym(r, variant_node -> as.variant_decl.name_id, variant_hash);

        if (variant_sym_id != SYMBOL_ID_NONE) {
            diagnostic_add_symbol_already_defined(
                &driver_ctx.diagnostics,
                module,
                variant_sym_id,
                variant_node_id
            );

            continue;
        }

        variant_sym_id = scope_add_sym(r, variant_node_id, variant_node -> as.variant_decl.name_id, SYM_VARIANT);

        module -> symbol_table.symbols[variant_sym_id].as.variant.type = symbol -> as.enums.type;
        module -> symbol_table.symbols[symbol_id].as.enums.variants[i] = variant_sym_id;
    }

    scope_exit(r);

    TypeEntry* type = &table -> entries[underlying_type];
    TypeEntry* sym_type = &table -> entries[symbol -> as.enums.type];

    sym_type -> size = type -> size;
    sym_type -> align = type -> align;

    return true;
}


bool resolve_function(Resolver* r, Module* module, SymbolId symbol_id) {
    Symbol* symbol = &module -> symbol_table.symbols[symbol_id];
    AstNode* node = &module -> ast.nodes[symbol -> declaration];

    bool failed_to_resolve_signature = false;

    TypeId return_type_id = resolve_type(r -> current_module_id, node -> as.func_decl.return_type_expr);

    if (return_type_id == TYPE_ID_NONE) {
        diagnostic_add_return_type_invalid(
            &driver_ctx.diagnostics,
            module,
            symbol -> id
        );

        failed_to_resolve_signature = true;
    }

    module -> symbol_table.symbols[symbol_id].as.function.return_type = return_type_id;

    scope_enter(r);

    for (u32 i = 0; i < node -> as.func_decl.param_count; i++) {
        AstNodeId param_id = node -> as.func_decl.params[i];
        AstNode* param = &module -> ast.nodes[param_id];

        TypeId param_type = resolve_type(r -> current_module_id, param -> as.param_decl.type_expr);

        if (param_type == TYPE_ID_NONE) {
            diagnostic_add_type_does_not_exist(
                &driver_ctx.diagnostics,
                module,
                param -> as.param_decl.type_expr 
            );
            continue;
        }

        if (param_type == driver_ctx.type_table.builtins.type_void) {
            diagnostic_add_type_cannot_be_void(
                &driver_ctx.diagnostics,
                module,
                param -> as.param_decl.type_expr
            );
            continue;
        }

        SymbolId param_symbol_id = table_get_sym(r, param -> as.param_decl.name_id);

        if (param_symbol_id != SYMBOL_ID_NONE) {
            diagnostic_add_symbol_already_defined(
                &driver_ctx.diagnostics,
                module,
                param_symbol_id,
                param_id
            );

            continue;
        }

        param_symbol_id = scope_add_sym(r, param_id, param -> as.param_decl.name_id, SYM_PARAMETER); 

        module -> symbol_table.symbols[param_symbol_id].as.parameter.type = param_type;
        module -> symbol_table.symbols[symbol_id].as.function.params[i] = param_symbol_id;

        debug_printf("Symbols: Added parameter (%d) to function(%d)\n", param_symbol_id, symbol -> id);
    }

    if (failed_to_resolve_signature) {
        scope_exit(r);
        return false;
    }

    bool result = true;
    
    if (!(node -> flags & AST_FLAGS_IS_EXTERNAL)) {
        FunctionContext fn_ctx = {
            .name_id = symbol -> name,
            .return_type = return_type_id
        };

        bool always_returns = false;
        
        result = resolve_block(r, module, node -> as.func_decl.block, &fn_ctx, &always_returns);

        if (!result) {
            printf("ERROR RESOLVING FUNCTION BODY\n");
        }

        bool needs_return = return_type_id != driver_ctx.type_table.builtins.type_void;

        if (needs_return && !always_returns) {
            printf("Missing return!\n");
            result = false;
        }
    }

    scope_exit(r);

    return result;
}

static bool resolve_constant(Resolver* r, Module* module, SymbolId symbol_id) {
    Symbol* symbol = &module -> symbol_table.symbols[symbol_id];
    AstNode* node = &module -> ast.nodes[symbol -> declaration];

    TypeId type = resolve_type(module -> id, node -> as.const_decl.type_expr);
    if (type == TYPE_ID_NONE) {
        diagnostic_add_type_does_not_exist(
            &driver_ctx.diagnostics,
            module,
            node -> as.const_decl.type_expr
        );

        return false;
    }

    module -> symbol_table.symbols[symbol_id].as.constant.type = type;

    TypeId value_type = resolve_expression(r, module, node -> as.const_decl.value_expr, type);
    if (value_type == TYPE_ID_NONE) {
        return false;
    }

    if (type != value_type) {
        // todo
        printf("Constant does not have correct type!\n");
        return false;
    }

    module -> symbol_table.symbols[symbol_id].as.constant.value = node -> as.const_decl.value_expr;

    return true;
}

static bool resolve_block(
    Resolver* r,
    Module* module,
    AstNodeId block_id,
    FunctionContext* fn_ctx,
    bool* out_always_returns
) {
    AstNode* node = &module -> ast.nodes[block_id];
    u32 count = node -> as.block.stmt_count;

    bool result = true;
    bool always_returns = false;

    scope_enter(r);

    for (u32 i = 0; i < count; i++) {
        AstNodeId stmt_id = node -> as.block.stmts[i];

        bool stmt_returns = false;

        if (!resolve_statement(r, module, stmt_id, fn_ctx, &stmt_returns)) {
            result = false;
        }

        if (stmt_returns) {
            always_returns = true;
        }
    }

    scope_exit(r);

    node -> resolved_type = driver_ctx.type_table.builtins.type_void;

    *out_always_returns = always_returns;

    return result;
}

static bool resolve_statement(
    Resolver* r,
    Module* module,
    AstNodeId stmt_id,
    FunctionContext* fn_ctx,
    bool* out_stmt_returns
) {
    AstNode* stmt_node = &module -> ast.nodes[stmt_id];

    bool result = true;
    bool returns = false;

    switch (stmt_node -> kind) {
        case AST_RETURN: {
            bool needs_return = fn_ctx != NULL && fn_ctx -> return_type != driver_ctx.type_table.builtins.type_void;

            if (!needs_return) {
                if (stmt_node -> as.return_stmt.stmt != AST_NODE_ID_NONE) {
                    diagnostic_add_void_function_returns_value(&driver_ctx.diagnostics, module, stmt_id);
                    result = false;
                }

                stmt_node -> resolved_type = driver_ctx.type_table.builtins.type_void;
            } else {
                if (!resolve_return_stmt(r, module, stmt_id, fn_ctx -> return_type, fn_ctx -> name_id)) {
                    result = false;
                }

                stmt_node -> resolved_type = fn_ctx -> return_type;
            }

            returns = true;
            break;
        }

        case AST_DEFER: {
            TypeId t = resolve_expression(r, module, stmt_node -> as.defer_stmt.stmt, TYPE_ID_NONE);
            if (t == TYPE_ID_NONE) {
                result = false;
            }

            stmt_node -> resolved_type = driver_ctx.type_table.builtins.type_void;
            break;
        }

        case AST_CONST:
            if (!resolve_block_constant(r, module, stmt_id)) {
                result = false;
            }
            break;

        case AST_LET:
            if (!resolve_let_declaration(r, module, stmt_id)) {
                result = false;
            }
            break;

        case AST_IF: {
            AstIf* if_stmt = &stmt_node -> as.if_stmt;
            bool all_paths_return = true;

            for (u32 i = 0; i < if_stmt -> branch_count; i++) {
                AstNode* branch_node = &module -> ast.nodes[if_stmt -> branches[i]];
                AstBranch* branch = &branch_node -> as.branch;

                TypeId cond_type = resolve_expression(r, module, branch -> condition, driver_ctx.type_table.builtins.type_bool);
                if (cond_type == TYPE_ID_NONE || cond_type != driver_ctx.type_table.builtins.type_bool) {
                    result = false;
                }

                branch_node -> resolved_type = driver_ctx.type_table.builtins.type_void;

                bool branch_returns = false;

                if (!resolve_block(r, module, branch -> block, fn_ctx, &branch_returns)) {
                    result = false;
                }

                if (!branch_returns) {
                    all_paths_return = false;
                }
            }

            if (if_stmt -> else_block != AST_NODE_ID_NONE) {
                bool else_returns = false;

                if (!resolve_block(r, module, if_stmt -> else_block, fn_ctx, &else_returns)) {
                    result = false;
                }

                if (!else_returns) {
                    all_paths_return = false;
                }
            } else {
                all_paths_return = false;
            }

            returns = all_paths_return;

            stmt_node -> resolved_type = driver_ctx.type_table.builtins.type_void;

            break;
        }

        case AST_FOR: {
            AstFor* for_loop = &stmt_node -> as.for_loop;

            scope_enter(r);

            if (for_loop -> init != AST_NODE_ID_NONE) {
                bool unused = false;

                if (!resolve_statement(r, module, for_loop -> init, fn_ctx, &unused)) {
                    result = false;
                }
            }

            if (for_loop -> cond != AST_NODE_ID_NONE) {
                TypeId cond_type = resolve_expression(r, module, for_loop -> cond, driver_ctx.type_table.builtins.type_bool);
                if (cond_type == TYPE_ID_NONE || cond_type != driver_ctx.type_table.builtins.type_bool) {
                    result = false;
                }
            }

            if (for_loop -> step != AST_NODE_ID_NONE) {
                if (TYPE_ID_NONE == resolve_expression(r, module, for_loop -> step, TYPE_ID_NONE)) {
                    result = false;
                }
            }

            bool body_returns = false;

            if (!resolve_block(r, module, for_loop -> block, fn_ctx, &body_returns)) {
                result = false;
            }

            scope_exit(r);

            returns = false;
            stmt_node -> resolved_type = driver_ctx.type_table.builtins.type_void;
            break;
        }

        case AST_WHILE: {
            AstWhile* while_loop = &stmt_node -> as.while_loop;

            TypeId cond_type = resolve_expression(r, module, while_loop -> condition, driver_ctx.type_table.builtins.type_bool);
            if (cond_type == TYPE_ID_NONE || cond_type != driver_ctx.type_table.builtins.type_bool) {
                result = false;
            }

            bool body_returns = false;

            if (!resolve_block(r, module, while_loop -> block, fn_ctx, &body_returns)) {
                result = false;
            }
            
            returns = false;
            stmt_node -> resolved_type = driver_ctx.type_table.builtins.type_void;
            break;
        }

        default: {
            TypeId t = resolve_expression(r, module, stmt_id, TYPE_ID_NONE);
            if (t == TYPE_ID_NONE) {
                result = false;
            }

            stmt_node -> resolved_type = t;
            break;
        }
    }

    *out_stmt_returns = returns;

    return result;
}

static bool resolve_block_constant(Resolver* r, Module* module, AstNodeId stmt_id) {
    AstNode* node = &module -> ast.nodes[stmt_id];

    SymbolId id = table_get_sym(r, node -> as.const_decl.name_id);

    if (id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            id,
            stmt_id 
        );

        return false;
    }

    id = scope_add_sym(r, stmt_id, node -> as.const_decl.name_id, SYM_CONSTANT);

    Symbol* symbol = &module -> symbol_table.symbols[id];

    TypeId const_type = resolve_type(module -> id, node -> as.const_decl.type_expr);
    if (const_type == TYPE_ID_NONE) {
        diagnostic_add_type_does_not_exist(
            &driver_ctx.diagnostics,
            module,
            node -> as.const_decl.type_expr
        );

        return false;
    }

    symbol -> as.constant.type = const_type;
    symbol -> as.constant.value = node -> as.const_decl.value_expr;

    TypeId value_type = resolve_expression(r, module, node -> as.const_decl.value_expr, const_type);
    if (value_type == TYPE_ID_NONE) {
        printf("ASDL:KJ\n");
        return false;
    }

    if (value_type != const_type) {
        printf("Value resolved type (%u) != constiable type (%u)\n", value_type, const_type);
        return false;
    }

    return true;
}

static bool resolve_let_declaration(Resolver* r, Module* module, AstNodeId stmt_id) {
    AstNode* node = &module -> ast.nodes[stmt_id];

    SymbolId id = table_get_sym(r, node -> as.var_decl.name_id);

    if (id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            id,
            stmt_id 
        );

        return false;
    }

    id = scope_add_sym(r, stmt_id, node -> as.var_decl.name_id, SYM_VARIABLE);

    Symbol* symbol = &module -> symbol_table.symbols[id];

    TypeId var_type = resolve_type(module -> id, node -> as.var_decl.type_expr);
    if (var_type == TYPE_ID_NONE) {
        diagnostic_add_type_does_not_exist(
            &driver_ctx.diagnostics,
            module,
            node -> as.var_decl.type_expr
        );

        return false;
    }

    symbol -> as.variable.type = var_type;
    symbol -> as.variable.value = node -> as.var_decl.value_expr;

    if (symbol -> as.variable.value == AST_NODE_ID_NONE) return true;

    TypeId value_type = resolve_expression(r, module, node -> as.var_decl.value_expr, var_type);
    if (value_type == TYPE_ID_NONE) {
        return false;
    }

    if (value_type != var_type) {
        printf("Value resolved type (%u) != variable type (%u)\n", value_type, var_type);
        return false;
    }

    return true;
}

static bool resolve_return_stmt(Resolver* r, Module* module, AstNodeId stmt_id, TypeId return_type, StringId name) {
    bool result = true;

    AstNode* node = &module -> ast.nodes[stmt_id];

    TypeId type = resolve_expression(r, module, node -> as.return_stmt.stmt, return_type);

    if (type != return_type) {
        result = false;

        diagnostic_add_function_expects_but_returns(
            &driver_ctx.diagnostics,
            module,
            name,
            stmt_id,
            return_type,
            type
        );
    }

    return result;
}
