#include "ast/nodes/nodes.h"
#include "ast/nodes/types.h"
#include "ast/parser/expr/expr.h"
#include "ast/parser/parser.h"
#include "ast/parser/types/ty.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "token/types.h"

AstNodeId parse_param_type(Parser* p) {
    if (parser_check(p, TOK_ELLIPSIS)) {
        parser_advance(p);

        AstNodeId id  = parser_create_node(p, AST_TYPE_VARIADIC);
        AstNode* node = ast_node_get(&p -> module -> ast, id);

        node -> as.type_variadic_expr.element_type = AST_NODE_ID_NONE;

        return id;
    }

    return parse_type_expr(p);
}

AstNodeId parse_type_expr(Parser* p) {
    u32 start_index = p -> cursor;
    bool is_const = false;

    if (parser_check(p, TOK_CONST)) {
        parser_advance(p);
        is_const = true;
    }

    AstNodeId primary = parse_expression(p, 140);

    if (primary == AST_NODE_ID_NONE) {
        return AST_NODE_ID_NONE;
    }

    AstNode* primary_node = ast_node_get(&p -> module -> ast, primary);

    if (primary_node -> kind != AST_IDENT && primary_node -> kind != AST_MACRO_CALL) {
        diagnostic_add_token(
            &driver_ctx.diagnostics,
            p -> id,
            DIAG_ERROR,
            primary_node -> source_token,
            DIAG_LOC_WHOLE_TOK,
            "expected type name or macro call",
            "type must be an identifier or #macro(...)"
        );

        return AST_NODE_ID_NONE;
    }

    AstNodeId id  = parser_create_node(p, AST_TYPE_BASE);
    AstNode* node = ast_node_get(&p -> module -> ast, id);

    if (is_const) {
        node -> flags |= AST_FLAGS_IS_CONST;
    }

    node -> token_span.start = start_index;
    node -> as.type_base_expr.ident = primary;

    while (p -> cursor < p -> token_count) {
        if (parser_check(p, TOK_STAR)) {
            AstNodeId ptr_id = parser_create_node(p, AST_TYPE_POINTER);
            AstNode* ptr_node = ast_node_get(&p -> module -> ast, ptr_id);

            ptr_node -> as.type_pointer_expr.base_type = id;

            id = ptr_id;

            parser_advance(p);

            continue;
        }

        if (parser_check(p, TOK_LBRACKET)) {
            parser_advance(p);

            if (parser_check(p, TOK_RBRACKET)) {
                diagnostic_add_token(
                    &driver_ctx.diagnostics,
                    p -> id,
                    DIAG_ERROR,
                    parser_peek_previous(p),
                    DIAG_LOC_END_OF_TOK,
                    "sadly no vectors just yet",
                    "TODO: Add slices/vectors?"
                );

                return AST_NODE_ID_NONE;
            }

            AstNodeId size_expr = parse_expression(p, 0);

            if (!parser_check(p, TOK_RBRACKET)) {
                diagnostic_add_token(
                    &driver_ctx.diagnostics,
                    p -> id,
                    DIAG_ERROR,
                    parser_peek_previous(p),
                    DIAG_LOC_END_OF_TOK,
                    "expected ']'",
                    "add a ']' here"
                );

                return AST_NODE_ID_NONE;
            }

            parser_advance(p);

            AstNodeId arr_id = parser_create_node(p, AST_TYPE_ARRAY);
            AstNode* arr_node = ast_node_get(&p -> module -> ast, arr_id);

            arr_node -> as.type_array_expr.element = id;
            arr_node -> as.type_array_expr.size_expr = size_expr;

            id = arr_id;
            continue;
        }

        break;
    }

    node -> token_span.end = p -> cursor;

    return id;
}
