#include "ast/nodes/nodes.h"
#include "ast/nodes/types.h"
#include "ast/parser/expr/expr.h"
#include "ast/parser/parser.h"
#include "ast/parser/recovery/recovery.h"
#include "ast/parser/recovery/types.h"
#include "ast/parser/types.h"
#include "ast/parser/types/ty.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "ids.h"
#include "token/types.h"
#include "utils/types.h"

#include <assert.h>

static constexpr u8 type_modifier_max = U8_MAX;

typedef enum {
    TYPE_MOD_POINTER,
    TYPE_MOD_ARRAY
} TypeModifierKind;

typedef struct {
    TypeModifierKind kind;
    AstNodeId size_expr;
} TypeModifier;


AstNodeId parse_param_type_expr(Parser* p) {
    if (parser_check(p, TOK_ELLIPSIS)) {
        parser_advance(p);

        AstNodeId id  = parser_create_node(p, AST_TYPE_VARIADIC, AST_FLAGS_NONE, -1);
        AstNode* node = parser_get_node(p, id);

        node -> as.type_variadic.element_type = AST_NODE_ID_NONE;
        node -> tokens.end = p -> cursor - 1;

        return id;
    }

    return parse_type_expr(p);
}

AstNodeId parse_type_expr(Parser* p) {
    u32 start_index = p -> cursor;
    u32 flags = AST_FLAGS_NONE;

    // NOTE: caller must set its identifier to const as well

    if (parser_check(p, TOK_KW_CONST)) {
        parser_advance(p);

        flags |= AST_FLAGS_IS_CONSTANT;
    }

    // modifiers: [], *

    TypeModifier modifiers[type_modifier_max];
    u32 modifier_count = 0;

    while (p -> cursor < p -> token_count) {
        if (parser_check(p, TOK_STAR)) {
            if (modifier_count >= type_modifier_max) {
                Token token = parser_peek(p);

                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &token,
                    DIAG_LOC_START_OF_TOK,
                    "too many type modifiers",
                    "type expression has too many pointer/array modifiers"
                );

                AstNodeId id = parser_create_node(p, AST_ERROR, AST_FLAGS_NONE, -(p -> cursor - start_index));
                return parser_error(p, id, RECOVERY_TYPE);
            }

            modifiers[modifier_count++] = (TypeModifier) {
                .kind = TYPE_MOD_POINTER,
                .size_expr = AST_NODE_ID_NONE
            };

            parser_advance(p);
            continue;
        }

        if (parser_check(p, TOK_L_BRACKET)) {
            parser_advance(p);

            if (parser_check(p, TOK_R_BRACKET)) {
                modifiers[modifier_count++] = (TypeModifier) {
                    .kind = TYPE_MOD_ARRAY,
                    .size_expr = AST_NODE_ID_NONE
                };
                
                parser_advance(p);

                continue;
            }

            AstNodeId size_expr = parse_expression(p, 0);

            if (!parser_check(p, TOK_R_BRACKET)) {
                Token token = parser_peek_previous(p);

                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &token,
                    DIAG_LOC_END_OF_TOK,
                    "expected ']'",
                    "add a ']' here"
                );

                AstNodeId id = parser_create_node(p, AST_ERROR, AST_FLAGS_NONE, -(p -> cursor - start_index));
                return parser_error(p, id, RECOVERY_TYPE);
            }

            parser_advance(p);

            if (modifier_count >= type_modifier_max) {
                Token token = parser_peek_previous(p);

                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &token,
                    DIAG_LOC_END_OF_TOK,
                    "too many type modifiers",
                    "type expression has too many pointer/array modifiers"
                );

                AstNodeId id = parser_create_node(p, AST_ERROR, AST_FLAGS_NONE, -(p -> cursor - start_index));
                return parser_error(p, id, RECOVERY_TYPE);
            }

            modifiers[modifier_count++] = (TypeModifier) {
                .kind = TYPE_MOD_ARRAY,
                .size_expr = size_expr
            };

            continue;
        }

        break;
    }

    // base type

    AstNodeId base_id = AST_NODE_ID_NONE;

    // function type
    if (parser_check(p, TOK_KW_FN)) {
        u32 fn_start = p -> cursor;

        parser_advance(p);

        if (!parser_check(p, TOK_L_PAREN)) {
            Token token = parser_peek_previous(p);

            diagnostic_add_token(
                p -> current_file -> id,
                DIAG_ERROR,
                &token,
                DIAG_LOC_END_OF_TOK,
                "expected '(' after 'fn'",
                "function type parameters start with '('"
            );

            AstNodeId id = parser_create_node(p, AST_ERROR, AST_FLAGS_NONE, -(p -> cursor - start_index));
            return parser_error(p, id, RECOVERY_TYPE);
        }

        parser_advance(p);

        AstNodeId fn_id  = parser_create_node(p, AST_TYPE_FUNCTION, flags, 0);
        AstNode* fn_node = parser_get_node(p, fn_id);

        fn_node -> tokens.start = fn_start;

        while (!parser_check(p, TOK_R_PAREN)) {
            if (p -> cursor >= p -> token_count) {
                Token token = parser_peek_previous(p);

                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &token,
                    DIAG_LOC_END_OF_TOK,
                    "expected ')'",
                    "add ')' to close the function type"
                );

                AstNodeId id = parser_create_node(p, AST_ERROR, AST_FLAGS_NONE, -(p -> cursor - start_index));
                return parser_error(p, id, RECOVERY_TYPE);
            }

            AstNodeId param_type_id = parse_param_type_expr(p);

            if (IS_NODE_ERROR(p, param_type_id)) {
                AstNodeId id = parser_create_node(p, AST_ERROR, AST_FLAGS_NONE, -(p -> cursor - start_index));
                return parser_error(p, id, RECOVERY_TYPE);
            }

            ast_id_list_append(&fn_node -> as.type_function.parameters, &p -> current_file -> ast, param_type_id);

            if (parser_check(p, TOK_COMMA)) {
                parser_advance(p);
                continue;
            }

            if (!parser_check(p, TOK_R_PAREN)) {
                Token token = parser_peek_previous(p);

                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &token,
                    DIAG_LOC_END_OF_TOK,
                    "expected ',' or ')'",
                    "separate function parameter types with commas"
                );

                AstNodeId id = parser_create_node(p, AST_ERROR, AST_FLAGS_NONE, -(p -> cursor - start_index));
                return parser_error(p, id, RECOVERY_TYPE);
            }
        }

        parser_advance(p);

        // return type, mandatory

        if (!parser_check(p, TOK_ARROW)) {
            Token token = parser_peek_previous(p);

            diagnostic_add_token(
                p -> current_file -> id,
                DIAG_ERROR,
                &token,
                DIAG_LOC_END_OF_TOK,
                "expected ' -> '",
                "function types must specify a return type"
            );

            AstNodeId id = parser_create_node(p, AST_ERROR, AST_FLAGS_NONE, -(p -> cursor - start_index));
            return parser_error(p, id, RECOVERY_TYPE);
        }

        parser_advance(p);

        AstNodeId return_type_id = parse_type_expr(p);

        if (IS_NODE_ERROR(p, return_type_id)) {
            AstNodeId id = parser_create_node(p, AST_ERROR, AST_FLAGS_NONE, -(p -> cursor - start_index));
            return parser_error(p, id, RECOVERY_TYPE);
        }

        fn_node = parser_get_node(p, fn_id);

        fn_node -> as.type_function.return_type = return_type_id;
        fn_node -> tokens.end = p -> cursor - 1;

        base_id = fn_id;
    } else { 

        // regular type

        AstNodeId primary_id = parse_expression(p, 119);

        if (IS_NODE_ERROR(p, primary_id)) {
            AstNodeId id = parser_create_node(p, AST_ERROR, AST_FLAGS_NONE, -(p -> cursor - start_index));
            return parser_error(p, id, RECOVERY_TYPE);
        }

        AstNode* primary_node = parser_get_node(p, primary_id);

        if (primary_node -> kind != AST_IDENTIFIER && primary_node -> kind != AST_FUNCTION_CALL) {
            diagnostic_add_token_span(
                p -> current_file -> id,
                DIAG_ERROR,
                primary_node -> tokens,
                "expected type name, function type, or macro call",
                "type must be an identifier, function type, or function call"
            );

            AstNodeId id = parser_create_node(p, AST_ERROR, AST_FLAGS_NONE, -(p -> cursor - start_index));
            return parser_error(p, id, RECOVERY_TYPE);
        }


        AstNodeId type_id  = parser_create_node(p, AST_TYPE_BASE, flags, 0);
        AstNode* type_node = parser_get_node(p, type_id);

        type_node -> tokens.start = start_index;
        type_node -> as.type_base.expr = primary_id;

        base_id = type_id;
    }


    AstNodeId id = base_id;

    // create the final type with modifiers
    assert(modifier_count < U8_MAX);

    while (modifier_count > 0) {
        TypeModifier modifier = modifiers[--modifier_count];

        if (modifier.kind == TYPE_MOD_POINTER) {
            AstNodeId ptr_id  = parser_create_node(p, AST_TYPE_POINTER, AST_FLAGS_NONE, 0);
            AstNode* ptr_node = parser_get_node(p, ptr_id);

            ptr_node -> as.type_pointer.base_type = id;

            id = ptr_id;
        } else if (modifier.kind == TYPE_MOD_ARRAY) {
            AstNodeId arr_id  = parser_create_node(p, AST_TYPE_ARRAY, AST_FLAGS_NONE, 0);
            AstNode* arr_node = parser_get_node(p, arr_id);

            arr_node -> as.type_array.element = id;
            arr_node -> as.type_array.size_expr = modifier.size_expr;

            id = arr_id;
        }
    }

    AstNode* node = parser_get_node(p, id);

    node -> tokens.start = start_index;
    node -> tokens.end = p -> cursor - 1;

    return id;
}
