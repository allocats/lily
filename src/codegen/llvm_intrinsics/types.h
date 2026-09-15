#ifndef LILY_CODEGEN_LLVM_INTRINSICS_TYPES_H
#define LILY_CODEGEN_LLVM_INTRINSICS_TYPES_H

#include "ids.h"
#include "meowrena/meowrena.h"
#include "utils/types.h"

typedef struct {
    u32 hash; // hash(function_name, symbol_function, parameter types)
    IntrinsicId id;
} IntrinsicBucket;

typedef struct {
    StringId function_name; // = intern_cstr("memset");
    StringId intrinsic_name; // = intern_cstr("llvm.memset.p0");

    TypeId parameters[3];
    u32 parameter_count;

    TypeId return_type;
} IntrinsicEntry;

typedef struct {
    Arena arena;

    IntrinsicEntry* entries;
    u32 entry_count;

    IntrinsicBucket* buckets;
    u32 bucket_count;
    u32 bucket_capacity; // for hash & (capacity - 1)
} IntrinsicTable;

#endif // !LILY_CODEGEN_LLVM_INTRINSICS_TYPES_H
