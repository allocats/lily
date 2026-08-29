#ifndef LILY_RESOLVER_STACK_H
#define LILY_RESOLVER_STACK_H

#include "resolver_stack/types.h"

bool resolver_stack_push(ResolveQuery query);
void resolver_stack_pop();

i32 resolver_stack_find(ResolveQuery query);

#endif // !LILY_RESOLVER_STACK_H
