#include "ast/nodes/nodes.h"
#include "ast/parser/expr/expr.h"
#include "ast/parser/stmts/stmts.h"
#include "diagnostics/diagnostics.h"
#include "utils/debug.h"
#include "utils/macros.h"

AstNodeId parse_if_stmt(Parser* p) {
    AstNodeId id  = parser_create_node(p, AST_IF);
    AstNode* node = ast_node_get(&p -> module -> ast, id);

    node -> as.if_stmt.branches = arena_alloc(&p -> module -> ast.gpa_arena, sizeof(AstNodeId) * 4);
    node -> as.if_stmt.branch_count = 0;
    node -> as.if_stmt.branch_capacity = 4;

    while (p -> cursor < p -> token_count) {
        if (UNLIKELY(node -> as.if_stmt.branch_count >= node -> as.if_stmt.branch_capacity)) {
            u64 size = sizeof(AstNodeId) * node -> as.if_stmt.branch_capacity;

            node -> as.if_stmt.branches = arena_realloc(
                &p -> module -> ast.gpa_arena,
                node -> as.if_stmt.branches,
                size,
                size * 2
            );
            node -> as.if_stmt.branch_capacity *= 2;

            debug_printf("block realloc from %ld -> %ld bytes\n", size, size * 2);
        }

        AstNodeId condition = parse_expression(p, 0);

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

            return parser_error_stmt(p, node);
        }

        parser_advance(p);

        AstNodeId block = parse_block(p);

        AstNodeId branch_id  = parser_create_node(p, AST_BRANCH);
        AstNode* branch_node = ast_node_get(&p -> module -> ast, branch_id);

        branch_node -> as.branch.condition = condition;
        branch_node -> as.branch.block = block;

        node -> as.if_stmt.branches[node -> as.if_stmt.branch_count++] = branch_id;

        if (!parser_check(p, TOK_ELSE)) break;

        parser_advance(p);

        if (parser_check(p, TOK_LBRACE)) {
            parser_advance(p);
            node -> as.if_stmt.else_block = parse_block(p);
            break;
        }

        if (!parser_check(p, TOK_IF)) {
            diagnostic_add_token(
                &driver_ctx.diagnostics,
                p -> id,
                DIAG_ERROR,
                parser_peek_previous(p),
                DIAG_LOC_END_OF_TOK,
                "expected 'if'",
                "add a 'if' here"
            );

            return parser_error_stmt(p, node);
        }

        parser_advance(p);
    }

    node -> token_span.end = p -> cursor;

    return id;
}
