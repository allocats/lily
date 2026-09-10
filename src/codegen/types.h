#ifndef LILY_CODEGEN_TYPES_H
#define LILY_CODEGEN_TYPES_H

#include "files/types.h"

#include <llvm-c/Types.h>

typedef enum {
    VA_LIST_NONE,
    VA_LIST_START,
    VA_LIST_END,
    VA_LIST_DONE
} VaListState;

typedef struct {
    LLVMTypeRef va_list_type;
    LLVMValueRef ap;

    VaListState state;
} VaListCtx;

typedef struct {
    Arena arena;

    File* file;

    LLVMContextRef ctx;
    LLVMModuleRef module;
    LLVMBuilderRef builder;

    LLVMValueRef fn;

    LLVMValueRef* symbol_values;
    LLVMValueRef* string_values;

    VaListCtx va_ctx;
} CodegenCtx;

#endif // !LILY_CODEGEN_TYPES_H
