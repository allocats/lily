#include "semantics/semantics.h"
#include "ast/nodes/nodes.h"
#include "ast/nodes/types.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/diagnostics.h"
#include "driver/types.h"
#include "hash/hash.h"
#include "ids.h"
#include "modules/modules.h"
#include "resolver/types.h"
#include "symbols/resolve/resolve.h"
#include "symbols/symbols.h"
#include "symbols/types.h"
#include "token/token.h"
#include "token/types.h"
#include "types/ty.h"
#include "types/types.h"
#include "utils/debug.h"
#include "utils/types.h"

extern LilyCtx driver_ctx;

static TypeId resolve_literal_type(Resolver* r, AstNode* node, TypeId expected_type);
static TypeId resolve_identifier(Resolver* r, Module* module, AstNode* node);

static SymbolRef resolve_member_ref(TypeId base_type, StringId member);
static TypeId resolve_member_type(SymbolRef symbol_ref);

static Symbol* resolve_func_call(Resolver* r, Module* module, ModuleId* sym_module, AstNode* node);
static Symbol* resolve_macro_call(Resolver* r, Module* module, ModuleId* sym_module, AstNode* node);

static SymbolId resolve_ident_symbol(Resolver* r, Module* module, AstNode* node, ModuleId* owning_module);

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

        case AST_BINOP: {
            AstNodeId lhs_id = expr -> as.binary_op.left;
            AstNodeId rhs_id = expr -> as.binary_op.right;
            TokenKind op = expr -> as.binary_op.op;

            if (token_is_assignment(op)) {
                TypeId lhs_type = resolve_expression(r, module, lhs_id, TYPE_ID_NONE);

                bool is_ptr_arithmetic_assign = (
                    (op == TOK_PLUS_EQ || op == TOK_MINUS_EQ) && type_is_kind(lhs_type, TYPE_POINTER)
                );

                TypeId rhs_type = resolve_expression(
                    r,
                    module,
                    rhs_id,
                    is_ptr_arithmetic_assign ? driver_ctx.type_table.builtins.type_usize : lhs_type
                );

                if (is_ptr_arithmetic_assign) {
                    if (types_is_integer(rhs_type)) {
                        type = lhs_type;
                    } else {
                        diagnostic_add_pointer_compound_assign_requires_integer(
                            &driver_ctx.diagnostics,
                            module,
                            rhs_id
                        );
                        type = TYPE_ID_NONE;
                    }
                } else if (lhs_type != TYPE_ID_NONE && rhs_type == lhs_type) {
                    type = lhs_type;
                } else {
                    if (lhs_type != TYPE_ID_NONE) {
                        diagnostic_add_assignment_type_mismatch(
                            &driver_ctx.diagnostics,
                            module,
                            rhs_id,
                            lhs_type,
                            rhs_type
                        );
                    }

                    type = TYPE_ID_NONE;
                }

                if (ast_is_kind(&module -> ast, lhs_id, AST_IDENT) && lhs_type != TYPE_ID_NONE) {
                    SymbolRef lhs_ref = module -> ast.nodes[lhs_id].as.ident.symbol_ref;
                    Symbol* lhs_sym = &MODULE_ID_LOOKUP_REF(lhs_ref.module_id) -> symbol_table.symbols[lhs_ref.symbol_id];
                    if (lhs_sym -> kind == SYM_CONSTANT) {
                        diagnostic_add_cannot_reassign_constant(
                            &driver_ctx.diagnostics,
                            module,
                            expr_id // binary op
                        );
                    }
                }

                break;
            }

            TypeId lhs = resolve_expression(r, module, lhs_id, expected_type);

            bool is_additive = (op == TOK_PLUS || op == TOK_MINUS);

            if (is_additive && type_is_kind(lhs, TYPE_POINTER)) {
                TypeId rhs = resolve_expression(r, module, rhs_id, driver_ctx.type_table.builtins.type_usize);

                if (type_is_kind(rhs, TYPE_POINTER)) {
                    if (
                        op == TOK_MINUS &&
                        driver_ctx.type_table.entries[lhs].as.pointer.base ==
                        driver_ctx.type_table.entries[rhs].as.pointer.base
                    ) {
                        type = driver_ctx.type_table.builtins.type_usize;
                    } else {
                        diagnostic_add_pointer_subtraction_type_mismatch(
                            &driver_ctx.diagnostics,
                            module,
                            expr_id
                        );
                        type = TYPE_ID_NONE;
                    }
                } else if (types_is_integer(rhs)) {
                    type = lhs;
                } else {
                    diagnostic_add_pointer_arithmetic_requires_integer(
                        &driver_ctx.diagnostics,
                        module,
                        rhs_id
                    );
                    type = TYPE_ID_NONE;
                }

                break;
            }

            TypeId rhs_expected = expected_type != TYPE_ID_NONE ? expected_type : lhs;
            TypeId rhs = resolve_expression(r, module, rhs_id, rhs_expected);

            bool is_comparison = (
                op == TOK_EQ_EQ || op == TOK_BANG_EQ ||
                op == TOK_LT    || op == TOK_LT_EQ   ||
                op == TOK_GT    || op == TOK_GT_EQ
            );

            bool is_logical = (op == TOK_AMP_AMP || op == TOK_PIPE_PIPE);

            if (is_comparison) {
                if (lhs != TYPE_ID_NONE && lhs == rhs) {
                    type = driver_ctx.type_table.builtins.type_bool;
                } else {
                    diagnostic_add_comparison_type_mismatch(
                        &driver_ctx.diagnostics,
                        module,
                        expr_id
                    );
                    type = TYPE_ID_NONE;
                }
            } else if (is_logical) {
                TypeId bool_type = driver_ctx.type_table.builtins.type_bool;

                if (lhs == bool_type && rhs == bool_type) {
                    type = bool_type;
                } else {
                    diagnostic_add_logical_operator_requires_bool(
                        &driver_ctx.diagnostics,
                        module,
                        expr_id
                    );
                    type = TYPE_ID_NONE;
                }
            } else {
                type = (lhs != TYPE_ID_NONE && lhs == rhs) ? lhs : TYPE_ID_NONE;
            }

            break;
        }

        case AST_UNARY:
            type = resolve_expression(r, module, expr -> as.unary_op.operand, expected_type);

            if (expr -> as.unary_op.op == TOK_AMP) {
                if (ast_is_kind(&module -> ast, expr -> as.unary_op.operand, AST_LITERAL)) {
                    diagnostic_add_cannot_reference_rvalue(
                        &driver_ctx.diagnostics,
                        module,
                        expr -> as.unary_op.operand
                    );
                    type = TYPE_ID_NONE;
                    break;
                }

                type = type_table_register_pointer(type);
            } else if (expr -> as.unary_op.op == TOK_STAR) {
                if (!type_is_kind(type, TYPE_POINTER)) {
                    diagnostic_add_cannot_dereference_non_pointer(
                        &driver_ctx.diagnostics,
                        module,
                        expr_id
                    );
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
                break;
            }

            u32 count = check_call_arity(
                expr -> as.func_call.arg_count,
                fn -> as.function.count,
                fn -> as.function.is_variadic
            );

            if (count == U32_MAX) {
                diagnostic_add_call_argument_count_mismatch(
                    &driver_ctx.diagnostics,
                    module,
                    expr_id,
                    fn -> as.function.count,
                    expr -> as.func_call.arg_count,
                    fn -> as.function.is_variadic
                );
                break;
            }

            SymbolTable* table = &MODULE_ID_LOOKUP_REF(sym_module_id) -> symbol_table;

            for (u32 i = 0; i < count; i++) {
                TypeId param_type = table -> symbols[fn -> as.function.params[i]].as.parameter.type;
                TypeId arg_type = resolve_expression(r, module, expr -> as.func_call.args[i], param_type);

                if (arg_type != param_type) {
                    diagnostic_add_argument_type_mismatch(
                        &driver_ctx.diagnostics,
                        module,
                        expr -> as.func_call.args[i],
                        param_type,
                        arg_type,
                        i
                    );
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
                break;
            }

            u32 count = check_call_arity(
                expr -> as.func_call.arg_count,
                macro -> as.function.count,
                macro -> as.function.is_variadic
            );

            if (count == U32_MAX) {
                diagnostic_add_call_argument_count_mismatch(
                    &driver_ctx.diagnostics,
                    module,
                    expr_id,
                    macro -> as.function.count,
                    expr -> as.func_call.arg_count,
                    macro -> as.function.is_variadic
                );
                break;
            }

            SymbolTable* table = &MODULE_ID_LOOKUP_REF(sym_module_id) -> symbol_table;

            for (u32 i = 0; i < count; i++) {
                TypeId param_type = table -> symbols[macro -> as.macro.params[i]].as.parameter.type;
                TypeId arg_type = resolve_expression(r, module, expr -> as.macro_call.args[i], param_type);

                if (arg_type != param_type) {
                    diagnostic_add_argument_type_mismatch(
                        &driver_ctx.diagnostics,
                        module,
                        expr -> as.macro_call.args[i],
                        param_type,
                        arg_type,
                        i
                    );
                    break;
                }
            }

            type = macro -> as.macro.return_type;
            break;
        }

        case AST_INDEX:
            break;

        case AST_MEMBER_ACCESS: {
            TypeId ident_type = resolve_identifier(r, module, &module -> ast.nodes[expr -> as.member_access.ident]);

            if (ident_type == TYPE_ID_NONE) {
                type = TYPE_ID_NONE;
                break;
            }

            TypeId base_type;

            if (expr -> as.member_access.pointer_access) {
                if (!type_is_kind(ident_type, TYPE_POINTER)) {
                    diagnostic_add_arrow_access_on_non_pointer(
                        &driver_ctx.diagnostics,
                        module,
                        expr_id
                    );
                    type = TYPE_ID_NONE;
                    break;
                }

                base_type = driver_ctx.type_table.entries[ident_type].as.pointer.base;
            } else {
                if (type_is_kind(ident_type, TYPE_POINTER)) {
                    diagnostic_add_dot_access_on_pointer(
                        &driver_ctx.diagnostics,
                        module,
                        expr_id
                    );
                    type = TYPE_ID_NONE;
                    break;
                }

                base_type = ident_type;
            }

            if (
                !type_is_kind(base_type, TYPE_STRUCT) &&
                !type_is_kind(base_type, TYPE_UNION) &&
                !type_is_kind(base_type, TYPE_ENUM)
            ) {
                diagnostic_add_member_access_invalid_base_type(
                    &driver_ctx.diagnostics,
                    module,
                    expr_id,
                    base_type
                );
                type = TYPE_ID_NONE;
                break;
            }

            SymbolRef member_ref = resolve_member_ref(base_type, expr -> as.member_access.field_id);

            if (member_ref.symbol_id == SYMBOL_ID_NONE) {
                diagnostic_add_use_of_undeclared_member(
                    &driver_ctx.diagnostics,
                    module,
                    expr_id,
                    TYPE_ID_LOOKUP_REF(base_type) -> name,
                    expr -> as.member_access.field_id
                );
                
                type = TYPE_ID_NONE;
                break;
            }

            TypeId member_type = resolve_member_type(member_ref);
            if (member_type == TYPE_ID_NONE) {
                type = TYPE_ID_NONE;
                break;
            }

            type = member_type;
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
            if (expected_type != TYPE_ID_NONE && type_is_kind(expected_type, TYPE_POINTER)) {
                return expected_type;
            }
            
            return type_table_register_pointer(driver_ctx.type_table.builtins.type_void);
        }

        default:
            return TYPE_ID_NONE;
    }
}

static TypeId resolve_identifier(Resolver* r, Module* module, AstNode* node) {
    ModuleId owning_module = MODULE_ID_NONE;

    SymbolId sym_id = resolve_ident_symbol(r, module, node, &owning_module);
    if (sym_id == SYMBOL_ID_NONE) {
        return TYPE_ID_NONE;
    }

    Symbol* symbol = &MODULE_ID_LOOKUP_REF(owning_module) -> symbol_table.symbols[sym_id];
    return symbol_get_type_id(symbol);
}

static Symbol* resolve_func_call(Resolver* r, Module* module, ModuleId* symbol_module, AstNode* node) {
    AstFnCall* call = &node -> as.func_call;

    SymbolId sym_id = resolve_ident_symbol(r, module, &module -> ast.nodes[call -> ident], symbol_module);
    if (sym_id == SYMBOL_ID_NONE) {
        return null;
    }

    return &MODULE_ID_LOOKUP_REF(*symbol_module) -> symbol_table.symbols[sym_id];
}

static Symbol* resolve_macro_call(Resolver* r, Module* module, ModuleId* symbol_module, AstNode* node) {
    AstMacroCall* call = &node -> as.macro_call;

    SymbolId sym_id = resolve_ident_symbol(r, module, &module -> ast.nodes[call -> ident], symbol_module);
    if (sym_id == SYMBOL_ID_NONE) {
        return null;
    }

    return &MODULE_ID_LOOKUP_REF(*symbol_module) -> symbol_table.symbols[sym_id];
}

static SymbolId resolve_ident_symbol(Resolver* r, Module* module, AstNode* node, ModuleId* owning_module) {
    AstIdent* ident = &node -> as.ident;
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
            diagnostics_add_unknown_namespace(
                &driver_ctx.diagnostics,
                module,
                node -> id
            );

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
        diagnostic_add_use_of_undeclared_identifier(
            &driver_ctx.diagnostics,
            module,
            node
        );
        return SYMBOL_ID_NONE;
    }

    if (!symbols_resolve_by_id(*owning_module, sym_id)) {
        return SYMBOL_ID_NONE;
    }

    ident -> symbol_ref = (SymbolRef) {
        .module_id = *owning_module,
        .symbol_id = sym_id
    };

    return sym_id;
}

static SymbolRef resolve_member_ref(TypeId base_type, StringId member) {
    TypeEntry* entry = &driver_ctx.type_table.entries[base_type];

    if (entry -> declaration.module_id == MODULE_ID_NONE) {
        return (SymbolRef) { .module_id = MODULE_ID_NONE, .symbol_id = SYMBOL_ID_NONE };
    }

    ModuleId owning_module = entry -> declaration.module_id;
    SymbolTable* table = &MODULE_ID_LOOKUP_REF(owning_module) -> symbol_table;
    Symbol* object_sym = &table -> symbols[entry -> declaration.symbol_id];

    switch (object_sym -> kind) {
        case SYM_STRUCT:
            for (u32 i = 0; i < object_sym -> as.structs.count; i++) {
                SymbolId field_id = object_sym -> as.structs.fields[i];
                Symbol* field = &table -> symbols[field_id];

                if (field -> name == member) {
                    return (SymbolRef) { .module_id = owning_module, .symbol_id = field_id};
                }
            }
            break;

        case SYM_UNION:
            for (u32 i = 0; i < object_sym -> as.unions.count; i++) {
                SymbolId field_id = object_sym -> as.unions.fields[i];
                Symbol* field = &table -> symbols[field_id];

                if (field -> name == member) {
                    return (SymbolRef) { .module_id = owning_module, .symbol_id = field_id};
                }
            }
            break;

        case SYM_ENUM:
            for (u32 i = 0; i < object_sym -> as.enums.count; i++) {
                SymbolId variant_id = object_sym -> as.enums.variants[i];
                Symbol* variant = &table -> symbols[variant_id];

                if (variant -> name == member) {
                    return (SymbolRef) { .module_id = owning_module, .symbol_id = variant_id};
                }
            }
            break;

        default:
            break;
    }

    return (SymbolRef) {
        .module_id = MODULE_ID_NONE,
        .symbol_id = SYMBOL_ID_NONE
    };
}

static TypeId resolve_member_type(SymbolRef symbol_ref) {
    SymbolTable* table = &MODULE_ID_LOOKUP_REF(symbol_ref.module_id) -> symbol_table;
    Symbol* symbol = &table -> symbols[symbol_ref.symbol_id];

    TypeId result = TYPE_ID_NONE;

    switch (symbol -> kind) {
        case SYM_FIELD:
            result = symbol -> as.field.type;
            break;

        case SYM_VARIANT:
            result = symbol -> as.variant.type;
            break;

        default:
            result = TYPE_ID_NONE;
            break;
    }

    return result;
}

static u32 check_call_arity(u32 arg_count, u32 param_count, bool is_variadic) {
    if (is_variadic) {
        return arg_count >= param_count - 1 ? param_count - 1 : U32_MAX;
    }

    return arg_count == param_count ? param_count : U32_MAX;
}
