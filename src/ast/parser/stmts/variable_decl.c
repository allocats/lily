#include "ast/nodes/nodes.h"
#include "ast/nodes/types.h"
#include "ast/parser/expr/expr.h"
#include "ast/parser/parser.h"
#include "ast/parser/recovery/recovery.h"
#include "ast/parser/stmts/stmts.h"
#include "ast/parser/types/ty.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "ids.h"
#include "string_interner/interner.h"
#include "token/types.h"

AstNodeId parse_variable_decl(Parser* p) {
    AstNodeId id  = parser_create_node(p, AST_VARIABLE_DECL, AST_FLAGS_NONE, 0);
    AstNode* node = parser_get_node(p, id);

    Token name_token = parser_peek(p); // no need to check if ident, can only enter function if it was ident

    node -> as.variable_decl.name = string_intern_token(p -> current_file -> id, name_token);

    parser_advance(p); // advance past identifier 
    parser_advance(p); // advance past ':', again do not have to check

    AstNodeId type_expr_id = parse_type_expr(p);

    if (IS_NODE_ERROR(p, type_expr_id)) {
        return parser_error(p, id, RECOVERY_NONE);
    }

    node = parser_get_node(p, id);

    node -> as.variable_decl.type_expr = type_expr_id;

    if (is_node_constant(&p -> current_file -> ast, type_expr_id)) {
        node -> flags |= AST_FLAGS_IS_CONSTANT;
    }

    if (parser_check(p, TOK_SEMI)) {
        parser_advance(p); // advance past ';'

        node -> as.variable_decl.value_expr = AST_NODE_ID_NONE;

        return id;
    }

    if (!parser_check(p, TOK_EQ)) {
        Token previous = parser_peek_previous(p);

        diagnostic_add_token(
            p -> current_file -> id,
            DIAG_ERROR,
            &previous,
            DIAG_LOC_END_OF_TOK,
            "expected '=' or ';'",
            "add '=' or ';' here for variable declaration"
        );

        return parser_error(p, id, RECOVERY_NONE);
    }

    parser_advance(p); // advanced past '='

    AstNodeId value_expr_id = parse_expression(p, 0);

    if (IS_NODE_ERROR(p, value_expr_id)) {
        return parser_error(p, id, RECOVERY_NONE);
    }

    node = parser_get_node(p, id);

    node -> as.variable_decl.value_expr = value_expr_id;

    if (!parser_check(p, TOK_SEMI)) {
        Token previous = parser_peek_previous(p);

        diagnostic_add_token(
            p -> current_file -> id,
            DIAG_ERROR,
            &previous,
            DIAG_LOC_END_OF_TOK,
            "expected ';'",
            "add ';' here after variable declaration"
        );

        return parser_error(p, id, RECOVERY_NONE);
    }

    node -> tokens.end = p -> cursor;

    parser_advance(p); // advance past ';'

    return id;
}
