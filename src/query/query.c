#include "query/query.h"
#include "utils/macros.h"

static bool query_eq(Query* a, Query* b) {
    if (a -> kind != b -> kind || a -> module_id != b -> module_id) {
        return false;
    }

    switch (a -> kind) {
        case QUERY_TYPE:       
            return a -> as.type_id == b -> as.type_id;

        case QUERY_SYMBOL:     
            return a -> as.symbol_id == b -> as.symbol_id;

        case QUERT_CONST_EVAL: 
            return a -> as.node_id == b -> as.node_id;
    }

    return false;
}

bool query_stack_push(QueryStack* stack, Query query) {
    if (UNLIKELY(stack -> top >= QUERY_STACK_MAX)) {
        return false;
    }

    stack -> items[stack -> top++] = query;

    return true;
}

void query_stack_pop(QueryStack* stack) {
    stack -> top--;
}

i32 query_stack_find(QueryStack* stack, Query query) {
    for (u32 i = 0; i < stack -> top; i++) {
        if (query_eq(&stack -> items[i], &query)) {
            return (i32) i;
        }
    }

    return -1;
}
