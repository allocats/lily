#ifndef LILY_CODEGEN_TYPES_H
#define LILY_CODEGEN_TYPES_H

#include "codegen/llvm_intrinsics/types.h"
#include "files/types.h"

#include <llvm-c/Types.h>

static constexpr u32 defer_stack_init_cap = 4;

typedef enum {
    CODEGEN_ERROR = 0,
    CODEGEN_FALLTHROUGH,
    CODEGEN_TERMINATED,
} CodegenResult;

typedef enum {
    VA_LIST_NONE = 0,
    VA_LIST_START,
    VA_LIST_END,
    VA_LIST_DONE
} VaListState;

typedef struct {
    LLVMTypeRef va_list_type;
    LLVMValueRef ap;

    VaListState state;
} VaListCtx;

typedef struct DeferStack DeferStack;
typedef struct DeferStack {
    AstNodeId* ids;
    u32 count;
    u32 capacity;

    DeferStack* previous;
} DeferStack;

typedef struct {
    Arena arena;

    DeferStack* stack;
} DeferList;

typedef struct LoopCtx LoopCtx;
typedef struct LoopCtx {
    LLVMBasicBlockRef break_target;
    LLVMBasicBlockRef continue_target;

    DeferStack* defer_boundary;

    LoopCtx* previous;
} LoopCtx;

typedef struct {
    Arena map_arena;
    Arena scratch;

    File* file;

    IntrinsicTable intrinsics;

    LLVMContextRef ctx;
    LLVMModuleRef module;
    LLVMBuilderRef builder;

    LLVMDIBuilderRef debug_builder;
    LLVMMetadataRef debug_unit;
    LLVMMetadataRef debug_file_metadata;
    LLVMMetadataRef debug_scope;

    LLVMMetadataRef debug_fn;

    LLVMValueRef fn;

    LoopCtx* loop_ctx;
    DeferList defer_list;

    LLVMValueRef* symbol_map;
    LLVMValueRef* string_map;
    LLVMTypeRef*  type_map;

    LLVMMetadataRef* dwarf_type_map;

    VaListCtx va_ctx;

    bool is_release_mode;
} CodegenCtx;

#endif // !LILY_CODEGEN_TYPES_H
