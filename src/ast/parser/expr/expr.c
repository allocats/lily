#include "ast/nodes/nodes.h"
#include "ast/nodes/types.h"
#include "ast/parser/expr/expr.h"
#include "ast/parser/expr/types.h"
#include "ast/parser/parser.h"
#include "ast/parser/recovery/recovery.h"
#include "ast/parser/recovery/types.h"
#include "ast/parser/types.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "ids.h"
#include "string_interner/interner.h"
#include "token/token.h"
#include "token/types.h"
#include "utils/types.h"

static AstNodeId nud(Parser* p, Token token);
static AstNodeId led(Parser* p, Token token, AstNodeId left);

static u8 lbp_of(TokenKind kind);

static void parse_call_args(Parser* p, AstNodeId id);
static AstNodeId parse_field_init(Parser* p);

AstNodeId parse_expression(Parser* p, u32 min_bp) {
    u32 start_index = p -> cursor;

    Token token = parser_advance(p);

    AstNodeId left = nud(p, token);

    while (lbp_of(parser_peek(p).kind) > min_bp) {
        Token op = parser_advance(p);

        left = led(p, op, left);
    }

    u32 end_index = p -> cursor - 1;

    AstNode* node = parser_get_node(p, left);

    node -> tokens.start = start_index;
    node -> tokens.end = end_index;

    return left;
}

static AstNodeId nud(Parser* p, Token token) {
    switch (token.kind) {
        case TOK_INTEGER_LIT: {
            AstNodeId id  = parser_create_node(p, AST_LITERAL, AST_FLAGS_NONE, 0);
            AstNode* node = parser_get_node(p, id);

            node -> as.literal.kind = LITERAL_INTEGER;
            node -> as.literal.as.integer = token_get_int_literal(p -> current_file -> id, token);

            return id;
        }

        case TOK_FLOAT_LIT: {
            AstNodeId id  = parser_create_node(p, AST_LITERAL, AST_FLAGS_NONE, 0);
            AstNode* node = parser_get_node(p, id);

            node -> as.literal.kind = LITERAL_FLOAT;
            node -> as.literal.as.floating = token_get_float_literal(p -> current_file -> id, token);

            return id;
        }

        case TOK_STRING_LIT: {
            AstNodeId id = parser_create_node(p, AST_LITERAL, AST_FLAGS_NONE, 0);
            AstNode* node = parser_get_node(p, id);

            token.length -= 2;
            token.start  += 1;

            node -> as.literal.kind = LITERAL_STRING;
            node -> as.literal.as.string = string_intern_token(p -> current_file -> id, token);

            token.length += 2;
            token.start  -= 1;

            return id;
        }

        case TOK_CHAR_LIT: {
            AstNodeId id  = parser_create_node(p, AST_LITERAL, AST_FLAGS_NONE, 0);
            AstNode* node = parser_get_node(p, id);

            node -> as.literal.kind = LITERAL_CHAR;
            node -> as.literal.as.character = token_get_char_literal(p -> current_file -> id, token);

            return id;
        }

        case TOK_KW_TRUE:
        case TOK_KW_FALSE: {
            AstNodeId id  = parser_create_node(p, AST_LITERAL, AST_FLAGS_NONE, 0);
            AstNode* node = parser_get_node(p, id);

            node -> as.literal.kind = LITERAL_BOOL;
            node -> as.literal.as.boolean = (token.kind == TOK_KW_TRUE);

            return id;
        }

        case TOK_KW_NULL: {
            AstNodeId id = parser_create_node(p, AST_LITERAL, AST_FLAGS_NONE, 0);
            AstNode* node = parser_get_node(p, id);

            node -> as.literal.kind = LITERAL_NULL;

            return id;
        }

        case TOK_IDENT: {
            AstNodeId id  = parser_create_node(p, AST_IDENTIFIER, AST_FLAGS_NONE, 0);
            AstNode* node = parser_get_node(p, id);

            node -> as.identifier.name = string_intern_token(p -> current_file -> id, token);

            return id;
        }

        case TOK_L_PAREN: {
            AstNodeId id = parse_expression(p, 0);

            if (!parser_check(p, TOK_R_PAREN)) {
                Token previous = parser_peek_previous(p);

                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &previous,
                    DIAG_LOC_END_OF_TOK,
                    "expected ')'",
                    "add a ')' here"
                );

                return parser_error(p, id, RECOVERY_EXPR);
            }

            parser_advance(p);

            return id;
        }

        case TOK_STAR:
        case TOK_MINUS:
        case TOK_BANG:
        case TOK_TILDE:
        case TOK_AMP: {
            AstNodeId id  = parser_create_node(p, AST_UNARY_OP, AST_FLAGS_NONE, 0);
            AstNode* node = parser_get_node(p, id);

            node -> as.unary_op.op = token.kind;
            node -> as.unary_op.operand = parse_expression(p, 119);

            return id;
        }

        default: {
            diagnostic_add_token(
                p -> current_file -> id,
                DIAG_ERROR,
                &token,
                DIAG_LOC_WHOLE_TOK,
                "invalid start to expression",
                null
            );

            AstNodeId id  = parser_create_node(p, AST_ERROR, AST_FLAGS_NONE, -1);

            return parser_error(p, id, RECOVERY_EXPR);
        } 
    }
}

static AstNodeId led(Parser* p, Token token, AstNodeId left) {
    switch (token.kind) {
        case TOK_L_PAREN: {
            AstNodeId id  = parser_create_node(p, AST_FUNCTION_CALL, AST_FLAGS_NONE, 0);
            AstNode* node = parser_get_node(p, id);

            node -> as.function_call.identifier = left;

            parse_call_args(p, id);

            if (!parser_check(p, TOK_R_PAREN)) {
                Token previous = parser_peek_previous(p);

                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &previous,
                    DIAG_LOC_END_OF_TOK,
                    "expected ')'",
                    "add a ')' here" 
                );

                return parser_error(p, id, RECOVERY_EXPR);
            }

            parser_advance(p);

            return id;
        }

        case TOK_L_BRACKET: {
            AstNodeId id  = parser_create_node(p, AST_INDEX, AST_FLAGS_NONE, 0);
            AstNode* node = parser_get_node(p, id);

            node -> as.index.object = left;
            node -> as.index.index_expr = parse_expression(p, 0);

            if (!parser_check(p, TOK_R_BRACKET)) {
                Token previous = parser_peek_previous(p);

                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &previous,
                    DIAG_LOC_END_OF_TOK,
                    "expected ']'",
                    "add a ']' here" 
                );

                return parser_error(p, id, RECOVERY_EXPR);
            }

            parser_advance(p);

            return id;
        }

        case TOK_ARROW:
        case TOK_DOT: {
            if (parser_check(p, TOK_L_BRACE)) {
                AstNodeId id = parser_create_node(p, AST_STRUCT_LITERAL, AST_FLAGS_NONE, -1);

                parser_advance(p);

                if (!parser_check(p, TOK_R_BRACE)) {
                    while (p -> cursor < p -> token_count) {
                        AstNodeId field_id = parse_field_init(p);

                        AstNode* node = parser_get_node(p, id);
                        ast_id_list_append(&node -> as.struct_literal.inits, &p -> current_file -> ast, field_id);

                        if (!parser_check(p, TOK_COMMA)) {
                            break;
                        }

                        parser_advance(p);

                        if (parser_check(p, TOK_R_BRACE)) {
                            break;
                        }
                    }
                }

                if (parser_check(p, TOK_R_BRACE)) {
                    parser_advance(p);
                } else {
                    Token bad = parser_peek(p);

                    diagnostic_add_token(
                        p -> current_file -> id,
                        DIAG_ERROR,
                        &bad,
                        DIAG_LOC_WHOLE_TOK,
                        "expected '}' to close struct literal",
                        "add a closing brace"
                    );
                }

                AstNode* node = parser_get_node(p, id);

                node -> as.struct_literal.struct_type = left;

                return id;
            }

            if (!parser_check(p, TOK_IDENT)) {
                Token bad = parser_peek(p);

                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &bad,
                    DIAG_LOC_WHOLE_TOK,
                    token.kind == TOK_ARROW ? "expected identifier after '->'" : "expected identifier after '.'",
                    "member access looks like: object.field"
                );

                AstNodeId id = parser_create_node(p, AST_ERROR, AST_FLAGS_NONE, -1);

                return parser_error(p, id, RECOVERY_EXPR);
            }

            Token member_tok = parser_advance(p);

            AstNodeId member_node_id = parser_create_node(p, AST_IDENTIFIER, AST_FLAGS_NONE, 0);
            AstNode* member_node = parser_get_node(p, member_node_id);

            member_node -> as.identifier.name = string_intern_token(p -> current_file -> id, member_tok);
            member_node -> tokens.start = p -> cursor - 1;
            member_node -> tokens.end = p -> cursor - 1;

            AstNodeId id = parser_create_node(p, AST_MEMBER_ACCESS, AST_FLAGS_NONE, 0);
            AstNode* node = parser_get_node(p, id);

            node -> as.member_access.used_pointer_access = (token.kind == TOK_ARROW);
            node -> as.member_access.object = left;
            node -> as.member_access.member = member_node_id;

            return id;
        }

        default: {
            AstNodeId id  = parser_create_node(p, AST_BINARY_OP, AST_FLAGS_NONE, 0);
            AstNode* node = parser_get_node(p, id);

            node -> as.binary_op.op = token.kind;
            node -> as.binary_op.left = left;

            AstNodeId right = parse_expression(p, op_table[token.kind].rbp);

            node = parser_get_node(p, id);
            node -> as.binary_op.right = right;

            return id;
        }
    }
}

static inline u8 lbp_of(TokenKind kind) {
    if ((u32) kind >= OP_TABLE_LEN) return 0;
    return op_table[kind].lbp;
}

static void parse_call_args(Parser* p, AstNodeId id) {
    AstNode* call = parser_get_node(p, id);

    if (parser_check(p, TOK_R_PAREN)) {
        return;
    }

    do {
        AstNodeId arg_id = parse_expression(p, 0);

        call = parser_get_node(p, id);

        ast_id_list_append(&call -> as.function_call.arguments, &p ->current_file -> ast, arg_id);

        if (!parser_check(p, TOK_COMMA)) {
            break;
        }

        parser_advance(p);
    } while (p -> cursor < p -> token_count);
}

static AstNodeId parse_field_init(Parser* p) {
    AstNodeId id = parser_create_node(p, AST_FIELD_INIT, AST_FLAGS_NONE, 0);

    if (parser_check(p, TOK_DOT)) {
        parser_advance(p);
    } else {
        Token bad = parser_peek(p);

        diagnostic_add_token(
            p -> current_file -> id,
            DIAG_ERROR,
            &bad,
            DIAG_LOC_WHOLE_TOK,
            "expected '.' before field name",
            "struct literal fields look like: .name = value"
        );

        return parser_error(p, id, RECOVERY_EXPR);
    }
 
    AstNodeId field = parse_expression(p, 119);
 
    if (parser_check(p, TOK_EQ)) {
        parser_advance(p);
    } else {
        Token bad = parser_peek(p);

        diagnostic_add_token(
            p -> current_file -> id,
            DIAG_ERROR,
            &bad,
            DIAG_LOC_WHOLE_TOK,
            "expected '=' after field name",
            "struct literal fields look like: .name = value"
        );

        return parser_error(p, id, RECOVERY_EXPR);
    }
 
    AstNodeId value = parse_expression(p, 0);
 
    AstNode* node = parser_get_node(p, id);
    node -> as.field_init.field = field;
    node -> as.field_init.value = value;
 
    return id;
}
