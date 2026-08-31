#include "types.h"
#include "resolver_stack/stack.h"
#include "resolver_stack/types.h"
#include "utils/macros.h"

static ResolverStack resolver_stack = {0};

static bool resolver_is_eq(ResolveQuery a, ResolveQuery b) {
    if (a.kind != b.kind) {
        return false;
    }

    switch (a.kind) {
        case QUERY_SYMBOL:
            return a.as.symbol == b.as.symbol;
    }

    return false;
}

bool resolver_stack_push(ResolveQuery query) {
    if (UNLIKELY(resolver_stack.top >= RESOLVER_STACK_MAX)) {
        return false;
    }

    resolver_stack.items[resolver_stack.top++] = query; 

    return true;
}

void resolver_stack_pop() {
    resolver_stack.top -= 1;
}

i32 resolver_stack_find(ResolveQuery query) {
    for (u32 i = 0; i < resolver_stack.top; i++) {
        if (resolver_is_eq(resolver_stack.items[i], query)) {
            return i;
        }
    }

    return RESOLVER_STACK_MAX;
}
