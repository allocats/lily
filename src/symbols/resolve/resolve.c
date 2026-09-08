#include "ast/nodes/types.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "driver/types.h"
#include "files/files.h"
#include "ids.h"
#include "resolver_stack/stack.h"
#include "resolver_stack/types.h"
#include "symbols/register/register.h"
#include "symbols/resolve/resolve.h"
#include "symbols/resolve/types.h"
#include "symbols/scope/scope.h"
#include "symbols/symbols/symbols.h"
#include "symbols/symbols/types.h"
#include "symbols/table/table.h"
#include "token/types.h"
#include "types/entries/entries.h"
#include "types/entries/types.h"
#include "types/resolve/resolve.h"
#include "types/table/table.h"
#include "utils/macros.h"
#include "utils/types.h"

#include <assert.h>
#include <stdio.h>

extern DriverCtx driver;

static bool resolve_symbol_body(SymbolId id);

static bool resolve_function_signature(SymbolId id);
static bool resolve_function(Resolver* r, SymbolId id);
static bool resolve_struct(Resolver* r, SymbolId id);
static bool resolve_union(Resolver* r, SymbolId id);
static bool resolve_enum(Resolver* r, SymbolId id);
static bool resolve_variable(Resolver* r, SymbolId id);

static bool resolve_block(Resolver* r, AstNodeId id);
static bool resolve_defer_stmt(Resolver* r, AstNode* node);
static bool resolve_return_stmt(Resolver* r, AstNode* node);
static bool resolve_for_loop(Resolver* r, AstNode* node);
static bool resolve_while_loop(Resolver* r, AstNode* node);
static bool resolve_if_stmt(Resolver* r, AstNode* node);
static bool resolve_switch_stmt(Resolver* r, AstNode* node);
static bool resolve_variable_declaration(Resolver* r, AstNode* node);

static TypeId resolve_expression(ScopeId scope_id, FileId file_id, AstNodeId expr_id, TypeId expected_type);
static TypeId resolve_literal(AstNode* node, TypeId expected_type);
static TypeId resolve_identifier(ScopeId scope_id, AstNode* node, FileId file_id);
static TypeId resolve_unary_op(ScopeId scope_id, AstNode* node, FileId file_id, TypeId expected_type);
static TypeId resolve_binary_op(ScopeId scope_id, AstNode* node, FileId file_id, TypeId expected_type);
static TypeId resolve_function_call(ScopeId scope_id, AstNode* node, FileId file_id, TypeId expected_type);
static TypeId resolve_index(ScopeId scope_id, AstNode* node, FileId file_id, TypeId expected_type);
static TypeId resolve_member_access(AstNode* node, FileId file_id, TypeId expected_type);
static TypeId resolve_struct_literal(ScopeId scope_id, AstNode* node, FileId file_id, TypeId expected_type);

static SymbolId resolve_field(Resolver* r, File* file, AstNode* owner, AstNodeId id);
static SymbolId resolve_variant(Resolver* r, File* file, AstNodeId id, TypeId type_id, u32 index);

static BinaryOpKind binary_op_kind(TokenKind kind);
static bool is_expr_assignable(ScopeId scope_id, FileId file_id, AstNodeId expr_id);

static TypeId resolve_assignment(ScopeId scope_id, FileId file_id, AstNode* l, AstNode* r, TokenKind op);
static TypeId resolve_additive(ScopeId scope_id, FileId file_id, AstNode* lhs, AstNode* rhs, TypeId expected_type);
static TypeId resolve_multiplicative(ScopeId scope_id, FileId file_id, AstNode* lhs, AstNode* rhs, TypeId expected_type);
static TypeId resolve_bitwise(ScopeId scope_id, FileId file_id, AstNode* lhs, AstNode* rhs, TypeId expected_type);
static TypeId resolve_bitshift(ScopeId scope_id, FileId file_id, AstNode* lhs, AstNode* rhs, TypeId expected_type);
static TypeId resolve_comparison(ScopeId scope_id, FileId file_id, AstNode* lhs, AstNode* rhs, TypeId expected_type);
static TypeId resolve_logical(ScopeId scope_id, FileId file_id, AstNode* lhs, AstNode* rhs, TypeId expected_type);

static u32 get_arg_count(u32 params, u32 args, bool is_variadic);

inline void resolve_symbols(void) {
    arena_reset(&driver.scratch);

    u32 symbol_count = driver.symbol_table.symbol_count;

    for (u32 i = 0; i < symbol_count; i++) {
        resolve_symbol(i);
    }
}

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

SymbolId resolve_name_expr(File* file, AstNodeId node_id) {
    AstNode* node = &file -> ast.nodes[node_id];

    switch (node -> kind) {
        case AST_IDENTIFIER:
            return scope_lookup(file -> scope_id, node -> as.identifier.name);
            // return symbol_table_lookup(file -> scope_id, node -> as.identifier.name, file -> id);

        case AST_MEMBER_ACCESS: {
            SymbolId object_id = resolve_name_expr(file, node -> as.member_access.object);

            if (object_id == SYMBOL_ID_NONE) {
                return SYMBOL_ID_NONE;
            }

            Symbol* object = SYMBOL_ID_LOOKUP_REF(object_id);

            AstNode* member_node = &file -> ast.nodes[node -> as.member_access.member];
            assert(member_node -> kind == AST_IDENTIFIER);

            StringId member_name = member_node -> as.identifier.name;

            if (object -> kind == SYMBOL_IMPORT) {
                File* imported_file = file_lookup_id(object -> as.import_symbol.file_id);
                return scope_lookup(imported_file -> scope_id, member_name);
            }

            return SYMBOL_ID_NONE;
        }

        case AST_ERROR: {
            return SYMBOL_ID_NONE;
        }

        default:
            printf("Found: %s\n", AST_NODE_KIND_STRINGS[node -> kind]);
            UNREACHABLE("resolve_name_expr()");
    }
}

StringId resolve_name_id(File* file, AstNodeId node_id) {
    AstNode* node = &file -> ast.nodes[node_id];

    switch (node -> kind) {
        case AST_IDENTIFIER:
            return node -> as.identifier.name;

        case AST_MEMBER_ACCESS:
            return resolve_name_id(file, node -> as.member_access.member);

        case AST_ERROR:
            return STRING_ID_NONE;

        default:
            printf("Found: %s\n", AST_NODE_KIND_STRINGS[node -> kind]);
            UNREACHABLE("resolve_name_id()");
    }
}

static bool resolve_symbol_body(SymbolId id) {
    assert(id < driver.symbol_table.symbol_count);

    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(id);

    File* file = file_lookup_id(symbol -> file_id);

    bool result = false;

    Resolver r = {
        .file = file,
        .scope_id = file -> scope_id,
        .current_symbol = id,
        .in_loop_ctx = false
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

        case SYMBOL_VARIABLE:
            result = resolve_variable(&r, id);
            break;

        case SYMBOL_FUNCTION:
            result = resolve_function(&r, id);
            break;

        case SYMBOL_ERROR:
            result = false;
            break;

        default:
            printf("Found = %u\n", symbol -> kind);
            UNREACHABLE("resolve_symbol_body()");
    }

    return result;
}

static SymbolId resolve_field(Resolver* r, File* file, AstNode* owner, AstNodeId id) {
    AstNode* field_node = &file -> ast.nodes[id]; 

    StringId field_name = field_node -> as.field.name;

    SymbolId field_symbol_id = scope_lookup(r -> scope_id, field_name);

    if (field_symbol_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_redefined(
            file -> id,
            id,
            field_symbol_id,
            field_name
        );

        return SYMBOL_ID_NONE;
    }

    field_symbol_id = scope_intern_from_node(r -> scope_id, file -> id, field_name, id);

    TypeId field_type_id = resolve_type_expr(file -> id, field_node -> as.field.type_expr);

    if (field_type_id == TYPE_ID_NONE) {
        diagnostic_add_node_field(
            file -> id,
            DIAG_ERROR,
            owner -> tokens,
            field_node -> tokens,
            "field's type makes use of an undefined identifier",
            null
        );

        Symbol* field_symbol = SYMBOL_ID_LOOKUP_REF(field_symbol_id);

        field_symbol -> state = RESOLVE_ERROR;

        return SYMBOL_ID_NONE;
    }
 
    if (is_type_void(field_type_id)) {
        diagnostic_add_node_field(
            file -> id,
            DIAG_ERROR,
            owner -> tokens,
            field_node -> tokens,
            "field cannot be of type 'void'",
            "did you mean *void?"
        );

        Symbol* field_symbol = SYMBOL_ID_LOOKUP_REF(field_symbol_id);

        field_symbol -> state = RESOLVE_ERROR;

        return SYMBOL_ID_NONE;
    }

    Symbol* field_symbol = SYMBOL_ID_LOOKUP_REF(field_symbol_id);

    field_symbol -> as.field_symbol.type_id = field_type_id;
    field_symbol -> state = RESOLVE_RESOLVED;

    field_node -> resolved_type = field_type_id;

    return field_symbol_id;
}

static SymbolId resolve_variant(Resolver* r, File* file, AstNodeId id, TypeId type_id, u32 index) {
    AstNode* variant_node = &file -> ast.nodes[id];

    StringId variant_name_id = variant_node -> as.variant.name;

    SymbolId variant_symbol_id = scope_lookup(r -> scope_id, variant_name_id);

    if (variant_symbol_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_redefined(
            file -> id,
            id,
            variant_symbol_id,
            variant_name_id
        );

        return SYMBOL_ID_NONE;
    }
 
    variant_symbol_id = scope_intern_from_node(r -> scope_id, file -> id, variant_name_id, id);
 
    Symbol* variant_symbol = SYMBOL_ID_LOOKUP_REF(variant_symbol_id);
 
    variant_symbol -> as.variant_symbol.type_id = type_id;
 
    if (variant_node -> as.variant.value_expr == AST_NODE_ID_NONE) {
        variant_symbol -> as.variant_symbol.value = index;
    } else {
        // TODO: compile time interpreter
        // variant_symbol -> as.variant_symbol.value = compute_value();
        // if type != enum type 
    }

    variant_symbol -> state = RESOLVE_RESOLVED;

    variant_node -> resolved_type = type_id;
 
    return variant_symbol_id;
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

        SymbolId field_symbol_id = resolve_field(r, file, node, field_id);

        symbol -> as.struct_symbol.fields[i] = field_symbol_id;

        if (field_symbol_id == SYMBOL_ID_NONE) {

            result = false;

            continue;
        }

        Symbol* field_symbol = SYMBOL_ID_LOOKUP_REF(field_symbol_id);
        TypeEntry* field_type_entry = TYPE_ID_LOOKUP_REF(field_symbol -> as.field_symbol.type_id);

        u16 field_alignment = field_type_entry -> alignment;

        u32 misalignment = size % field_alignment;
        u32 padding = misalignment == 0 ? 0 : field_alignment - misalignment;

        size += padding;

        field_symbol -> as.field_symbol.offset = size;

        size += field_type_entry -> size;
        align = MAX(align, field_alignment);
    }

    scope_exit(r);

    TypeEntry* entry = TYPE_ID_LOOKUP_REF(symbol -> as.struct_symbol.resolved_type_id);

    entry -> as.struct_type.symbol_id = id;
    entry -> size = size;
    entry -> alignment = align;

    node -> resolved_type = symbol -> as.struct_symbol.resolved_type_id;

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

        SymbolId field_symbol_id = resolve_field(r, file, node, field_id);

        symbol -> as.union_symbol.fields[i] = field_symbol_id;

        if (field_symbol_id == SYMBOL_ID_NONE) {

            result = false;

            continue;
        }

        Symbol* field_symbol = SYMBOL_ID_LOOKUP_REF(field_symbol_id);
        TypeEntry* field_type_entry = TYPE_ID_LOOKUP_REF(field_symbol -> as.field_symbol.type_id);

        size  = MAX(size, field_type_entry -> size);
        align = MAX(align, field_type_entry -> alignment);
    }

    scope_exit(r);

    TypeEntry* entry = TYPE_ID_LOOKUP_REF(symbol -> as.union_symbol.resolved_type_id);

    entry -> as.union_type.symbol_id = id;
    entry -> size = size;
    entry -> alignment = align;

    node -> resolved_type = symbol -> as.union_symbol.resolved_type_id;

    return result;
}

static bool resolve_enum(Resolver* r, SymbolId id) {
    bool result = true;

    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(id);
    File* file = file_lookup_id(symbol -> file_id);
    AstNode* node = &file -> ast.nodes[symbol -> ast_node_id];

    TypeId enum_type_id = symbol -> as.enum_symbol.resolved_type_id;

    TypeId underlying_type_id = TYPE_ID_NONE;

    if (node -> as.enum_decl.type_expr != AST_NODE_ID_NONE) {
        underlying_type_id = resolve_type_expr(file -> id, node -> as.enum_decl.type_expr);

        if (underlying_type_id == TYPE_ID_NONE) {
            result = false;
        }
    } else {
        underlying_type_id = driver.type_table.builtins.type_i32;
    }

    u32 variant_count = node -> as.enum_decl.variants.count;

    scope_enter(r);

    for (u32 i = 0; i < variant_count; i++) {
        AstNodeId variant_id  = node -> as.enum_decl.variants.ids[i];

        SymbolId variant_symbol_id = resolve_variant(r, file, variant_id, enum_type_id, i);

        if (variant_symbol_id == SYMBOL_ID_NONE) {
            result = false;
        }

        symbol -> as.enum_symbol.variants[i] = variant_symbol_id;
    }

    scope_exit(r);

    TypeEntry* entry = TYPE_ID_LOOKUP_REF(enum_type_id);
    TypeEntry* underlying_type = TYPE_ID_LOOKUP_REF(underlying_type_id); 

    entry -> as.enum_type.symbol_id = id;
    entry -> as.enum_type.underlying_type = underlying_type_id;

    entry -> size = underlying_type -> size;
    entry -> alignment = underlying_type -> alignment;

    node -> resolved_type = symbol -> as.enum_symbol.resolved_type_id;

    return result;
}

static bool resolve_variable(Resolver* r, SymbolId id) {
    bool result = true;

    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(id);
    File* file = file_lookup_id(symbol -> file_id);
    AstNode* node = &file -> ast.nodes[symbol -> ast_node_id];

    TypeId type = resolve_type_expr(file -> id, node -> as.variable_decl.type_expr);

    if (type == TYPE_ID_NONE) {
        result = false;
    }

    if (node -> as.variable_decl.value_expr != AST_NODE_ID_NONE) {
        TypeId expr_type = resolve_expression(r -> scope_id, file -> id, node -> as.variable_decl.value_expr, type);

        if (expr_type == TYPE_ID_NONE) {
            result = false;
        } else {
            if (!are_types_compatible(type, expr_type)) {
                if (can_type_cast_to(type, expr_type)) {
                    diagnostic_add_try_cast_to(file -> id, node -> as.variable_decl.value_expr, type, expr_type);
                } else {
                    diagnostic_add_mismatched_types(file -> id, node -> id, type, expr_type);
                }

                result = false;
            }
        }
    }

    symbol -> as.variable_symbol.type_id = type;

    node -> resolved_type = type;

    return result;
}

static bool resolve_function_signature(SymbolId id) {
    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(id);

    if (symbol -> as.function_symbol.signature_state == RESOLVE_RESOLVED) return true;
    if (symbol -> as.function_symbol.signature_state == RESOLVE_ERROR)    return false;

    if (symbol -> as.function_symbol.signature_state == RESOLVE_RESOLVING) {
        diagnostic_add_generic(DIAG_ERROR, "cyclic function signature");

        symbol -> as.function_symbol.signature_state = RESOLVE_ERROR;

        return false;
    }

    symbol -> as.function_symbol.signature_state = RESOLVE_RESOLVING;

    bool result = true;

    File* file = file_lookup_id(symbol -> file_id);
    AstNode* node = &file -> ast.nodes[symbol -> ast_node_id];

    Resolver r = {
        .file = file,
        .scope_id = file -> scope_id,
    };

    TypeId return_type_id = resolve_type_expr(file -> id, node -> as.function_decl.return_type_expr);

    if (return_type_id == TYPE_ID_NONE) {
        result = false;
    }

    symbol -> as.function_symbol.return_type_id = return_type_id;

    ScopeId signature_scope_id = scope_enter(&r);

    symbol -> as.function_symbol.scope_id = signature_scope_id;

    u32 parameter_count = node -> as.function_decl.parameters.count;

    for (u32 i = 0; i < parameter_count; i++) {
        AstNodeId parameter_node_id = node -> as.function_decl.parameters.ids[i];
        AstNode* parameter_node = &file -> ast.nodes[parameter_node_id];

        StringId parameter_name = parameter_node -> as.parameter_decl.name;

        SymbolId parameter_symbol_id = symbol_table_lookup(signature_scope_id, parameter_name, file -> id);

        if (parameter_symbol_id != SYMBOL_ID_NONE) {
            diagnostic_add_symbol_redefined(
                file -> id,
                parameter_node_id,
                parameter_symbol_id,
                parameter_name
            );

            symbol -> as.function_symbol.parameters[i] = SYMBOL_ID_NONE;

            result = false;

            continue;
        }

        parameter_symbol_id = scope_intern_from_node(
            signature_scope_id,
            file -> id,
            parameter_name,
            parameter_node_id
        );

        Symbol* parameter_symbol = SYMBOL_ID_LOOKUP_REF(parameter_symbol_id);
        TypeId parameter_type_id = resolve_type_expr(file -> id, parameter_node -> as.parameter_decl.type_expr);

        if (parameter_type_id == TYPE_ID_NONE) {
            result = false;
        }

        parameter_symbol -> as.parameter_symbol.type_id = parameter_type_id;
        parameter_symbol -> state = RESOLVE_RESOLVED;

        parameter_node -> resolved_type = parameter_type_id;

        symbol -> as.function_symbol.parameters[i] = parameter_symbol_id;
    }

    symbol -> as.function_symbol.signature_state = result ? RESOLVE_RESOLVED : RESOLVE_ERROR;

    return result;
}

static bool resolve_function(Resolver* r, SymbolId id) {
    bool result = resolve_function_signature(id);

    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(id);
    File* file = file_lookup_id(symbol -> file_id);
    AstNode* node = &file -> ast.nodes[symbol -> ast_node_id];

    ScopeId caller_scope_id = r -> scope_id;

    r -> scope_id = symbol -> as.function_symbol.scope_id;

    if (!(node -> flags & AST_FLAGS_IS_EXTERNAL)) {
        resolve_block(r, node -> as.function_decl.block);
    } 

    r -> scope_id = caller_scope_id;

    return result;
}

static bool resolve_block(Resolver* r, AstNodeId id) {
    AstNode* node = &r -> file -> ast.nodes[id];

    u32 count = node -> as.block.statements.count;

    bool result = true;

    for (u32 i = 0; i < count; i++) {
        AstNodeId stmt_id = node -> as.block.statements.ids[i];
        AstNode* stmt_node = &r -> file -> ast.nodes[stmt_id];

        /*
         *
         *  TODO: Add return checker? ensure that all possible CFGs return
         *
         */

        switch (stmt_node -> kind) {
            case AST_DEFER_STMT:
                result = resolve_defer_stmt(r, stmt_node);
                break;

            case AST_RETURN_STMT:
                result = resolve_return_stmt(r, stmt_node);
                break;

            case AST_FOR_LOOP:
                r -> in_loop_ctx = true;
                result = resolve_for_loop(r, stmt_node);
                r -> in_loop_ctx = false;
                break;

            case AST_WHILE_LOOP:
                r -> in_loop_ctx = true;
                result = resolve_while_loop(r, stmt_node);
                r -> in_loop_ctx = false;
                break;

            case AST_IF_STMT:
                result = resolve_if_stmt(r, stmt_node);
                break;

            // TODO: improve this function, check comments at definition 
            case AST_SWITCH_STMT:
                result = resolve_switch_stmt(r, stmt_node);
                break;

            case AST_VARIABLE_DECL:
                result = resolve_variable_declaration(r, stmt_node);
                break;
            
            case AST_BLOCK:
                scope_enter(r);
                result = resolve_block(r, stmt_id);
                scope_exit(r);
                break;

            case AST_BREAK_STMT:
                if (r -> in_loop_ctx) {
                    result = true;
                } else {
                    result = false;

                    diagnostic_add_token_span(
                        r -> file -> id,
                        DIAG_ERROR,
                        stmt_node -> tokens,
                        "'break' not inside of a loop",
                        "'break' has to placed inside of a loop"
                    );
                }
                break;

            case AST_CONTINUE_STMT:
                if (r -> in_loop_ctx) {
                    result = true;
                } else {
                    result = false;

                    diagnostic_add_token_span(
                        r -> file -> id,
                        DIAG_ERROR,
                        stmt_node -> tokens,
                        "'continue' not inside of a loop",
                        "'continue' has to placed inside of a loop"
                    );
                }
                break;

            case AST_ERROR:
                result = false;
                break;

            default:
                if (resolve_expression(r -> scope_id, r -> file -> id, stmt_id, TYPE_ID_NONE) == TYPE_ID_NONE) {
                    result = false;
                }
                break;
        }
    }

    return result;
}

static inline bool resolve_defer_stmt(Resolver* r, AstNode* node) {
    TypeId type = resolve_expression(r -> scope_id, r -> file -> id, node -> as.defer_stmt.stmt, TYPE_ID_NONE);

    if (type == TYPE_ID_NONE) {
        return false;
    }

    return true;
}

static bool resolve_return_stmt(Resolver* r, AstNode* node) {
    TypeId ret_type = get_type_from_symbol(r -> current_symbol);

    if (ret_type == TYPE_ID_NONE) {
        return false;
    }

    AstNodeId expr_id = node -> as.return_stmt.expr;

    if (expr_id == AST_NODE_ID_NONE) {
        if (ret_type != driver.type_table.builtins.type_void) {
            diagnostic_add_token_span(
                r -> file -> id,
                DIAG_ERROR,
                node -> tokens,
                "non-void function expects to return a value",
                "add an expression to this return statement"
            );

            return false;
        }
        
        return true;
    }

    TypeId type = resolve_expression(r -> scope_id, r -> file -> id, expr_id, ret_type);

    if (type == TYPE_ID_NONE) {
        return false;
    }

    if (!are_types_compatible(ret_type, type)) {
        if (can_type_cast_to(ret_type, type)) {
            diagnostic_add_try_cast_to(r -> file -> id, node -> id, ret_type, type);
        } else {
            diagnostic_add_mismatched_types(r -> file -> id, node -> id, ret_type, type);
        }

        return false;
    }

    return true;
}

static bool resolve_for_loop(Resolver* r, AstNode* node) {
    TypeId bool_type = driver.type_table.builtins.type_bool;

    scope_enter(r);

    bool result = true;

    File* file = r -> file;

    AstNodeId init_id  = node -> as.for_loop.init;
    AstNode* init_node = &file -> ast.nodes[init_id]; 

    if (!resolve_variable_declaration(r, init_node)) {
        result = false;
    }

    StringId name_id = init_node -> as.variable_decl.name;

    SymbolId init_symbol_id = scope_lookup(r -> scope_id, name_id);

    TypeId init_type_id = TYPE_ID_NONE;

    if (init_symbol_id != SYMBOL_ID_NONE) {
        init_type_id = get_type_from_symbol(init_symbol_id);
    }

    if (init_type_id == TYPE_ID_NONE) {
        result = false;
    }

    TypeId condition_type_id = resolve_expression(r -> scope_id, file -> id, node -> as.for_loop.cond, bool_type);

    if (condition_type_id == TYPE_ID_NONE) {
        result = false;
    }

    if (condition_type_id != TYPE_ID_NONE && condition_type_id != bool_type) {
        AstNode* condition = &file -> ast.nodes[node -> as.for_loop.cond];

        diagnostic_add_token_span(
            file -> id,
            DIAG_ERROR,
            condition -> tokens,
            "expression does not evaluate to a bool",
            "condition must evaluate to a boolean"
        );
        
        result = false;
    }

    TypeId step_type_id = resolve_expression(r -> scope_id, file -> id, node -> as.for_loop.step, TYPE_ID_NONE);

    if (step_type_id == TYPE_ID_NONE) {
        result = false;
    }

    bool block_result = resolve_block(r, node -> as.for_loop.block);

    scope_exit(r);

    if (block_result == false || result == false) {
        result = false;
    }

    return result;
}

static bool resolve_while_loop(Resolver* r, AstNode* node) {
    TypeId bool_type = driver.type_table.builtins.type_bool;

    File* file = r -> file;

    bool result = true;

    TypeId condition_type_id = resolve_expression(r -> scope_id, file -> id, node -> as.while_loop.cond, bool_type);

    if (condition_type_id == TYPE_ID_NONE) {
        result = false;
    }

    if (condition_type_id != TYPE_ID_NONE && condition_type_id != bool_type) {
        AstNode* condition = &file -> ast.nodes[node -> as.for_loop.cond];

        diagnostic_add_token_span(
            file -> id,
            DIAG_ERROR,
            condition -> tokens,
            "expression does not evaluate to a bool",
            "condition must evaluate to a boolean"
        );
        
        result = false;
    }

    scope_enter(r);
    
    bool block_result = resolve_block(r, node -> as.while_loop.block);

    scope_exit(r);

    if (block_result == false || result == false) {
        result = false;
    }

    return result;
}

static bool resolve_if_stmt(Resolver* r, AstNode* node) {
    TypeId bool_type = driver.type_table.builtins.type_bool;

    File* file = r -> file;

    bool result = true;

    u32 branch_count = node -> as.if_stmt.branches.count;

    for (u32 i = 0; i < branch_count; i++) {
        AstNodeId branch_id  = node -> as.if_stmt.branches.ids[i];
        AstNode* branch_node = &file -> ast.nodes[branch_id];

        AstNodeId condition_id  = branch_node -> as.branch.condition;
        AstNode* condition_node = &file -> ast.nodes[condition_id];

        TypeId condition_type_id = resolve_expression(r -> scope_id, file -> id, condition_id, bool_type);

        if (condition_type_id == TYPE_ID_NONE) {
            result = false;
        }

        if (condition_type_id != TYPE_ID_NONE && condition_type_id != bool_type) {
            diagnostic_add_token_span(
                file -> id,
                DIAG_ERROR,
                condition_node -> tokens,
                "expression does not evaluate to a bool",
                "condition must evaluate to a boolean"
            );
            
            result = false;

            condition_node -> resolved_type = TYPE_ID_NONE;
        } else {
            condition_node -> resolved_type = condition_type_id;
        }

        scope_enter(r);
        
        bool block_result = resolve_block(r, branch_node -> as.branch.block);

        scope_exit(r);

        if (block_result == false || result == false) {
            result = false;
        }
    }

    if (node -> as.if_stmt.else_block != AST_NODE_ID_NONE) {
        scope_enter(r);
        
        bool block_result = resolve_block(r, node -> as.if_stmt.else_block);

        scope_exit(r);

        if (block_result == false || result == false) {
            result = false;
        }
    }

    return result;
}

// TODO: Improve the switch statement lacking checking that all 
// cases are covered and cannot check for duplicates
static bool resolve_switch_stmt(Resolver* r, AstNode* node) {
    File* file = r -> file;

    TypeId type_id = resolve_expression(r -> scope_id, file -> id, node -> as.switch_stmt.value, TYPE_ID_NONE);

    bool result = true;

    if (type_id == TYPE_ID_NONE) {
        result = false;
    }

    u32 case_count = node -> as.switch_stmt.cases.count;

    for (u32 i = 0; i < case_count; i++) {
        AstNodeId case_id  = node -> as.switch_stmt.cases.ids[i];
        AstNode* case_node = &file -> ast.nodes[case_id];

        if (case_node -> kind == AST_ERROR) {
            result = false;
        } else {
            assert(case_node -> kind == AST_SWITCH_CASE);

            u32 pattern_count = case_node -> as.switch_case.patterns.count;

            for (u32 n = 0; n < pattern_count; n++) {
                AstNodeId pattern_id  = case_node -> as.switch_case.patterns.ids[n];
                AstNode* pattern_node = &file -> ast.nodes[pattern_id]; 

                TypeId pattern_type_id = resolve_expression(r -> scope_id, file -> id, pattern_id, type_id);

                if (pattern_type_id == TYPE_ID_NONE) {
                    result = false;
                }

                if (pattern_type_id != TYPE_ID_NONE && !are_types_compatible(type_id, pattern_type_id)) {
                    if (can_type_cast_to(type_id, pattern_type_id)) {
                        diagnostic_add_try_cast_to(file -> id, pattern_id, type_id, pattern_type_id);
                    } else {
                        diagnostic_add_mismatched_types(file -> id, pattern_id, type_id, pattern_type_id);
                    }

                    result = false;
                }
            }
        }

        scope_enter(r);

        bool block_result = resolve_block(r, case_node -> as.switch_case.block);

        if (block_result == false) {
            result = false;
        }

        scope_exit(r);
    }

    if (node -> as.switch_stmt.default_case != AST_NODE_ID_NONE) {
        scope_enter(r);

        bool block_result = resolve_block(r, node -> as.switch_stmt.default_case);

        if (block_result == false) {
            result = false;
        }

        scope_exit(r);
    }

    return result;
}

static bool resolve_variable_declaration(Resolver* r, AstNode* node) {
    SymbolId id = register_variable(r, node);

    if (id == SYMBOL_ID_NONE) {
        return false;
    }

    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(id);

    symbol -> state = RESOLVE_RESOLVING;

    bool result = resolve_variable(r, id);

    symbol -> state = result ? RESOLVE_RESOLVED : RESOLVE_ERROR;

    return result;
}


static TypeId resolve_expression(ScopeId scope_id, FileId file_id, AstNodeId expr_id, TypeId expected_type) {
    File* file = file_lookup_id(file_id);
    AstNode* node = &file -> ast.nodes[expr_id];

    TypeId id = TYPE_ID_NONE;

    switch (node -> kind) {
        case AST_LITERAL:
            id = resolve_literal(node, expected_type);
            break;

        case AST_IDENTIFIER:
            id = resolve_identifier(scope_id, node, file_id);
            break;

        case AST_UNARY_OP:
            id = resolve_unary_op(scope_id, node, file_id, expected_type);
            break;

        case AST_BINARY_OP:
            id = resolve_binary_op(scope_id, node, file_id, expected_type);
            break;

        case AST_FUNCTION_CALL:
            id = resolve_function_call(scope_id, node, file_id, expected_type);
            break;

        case AST_INDEX:
            id = resolve_index(scope_id, node, file_id, expected_type);
            break;
        
        case AST_MEMBER_ACCESS:
            id = resolve_member_access(node, file_id, expected_type);
            break;

        case AST_STRUCT_LITERAL:
            id = resolve_struct_literal(scope_id, node, file_id, expected_type);
            break;

        case AST_ERROR:
            id = TYPE_ID_NONE;
            break;

        default:
            printf("Found: %s\n", AST_NODE_KIND_STRINGS[node -> kind]);
            UNREACHABLE("resolve_expression()");
    }

    node -> resolved_type = id;

    return id;
}

static TypeId resolve_literal(AstNode* node, TypeId expected_type) {
    TypeId id = TYPE_ID_NONE;

    switch (node -> as.literal.kind) {
        case LITERAL_BOOL:
            id = driver.type_table.builtins.type_bool;
            break;

        case LITERAL_CHAR:
            id = driver.type_table.builtins.type_i64;
            break;

        case LITERAL_FLOAT:
            if (is_type_float(expected_type)) {
                id = expected_type;
            } else {
                id = driver.type_table.builtins.type_f64;
            }
            break;

        case LITERAL_INTEGER:
            if (is_type_int(expected_type)) {
                id = expected_type;
            } else {
                id = driver.type_table.builtins.type_i64;
            }
            break;

        case LITERAL_STRING:
            id = type_table_intern_pointer(driver.type_table.builtins.type_u8);
            break;

        case LITERAL_NULL:
            if (is_type(expected_type, TYPE_POINTER)) {
                id = expected_type;
            } else  {
                id = type_table_intern_pointer(driver.type_table.builtins.type_void);
            }
            break;

        default:
            UNREACHABLE("resolve_literal()");
            break;
    }

    return id;
}

static TypeId resolve_identifier(ScopeId scope_id, AstNode* node, FileId file_id) {
    StringId name_id = node -> as.identifier.name;

    SymbolId symbol_id = symbol_table_lookup(scope_id, name_id, file_id);

    if (symbol_id == SYMBOL_ID_NONE) {
        diagnostic_add_symbol_does_not_exist(file_id, node -> id, name_id);
        return TYPE_ID_NONE;
    }

    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(symbol_id);

    switch (symbol -> kind) {
        case SYMBOL_FUNCTION:
            if (!resolve_function_signature(symbol_id)) {
                return TYPE_ID_NONE;
            }
            break;

        case SYMBOL_VARIABLE:
            if (symbol -> state == RESOLVE_RESOLVING) {
                diagnostic_add_token_span(
                    file_id,
                    DIAG_ERROR,
                    node -> tokens,
                    "variable used in its own initializer",
                    "variables cannot be used in their own initializer"
                );

                symbol -> state = RESOLVE_ERROR;

                return TYPE_ID_NONE;
            }

            if (symbol -> state == RESOLVE_ERROR) {
                return TYPE_ID_NONE;
            }

            if (symbol -> state == RESOLVE_UNRESOLVED) {
                if (!resolve_symbol(symbol_id)) {
                    return TYPE_ID_NONE;
                }
            }
            break;

        default:
            break;
    }

    return get_type_from_symbol(symbol_id);
}

static TypeId resolve_unary_op(ScopeId scope_id, AstNode* node, FileId file_id, TypeId expected_type) {
    File* file = file_lookup_id(file_id);

    AstNodeId operand_id  = node -> as.unary_op.operand;
    AstNode* operand_node = &file -> ast.nodes[operand_id];

    TokenKind op = node -> as.unary_op.op;

    TypeId operand_expected_type = expected_type;

    if (op == TOK_AMP) {
        if (is_type(expected_type, TYPE_POINTER)) {
            operand_expected_type = driver.type_table.entries[expected_type].as.pointer_type.base;
        } else {
            operand_expected_type = TYPE_ID_NONE;
        }
    } else if (op == TOK_STAR) {
        if (expected_type != TYPE_ID_NONE) {
            operand_expected_type = type_table_intern_pointer(expected_type);
        } else {
            operand_expected_type = TYPE_ID_NONE;
        }
    }

    TypeId id = resolve_expression(scope_id, file_id, operand_id, operand_expected_type);

    if (id == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    switch (op) {
        case TOK_AMP: {
            if (
                operand_node -> kind != AST_IDENTIFIER &&
                operand_node -> kind != AST_MEMBER_ACCESS &&
                operand_node -> kind != AST_INDEX
            ) {
                diagnostic_add_cannot_reference_rvalue(file_id, operand_id);
                return TYPE_ID_NONE;
            }

            return type_table_intern_pointer(id);
        }

        case TOK_STAR: {
            if (!is_type(id, TYPE_POINTER)) {
                diagnostic_add_cannot_dereference_non_pointer(file_id, operand_id);
                return TYPE_ID_NONE;
            }

            return driver.type_table.entries[id].as.pointer_type.base;
        }

        case TOK_MINUS: {
            if (!is_type_int(id) && !is_type_float(id)) {
                diagnostic_add_token_span(
                    file_id,
                    DIAG_ERROR,
                    operand_node -> tokens,
                    "invalid operand for unary +/-",
                    "expects a numeric expression i.e. i32"
                );

                return TYPE_ID_NONE;
            }

            if (is_type_unsigned_int(id)) {
                id += 5;

                assert(is_type_signed_int(id));
            }

            return id;
        }

        case TOK_PLUS: {
            if (!is_type_int(id) && !is_type_float(id)) {
                diagnostic_add_token_span(
                    file_id,
                    DIAG_ERROR,
                    operand_node -> tokens,
                    "invalid operand for unary +/-",
                    "expects a numeric expression i.e. i32"
                );

                return TYPE_ID_NONE;
            }

            return id;
        }

        case TOK_BANG: {
            if (id != driver.type_table.builtins.type_bool) {
                diagnostic_add_token_span(
                    file_id,
                    DIAG_ERROR,
                    operand_node -> tokens,
                    "invalid operand type for '!'",
                    "expects an expression of type bool"
                );

                return TYPE_ID_NONE;
            }

            return id;
        }

        case TOK_TILDE: {
            if (!is_type_int(id)) {
                diagnostic_add_token_span(
                    file_id,
                    DIAG_ERROR,
                    operand_node -> tokens,
                    "invalid operand type for '~'",
                    "negation requires an integer"
                );

                return TYPE_ID_NONE;
            }

            return id;
        }

        default:
            UNREACHABLE("resolve_unary_op()");
    }
}

static TypeId resolve_binary_op(ScopeId scope_id, AstNode* node, FileId file_id, TypeId expected_type) {
    File* file = file_lookup_id(file_id);

    AstNodeId lhs_id = node -> as.binary_op.left;
    AstNodeId rhs_id = node -> as.binary_op.right;

    AstNode* lhs = &file -> ast.nodes[lhs_id];
    AstNode* rhs = &file -> ast.nodes[rhs_id];

    TokenKind op = node -> as.binary_op.op;

    TypeId id = TYPE_ID_NONE;

    switch (binary_op_kind(op)) {
        case BINARY_OP_ASSIGN:
            id = resolve_assignment(scope_id, file_id, lhs, rhs, op);
            break;

        case BINARY_OP_ADDITIVE:
            id = resolve_additive(scope_id, file_id, lhs, rhs, expected_type);
            break;

        case BINARY_OP_MULTIPLICATIVE:
            id = resolve_multiplicative(scope_id, file_id, lhs, rhs, expected_type);
            break;

        case BINARY_OP_BITWISE:
            id = resolve_bitwise(scope_id, file_id, lhs, rhs, expected_type);
            break;

        case BINARY_OP_SHIFT:
            id = resolve_bitshift(scope_id, file_id, lhs, rhs, expected_type);
            break;

        case BINARY_OP_COMPARISON:
            id = resolve_comparison(scope_id, file_id, lhs, rhs, expected_type);
            break;

        case BINARY_OP_LOGICAL:
            id = resolve_logical(scope_id, file_id, lhs, rhs, expected_type);
            break;

        case BINARY_OP_ERROR:
            UNREACHABLE("resolve_binary_op() error case");
    }

    node -> resolved_type = id;

    return id;
}

static TypeId resolve_function_call(ScopeId scope_id, AstNode* node, FileId file_id, TypeId expected_type) {
    File* file = file_lookup_id(file_id);

    SymbolId symbol_id = resolve_name_expr(file, node -> as.function_call.identifier);

    if (symbol_id == SYMBOL_ID_NONE) {
        diagnostic_add_undefined_function_call(file_id, node -> id);

        return TYPE_ID_NONE;
    }

    if (!resolve_function_signature(symbol_id)) {
        return TYPE_ID_NONE;
    }

    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(symbol_id);

    bool is_variadic = symbol -> flags & AST_FLAGS_IS_VARIADIC;

    u32 arg_count = node -> as.function_call.arguments.count;
    u32 param_count = symbol -> as.function_symbol.parameter_count;
    u32 fixed_count = is_variadic ? param_count - 1 : param_count;

    u32 count = get_arg_count(param_count, arg_count, is_variadic);

    if (count == U32_MAX) {
        diagnostic_add_incorrect_call_arity(file_id, node -> tokens, arg_count, param_count);
        return TYPE_ID_NONE;
    }

    bool failed = false;

    for (u32 i = 0; i < fixed_count; i++) {
        SymbolId param_symbol_id = symbol -> as.function_symbol.parameters[i];
        Symbol* param_symbol = SYMBOL_ID_LOOKUP_REF(param_symbol_id);

        assert(param_symbol -> kind == SYMBOL_PARAMETER);

        TypeId param_type = param_symbol -> as.parameter_symbol.type_id;

        if (param_type == TYPE_ID_NONE) {
            failed = true;
            continue;
        }

        AstNodeId arg_expr_id = node -> as.function_call.arguments.ids[i];
        AstNode* arg_expr = &file -> ast.nodes[arg_expr_id];
        
        TypeId arg_type = resolve_expression(scope_id, file_id, arg_expr_id, param_type);

        if (arg_type == TYPE_ID_NONE) {
            failed = true;
            continue;
        }

        // are_types_compatible() checks for TYPE_ID_NONE 
        if (!are_types_compatible(param_type, arg_type)) {
            if (can_type_cast_to(param_type, arg_type)) {
                diagnostic_add_try_cast_to(file_id, arg_expr_id, param_type, arg_type);
            } else {
                diagnostic_add_mismatched_types(file_id, arg_expr_id, param_type, arg_type);
            }

            failed = true;
            continue;
        }

        arg_expr -> resolved_type = arg_type;
    }

    if (is_variadic) {
        for (u32 i = fixed_count; i < count; i++) {
            AstNodeId arg_expr_id = node -> as.function_call.arguments.ids[i];
            AstNode* arg_expr = &file -> ast.nodes[arg_expr_id];
            
            TypeId arg_type = resolve_expression(scope_id, file_id, arg_expr_id, TYPE_ID_NONE);

            if (arg_type == TYPE_ID_NONE) {
                failed = true;
                continue;
            }

            arg_expr -> resolved_type = arg_type;
        }
    }

    if (failed) {
        return TYPE_ID_NONE;
    }

    TypeId return_type = symbol -> as.function_symbol.return_type_id;

    if (return_type == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    if (expected_type != TYPE_ID_NONE && !are_types_compatible(expected_type, return_type)) {
        if (can_type_cast_to(expected_type, return_type)) {
            diagnostic_add_try_cast_to(file_id, node -> id, expected_type, return_type);
        } else {
            diagnostic_add_mismatched_types(file_id, node -> id, expected_type, return_type);
        }

        return TYPE_ID_NONE;
    }

    node -> resolved_type = return_type;

    return return_type;
}

// TODO/NOTE: need to finish ARRAYS in the TypeTable for this to function correctly
static TypeId resolve_index(ScopeId scope_id, AstNode* node, FileId file_id, TypeId expected_type) {
    File* file = file_lookup_id(file_id);

    TypeId type = resolve_expression(scope_id, file_id, node -> as.index.object, TYPE_ID_NONE);

    if (type == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    TypeId index_type = resolve_expression(
        scope_id,
        file_id,
        node -> as.index.index_expr,
        driver.type_table.builtins.type_usize
    );

    if (index_type == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    if (!is_type(type, TYPE_ARRAY) && !is_type(type, TYPE_SLICE)) {
        diagnostic_add_token_span(
            file_id,
            DIAG_ERROR,
            node -> tokens,
            "indexed object is not an array or slice",
            "can only index slices and arrays"
        );

        return TYPE_ID_NONE;
    }

    if (!is_type_unsigned_int(index_type)) {
        AstNode* index_expr = &file -> ast.nodes[node -> as.index.index_expr];

        if (can_type_cast_to(driver.type_table.builtins.type_usize, index_type)) {
            diagnostic_add_try_cast_to(file_id, index_expr -> id, driver.type_table.builtins.type_usize, index_type);
        } else {
            diagnostic_add_token_span(
                file_id,
                DIAG_ERROR,
                index_expr -> tokens,
                "indexes can only be performed with unsigned integers",
                "make this expression an unsigned integer"
            );
        }

        return TYPE_ID_NONE;
    }

    TypeEntry* object_type = TYPE_ID_LOOKUP_REF(type); 

    TypeId element_type = TYPE_ID_NONE;

    switch (object_type -> kind) {
        case TYPE_ARRAY:
            element_type = object_type -> as.array_type.element;
            break;

        case TYPE_SLICE:
            element_type = object_type -> as.slice_type.element;
            break;

        case TYPE_ERROR:
            element_type = TYPE_ID_NONE;
            break;

        default:
            UNREACHABLE("resolve_index()");
    }

    if (expected_type != TYPE_ID_NONE && !are_types_compatible(expected_type, element_type)) {
        if (can_type_cast_to(expected_type, element_type)) {
            diagnostic_add_try_cast_to(file_id, node -> id, expected_type, element_type);
        } else {
            diagnostic_add_mismatched_types(file_id, node -> id, expected_type, element_type);
        }

        return TYPE_ID_NONE;
    }

    return element_type;
}

static TypeId resolve_member_access(AstNode* node, FileId file_id, TypeId expected_type) {
    File* file = file_lookup_id(file_id);

    SymbolId object_symbol_id = resolve_name_expr(file, node -> as.member_access.object);

    if (object_symbol_id == SYMBOL_ID_NONE) {
        return TYPE_ID_NONE;
    }

    Symbol* object_symbol = SYMBOL_ID_LOOKUP_REF(object_symbol_id);

    if (object_symbol -> kind == SYMBOL_FUNCTION) {
        diagnostic_add_token_span(
            file_id,
            DIAG_ERROR,
            node -> tokens,
            "invalid member access",
            "cannot use member access on functions"
        );

        return TYPE_ID_NONE;
    }

    AstNode* member_node = &file -> ast.nodes[node -> as.member_access.member];
    assert(member_node -> kind == AST_IDENTIFIER);

    StringId member_name = member_node -> as.identifier.name;

    SymbolId member_symbol_id = SYMBOL_ID_NONE;

    if (object_symbol -> kind == SYMBOL_IMPORT) {
        File* imported_file = file_lookup_id(object_symbol -> as.import_symbol.file_id);

        member_symbol_id = scope_lookup(imported_file -> scope_id, member_name);
    } else {
        TypeId object_type_id = get_type_from_symbol(object_symbol_id);

        if (object_type_id == TYPE_ID_NONE) {
            return TYPE_ID_NONE;
        }

        // auto-deref
        if (is_type(object_type_id, TYPE_POINTER)) {
            object_type_id = driver.type_table.entries[object_type_id].as.pointer_type.base;
        }

        TypeEntry* object_type = TYPE_ID_LOOKUP_REF(object_type_id);

        SymbolId* member_symbols = null;
        u32 member_count = 0;

        switch (object_type -> kind) {
            case TYPE_STRUCT: {
                Symbol* struct_symbol = SYMBOL_ID_LOOKUP_REF(object_type -> symbol_id);
                member_symbols = struct_symbol -> as.struct_symbol.fields;
                member_count = struct_symbol -> as.struct_symbol.field_count;
                break;
            }

            case TYPE_UNION: {
                Symbol* union_symbol = SYMBOL_ID_LOOKUP_REF(object_type -> symbol_id);
                member_symbols = union_symbol -> as.union_symbol.fields;
                member_count = union_symbol -> as.union_symbol.field_count;
                break;
            }

            case TYPE_ENUM: {
                Symbol* enum_symbol = SYMBOL_ID_LOOKUP_REF(object_type -> symbol_id);
                member_symbols = enum_symbol -> as.enum_symbol.variants;
                member_count = enum_symbol -> as.enum_symbol.variant_count;
                break;
            }

            default: {
                diagnostic_add_token_span(
                    file_id,
                    DIAG_ERROR,
                    node -> tokens,
                    "invalid member access",
                    "member access requires a struct, union, or enum type"
                );

                return TYPE_ID_NONE;
            }
        }

        for (u32 i = 0; i < member_count; i++) {
            if (member_symbols[i] == SYMBOL_ID_NONE) {
                continue;
            }

            Symbol* entry = SYMBOL_ID_LOOKUP_REF(member_symbols[i]);

            if (entry -> name_id == member_name) {
                member_symbol_id = member_symbols[i];
                break;
            }
        }
    }

    if (member_symbol_id == SYMBOL_ID_NONE) {
        diagnostic_add_symbol_does_not_exist(file_id, node -> as.member_access.member, member_name);
        return TYPE_ID_NONE;
    }

    TypeId member_type = get_type_from_symbol(member_symbol_id);

    if (member_type == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    if (expected_type != TYPE_ID_NONE && !are_types_compatible(expected_type, member_type)) {
        if (can_type_cast_to(expected_type, member_type)) {
            diagnostic_add_try_cast_to(file_id, node -> id, expected_type, member_type);
        } else {
            diagnostic_add_mismatched_types(file_id, node -> id, expected_type, member_type);
        }

        return TYPE_ID_NONE;
    }

    node -> resolved_type = member_type;

    return member_type;
}

static TypeId resolve_struct_literal(ScopeId scope_id, AstNode* node, FileId file_id, TypeId expected_type) {
    File* file = file_lookup_id(file_id);

    TypeId type_id = resolve_expression(scope_id, file_id, node -> as.struct_literal.struct_type, expected_type);

    if (type_id == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    if (!is_type(type_id, TYPE_UNION) && !is_type(type_id, TYPE_STRUCT)) {
        AstNode* type_node = &file -> ast.nodes[node -> as.struct_literal.struct_type];

        diagnostic_add_token_span(
            file_id,
            DIAG_ERROR,
            type_node -> tokens,
            "invalid type for struct literal",
            "expected union or struct type"
        );

        return TYPE_ID_NONE;
    }

    TypeEntry* entry = TYPE_ID_LOOKUP_REF(type_id);

    // struct and union are same struct
    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(entry -> as.struct_type.symbol_id);

    u32 count = node -> as.struct_literal.inits.count;

    bool is_ok = true;

    SymbolId* checked_fields = arena_alloc(&driver.scratch, count * sizeof(SymbolId));
    u32 checked_count = 0;

    for (u32 i = 0; i < count; i++) {
        bool result = false;

        AstNodeId init_id  = node -> as.struct_literal.inits.ids[i];
        AstNode* init_node = &file -> ast.nodes[init_id];

        assert(init_node -> kind == AST_FIELD_INIT);

        AstNodeId field_id  = init_node -> as.field_init.field;
        AstNode* field_node = &file -> ast.nodes[field_id];

        StringId field_name_id = resolve_name_id(file, field_id);

        if (field_name_id == STRING_ID_NONE) {
            result = false;
        }

        AstNodeId value_id = init_node -> as.field_init.value;

        SymbolId field_symbol_id = SYMBOL_ID_NONE;

        for (u32 n = 0; n < symbol -> as.struct_symbol.field_count; n++) {
            SymbolId candidate_id = symbol -> as.struct_symbol.fields[n];
            Symbol* candidate = SYMBOL_ID_LOOKUP_REF(candidate_id);

            if (candidate -> name_id == field_name_id) {
                field_symbol_id = candidate_id;
                result = true;
                break;
            }
        }

        if (!result) {
            diagnostic_add_token_span(
                file_id,
                DIAG_ERROR,
                field_node -> tokens,
                "field does not exist",
                null
            );

            is_ok = false;
            continue;
        }

        for (u32 n = 0; n < checked_count; n++) {
            if (checked_fields[n] == field_symbol_id) {
                diagnostic_add_token_span(
                    file_id,
                    DIAG_ERROR,
                    field_node -> tokens,
                    "field is specified more than once",
                    null
                );

                is_ok = false;
                continue;
            }
        }

        checked_fields[checked_count++] = field_symbol_id;

        Symbol* field_symbol = SYMBOL_ID_LOOKUP_REF(field_symbol_id);

        TypeId field_type = field_symbol -> as.field_symbol.type_id;
        TypeId value_type = resolve_expression(scope_id, file_id, value_id, field_type);

        if (value_type == TYPE_ID_NONE) {
            is_ok = false;
            continue;
        }

        if (!are_types_compatible(field_type, value_type)) {
            if (can_type_cast_to(field_type, value_type)) {
                diagnostic_add_try_cast_to(file_id, init_id, field_type, value_type);
            } else {
                diagnostic_add_mismatched_types(file_id, init_id, field_type, value_type);
            }

            is_ok = false;
            continue;
        }

        init_node -> resolved_type = field_type;
    }

    arena_reset(&driver.scratch);

    if (!is_ok) {
        return TYPE_ID_NONE;
    }

    return type_id;
}


static BinaryOpKind binary_op_kind(TokenKind kind) {
    static BinaryOpKind binary_op_kind_lut[TOKEN_KIND_COUNT] = {
        [TOK_EQ]            = BINARY_OP_ASSIGN,

        [TOK_PLUS_EQ]       = BINARY_OP_ASSIGN,
        [TOK_MINUS_EQ]      = BINARY_OP_ASSIGN,
        [TOK_STAR_EQ]       = BINARY_OP_ASSIGN,
        [TOK_SLASH_EQ]      = BINARY_OP_ASSIGN,
        [TOK_PERCENT_EQ]    = BINARY_OP_ASSIGN,

        [TOK_AMP_EQ]        = BINARY_OP_ASSIGN,
        [TOK_PIPE_EQ]       = BINARY_OP_ASSIGN,
        [TOK_CARET_EQ]      = BINARY_OP_ASSIGN,
        [TOK_SHL_EQ]        = BINARY_OP_ASSIGN,
        [TOK_SHR_EQ]        = BINARY_OP_ASSIGN,

        [TOK_PLUS]          = BINARY_OP_ADDITIVE,
        [TOK_MINUS]         = BINARY_OP_ADDITIVE,

        [TOK_STAR]          = BINARY_OP_MULTIPLICATIVE,
        [TOK_SLASH]         = BINARY_OP_MULTIPLICATIVE,
        [TOK_PERCENT]       = BINARY_OP_MULTIPLICATIVE,

        [TOK_AMP]           = BINARY_OP_BITWISE,
        [TOK_PIPE]          = BINARY_OP_BITWISE,
        [TOK_CARET]         = BINARY_OP_BITWISE,

        [TOK_SHL]           = BINARY_OP_SHIFT,
        [TOK_SHR]           = BINARY_OP_SHIFT,

        [TOK_LT]            = BINARY_OP_COMPARISON,
        [TOK_LT_EQ]         = BINARY_OP_COMPARISON,
        [TOK_GT]            = BINARY_OP_COMPARISON,
        [TOK_GT_EQ]         = BINARY_OP_COMPARISON,
        [TOK_EQ_EQ]         = BINARY_OP_COMPARISON,
        [TOK_BANG_EQ]       = BINARY_OP_COMPARISON,

        [TOK_AMP_AMP]       = BINARY_OP_LOGICAL,
        [TOK_PIPE_PIPE]     = BINARY_OP_LOGICAL,
    };


    return binary_op_kind_lut[kind];
}

static bool is_expr_assignable(ScopeId scope_id, FileId file_id, AstNodeId expr_id) {
    File* file = file_lookup_id(file_id);
    AstNode* expr = &file -> ast.nodes[expr_id];

    switch (expr -> kind) {
        case AST_IDENTIFIER: {
            SymbolId id = symbol_table_lookup(scope_id, expr -> as.identifier.name, file_id);

            if (id == SYMBOL_ID_NONE) {
                diagnostic_add_symbol_does_not_exist(file_id, expr_id, expr -> as.identifier.name);
                return false;
            }

            Symbol* symbol = SYMBOL_ID_LOOKUP_REF(id);

            switch (symbol -> kind) {
                case SYMBOL_PARAMETER: 
                case SYMBOL_VARIABLE: 
                case SYMBOL_FIELD: 
                    break;

                default:
                    diagnostic_add_expression_is_not_assignable(file_id, expr_id);
                    return false;
            }

            if (symbol -> flags & AST_FLAGS_IS_CONSTANT) {
                diagnostic_add_cannot_reassign_constant(file_id, expr_id);
                return false;
            }

            return true;
        }

        case AST_MEMBER_ACCESS: {
            // checking that the object not only exists but isnt const
            if (!is_expr_assignable(scope_id, file_id, expr -> as.member_access.object)) {
                return false;
            }

            // checking the field for existence and constness
            if (!is_expr_assignable(scope_id, file_id, expr -> as.member_access.member)) {
                return false;
            }

            return true;
        }

        case AST_UNARY_OP: {
            if (expr -> as.unary_op.op != TOK_STAR) {
                diagnostic_add_expression_is_not_assignable(file_id, expr_id);
                return false;
            }

            AstNodeId operand_id = expr -> as.unary_op.operand;
            TypeId id = resolve_expression(scope_id, file_id, operand_id, TYPE_ID_NONE);

            if (id == TYPE_ID_NONE) {
                return false;
            }

            SymbolId symbol_id = resolve_name_expr(file, operand_id);
            Symbol* symbol = SYMBOL_ID_LOOKUP_REF(symbol_id);

            if (symbol -> flags & AST_FLAGS_IS_CONSTANT) {
                diagnostic_add_cannot_reassign_constant(file_id, expr_id);
                return false;
            }

            if (!is_type(id, TYPE_POINTER)) {
                diagnostic_add_cannot_dereference_non_pointer(file_id, operand_id);
                return false;
            }

            return true;
        }

        case AST_INDEX: {
            // if (expr -> flags & AST_FLAGS_IS_CONSTANT) {
            //     diagnostic_add_cannot_reassign_constant(file_id, expr_id);
            //     return false;
            // }

            if (!is_expr_assignable(scope_id, file_id, expr -> as.index.object)) {
                return false;
            }

            return true;
        }

        default: {
            diagnostic_add_expression_is_not_assignable(file_id, expr_id);
            return false;
        }
    }
}

static TypeId resolve_assignment(ScopeId scope_id, FileId file_id, AstNode* l, AstNode* r, TokenKind op) {
    if (!is_expr_assignable(scope_id, file_id, l -> id)) {
        return TYPE_ID_NONE;
    }

    TypeId lhs_type = resolve_expression(scope_id, file_id, l -> id, TYPE_ID_NONE);

    if (lhs_type == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    TypeId expected = lhs_type;

    if (is_type(lhs_type, TYPE_POINTER)) {
        if (op != TOK_PLUS_EQ && op != TOK_MINUS_EQ && op != TOK_EQ) {
            SpanU32 span = { .start = l -> tokens.start, .end = r -> tokens.end }; 

            diagnostic_add_token_span(
                file_id,
                DIAG_ERROR,
                span,
                "invalid assignment operator usage on pointer",
                "only '+=', '-=' and '=' are valid assignment operators for pointers"
            );

            return TYPE_ID_NONE;
        }

        if (op == TOK_EQ) {
            // redundant assignment, but clearly shows state behaviour SSA will remove this probably anyways
            expected = lhs_type;
        } else {
            expected = driver.type_table.builtins.type_usize;
        }
    }

    TypeId rhs_type = resolve_expression(scope_id, file_id, r -> id, expected);

    if (rhs_type == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    if (is_type(lhs_type, TYPE_POINTER)) {
        if (op == TOK_MINUS_EQ || op == TOK_PLUS_EQ) {
            if (is_type_unsigned_int(rhs_type)) {
                return lhs_type;
            }

            diagnostic_add_token_span(
                file_id,
                DIAG_ERROR,
                r -> tokens,
                "expression doesn't evaluate to an unsigned integer",
                "pointer arithmetic requires the arithmetic expression to evaluate to an unsigned integer"
            );

            return TYPE_ID_NONE;
        } else if (are_types_compatible(lhs_type, rhs_type)) {
            return lhs_type;
        } else {
            if (can_type_cast_to(lhs_type, rhs_type)) {
                diagnostic_add_try_cast_to(file_id, r -> id, lhs_type, rhs_type);
            } else {
                diagnostic_add_mismatched_types(file_id, r -> id, lhs_type, rhs_type);
            }

            return TYPE_ID_NONE;
        }
    } else if (are_types_compatible(lhs_type, rhs_type)) {
        return lhs_type;
    } else {
        if (can_type_cast_to(lhs_type, rhs_type)) {
            diagnostic_add_try_cast_to(file_id, r -> id, lhs_type, rhs_type);
        } else {
            diagnostic_add_mismatched_types(file_id, r -> id, lhs_type, rhs_type);
        }

        return TYPE_ID_NONE;
    }
}

static TypeId resolve_additive(ScopeId scope_id, FileId file_id, AstNode* lhs, AstNode* rhs, TypeId expected_type) {
    TypeId lhs_type = resolve_expression(scope_id, file_id, lhs -> id, expected_type);
    
    if (lhs_type == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    TypeId rhs_type = resolve_expression(scope_id, file_id, rhs -> id, lhs_type);

    if (rhs_type == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    if (!are_types_compatible(lhs_type, rhs_type)) {
        if (can_type_cast_to(lhs_type, rhs_type)) {
            diagnostic_add_try_cast_to(file_id, rhs -> id, lhs_type, rhs_type);
        } else {
            diagnostic_add_mismatched_types(file_id, rhs -> id, lhs_type, rhs_type);
        }

        return TYPE_ID_NONE;
    }

    return lhs_type;
}

static TypeId resolve_multiplicative(ScopeId scope_id, FileId file_id, AstNode* lhs, AstNode* rhs, TypeId expected_type) {
    TypeId lhs_type = resolve_expression(scope_id, file_id, lhs -> id, expected_type);
    
    if (lhs_type == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    TypeId rhs_type = resolve_expression(scope_id, file_id, rhs -> id, lhs_type);

    if (rhs_type == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    if (!are_types_compatible(lhs_type, rhs_type)) {
        if (can_type_cast_to(lhs_type, rhs_type)) {
            diagnostic_add_try_cast_to(file_id, rhs -> id, lhs_type, rhs_type);
        } else {
            diagnostic_add_mismatched_types(file_id, rhs -> id, lhs_type, rhs_type);
        }

        return TYPE_ID_NONE;
    }

    return lhs_type;
}

static TypeId resolve_bitwise(ScopeId scope_id, FileId file_id, AstNode* lhs, AstNode* rhs, TypeId expected_type) {
    TypeId lhs_type = resolve_expression(scope_id, file_id, lhs -> id, expected_type);

    if (lhs_type == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    if (!is_type_int(lhs_type)) {
        diagnostic_add_token_span(
            file_id,
            DIAG_ERROR,
            lhs -> tokens,
            "invalid bitwise target",
            "bitwise operations can only be performed on integers"
        );

        return TYPE_ID_NONE;
    }

    TypeId rhs_type = resolve_expression(scope_id, file_id, rhs -> id, TYPE_ID_NONE);

    if (rhs_type == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }
    
    if (!is_type_int(rhs_type)) {
        if (can_type_cast_to(lhs_type, rhs_type)) {
            // check this
            diagnostic_add_try_cast_to(file_id, rhs -> id, lhs_type, rhs_type);
        } else {
            diagnostic_add_token_span(
                file_id,
                DIAG_ERROR,
                rhs -> tokens,
                "invalid bitwise value",
                "bitwise operations requires an integer"
            );
        }

        return TYPE_ID_NONE;
    }

    return lhs_type;
}

static TypeId resolve_bitshift(ScopeId scope_id, FileId file_id, AstNode* lhs, AstNode* rhs, TypeId expected_type) {
    TypeId lhs_type = resolve_expression(scope_id, file_id, lhs -> id, expected_type);

    if (lhs_type == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    if (!is_type_int(lhs_type)) {
        diagnostic_add_token_span(
            file_id,
            DIAG_ERROR,
            lhs -> tokens,
            "invalid bitshift target",
            "shifting operations can only be performed on integers"
        );

        return TYPE_ID_NONE;
    }

    TypeId rhs_type = resolve_expression(scope_id, file_id, rhs -> id, TYPE_ID_NONE);

    if (rhs_type == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }
    
    if (!is_type_int(rhs_type)) {
        if (can_type_cast_to(lhs_type, rhs_type)) {
            // check this
            diagnostic_add_try_cast_to(file_id, rhs -> id, lhs_type, rhs_type);
        } else {
            diagnostic_add_token_span(
                file_id,
                DIAG_ERROR,
                rhs -> tokens,
                "invalid bitwise value",
                "bitwise operations requires an integer"
            );
        }

        return TYPE_ID_NONE;
    }

    return lhs_type;
}

static TypeId resolve_comparison(ScopeId scope_id, FileId file_id, AstNode* lhs, AstNode* rhs, TypeId expected_type) {
    TypeId lhs_type = resolve_expression(scope_id, file_id, lhs -> id, expected_type);
    
    if (lhs_type == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    TypeId rhs_type = resolve_expression(scope_id, file_id, rhs -> id, lhs_type);

    if (rhs_type == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    if (are_types_compatible(lhs_type, rhs_type)) {
        return driver.type_table.builtins.type_bool;
    }

    diagnostic_add_mismatched_types(file_id, rhs -> id, lhs_type, rhs_type);

    return TYPE_ID_NONE;
}

static TypeId resolve_logical(ScopeId scope_id, FileId file_id, AstNode* lhs, AstNode* rhs, TypeId expected_type) {
    TypeId bool_id = driver.type_table.builtins.type_bool;

    TypeId lhs_type = resolve_expression(scope_id, file_id, lhs -> id, expected_type);

    if (lhs_type == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    if (lhs_type != bool_id) {
        diagnostic_add_token_span(
            file_id,
            DIAG_ERROR,
            lhs -> tokens,
            "logical expression does not evaluate to a boolean",
            "this expression must resolve to a bool"    
        );

        return TYPE_ID_NONE;
    }

    TypeId rhs_type = resolve_expression(scope_id, file_id, rhs -> id, lhs_type);

    if (rhs_type == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    if (rhs_type != bool_id) {
        diagnostic_add_token_span(
            file_id,
            DIAG_ERROR,
            rhs -> tokens,
            "logical expression does not evaluate to a boolean",
            "this expression must resolve to a bool"    
        );

        return TYPE_ID_NONE;
    }

    return bool_id;
}

static u32 get_arg_count(u32 params, u32 args, bool is_variadic) {
    if (is_variadic) {
        return args >= params - 1 ? args : U32_MAX;
    }

    return args == params ? params : U32_MAX;
}
