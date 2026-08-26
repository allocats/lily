#include "types.h"
#include "resolver_stack/stack.h"
#include "resolver_stack/types.h"
#include "utils/macros.h"

static ResolverStack resolver_stack = {0};

static bool resolver_is_eq(ResolveQuery a, ResolveQuery b) {
    if (a.kind != b.kind || a.file_id != b.file_id) {
        return false;
    }

    switch (a.kind) {
        case QUERY_SYMBOL:
            return a.as.symbol == b.as.symbol;

        case QUERY_TYPE:
            return a.as.type == b.as.type;
    }

    return false;
}

bool resolver_stack_push(ResolveQuery item) {
    if (UNLIKELY(resolver_stack.top >= RESOLVER_STACK_MAX)) {
        return false;
    }

    resolver_stack.items[resolver_stack.top++] = item; 

    return true;
}

void resolver_stack_pop() {
    resolver_stack.top -= 1;
}

i32 resolver_stack_find(ResolveQuery item) {
    for (u32 i = 0; i < resolver_stack.top; i++) {
        if (resolver_is_eq(resolver_stack.items[i], item)) {
            return i;
        }
    }

    return RESOLVER_STACK_MAX;
}
