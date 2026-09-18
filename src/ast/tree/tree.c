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
static constexpr u64 attributes_arena_init_size_kb = nodes_arena_init_size_kb / 8;
static constexpr u64 attributes_init_capacity = ARENA_KB(attributes_arena_init_size_kb) / sizeof(AstNodeIdList);

static_assert(ast_init_capacity > 0);
static_assert(nodes_arena_init_size_kb > 0);
static_assert(gpa_init_size_kb > 0);

void ast_init(Ast* ast) {
    assert(ast != null);

    arena_init(&ast -> gpa, ARENA_KB(gpa_init_size_kb), ALIGN_DEFAULT);
    debug_printf("ast(%p) -> gpa init arena with %luKB", ast, gpa_init_size_kb);

    arena_init(&ast -> nodes_arena, ARENA_KB(nodes_arena_init_size_kb), ALIGN_DEFAULT);
    debug_printf("ast(%p) -> nodes_arena init arena with %luKB", ast, nodes_arena_init_size_kb);

    arena_init(&ast -> attributes_arena, ARENA_KB(attributes_arena_init_size_kb), ALIGN_DEFAULT);
    debug_printf("ast(%p) -> attributes_arena init arena with %luKB", ast, attributes_arena_init_size_kb);

    ast -> nodes = arena_alloc(&ast -> nodes_arena, nodes_array_init_alloc_size);
    ast -> node_count = 0;
    ast -> node_capacity = ast_init_capacity;

    ast -> attributes = arena_alloc(&ast -> attributes_arena, ARENA_KB(attributes_arena_init_size_kb));
    ast -> attribute_count = 0;
    ast -> attribute_capacity = attributes_init_capacity;

    ast -> declaration_count = 0;
    ast -> top_level_declaration_count = 0;

    debug_printf(
        "Allocated AST ast -> nodes with %lu bytes (%u nodes)",
        nodes_array_init_alloc_size,
        ast_init_capacity
    );
}

AstNodeId ast_alloc_node(Ast* ast) {
    if (UNLIKELY(ast -> node_count >= ast -> node_capacity)) {
        u64 old_capacity = ast -> node_capacity;

        u64 old_size = old_capacity * sizeof(AstNode);
        u64 new_size = old_size * 2;

        ast -> nodes = arena_realloc(&ast -> nodes_arena, ast -> nodes, old_size, new_size);
        ast -> node_capacity *= 2;

        debug_printf("ast -> nodes realloc from %lu to %lu bytes", old_size, new_size);
    }

    return ast -> node_count++;
}

AstAttributeId ast_alloc_attribute(Ast* ast) {
    if (UNLIKELY(ast -> attribute_count >= ast -> attribute_capacity)) {
        u64 old_capacity = ast -> attribute_capacity;

        u64 old_size = old_capacity * sizeof(AstNodeIdList);
        u64 new_size = old_size * 2;

        ast -> attributes = arena_realloc(&ast -> attributes_arena, ast -> attributes, old_size, new_size);
        ast -> attribute_capacity *= 2;

        debug_printf("ast -> attributes realloc from %lu to %lu bytes", old_size, new_size);
    }

    return ast -> attribute_count++;
}

inline AstNode* ast_get_node(Ast* ast, AstNodeId id) {
    debug_assert(id < ast -> node_count);
    debug_assert(id >= 0);

    return &ast -> nodes[id];
}
