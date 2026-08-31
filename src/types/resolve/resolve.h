#ifndef LILY_TYPES_RESOLVE_H
#define LILY_TYPES_RESOLVE_H

#include "ids.h"

void resolve_top_level_types(void);

TypeId resolve_type_expr(FileId file_id, AstNodeId expr_id);

#endif // !LILY_TYPES_RESOLVE_H
