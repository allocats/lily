#include "ast/nodes/nodes.h"
#include "ast/nodes/types.h"
#include "ast/parser/decl/decl.h"
#include "ast/parser/parser.h"
#include "ast/parser/recovery/recovery.h"
#include "ast/parser/recovery/types.h"
#include "ast/parser/stmts/stmts.h"
#include "ast/parser/types.h"
#include "ast/parser/types/ty.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "ids.h"
#include "string_interner/interner.h"
#include "token/types.h"

AstNodeId parse_function_decl(Parser* p, StringId name) {
    u32 flags = AST_FLAGS_IS_TOP_DECL;
    u32 start = p -> cursor;

    while (!parser_check(p, TOK_KW_FN)) {
        Token token = parser_advance(p);

        u32 flag = 0;

        switch (token.kind) {
            case TOK_KW_NOINLINE: {
                if (flags & AST_FLAGS_IS_INLINE) {
                    diagnostic_add_token(
                        p -> current_file -> id,
                        DIAG_ERROR,
                        &token,
                        DIAG_LOC_WHOLE_TOK,
                        "invalid function modifier",
                        "cannot add noinline modifier to function with inline modifier"
                    );

                    break;
                }

                flag = AST_FLAGS_IS_NOINLINE;
            } break;

            case TOK_KW_INLINE: {
                if (flags & AST_FLAGS_IS_NOINLINE) {
                    diagnostic_add_token(
                        p -> current_file -> id,
                        DIAG_ERROR,
                        &token,
                        DIAG_LOC_WHOLE_TOK,
                        "invalid function modifier",
                        "cannot add inline modifier to function with noinline modifier"
                    );

                    break;
                }

                flag = AST_FLAGS_IS_INLINE;
            } break;

            case TOK_KW_INTRINSIC: {
                if (flags & AST_FLAGS_IS_FOREIGN) {
                    diagnostic_add_token(
                        p -> current_file -> id,
                        DIAG_ERROR,
                        &token,
                        DIAG_LOC_WHOLE_TOK,
                        "invalid function modifier",
                        "cannot add intrinsic modifier to function with foreign modifier"
                    );

                    break;
                }

                flag = AST_FLAGS_IS_INTRINSIC;
            } break;

            case TOK_KW_FOREIGN: {
                if (flags & AST_FLAGS_IS_INTRINSIC) {
                    diagnostic_add_token(
                        p -> current_file -> id,
                        DIAG_ERROR,
                        &token,
                        DIAG_LOC_WHOLE_TOK,
                        "invalid function modifier",
                        "cannot add foreign modifier to function with intrinsic modifier"
                    );

                    break;
                }

                flag = AST_FLAGS_IS_FOREIGN;
            } break;

            default: {
                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &token,
                    DIAG_LOC_WHOLE_TOK,
                    "invalid function modifier or fn",
                    "expected (inline | noinline | foreign | intrinsic | fn)"
                );
            } break;
        }

        if (flag != 0 && flags & flag) {
            diagnostic_add_token(
                p -> current_file -> id,
                DIAG_ERROR,
                &token,
                DIAG_LOC_WHOLE_TOK,
                "duplicated function modifier",
                "remove this modifier"
            );
        }

        flags |= flag;
    }

    parser_advance(p);

    p -> is_foreign_allowed = false;

    AstNodeId id = parser_create_node(p, AST_FUNCTION_DECL, flags, start - p -> cursor);
    AstNode* node = parser_get_node(p, id);

    node -> as.function_decl.name = name;
    node -> as.function_decl.return_type_expr = AST_NODE_ID_NONE;
    node -> as.function_decl.block = AST_NODE_ID_NONE;

    if (!parser_check(p, TOK_L_PAREN)) {
        Token token = parser_peek_previous(p);

        diagnostic_add_token(
            p -> current_file -> id,
            DIAG_ERROR,
            &token,
            DIAG_LOC_START_OF_TOK,
            "expected '(' after 'fn'",
            "add a '(' here"
        );

        return parser_error(p, id, RECOVERY_DECL);
    }

    parser_advance(p);

    while (p -> cursor < p -> token_count) {
        if (parser_check(p, TOK_R_PAREN)) {
            parser_advance(p);
            break;
        }

        Token param_name_token = parser_advance(p);

        if (param_name_token.kind != TOK_IDENT) {
            diagnostic_add_token(
                p -> current_file -> id,
                DIAG_ERROR,
                &param_name_token,
                DIAG_LOC_WHOLE_TOK,
                "expected identifier",
                "add a valid identifier here"
            );

            return parser_error(p, id, RECOVERY_DECL);
        }

        AstNodeId param_node_id = parser_create_node(p, AST_PARAMETER, AST_FLAGS_NONE,  -1);
        AstNode* param_node = parser_get_node(p, param_node_id);

        param_node -> as.parameter_decl.name = string_intern_token(
            p -> current_file -> id,
            param_name_token
        ); 

        if (!parser_check(p, TOK_COLON)) {
            diagnostic_add_token(
                p -> current_file -> id,
                DIAG_ERROR,
                &param_name_token,
                DIAG_LOC_END_OF_TOK,
                "expected ':'",
                "add a ':' here"
            );

            return parser_error(p, id, RECOVERY_DECL);
        }

        parser_advance(p); // advance past ':'

        AstNodeId param_type_expr_id = parse_param_type_expr(p);

        if (IS_NODE_ERROR(p, param_type_expr_id)) {
            return parser_error(p, id, RECOVERY_DECL);
        }

        AstNode* param_type_node = parser_get_node(p, param_type_expr_id);

        param_node = parser_get_node(p, param_node_id);
        param_node -> as.parameter_decl.type_expr = param_type_expr_id;

        node = parser_get_node(p, id);

        param_node -> flags |= param_type_node -> flags;

        if (param_type_node -> kind == AST_TYPE_VARIADIC) {
            node -> flags |= AST_FLAGS_IS_VARIADIC;

            if (parser_check(p, TOK_COMMA)) {
                Token token = parser_peek_previous(p);

                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &token,
                    DIAG_LOC_WHOLE_TOK,
                    "variadic parameter must be the last parameter",
                    "remove the parameters after '...'"
                );

                return parser_error(p, id, RECOVERY_DECL);
            }

            if (!parser_check(p, TOK_R_PAREN)) {
                Token token = parser_peek_previous(p);

                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &token,
                    DIAG_LOC_END_OF_TOK,
                    "expected ')'",
                    "add a ')' here"
                );

                return parser_error(p, id, RECOVERY_DECL);
            }

            ast_id_list_append(
                &node -> as.function_decl.parameters,
                &p -> current_file -> ast,
                param_node_id
            );

            parser_advance(p);
            break;
        }

        ast_id_list_append(
            &node -> as.function_decl.parameters,
            &p -> current_file -> ast,
            param_node_id
        );

        if (parser_check(p, TOK_R_PAREN)) {
            parser_advance(p);
            break;
        }

        if (parser_check(p, TOK_COMMA)) {
            parser_advance(p);
            continue;
        }

        Token previous = parser_peek_previous(p);

        diagnostic_add_token(
            p -> current_file -> id,
            DIAG_ERROR,
            &previous,
            DIAG_LOC_END_OF_TOK,
            "expected ',' or ')'",
            "add a ',' or ')' here"
        );

        return parser_error(p, id, RECOVERY_DECL);
    }

    if (parser_check(p, TOK_ARROW)) {
        parser_advance(p);

        AstNodeId return_type_expr = parse_type_expr(p);
        
        if (IS_NODE_ERROR(p, return_type_expr)) {
            return parser_error(p, id, RECOVERY_DECL);
        }

        node = parser_get_node(p, id);
        node -> as.function_decl.return_type_expr = return_type_expr;
    }

    p -> is_foreign_allowed = true;

    if (flags & AST_FLAGS_IS_INTRINSIC || flags & AST_FLAGS_IS_FOREIGN) {
        if (!parser_check(p, TOK_SEMI)) {
            Token token = parser_peek_previous(p);

            diagnostic_add_token(
                p -> current_file -> id,
                DIAG_ERROR,
                &token,
                DIAG_LOC_END_OF_TOK,
                "expected ';' after function declaration",
                "add a ';' here" 
            );

            return parser_error(p, id, RECOVERY_DECL);
        }

        node = parser_get_node(p, id);

        node -> tokens.end = p -> cursor;

        p -> current_file -> ast.declaration_count += node -> as.function_decl.parameters.count;

        parser_advance(p);

        return id;
    }

    if (!parser_check(p, TOK_L_BRACE)) {
        Token token = parser_peek_previous(p);

        diagnostic_add_token(
            p -> current_file -> id,
            DIAG_ERROR,
            &token,
            DIAG_LOC_END_OF_TOK,
            "expected '{' for function body",
            "add a '{' here" 
        );

        return parser_error(p, id, RECOVERY_DECL);
    }

    AstNodeId block_id = parse_block(p);

    if (IS_NODE_ERROR(p, block_id)) {
        return parser_error(p, id, RECOVERY_DECL);
    }

    node = parser_get_node(p, id);

    node -> as.function_decl.block = block_id;
    node -> tokens.end = p -> cursor;

    p -> current_file -> ast.declaration_count += node -> as.function_decl.parameters.count;

    return id;
}
