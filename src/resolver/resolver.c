#include "resolver/resolver.h"
#include "resolver/enums.h"
#include "resolver/types.h"
#include "utils/macros.h"

static bool resolver_is_eq(ResolveItem* a, ResolveItem* b) {
    if (a -> kind != b -> kind || a -> module_id != b -> module_id) {
        return false;
    }

    switch (a -> kind) {
        case RESOLVE_SYMBOL:
            return a -> as.symbol == b -> as.symbol;

        case RESOLVE_TYPE:
            return a -> as.type == b -> as.type;
    }

    return false;
}

bool resolver_stack_push(ResolveStack* stack, ResolveItem item) {
    if (UNLIKELY(stack -> top >= RESOLVE_STACK_MAX)) {
        return false;
    }

    stack -> items[stack -> top++] = item; 

    return true;
}

void resolver_stack_pop(ResolveStack* stack) {
    stack -> top--;
}

i32 resolver_stack_find(ResolveStack* stack, ResolveItem item) {
    for (u32 i = 0; i < stack -> top; i++) {
        if (resolver_is_eq(&stack -> items[i], &item)) {
            return i;
        }
    }

    return -1;
}
