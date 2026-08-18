#include "ast/nodes/nodes.h"
#include "ids.h"
#include "utils/debug.h"
#include "utils/macros.h"
#include "utils/types.h"

void ast_id_list_init(Arena* arena, AstNodeIdList* list, u32 capacity) {
    assert(capacity > 0);
    assert(capacity < U32_MAX);
    assert((capacity & (capacity - 1)) == 0); // assert that capacity is a power of two

    list -> ids = arena_alloc(arena, sizeof(AstNodeId) * capacity);
    list -> count = 0;
    list -> capacity = capacity;
}

void ast_id_list_append(AstNodeIdList* list, Ast* ast, AstNodeId id) {
    if (UNLIKELY(list -> count >= list -> capacity)) {
        u64 old_size = list -> capacity * sizeof(AstNodeId);
        u64 new_size = old_size * 2;

        list -> ids = arena_realloc(&ast -> gpa, list -> ids, old_size, new_size);
        list -> capacity *= 2;

        debug_printf("list -> ids realloc from %lu to %lu bytes", old_size, new_size);
    }

    list -> ids[list -> count++] = id;
}
