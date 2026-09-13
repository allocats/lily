#ifndef LILY_INTERPRETER_H
#define LILY_INTERPRETER_H

#include "files/types.h"
#include "ids.h"
#include "vm/types.h"

VmResult evaluate_const_expr(File* file, AstNodeId id);

#endif // !LILY_INTERPRETER_H
