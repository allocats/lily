#ifndef LILY_VM_TYPES_H
#define LILY_VM_TYPES_H

#include "files/types.h"
#include "meowrena/meowrena.h"
#include "utils/types.h"

static constexpr u64 VM_FRAME_MAX = 128;
static constexpr u64 VM_STACK_MAX = VM_FRAME_MAX * U8_MAX;

typedef enum {
    VM_VALUE_VOID = 0,
    VM_VALUE_BOOL,
    VM_VALUE_CHAR,

    VM_VALUE_U8,
    VM_VALUE_U16,
    VM_VALUE_U32,
    VM_VALUE_U64,
    VM_VALUE_USIZE,

    VM_VALUE_I8,
    VM_VALUE_I16,
    VM_VALUE_I32,
    VM_VALUE_I64,
    VM_VALUE_ISIZE,

    VM_VALUE_F32,
    VM_VALUE_F64,
} VmValueKind; 

typedef struct { 
    VmValueKind kind; 

    union {
        bool boolean; 
        char character; 

        u8 u8; 
        u16 u16; 
        u32 u32; 
        u64 u64; 
        usize usize; 

        i8 i8; 
        i16 i16; 
        i32 i32; 
        i64 i64; 
        isize isize; 
        
        f32 f32; 
        f64 f64; 
    } as; 
} VmValue;

typedef enum {
    VM_OK = 0,
    VM_ERR_DIV_BY_ZERO,
    VM_ERR_INVALID_OP_CODE,
    VM_ERR_NOT_CONST_EVALUABLE,
    VM_ERR_FRAME_STACK_OVERFLOW,
    VM_ERR_STACK_OVERFLOW,
    VM_ERR_STACK_UNDERFLOW,
    VM_ERR_TYPE_MISMATCH,
} VmResultKind;

typedef struct {
    VmResultKind kind;
    VmValue value;
} VmResult;

typedef enum {
    OP_INVALID = 0,

    OP_PUSH_CONST,
    OP_POP,

    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_EQ, OP_NEQ, OP_LT, OP_LTE, OP_GT, OP_GTE,
    OP_SHL, OP_SHR,
    OP_NOT, 
    OP_NEG, 
    
    OP_GET_LOCAL,
    OP_SET_LOCAL,

    OP_JUMP,
    OP_JUMP_IF_FALSE,

    OP_CALL,
    OP_RETURN
} VmOpCode;


typedef struct {
    u32 local_count; 

    // byte-code
    AstNodeId* code_nodes;

    u8* code;
    u32 code_count;
    u32 code_capacity;

    VmValue* constants;
    u32 constant_count;
    u32 constant_capacity;
} VmChunk;

typedef struct { 
    VmChunk* chunk;

    // program counter
    u8* pc;

    VmValue* locals; 
    u32 local_count; 
} VmFrame; 

typedef struct { 
    Arena arena; 
    
    VmValue stack[VM_STACK_MAX]; 
    u32 stack_top; 

    VmFrame frames[VM_FRAME_MAX]; 
    u32 frame_count; 
} VirtualMachine;

#endif // !LILY_VM_TYPES_H
