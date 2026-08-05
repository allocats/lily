#include "ast/nodes/nodes.h"
#include "ast/nodes/types.h"
#include "ast/parser/expr/expr.h"
#include "ast/parser/stmts/stmts.h"
#include "ast/parser/types/ty.h"
#include "diagnostics/diagnostics.h"
#include "string_interner/interner.h"

AstNodeId parse_const_decl(Parser* p) {
    AstNodeId id  = parser_create_node(p, AST_CONST);
    AstNode* node = ast_node_get(&p -> module -> ast, id);

    Token* name = parser_peek(p);

    if (name -> kind != TOK_IDENT) {
        diagnostic_add_token(
            &driver_ctx.diagnostics,
            p -> id,
            DIAG_ERROR,
            name,
            DIAG_LOC_WHOLE_TOK,
            "expected identifier for constant name",
            "add a valid identifier here"
        );
        
        // error
        return parser_error_stmt(p, node);
    }

    node -> as.const_decl.name_id = STRING_INTERNER_LOOKUP_TOKEN(name);

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

    AstNodeId const_type_expr = parse_type_expr(p);
    
    if (const_type_expr == AST_NODE_ID_NONE) {
        return parser_error_stmt(p, node);
    }

    node -> as.const_decl.type_expr = const_type_expr;

    if (!parser_check(p, TOK_EQ)) {
        diagnostic_add_token(
            &driver_ctx.diagnostics,
            p -> id,
            DIAG_ERROR,
            parser_peek_previous(p),
            DIAG_LOC_END_OF_TOK,
            "expected '='",
            "const declaration requires expression at declaration"
        );
        
        // error
        return parser_error_stmt(p, node);
    }

    parser_advance(p);

    node -> as.const_decl.value_expr = parse_expression(p, 0);

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
