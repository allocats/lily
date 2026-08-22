#include "ast/parser/directive/directive.h"
#include "ast/parser/parser.h"
#include "ast/parser/expr/expr.h"
#include "ast/parser/recovery/recovery.h"
#include "ast/parser/recovery/types.h"
#include "ast/parser/stmts/stmts.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "ids.h"
#include "token/types.h"

AstNodeId parse_statement(Parser* p) {
    Token token = parser_peek(p);

    switch (token.kind) {
        case TOK_HASHTAG:
            parser_advance(p);
            return parse_directive(p);

        case TOK_IDENT:
            if (parser_check_ahead_by(p, TOK_COLON, 1)) {
                return parse_variable_decl(p);
            } else {
                AstNodeId id = parse_expression(p, 0);

                if (IS_NODE_ERROR(p, id)) {
                    return parser_error(p, id, RECOVERY_STMT);
                }

                if (!parser_check(p, TOK_SEMI)) {
                    Token previous = parser_peek_previous(p);

                    diagnostic_add_token(
                        p -> current_file -> id,
                        DIAG_ERROR,
                        &previous,
                        DIAG_LOC_END_OF_TOK,
                        "expected ';'",
                        "add a ';' here"
                    );

                    return parser_error(p, id, RECOVERY_STMT);
                }

                parser_advance(p); // advance past ';'

                return id;
            }

        case TOK_KW_IF:
            return parse_if_statement(p);

        case TOK_KW_SWITCH:
            return parse_switch_statement(p);

        case TOK_KW_FOR:
            return parse_for_loop(p);

        case TOK_KW_WHILE:
            return parse_while_loop(p);

        case TOK_KW_DEFER:
            return parse_defer_statement(p);

        case TOK_KW_RETURN:
            return parse_return_statement(p);

        case TOK_L_BRACE:
            return parse_block(p);

        default:
            AstNodeId id = parse_expression(p, 0);

            if (IS_NODE_ERROR(p, id)) return id;

            if (!parser_check(p, TOK_SEMI)) {
                Token previous = parser_peek_previous(p);

                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &previous,
                    DIAG_LOC_END_OF_TOK,
                    "expected ';'",
                    "add a ';' here"
                );

                return parser_error(p, id, RECOVERY_STMT);
            }

            parser_advance(p); // advance past ';'

            return id;
    }
}
