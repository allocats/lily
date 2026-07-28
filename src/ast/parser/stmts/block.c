#include "ast/nodes/nodes.h"
#include "ast/parser/expr/expr.h"
#include "ast/parser/stmts/stmts.h"
#include "diagnostics/diagnostics.h"
#include "utils/debug.h"
#include "utils/macros.h"

#include <assert.h>

typedef AstNodeId (*ParseStmtFn)(Parser*);

static const ParseStmtFn STMT_DISPATCH[TOKEN_KIND_COUNT] = {
    [TOK_LBRACE] = parse_block,
    [TOK_LET]    = parse_var_decl,
    [TOK_CONST]  = parse_const_decl,
    [TOK_FOR]    = parse_for_loop,
    [TOK_WHILE]  = parse_while_loop,
    [TOK_IF]     = parse_if_stmt,
    [TOK_DEFER]  = parse_defer_stmt,
    [TOK_RETURN] = parse_return_stmt
};

AstNodeId parse_block(Parser* p) {
    AstNodeId id  = parser_create_node(p, AST_BLOCK);
    AstNode* node = ast_node_get(&p -> module -> ast, id);

    node -> as.block.stmts = arena_alloc(&p -> module -> ast.arena, sizeof(AstNodeId) * 8);
    node -> as.block.stmt_capacity = 8;
    node -> as.block.stmt_count = 0;

    while (p -> cursor < p -> token_count) {
        if (UNLIKELY(node -> as.block.stmt_count >= node -> as.block.stmt_capacity)) {
            u64 size = sizeof(AstNodeId) * node -> as.block.stmt_capacity;

            node -> as.block.stmts = arena_realloc(&p -> module -> ast.arena, node -> as.block.stmts, size, size * 2);
            node -> as.block.stmt_capacity *= 2;

            debug_printf("block realloc from %ld -> %ld bytes\n", size, size * 2);
        }

        Token* token = parser_peek(p);

        if (token -> kind == TOK_RBRACE) {
            parser_advance(p);
            return id;
        }

        if (token -> kind == TOK_IDENT) {
            AstNodeId expr_id  = parse_expression(p, 0);
            AstNode* expr_node = ast_node_get(&p -> module -> ast, expr_id);
            
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

                ast_block_push_stmt(&p -> module -> ast.arena, node, parser_error_stmt(p, expr_node));
            } else {
                parser_advance(p);
                ast_block_push_stmt(&p -> module -> ast.arena, node, expr_id);
            }
        } else {
            ParseStmtFn fn = STMT_DISPATCH[token -> kind];

            if (fn) {
                parser_advance(p);
                ast_block_push_stmt(&p -> module -> ast.arena, node, fn(p));
            } else {
                diagnostic_add_token(
                    &driver_ctx.diagnostics,
                    p -> id,
                    DIAG_ERROR,
                    token,
                    DIAG_LOC_WHOLE_TOK,
                    "unexpected token",
                    "expected: (ident | const | let | if | for | while | defer | return | block)"
                );

                parser_recover_stmt(p);
            }
        }
    }

    return id;
}
