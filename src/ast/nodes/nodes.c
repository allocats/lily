#include "ast/nodes/nodes.h"
#include "utils/debug.h"
#include "utils/macros.h"

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
