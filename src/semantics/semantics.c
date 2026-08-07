#include "semantics/semantics.h"
#include "ast/nodes/nodes.h"
#include "ast/nodes/types.h"
#include "driver/types.h"
#include "hash/hash.h"
#include "ids.h"
#include "modules/modules.h"
#include "symbols/resolve/resolve.h"
#include "symbols/symbols.h"
#include "symbols/types.h"
#include "types/ty.h"
#include "types/types.h"
#include "utils/debug.h"
#include "utils/types.h"

#include <stdio.h>

extern LilyCtx driver_ctx;

static TypeId resolve_literal_type(Resolver* r, AstNode* node, TypeId expected_type);
static TypeId resolve_identifier(Resolver* r, Module* module, AstNode* node);

static Symbol* resolve_func_call(Resolver* r, Module* module, ModuleId* sym_module, AstNode* node);
static Symbol* resolve_macro_call(Resolver* r, Module* module, ModuleId* sym_module, AstNode* node);

static SymbolId resolve_ident_symbol(Resolver* r, Module* module, AstIdent* ident, ModuleId* owning_module);

static u32 check_call_arity(u32 arg_count, u32 param_count, bool is_variadic);

TypeId resolve_expression(Resolver* r, Module* module, AstNodeId expr_id, TypeId expected_type) {
    AstNode* expr = &module -> ast.nodes[expr_id];

    TypeId type = TYPE_ID_NONE;

    switch (expr -> kind) {
        case AST_LITERAL:
            type = resolve_literal_type(r, expr, expected_type);
            break;

        case AST_IDENT:
            type = resolve_identifier(r, module, expr);
            break;

        case AST_BINOP:
            TypeId lhs = resolve_expression(r, module, expr -> as.binary_op.left, expected_type);
            TypeId rhs_expected = expected_type != TYPE_ID_NONE ? expected_type : lhs;
            TypeId rhs = resolve_expression(r, module, expr -> as.binary_op.right, rhs_expected);
            type = lhs == rhs ? lhs : TYPE_ID_NONE;
            break;

        case AST_UNARY:
            type = resolve_expression(r, module, expr -> as.unary_op.operand, expected_type);

            if (expr -> as.unary_op.op == TOK_AMP) {
                if (ast_is_kind(&module -> ast, expr -> as.unary_op.operand, AST_LITERAL)) {
                    printf("Cannot take reference of rvalues!\n");
                    type = TYPE_ID_NONE;
                    break;
                }

                type = type_table_register_pointer(type);
            } else if (expr -> as.unary_op.op == TOK_STAR) {
                if (!type_is_kind(type, TYPE_POINTER)) {
                    printf("Can only deref pointers!\n");
                    type = TYPE_ID_NONE;
                    break;
                }

                type = driver_ctx.type_table.entries[type].as.pointer.base;
            }
            break;

        case AST_FUNC_CALL: {
            ModuleId sym_module_id = MODULE_ID_NONE;
            Symbol* fn = resolve_func_call(r, module, &sym_module_id, expr);
            
            if (!fn) {
                printf("Function does not exist!\n");
                break;
            }

            u32 count = check_call_arity(
                expr -> as.func_call.arg_count,
                fn -> as.function.count,
                fn -> as.function.is_variadic
            );

            if (count == U32_MAX) {
                printf("Argument mismatch!\n");
                break;
            }

            SymbolTable* table = &MODULE_ID_LOOKUP_REF(sym_module_id) -> symbol_table;

            for (u32 i = 0; i < count; i++) {
                TypeId param_type = table -> symbols[fn -> as.function.params[i]].as.parameter.type;
                TypeId arg_type = resolve_expression(r, module, expr -> as.func_call.args[i], param_type);

                if (arg_type != param_type) {
                    printf("Argument (%u) does not match parameter (%u) index: %u!\n", arg_type, param_type, i);
                    break;
                }
            }

            type = fn -> as.function.return_type;
            break;
        }

        case AST_MACRO_CALL: {
            ModuleId sym_module_id = MODULE_ID_NONE;
            Symbol* macro = resolve_macro_call(r, module, &sym_module_id, expr);
            
            if (!macro) {
                printf("macro does not exist!\n");
                break;
            }

            u32 count = check_call_arity(
                expr -> as.func_call.arg_count,
                macro -> as.function.count,
                macro -> as.function.is_variadic
            );

            SymbolTable* table = &MODULE_ID_LOOKUP_REF(sym_module_id) -> symbol_table;

            for (u32 i = 0; i < count; i++) {
                TypeId param_type = table -> symbols[macro -> as.macro.params[i]].as.parameter.type;
                TypeId arg_type = resolve_expression(r, module, expr -> as.macro_call.args[i], param_type);

                if (arg_type != param_type) {
                    printf("Argument (%u) does not match parameter (%u) index: %u!\n", arg_type, param_type, i);
                    break;
                }
            }

            type = macro -> as.macro.return_type;
            break;
        }

        default:
            type = TYPE_ID_NONE;
            break;
    }
    
    debug_printf("resolve_expression(): Return %u\n", type);

    return type;
}

static TypeId resolve_literal_type(Resolver* r, AstNode* node, TypeId expected_type) {
    switch (node -> as.literal.kind) {
        case LITERAL_BOOL:
            return driver_ctx.type_table.builtins.type_bool;

        case LITERAL_INTEGER:
            if (expected_type != TYPE_ID_NONE && types_is_integer(expected_type)) {
                return expected_type;
            }
            return driver_ctx.type_table.builtins.type_i64;

        case LITERAL_FLOATING:
            if (expected_type != TYPE_ID_NONE && types_is_float(expected_type)) {
                return expected_type;
            }
            return driver_ctx.type_table.builtins.type_f64;

        case LITERAL_CHAR:
            return driver_ctx.type_table.builtins.type_char;

        case LITERAL_STRING: {
            TypeId id = type_table_lookup_pointer(driver_ctx.type_table.builtins.type_char);
            return id == TYPE_ID_NONE ? type_table_register_pointer(driver_ctx.type_table.builtins.type_char) : id;
        }

        case LITERAL_NULL: {
            TypeId id = type_table_lookup_pointer(driver_ctx.type_table.builtins.type_void);
            return id == TYPE_ID_NONE ? type_table_register_pointer(driver_ctx.type_table.builtins.type_void) : id;
        }

        default:
            return TYPE_ID_NONE;
    }
}

static TypeId resolve_identifier(Resolver* r, Module* module, AstNode* node) {
    AstIdent* ident = &node -> as.ident;
    ModuleId owning_module = MODULE_ID_NONE;

    SymbolId sym_id = resolve_ident_symbol(r, module, ident, &owning_module);
    if (sym_id == SYMBOL_ID_NONE) {
        return TYPE_ID_NONE;
    }

    Symbol* symbol = &MODULE_ID_LOOKUP_REF(owning_module) -> symbol_table.symbols[sym_id];
    return symbol_get_type_id(symbol);
}

static Symbol* resolve_func_call(Resolver* r, Module* module, ModuleId* symbol_module, AstNode* node) {
    AstFnCall* call = &node -> as.func_call;
    AstIdent* ident = &module -> ast.nodes[call -> ident].as.ident;

    SymbolId sym_id = resolve_ident_symbol(r, module, ident, symbol_module);
    if (sym_id == SYMBOL_ID_NONE) {
        return null;
    }

    return &MODULE_ID_LOOKUP_REF(*symbol_module) -> symbol_table.symbols[sym_id];
}

static Symbol* resolve_macro_call(Resolver* r, Module* module, ModuleId* symbol_module, AstNode* node) {
    AstMacroCall* call = &node -> as.macro_call;
    AstIdent* ident = &module -> ast.nodes[call -> ident].as.ident;

    SymbolId sym_id = resolve_ident_symbol(r, module, ident, symbol_module);
    if (sym_id == SYMBOL_ID_NONE) {
        return null;
    }

    return &MODULE_ID_LOOKUP_REF(*symbol_module) -> symbol_table.symbols[sym_id];
}

static SymbolId resolve_ident_symbol(Resolver* r, Module* module, AstIdent* ident, ModuleId* owning_module) {
    u32 hash = hash_fnv1a_u32(ident -> name_id);

    SymbolId sym_id = SYMBOL_ID_NONE;
    *owning_module = MODULE_ID_NONE;

    if (ident -> namespace_id == NAMESPACE_ID_NONE) {
        sym_id = scope_get_sym(r, ident -> name_id, hash);

        if (sym_id == SYMBOL_ID_NONE) {
            sym_id = table_get_sym(r, ident -> name_id);
        }

        *owning_module = module -> id;
    } else {
        bool found_import = false;

        for (u32 i = 0; i < module -> import_count; i++) {
            if (module -> imports[i] == ident -> namespace_id) {
                found_import = true;
                break;
            }
        }

        if (!found_import) {
            printf("Unknown namespace\n");
            return SYMBOL_ID_NONE;
        }

        *owning_module = module_lookup(ident -> namespace_id);

        Module* owner_module = MODULE_ID_LOOKUP_REF(*owning_module);

        SymbolTable* table = &owner_module -> symbol_table;
        SymbolTable* temp = r -> table;

        r -> table = table;
        sym_id = scope_get_sym_scope_id(r, 0, ident -> name_id, hash);
        r -> table = temp;
    }

    if (sym_id == SYMBOL_ID_NONE) {
        printf("Unknown identifier\n");
        return SYMBOL_ID_NONE;
    }

    if (!symbols_resolve_by_id(*owning_module, sym_id)) {
        printf("cannot resolve symbol!\n");
        return SYMBOL_ID_NONE;
    }

    ident -> symbol_ref = (SymbolRef) {
        .module_id = *owning_module,
        .symbol_id = sym_id
    };

    return sym_id;
}

static u32 check_call_arity(u32 arg_count, u32 param_count, bool is_variadic) {
    if (is_variadic) {
        return arg_count >= param_count - 1 ? param_count - 1 : U32_MAX;
    }

    return arg_count == param_count ? param_count : U32_MAX;
}
