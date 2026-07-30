#include "ast/nodes/nodes.h"
#include "ast/nodes/types.h"
#include "ast/parser/parser.h"
#include "ast/parser/expr/expr.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "string_interner/interner.h"
#include "token/types.h"

// TODO: Make this recursive? Need to explore that idea
AstNodeId parse_type_expr(Parser* p) {
    AstNodeId id  = parser_create_node(p, AST_TYPE_BASE);
    AstNode* node = ast_node_get(&p -> module -> ast, id);

    Token* base_type_token = parser_advance(p);

    if (base_type_token -> kind != TOK_IDENT) {
        diagnostic_add_token(
            &driver_ctx.diagnostics,
            p -> id,
            DIAG_ERROR,
            base_type_token,
            DIAG_LOC_WHOLE_TOK,
            "expected identifier for type",
            "add a valid identifier here"
        );

        return AST_NODE_ID_NONE;
    }

    node -> as.type_base_expr.name = string_intern_str8(base_type_token -> lexeme);

    while (parser_check(p, TOK_STAR)) {
        AstNodeId pointer_id = parser_create_node(p, AST_TYPE_POINTER);
        AstNode* pointer_node = ast_node_get(&p -> module -> ast, pointer_id);

        pointer_node -> as.type_pointer_expr.base_type = id;

        id = pointer_id;

        parser_advance(p);
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

        AstNodeId array_id = parser_create_node(p, AST_TYPE_ARRAY);
        AstNode* array_node = ast_node_get(&p -> module -> ast, array_id);

        array_node -> as.type_array_expr.element = id;
        array_node -> as.type_array_expr.size_expr = size_expr;

        id = array_id;
    }

    return id;
}
