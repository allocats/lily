#include "ast/tree/tree.h"
#include "ast/nodes/types.h"
#include "ast/tree/types.h"
#include "ids.h"
#include "token/types.h"
#include "utils/debug.h"
#include "utils/macros.h"
#include "utils/types.h"
#include <assert.h>

static constexpr u64 gpa_init_size_kb = 2;
static constexpr u32 ast_init_capacity = 64;
static constexpr u64 nodes_array_init_alloc_size = sizeof(AstNode) * ast_init_capacity;
static constexpr u64 nodes_arena_init_size_kb = (ast_init_capacity * sizeof(AstNode) * 2) / ARENA_KB(1);

static_assert(ast_init_capacity > 0);
static_assert(nodes_arena_init_size_kb > 0);
static_assert(gpa_init_size_kb > 0);

void ast_init(Ast* ast) {
    assert(ast != null);

    arena_init(&ast -> gpa, ARENA_KB(gpa_init_size_kb), ALIGN_DEFAULT);
    debug_printf("ast(%p) -> gpa init arena with %luKB", ast, gpa_init_size_kb);

    arena_init(&ast -> nodes_arena, ARENA_KB(nodes_arena_init_size_kb), ALIGN_DEFAULT);
    debug_printf("ast(%p) -> nodes_arena init arena with %luKB", ast, nodes_arena_init_size_kb);

    // TODO: test if need zeroed memory
    ast -> nodes = arena_alloc(&ast -> nodes_arena, nodes_array_init_alloc_size);
    ast -> count = 0;
    ast -> capacity = ast_init_capacity;

    ast -> top_level_declaration_count = 0;

    debug_printf(
        "Allocated AST ast -> nodes with %lu bytes (%u nodes)",
        nodes_array_init_alloc_size,
        ast_init_capacity
    );
}

AstNodeId ast_alloc_node(Ast* ast) {
    if (UNLIKELY(ast -> count >= ast -> capacity)) {
        u64 old_capacity = ast -> capacity;
        u64 new_capacity = old_capacity * 2;

        u64 old_size = old_capacity * sizeof(AstNode);
        u64 new_size = old_size * 2;

        ast -> nodes = arena_realloc(&ast -> nodes_arena, ast -> nodes, old_size, new_size);
        ast -> capacity *= 2;

        // TODO: test if need zeroed memory
        // arena_memset(ast -> nodes + old_capacity, 0, (new_capacity - old_capacity) * sizeof(AstNode));

        debug_printf("ast -> nodes realloc from %lu to %lu bytes", old_size, new_size);
    }

    return ast -> count++;
}

inline AstNode* ast_get_node(Ast* ast, AstNodeId id) {
    // TODO: Profile these asserts
    debug_assert(id < ast -> count);
    debug_assert(id >= 0);

    return &ast -> nodes[id];
}
