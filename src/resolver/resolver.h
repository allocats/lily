#ifndef LILY_RESOLVER_H
#define LILY_RESOLVER_H

#include "resolver/types.h"

bool resolver_stack_push(ResolveStack* stack, ResolveItem item);
void resolver_stack_pop(ResolveStack* stack);

i32 resolver_stack_find(ResolveStack* stack, ResolveItem item);

#endif // !LILY_RESOLVER_H
