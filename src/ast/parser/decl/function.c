#include "ast/nodes/nodes.h"
#include "ast/nodes/types.h"
#include "ast/parser/parser.h"
#include "ast/parser/decl/decl.h"
#include "ast/parser/stmts/stmts.h"
#include "ast/parser/types/ty.h"
#include "diagnostics/diagnostics.h"
#include "string_interner/interner.h"

AstNodeId parse_function_decl(Parser* p) {
    AstNodeId id  = parser_create_node(p, AST_FUNCTION);
    AstNode* node = ast_node_get(&p -> module -> ast, id);

    node -> as.func_decl.params = arena_alloc(&p -> module -> ast.arena, sizeof(AstNodeId) * 8);
    node -> as.func_decl.param_capacity = 8; 
    node -> as.func_decl.param_count = 0;

    Token* name = parser_peek(p);

    if (name -> kind != TOK_IDENT) {
        diagnostic_add_token(
            &driver_ctx.diagnostics,
            p -> id,
            DIAG_ERROR,
            name,
            DIAG_LOC_WHOLE_TOK,
            "expected identifier",
            "add a valid identifier here for the function name" 
        );

        return parser_error_decl(p, node);
    }

    node -> as.func_decl.name_id = STRING_INTERNER_LOOKUP_TOKEN(name);

    parser_advance(p);

    if (!parser_check(p, TOK_LPAREN)) {
        diagnostic_add_token(
            &driver_ctx.diagnostics,
            p -> id,
            DIAG_ERROR,
            name,
            DIAG_LOC_END_OF_TOK,
            "expected '('",
            "add a '(' here" 
        );

        return parser_error_decl(p, node);
    }
    
    parser_advance(p);

    while (p -> cursor < p -> token_count) {
        if (parser_check(p, TOK_RPAREN)) {
            parser_advance(p);
            break;
        }

        Token* param_tok = parser_peek(p);

        if (param_tok -> kind != TOK_IDENT) {
            diagnostic_add_token(
                &driver_ctx.diagnostics,
                p -> id,
                DIAG_ERROR,
                param_tok,
                DIAG_LOC_WHOLE_TOK,
                "expected identifier",
                "add a valid identifier here for the parameter name"
            );

            return parser_error_decl(p, node);
        }

        AstNodeId param_id  = parser_create_node(p, AST_PARAM);
        AstNode* param_node = ast_node_get(&p -> module -> ast, param_id);

        param_node -> as.param_decl.name_id = STRING_INTERNER_LOOKUP_TOKEN(param_tok);

        parser_advance(p);

        if (!parser_check(p, TOK_COLON)) {
            diagnostic_add_token(
                &driver_ctx.diagnostics,
                p -> id,
                DIAG_ERROR,
                param_tok,
                DIAG_LOC_END_OF_TOK,
                "expected ':'",
                "add a ':' here"
            );

            return parser_error_decl(p, node);
        }

        parser_advance(p);

        param_node -> as.param_decl.type_expr = parse_type_expr(p);

        node -> as.func_decl.params[node -> as.func_decl.param_count++] = param_id;

        if (parser_check(p, TOK_RPAREN)) {
            parser_advance(p);
            break;
        }

        if (parser_check(p, TOK_COMMA)) {
            parser_advance(p);
            continue;
        }

        diagnostic_add_token(
            &driver_ctx.diagnostics,
            p -> id,
            DIAG_ERROR,
            parser_peek_previous(p),
            DIAG_LOC_END_OF_TOK,
            "expected ',' or ')'",
            "add a ',' or ')' here"
        );

        return parser_error_decl(p, node);
    }

    if (parser_check(p, TOK_ARROW)) {
        parser_advance(p);

        node -> as.func_decl.return_type_expr = parse_type_expr(p);
    } else if (parser_check(p, TOK_LBRACE)) {
        node -> as.func_decl.return_type_expr = AST_NODE_ID_NONE;
    } else {
        diagnostic_add_token(
            &driver_ctx.diagnostics,
            p -> id,
            DIAG_ERROR,
            parser_peek_previous(p),
            DIAG_LOC_END_OF_TOK,
            "expected '->' or '{'",
            "add a '->' or '{' here" 
        );

        return parser_error_decl(p, node);
    }

    if (!parser_check(p, TOK_LBRACE)) {
        diagnostic_add_token(
            &driver_ctx.diagnostics,
            p -> id,
            DIAG_ERROR,
            parser_peek_previous(p),
            DIAG_LOC_END_OF_TOK,
            "expected '{'",
            "add a '{' here" 
        );

        return parser_error_decl(p, node);
    }

    parser_advance(p);

    node -> as.func_decl.block = parse_block(p);

    return id;
}
