#ifndef LILY_VM_H
#define LILY_VM_H

#include "vm/types.h"

#define VM_NUMERIC_BINARY_ARITHMETIC(a, b, out, OP)                                     \
    switch ((a).kind) {                                                                 \
        case VM_VALUE_U8:       (out).as.u8    = (a).as.u8    OP (b).as.u8;    break;   \
        case VM_VALUE_U16:      (out).as.u16   = (a).as.u16   OP (b).as.u16;   break;   \
        case VM_VALUE_U32:      (out).as.u32   = (a).as.u32   OP (b).as.u32;   break;   \
        case VM_VALUE_U64:      (out).as.u64   = (a).as.u64   OP (b).as.u64;   break;   \
        case VM_VALUE_USIZE:    (out).as.usize = (a).as.usize OP (b).as.usize; break;   \
        case VM_VALUE_I8:       (out).as.i8    = (a).as.i8    OP (b).as.i8;    break;   \
        case VM_VALUE_I16:      (out).as.i16   = (a).as.i16   OP (b).as.i16;   break;   \
        case VM_VALUE_I32:      (out).as.i32   = (a).as.i32   OP (b).as.i32;   break;   \
        case VM_VALUE_I64:      (out).as.i64   = (a).as.i64   OP (b).as.i64;   break;   \
        case VM_VALUE_ISIZE:    (out).as.isize = (a).as.isize OP (b).as.isize; break;   \
        case VM_VALUE_F32:      (out).as.f32   = (a).as.f32   OP (b).as.f32;   break;   \
        case VM_VALUE_F64:      (out).as.f64   = (a).as.f64   OP (b).as.f64;   break;   \
        default:                return vm_error(VM_ERR_TYPE_MISMATCH);                  \
    }

#define VM_INTEGER_BINARY_ARITHMETIC(a, b, out, OP)                                     \
    switch ((a).kind) {                                                                 \
        case VM_VALUE_U8:       (out).as.u8    = (a).as.u8    OP (b).as.u8;    break;   \
        case VM_VALUE_U16:      (out).as.u16   = (a).as.u16   OP (b).as.u16;   break;   \
        case VM_VALUE_U32:      (out).as.u32   = (a).as.u32   OP (b).as.u32;   break;   \
        case VM_VALUE_U64:      (out).as.u64   = (a).as.u64   OP (b).as.u64;   break;   \
        case VM_VALUE_USIZE:    (out).as.usize = (a).as.usize OP (b).as.usize; break;   \
        case VM_VALUE_I8:       (out).as.i8    = (a).as.i8    OP (b).as.i8;    break;   \
        case VM_VALUE_I16:      (out).as.i16   = (a).as.i16   OP (b).as.i16;   break;   \
        case VM_VALUE_I32:      (out).as.i32   = (a).as.i32   OP (b).as.i32;   break;   \
        case VM_VALUE_I64:      (out).as.i64   = (a).as.i64   OP (b).as.i64;   break;   \
        case VM_VALUE_ISIZE:    (out).as.isize = (a).as.isize OP (b).as.isize; break;   \
        default:                return vm_error(VM_ERR_TYPE_MISMATCH);                  \
    }

#define VM_NUMERIC_BINARY_COMPARE(a, b, out, OP)                                \
    switch ((a).kind) {                                                         \
        case VM_VALUE_U8:       (out) = (a).as.u8    OP (b).as.u8;    break;    \
        case VM_VALUE_U16:      (out) = (a).as.u16   OP (b).as.u16;   break;    \
        case VM_VALUE_U32:      (out) = (a).as.u32   OP (b).as.u32;   break;    \
        case VM_VALUE_U64:      (out) = (a).as.u64   OP (b).as.u64;   break;    \
        case VM_VALUE_USIZE:    (out) = (a).as.usize OP (b).as.usize; break;    \
        case VM_VALUE_I8:       (out) = (a).as.i8    OP (b).as.i8;    break;    \
        case VM_VALUE_I16:      (out) = (a).as.i16   OP (b).as.i16;   break;    \
        case VM_VALUE_I32:      (out) = (a).as.i32   OP (b).as.i32;   break;    \
        case VM_VALUE_I64:      (out) = (a).as.i64   OP (b).as.i64;   break;    \
        case VM_VALUE_ISIZE:    (out) = (a).as.isize OP (b).as.isize; break;    \
        case VM_VALUE_F32:      (out) = (a).as.f32   OP (b).as.f32;   break;    \
        case VM_VALUE_F64:      (out) = (a).as.f64   OP (b).as.f64;   break;    \
        default:                return vm_error(VM_ERR_TYPE_MISMATCH);          \
    }

void vm_init(VirtualMachine* vm);
void vm_destroy(VirtualMachine* vm);
VmResult vm_run(VirtualMachine* vm);

bool    vm_push(VirtualMachine* vm, VmValue value);
VmValue vm_pop(VirtualMachine* vm);

bool    vm_push_frame(VirtualMachine* vm, VmChunk* chunk);
VmFrame vm_pop_frame(VirtualMachine* vm);

void vm_chunk_init(VirtualMachine* vm, VmChunk* chunk);
u16  vm_chunk_add_constant(VirtualMachine* vm, VmChunk* chunk, VmValue value);

VmResult vm_ok(VmValue value);
VmResult vm_error(VmResultKind kind);

bool vm_is_value_zero(VmValue value);

void vm_emit_u8(VirtualMachine* vm, VmChunk* chunk, u8 byte, AstNodeId node_id);
void vm_emit_u16(VirtualMachine* vm, VmChunk* chunk, u8 byte_1, u8 byte_2, AstNodeId node_id);
u16  vm_read_u16(u8* pc);

#endif // !LILY_VM_H
