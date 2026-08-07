#include "ast/nodes/nodes.h"

#include "utils/debug.h"
#include "utils/macros.h"

AstNodeId ast_node_alloc(Arena* arena, Ast* ast) {
    if (UNLIKELY(ast -> count >= ast -> capacity)) {
        u64 size = ast -> capacity * sizeof(AstNode);

        ast -> nodes = arena_realloc(arena, ast -> nodes, size, size * 2);
        ast -> capacity *= 2;

        debug_printf("AST Nodes: Realloc %ld -> %ld bytes\n", size, size * 2);
    }

    return ast -> count++;
}

inline AstNode* ast_node_get(Ast* ast, AstNodeId id) {
    debug_assert(id != AST_NODE_ID_NONE && "Received no node id");
    debug_assert(id <  ast -> count     && "Out of bounds node lookup");

    return &ast -> nodes[id];
}

void ast_block_push_stmt(Arena* arena, AstNode* block, AstNodeId stmt) {
    if (UNLIKELY(block -> as.block.stmt_count >= block -> as.block.stmt_capacity)) {
        u64 size = sizeof(AstNodeId) * block -> as.block.stmt_capacity;

        block -> as.block.stmts = arena_realloc(arena, block -> as.block.stmts, size, size * 2);
        block -> as.block.stmt_capacity *= 2;

        debug_printf("block realloc from %ld -> %ld bytes\n", size, size * 2);
    }

    block -> as.block.stmts[block -> as.block.stmt_count++] = stmt;
}

bool ast_is_kind(Ast* ast, AstNodeId id, AstKind kind) {
    return kind == ast -> nodes[id].kind;
}
