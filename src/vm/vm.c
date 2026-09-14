#include "token/types.h"
#include "utils/macros.h" 
#include "utils/types.h"
#include "vm/types.h"
#include "vm/vm.h"

#include <assert.h>

VmResult vm_run(VirtualMachine* vm) {
    assert(vm -> frame_count > 0 && "vm_run called with no active frame — forgot vm_push_frame?");

    while (true) {
        if (vm -> frame_count >= VM_FRAME_MAX) {
            return vm_error(VM_ERR_FRAME_STACK_OVERFLOW);
        }

        VmFrame* frame = &vm -> frames[vm -> frame_count - 1];
        VmOpCode instr = *frame -> pc++;

        switch (instr) {
            case OP_PUSH_CONST: {
                u16 index = vm_read_u16(frame -> pc);

                frame -> pc += 2;

                if (!vm_push(vm, frame -> chunk -> constants[index])) {
                    return vm_error(VM_ERR_STACK_OVERFLOW);
                }
            } break; 

            case OP_POP: {
                vm_pop(vm);
            } break;

            case OP_ADD: 
            case OP_SUB: 
            case OP_MUL: 
            case OP_MOD: 
            case OP_SHL:
            case OP_SHR: {
                VmValue rhs = vm_pop(vm);
                VmValue lhs = vm_pop(vm);
                
                if (lhs.kind != rhs.kind) {
                    return vm_error(VM_ERR_TYPE_MISMATCH);
                }

                VmValue result = {
                    .kind = lhs.kind,
                };

                switch (instr) {
                    case OP_ADD: VM_NUMERIC_BINARY_ARITHMETIC(lhs, rhs, result,  +) break;
                    case OP_SUB: VM_NUMERIC_BINARY_ARITHMETIC(lhs, rhs, result,  -) break;
                    case OP_MUL: VM_NUMERIC_BINARY_ARITHMETIC(lhs, rhs, result,  *) break;
                    case OP_MOD: VM_INTEGER_BINARY_ARITHMETIC(lhs, rhs, result,  %) break;
                    case OP_SHL: VM_INTEGER_BINARY_ARITHMETIC(lhs, rhs, result, <<) break;
                    case OP_SHR: VM_INTEGER_BINARY_ARITHMETIC(lhs, rhs, result, >>) break;

                    default:
                        UNREACHABLE("Binary airthmetic")
                }

                vm_push(vm, result);
            } break;

            case OP_DIV: {
                VmValue rhs = vm_pop(vm);
                VmValue lhs = vm_pop(vm);

                if (lhs.kind != rhs.kind) {
                    return vm_error(VM_ERR_TYPE_MISMATCH);
                }

                if (vm_is_value_zero(rhs)) {
                    return vm_error(VM_ERR_DIV_BY_ZERO);
                }

                VmValue result = {
                    .kind = lhs.kind
                };

                VM_NUMERIC_BINARY_ARITHMETIC(lhs, rhs, result, /);

                vm_push(vm, result);
            } break;

            case OP_EQ:
            case OP_NEQ:
            case OP_LT:
            case OP_LTE:
            case OP_GT:
            case OP_GTE: {
                VmValue rhs = vm_pop(vm);
                VmValue lhs = vm_pop(vm);

                if (lhs.kind != rhs.kind) {
                    return vm_error(VM_ERR_TYPE_MISMATCH);
                }

                bool result;

                switch (instr) {
                    case OP_EQ:     VM_NUMERIC_BINARY_COMPARE(lhs, rhs, result, ==) break; 
                    case OP_NEQ:    VM_NUMERIC_BINARY_COMPARE(lhs, rhs, result, !=) break; 
                    case OP_LT:     VM_NUMERIC_BINARY_COMPARE(lhs, rhs, result, < ) break;
                    case OP_LTE:    VM_NUMERIC_BINARY_COMPARE(lhs, rhs, result, <=) break; 
                    case OP_GT:     VM_NUMERIC_BINARY_COMPARE(lhs, rhs, result, > ) break;
                    case OP_GTE:    VM_NUMERIC_BINARY_COMPARE(lhs, rhs, result, >=) break; 

                    default:
                        UNREACHABLE("Binary comparison")
                }

                VmValue result_value = {
                    .kind = VM_VALUE_BOOL,
                    .as.boolean = result
                };

                vm_push(vm, result_value);
            } break;

            case OP_NOT: {
                VmValue value = vm_pop(vm);

                if (value.kind != VM_VALUE_BOOL) {
                    return vm_error(VM_ERR_TYPE_MISMATCH);
                }

                VmValue result = {
                    .kind = VM_VALUE_BOOL,
                    .as.boolean = (!value.as.boolean)
                };

                vm_push(vm, result);
            } break;

            case OP_NEG: {
                VmValue value = vm_pop(vm);

                VmValue result = { 
                    .kind = value.kind
                };

                switch (value.kind) {
                    case VM_VALUE_I8:    result.as.i8    = -value.as.i8;    break;
                    case VM_VALUE_I16:   result.as.i16   = -value.as.i16;   break;
                    case VM_VALUE_I32:   result.as.i32   = -value.as.i32;   break;
                    case VM_VALUE_I64:   result.as.i64   = -value.as.i64;   break;
                    case VM_VALUE_ISIZE: result.as.isize = -value.as.isize; break;

                    case VM_VALUE_F32:   result.as.f32   = -value.as.f32;   break;
                    case VM_VALUE_F64:   result.as.f64   = -value.as.f64;   break;

                    default:
                        return vm_error(VM_ERR_TYPE_MISMATCH);
                }

                vm_push(vm, result);
            } break;

            case OP_GET_LOCAL: {
                u16 index = vm_read_u16(frame -> pc);

                frame -> pc += 2;

                if (!vm_push(vm, frame -> locals[index])) {
                    return vm_error(VM_ERR_STACK_OVERFLOW);
                }
            } break;

            case OP_SET_LOCAL: {
                u16 index = vm_read_u16(frame -> pc);

                frame -> pc += 2;

                VmValue value = vm_pop(vm);

                frame -> locals[index] = value;
            } break;

            case OP_JUMP: {
                u16 offset = vm_read_u16(frame -> pc);

                frame -> pc = frame -> chunk -> code + offset;
            } break;

            case OP_JUMP_IF_FALSE: {
                u16 offset = vm_read_u16(frame -> pc);

                frame -> pc += 2;

                VmValue condition = vm_pop(vm);

                if (condition.kind != VM_VALUE_BOOL) {
                    return vm_error(VM_ERR_TYPE_MISMATCH);
                }

                if (condition.as.boolean == false) {
                    frame -> pc = frame -> chunk -> code + offset;
                }
            } break;

            case OP_RETURN: {
                VmValue result = vm_pop(vm);

                vm_pop_frame(vm);

                if (vm -> frame_count == 0) {
                    return vm_ok(result);
                }

                if (!vm_push(vm, result)) {
                    return vm_error(VM_ERR_STACK_OVERFLOW);
                }
            } break;

            // TODO: function calls

            default:
                return vm_error(VM_ERR_INVALID_OP_CODE);
        }
    }
}

inline void vm_init(VirtualMachine* vm) {
    arena_init(&vm -> arena, ARENA_KB(4), ALIGN_DEFAULT);

    vm -> frame_count = 0;
    vm -> stack_top = 0;

    arena_memset(vm -> frames, 0, sizeof(vm -> frames));
    arena_memset(vm -> stack, 0, sizeof(vm -> stack));
}

inline void vm_destroy(VirtualMachine* vm) {
    arena_destroy(&vm -> arena);
}

inline bool vm_push(VirtualMachine* vm, VmValue value) {
    if (vm -> stack_top >= VM_STACK_MAX) {
        return false;
    }

    vm -> stack[vm -> stack_top++] = value;

    return true;
}

inline VmValue vm_pop(VirtualMachine* vm) {
    assert(vm -> stack_top > 0);
    return vm -> stack[--vm -> stack_top];
}

inline bool vm_push_frame(VirtualMachine* vm, VmChunk* chunk) {
    if (vm -> frame_count >= VM_FRAME_MAX) {
        return false;
    }

    u32 local_count = chunk -> local_count;

    VmFrame* frame = &vm -> frames[vm -> frame_count++];

    frame -> chunk = chunk;
    frame -> pc = chunk -> code;
    frame -> local_count = local_count;
    frame -> locals = local_count != 0 ? arena_calloc(&vm -> arena, local_count * sizeof(VmValue)) : null;

    return true;
}

inline VmFrame vm_pop_frame(VirtualMachine* vm) {
    assert(vm -> frame_count > 0);
    return vm -> frames[--vm -> frame_count];
}

void vm_chunk_init(VirtualMachine* vm, VmChunk* chunk) {
    chunk -> code_nodes = arena_alloc(&vm -> arena, 64 * sizeof(AstNodeId));

    chunk -> code = arena_alloc(&vm -> arena, 64);
    chunk -> code_count = 0;
    chunk -> code_capacity = 64;

    chunk -> constants = arena_alloc(&vm -> arena, 8 * sizeof(VmValue));
    chunk -> constant_count = 0;
    chunk -> constant_capacity = 8;

    chunk -> local_count = 0;
}

u16 vm_chunk_add_constant(VirtualMachine* vm, VmChunk* chunk, VmValue value) {
    if (UNLIKELY(chunk -> constant_count >= chunk -> constant_capacity)) {
        u64 old_capacity = chunk -> constant_capacity;
        u64 new_capacity = old_capacity == 0 ? 8 : old_capacity * 2;

        u64 old_size = old_capacity * sizeof(VmValue);
        u64 new_size = new_capacity * sizeof(VmValue);

        chunk -> constants = arena_realloc(&vm -> arena, chunk -> constants, old_size, new_size);
        chunk -> constant_capacity = new_capacity;
    }

    assert(chunk -> constant_count < U16_MAX && "too many constants in one chunk (OP_PUSH_CONST operand is u16)");

    u16 index = (u16) chunk -> constant_count++;

    chunk -> constants[index] = value;

    return index;
}

inline VmResult vm_ok(VmValue value) {
    return (VmResult) {
        .kind = VM_OK,
        .value = value,
    };
}

inline VmResult vm_error(VmResultKind kind) {
    return (VmResult) {
        .kind = kind,
        .value = {0}
    };
}

bool vm_is_value_zero(VmValue value) {
    switch (value.kind) {
        case VM_VALUE_U8:    return value.as.u8 == 0;
        case VM_VALUE_U16:   return value.as.u16 == 0;
        case VM_VALUE_U32:   return value.as.u32 == 0;
        case VM_VALUE_U64:   return value.as.u64 == 0;
        case VM_VALUE_USIZE: return value.as.usize == 0;

        case VM_VALUE_I8:    return value.as.i8 == 0;
        case VM_VALUE_I16:   return value.as.i16 == 0;
        case VM_VALUE_I32:   return value.as.i32 == 0;
        case VM_VALUE_I64:   return value.as.i64 == 0;
        case VM_VALUE_ISIZE: return value.as.isize == 0;

        case VM_VALUE_F32:   return value.as.f32 == 0.0f;
        case VM_VALUE_F64:   return value.as.f64 == 0.0;

        default:
            return false;
    }
}

void vm_emit_u8(VirtualMachine* vm, VmChunk* chunk, u8 byte, AstNodeId node_id) {
    if (UNLIKELY(chunk -> code_count >= chunk -> code_capacity)) {
        u64 old_size = chunk -> code_capacity;
        u64 new_size = old_size == 0 ? 8 : old_size * 2;

        chunk -> code = arena_realloc(&vm -> arena, chunk -> code, old_size, new_size);
        chunk -> code_nodes = arena_realloc(
            &vm -> arena,
            chunk -> code_nodes,
            old_size * sizeof(AstNodeId),
            new_size * sizeof(AstNodeId)
        );
        chunk -> code_capacity *= 2;
    }

    u32 index = chunk -> code_count++;

    chunk -> code[index] = byte;
    chunk -> code_nodes[index] = node_id;
}

inline void vm_emit_u16(VirtualMachine* vm, VmChunk* chunk, u8 byte_1, u8 byte_2, AstNodeId node_id) {
    vm_emit_u8(vm, chunk, byte_1, node_id);
    vm_emit_u8(vm, chunk, byte_2, node_id);
}

inline u16 vm_read_u16(u8* pc) {
    return (u16)((pc[0] << 8) | pc[1]);
}
