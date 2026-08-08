#include "ast/nodes/nodes.h"
#include "ast/nodes/types.h"
#include "ast/parser/expr/expr.h"
#include "ast/parser/stmts/stmts.h"
#include "ast/parser/types/ty.h"
#include "diagnostics/diagnostics.h"
#include "string_interner/interner.h"

AstNodeId parse_var_decl(Parser* p) {
    AstNodeId id  = parser_create_node(p, AST_LET, AST_FLAGS_NONE);
    AstNode* node = ast_node_get(&p -> module -> ast, id);

    Token* name = parser_peek(p);

    if (name -> kind != TOK_IDENT) {
        diagnostic_add_token(
            &driver_ctx.diagnostics,
            p -> id,
            DIAG_ERROR,
            name,
            DIAG_LOC_WHOLE_TOK,
            "expected identifier for variable name",
            "add a valid identifier here"
        );
        
        // error
        return parser_error_stmt(p, node);
    }

    node -> as.var_decl.name_id = STRING_INTERNER_LOOKUP_TOKEN(name);

    parser_advance(p);

    if (!parser_check(p, TOK_COLON)) {
        diagnostic_add_token(
            &driver_ctx.diagnostics,
            p -> id,
            DIAG_ERROR,
            name,
            DIAG_LOC_WHOLE_TOK,
            "expected ':'",
            "add a ':' here"
        );
        
        // error
        return parser_error_stmt(p, node);
    }

    parser_advance(p);

    AstNodeId var_type_expr = parse_type_expr(p);

    if (var_type_expr == AST_NODE_ID_NONE) {
        return parser_error_stmt(p, node);
    }

    node -> as.var_decl.type_expr = var_type_expr;

    Token* assign_tok = parser_peek(p);

    switch (assign_tok -> kind) {
        case TOK_EQ: {
            parser_advance(p);
            node -> as.var_decl.value_expr = parse_expression(p, 0);
        } break;

        case TOK_SEMI: {
            parser_advance(p);
            node -> as.var_decl.value_expr = AST_NODE_ID_NONE;
            return id;
        }

        default: {
            diagnostic_add_token(
                &driver_ctx.diagnostics,
                p -> id,
                DIAG_ERROR,
                parser_peek_previous(p),
                DIAG_LOC_END_OF_TOK,
                "expected ';' or '='",
                "add a ';' or '=' here"
            );

            // error
            return parser_error_stmt(p, node);
        }
    }

    if (!parser_check(p, TOK_SEMI)) {
        diagnostic_add_token(
            &driver_ctx.diagnostics,
            p -> id,
            DIAG_ERROR,
            parser_peek_previous(p),
            DIAG_LOC_END_OF_TOK,
            "expected ';'",
            "add a ';' here"
        );
        
        // error
        return parser_error_stmt(p, node);
    }

    parser_advance(p);

    node -> token_span.end = p -> cursor;

    return id;
}
