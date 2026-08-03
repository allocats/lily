#ifndef LILY_QUERY_H
#define LILY_QUERY_H

#include "query/types.h"

bool query_stack_push(QueryStack* stack, Query query);
void query_stack_pop(QueryStack* stack);

i32 query_stack_find(QueryStack* stack, Query query);

#endif // !LILY_QUERY_H
