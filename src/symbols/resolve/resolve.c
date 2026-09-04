#include "ast/nodes/types.h"
#include "diagnostics/diagnostics.h"
#include "driver/types.h"
#include "files/files.h"
#include "ids.h"
#include "resolver_stack/stack.h"
#include "resolver_stack/types.h"
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

extern DriverCtx driver;

static bool resolve_symbol_body(SymbolId id);
static bool resolve_struct(Resolver* r, SymbolId id);
static bool resolve_union(Resolver* r, SymbolId id);
static bool resolve_enum(Resolver* r, SymbolId id);
static bool resolve_variable(Resolver* r, SymbolId id);
static bool resolve_function(Resolver* r, SymbolId id);

static TypeId resolve_expression(ScopeId scope_id, FileId file_id, AstNodeId expr_id, TypeId expected_type);
static TypeId resolve_literal(AstNode* node, TypeId expected_type);
static TypeId resolve_identfier(ScopeId scope_id, StringId name_id, FileId file_id);
static TypeId resolve_unary_op(ScopeId scope_id, AstNode* node, FileId file_id, TypeId expected_type);
static TypeId resolve_binary_op(ScopeId scope_id, AstNode* node, FileId file_id, TypeId expected_type);

static SymbolId resolve_field(Resolver* r, File* file, AstNode* owner, AstNodeId id);
static SymbolId resolve_variant(Resolver* r, File* file, AstNodeId id, TypeId type_id, u32 index);

static BinaryOpKind binary_op_kind(TokenKind kind);
static bool is_expr_assignable(ScopeId scope_id, FileId file_id, AstNodeId expr_id);

static TypeId resolve_assignment(ScopeId scope_id, FileId file_id, AstNode* l, AstNode* r, TokenKind op);

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

        default:
            UNREACHABLE("resolve_name_expr()");
    }
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

        case SYMBOL_VARIABLE:
            result = resolve_variable(&r, id);
            break;

        case SYMBOL_FUNCTION:
            result = resolve_function(&r, id);
            break;

        default:
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
            result = false;
        }

        symbol -> as.enum_symbol.resolved_type_id = type_id;
    } else {
        symbol -> as.enum_symbol.resolved_type_id = driver.type_table.builtins.type_i32;
    }

    TypeId resolved_type_id = symbol -> as.enum_symbol.resolved_type_id;

    u32 variant_count = node -> as.enum_decl.variants.count;

    scope_enter(r);

    for (u32 i = 0; i < variant_count; i++) {
        AstNodeId variant_id  = node -> as.enum_decl.variants.ids[i];

        SymbolId variant_symbol_id = resolve_variant(r, file, variant_id, resolved_type_id, i);

        if (variant_symbol_id == SYMBOL_ID_NONE) {
            result = false;
        }

        symbol -> as.enum_symbol.variants[i] = variant_symbol_id;
    }

    scope_exit(r);

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

        if (expr_type == TYPE_ID_NONE || expr_type != type) {
            diagnostic_add_mismatched_types(file -> id, node -> id, type, expr_type);

            result = false;
        }
    }

    symbol -> as.variable_symbol.type_id = type;

    node -> resolved_type = type;

    return result;
}

static bool resolve_function(Resolver* r, SymbolId id) {
    bool result = true;

    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(id);
    File* file = file_lookup_id(symbol -> file_id);
    AstNode* node = &file -> ast.nodes[symbol -> ast_node_id];

    TypeId return_type_id = resolve_type_expr(file -> id, node -> as.function_decl.return_type_expr);

    if (return_type_id == TYPE_ID_NONE) {
        result = false;
    }

    symbol -> as.function_symbol.return_type_id = return_type_id;

    scope_enter(r);

    u32 parameter_count = node -> as.function_decl.parameters.count;

    for (u32 i = 0; i < parameter_count; i++) {
        AstNodeId parameter_node_id = node -> as.function_decl.parameters.ids[i];
        AstNode* parameter_node = &file -> ast.nodes[parameter_node_id];

        StringId parameter_name = parameter_node -> as.parameter_decl.name;

        SymbolId parameter_symbol_id = symbol_table_lookup(r -> scope_id, parameter_name, file -> id);

        if (parameter_symbol_id != SYMBOL_ID_NONE) {
            diagnostic_add_symbol_redefined(
                file -> id,
                parameter_node_id,
                parameter_symbol_id,
                parameter_name
            );

            result = false;

            continue;
        }

        parameter_symbol_id = scope_intern_from_node(r -> scope_id, file -> id, parameter_name, parameter_node_id);

        Symbol* parameter_symbol = SYMBOL_ID_LOOKUP_REF(parameter_symbol_id);

        TypeId parameter_type_id = resolve_type_expr(file -> id, parameter_node -> as.parameter_decl.type_expr);

        if (parameter_type_id == TYPE_ID_NONE) {
            result = false;
        }

        parameter_symbol -> as.parameter_symbol.type_id = parameter_type_id;
    }

    // TODO: walk the function body if it has one

    if (!(node -> flags & AST_FLAGS_IS_EXTERNAL)) {
    } 

    scope_exit(r);

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
            id = resolve_identfier(scope_id, node -> as.identifier.name, file_id);
            break;

        case AST_UNARY_OP:
            id = resolve_unary_op(scope_id, node, file_id, expected_type);
            break;

        case AST_BINARY_OP:
            id = resolve_binary_op(scope_id, node, file_id, expected_type);
            break;

        case AST_FUNCTION_CALL:
            break;

        case AST_INDEX:
            break;
        
        case AST_MEMBER_ACCESS:
            break;

        case AST_STRUCT_LITERAL:
            break;

        default:
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

static TypeId resolve_identfier(ScopeId scope_id, StringId name_id, FileId file_id) {
    SymbolId symbol = symbol_table_lookup(scope_id, name_id, file_id);

    TypeId id = TYPE_ID_NONE;

    if (symbol == TYPE_ID_NONE) {
        id = TYPE_ID_NONE;
    } else {
        id = get_type_from_symbol(symbol);
    }

    return id;
}

static TypeId resolve_unary_op(ScopeId scope_id, AstNode* node, FileId file_id, TypeId expected_type) {
    File* file = file_lookup_id(file_id);

    AstNodeId operand_id  = node -> as.unary_op.operand;
    AstNode* operand_node = &file -> ast.nodes[operand_id];

    TypeId id = resolve_expression(scope_id, file_id, operand_id, expected_type);

    if (id == TYPE_ID_NONE) {
        return id;
    }

    if (node -> as.unary_op.op == TOK_AMP) {
        if (
            operand_node -> kind != AST_IDENTIFIER && 
            operand_node -> kind != AST_MEMBER_ACCESS && 
            operand_node -> kind != AST_INDEX
        ) {
            diagnostic_add_cannot_reference_rvalue(file_id, operand_id);
        } else {
            id = type_table_intern_pointer(id);
        }
    } else if (node -> as.unary_op.op == TOK_STAR) {
        if (is_type(id, TYPE_POINTER)) {
            id = driver.type_table.entries[id].as.pointer_type.base;
        } else {
            diagnostic_add_cannot_dereference_non_pointer(file_id, operand_id);
        }
    }

    return id;
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
            // id = resolve_additive();
            break;

        case BINARY_OP_MULTIPLICATIVE:
            // id = resolve_multiplicative();
            break;

        case BINARY_OP_BITWISE:
            // id = resolve_bitwise();
            break;

        case BINARY_OP_SHIFT:
            // id = resolve_bitshift();
            break;

        case BINARY_OP_COMPARISON:
            // id = resolve_comparison();
            break;

        case BINARY_OP_LOGICAL:
            // id = resolve_logical();
            break;

        case BINARY_OP_ERROR:
            UNREACHABLE("resolve_binary_op() error case");
    }

    node -> resolved_type = id;

    return id;
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
        [TOK_TILDE]         = BINARY_OP_BITWISE,

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

            if (expr -> flags & AST_FLAGS_IS_CONSTANT) {
                diagnostic_add_cannot_reassign_constant(file_id, expr_id);
                return false;
            }

            return true;
        }

        case AST_MEMBER_ACCESS: {
            // TODO: resolve member

            if (!is_expr_assignable(scope_id, file_id, expr -> as.member_access.object)) {
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

            if (id == TYPE_ID_NONE || !is_type(id, TYPE_POINTER)) {
                diagnostic_add_cannot_dereference_non_pointer(file_id, operand_id);
                return false;
            }

            return true;
        }

        case AST_INDEX: {
            if (expr -> flags & AST_FLAGS_IS_CONSTANT) {
                diagnostic_add_cannot_reassign_constant(file_id, expr_id);
                return false;
            }

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
            diagnostic_add_mismatched_types(file_id, r -> id, lhs_type, rhs_type);
            return TYPE_ID_NONE;
        }
    } else if (are_types_compatible(lhs_type, rhs_type)) {
        return lhs_type;
    } else {
        diagnostic_add_mismatched_types(file_id, r -> id, lhs_type, rhs_type);
        return TYPE_ID_NONE;
    }
}
