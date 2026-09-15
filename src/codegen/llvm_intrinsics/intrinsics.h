#ifndef LILY_CODEGEN_LLVM_INTRINSICS_H
#define LILY_CODEGEN_LLVM_INTRINSICS_H

#include "codegen/llvm_intrinsics/types.h"
#include "ids.h"

void intrinsic_table_init(IntrinsicTable* table);
void intrinsic_table_destroy(IntrinsicTable* table);

IntrinsicId intrinsic_intern(IntrinsicTable* table, IntrinsicEntry intrinsic);
IntrinsicId intrinsic_lookup(IntrinsicTable* table, StringId fn, TypeId ret_type, u32 param_count, TypeId params[]);

#endif // !LILY_CODEGEN_LLVM_INTRINSICS_H
