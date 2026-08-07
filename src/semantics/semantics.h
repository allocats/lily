#ifndef LILY_SEMANTICS_H
#define LILY_SEMANTICS_H

#include "ids.h"
#include "modules/types.h"
#include "resolver/types.h"

TypeId resolve_expression(Resolver* r, Module* module, AstNodeId expr_id, TypeId expected_type);

#endif // !LILY_SEMANTICS_H
