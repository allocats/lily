#include "ast/nodes/nodes.h"
#include "ast/parser/parser.h"
#include "ast/parser/stmts/stmts.h"
#include "ast/parser/expr/expr.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "ids.h"
#include "token/types.h"

static AstNodeId parse_switch_case(Parser* p);
static AstNodeId parse_switch_default(Parser* p);
static AstNodeId parse_case_body(Parser* p);
 
AstNodeId parse_switch_statement(Parser* p) {
    AstNodeId id  = parser_create_node(p, AST_SWITCH_STMT, AST_FLAGS_NONE, 0);
    AstNode* node = parser_get_node(p, id);
 
    node -> as.switch_stmt.default_case = AST_NODE_ID_NONE;
 
    parser_advance(p); // advance past "switch"
 
    AstNodeId target = parse_expression(p, 0);
 
    node = parser_get_node(p, id);
    node -> as.switch_stmt.value = target;
 
    if (!parser_check(p, TOK_L_BRACE)) {
        Token previous = parser_peek_previous(p);

        diagnostic_add_token(
            p -> current_file -> id,
            DIAG_ERROR,
            &previous,
            DIAG_LOC_END_OF_TOK,
            "expected '{'",
            "add a '{' here"
        );

        return parser_error_stmt(p, id);
    }
 
    parser_advance(p); // advance past '{'
 
    bool has_default = false;
 
    while (p -> cursor < p -> token_count && !parser_check(p, TOK_R_BRACE)) {
        if (parser_check(p, TOK_KW_CASE)) {
            AstNodeId case_id = parse_switch_case(p);

            node = parser_get_node(p, id);

            ast_id_list_append(&node -> as.switch_stmt.cases, &p -> current_file -> ast, case_id);

        } else if (parser_check(p, TOK_KW_DEFAULT)) {
            Token default_token = parser_peek(p);

            AstNodeId default_id = parse_switch_default(p);

            if (has_default) {
                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &default_token,
                    DIAG_LOC_WHOLE_TOK,
                    "multiple 'default' cases in switch statement",
                    "a switch statement can only have one 'default' case"
                );
            } else {
                node = parser_get_node(p, id);
                node -> as.switch_stmt.default_case = default_id;

                has_default = true;
            }
        } else {
            Token token = parser_peek(p);

            diagnostic_add_token(
                p -> current_file -> id,
                DIAG_ERROR,
                &token,
                DIAG_LOC_WHOLE_TOK,
                "expected 'case' or 'default'",
                "add a valid case"
            );

            return parser_error_stmt(p, id);
        }
    }

    if (!parser_check(p, TOK_R_BRACE)) {
        Token previous = parser_peek_previous(p);

        diagnostic_add_token(
            p -> current_file -> id,
            DIAG_ERROR,
            &previous,
            DIAG_LOC_END_OF_TOK,
            "expected '}' to close switch statement",
            "add a '}' here"
        );

        return parser_error_stmt(p, id);
    }
 
    node = parser_get_node(p, id);
    node -> tokens.end = p -> cursor;
 
    parser_advance(p); // advance past '}'
 
    return id;
}
 
static AstNodeId parse_switch_case(Parser* p) {
    AstNodeId id  = parser_create_node(p, AST_SWITCH_CASE, AST_FLAGS_NONE, 0);
    AstNode* node = parser_get_node(p, id);
 
    parser_advance(p); // advance past "case"
 
    do {
        AstNodeId pattern_id = parse_expression(p, 0);

        node = parser_get_node(p, id);

        ast_id_list_append(&node -> as.switch_case.patterns, &p -> current_file -> ast, pattern_id);

        if (!parser_check(p, TOK_COMMA)) {
            break;
        }

        parser_advance(p); // advance past ','
    } while (p -> cursor < p -> token_count);

    if (!parser_check(p, TOK_COLON)) {
        Token previous = parser_peek_previous(p);

        diagnostic_add_token(
            p -> current_file -> id,
            DIAG_ERROR,
            &previous,
            DIAG_LOC_END_OF_TOK,
            "expected ':' after case pattern(s)",
            "add a ':' here"
        );

        return parser_error_stmt(p, id);
    }
 
    parser_advance(p); // advance past ':'
 
    AstNodeId body = parse_case_body(p);
 
    node = parser_get_node(p, id);
    node -> as.switch_case.block = body;
 
    return id;
}
 
static AstNodeId parse_switch_default(Parser* p) {
    parser_advance(p); // advance past "default"
 
    if (!parser_check(p, TOK_COLON)) {
        Token previous = parser_peek_previous(p);

        diagnostic_add_token(
            p -> current_file -> id,
            DIAG_ERROR,
            &previous,
            DIAG_LOC_END_OF_TOK,
            "expected ':' after 'default'",
            "add a ':' here"
        );

        AstNodeId id = parser_create_node(p, AST_ERROR, AST_FLAGS_NONE, -1);
        return parser_error_stmt(p, id);
    }
 
    parser_advance(p); // advance past ':'
 
    return parse_case_body(p);
}
 
static AstNodeId parse_case_body(Parser* p) {
    if (parser_check(p, TOK_L_BRACE)) {
        return parse_block(p);
    }
 
    AstNodeId id  = parser_create_node(p, AST_BLOCK, AST_FLAGS_NONE, 0);
    AstNode* node = parser_get_node(p, id);
 
    ast_id_list_init(&p -> current_file -> ast.gpa, &node -> as.block.statements, 8);
 
    while (p -> cursor < p -> token_count       &&
           !parser_check(p, TOK_KW_CASE)        &&
           !parser_check(p, TOK_KW_DEFAULT)     &&
           !parser_check(p, TOK_R_BRACE)
    ) {
        AstNodeId stmt_id = parse_statement(p);

        node = parser_get_node(p, id);

        ast_id_list_append(&node -> as.block.statements, &p -> current_file -> ast, stmt_id);
    }
 
    node -> tokens.end = p -> cursor;
 
    return id;
}
