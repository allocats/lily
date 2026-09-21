#include "ast/nodes/types.h"
#include "driver/types.h"
#include "ids.h"
#include "symbols/symbols/types.h"
#include "symbols/table/table.h"
#include "token/types.h"
#include "types/entries/entries.h"
#include "utils/macros.h"
#include "vm/types.h"
#include "vm/vm.h"

extern DriverCtx driver;

static const VmOpCode operator_op_code_lut[TOKEN_KIND_COUNT] = {
    [TOK_PLUS]      = OP_ADD,
    [TOK_MINUS]     = OP_SUB,
    [TOK_STAR]      = OP_MUL,
    [TOK_SLASH]     = OP_DIV,
    [TOK_PERCENT]   = OP_MOD,

    [TOK_SHL]       = OP_SHL,
    [TOK_SHR]       = OP_SHR,

    [TOK_EQ_EQ]     = OP_EQ,
    [TOK_BANG_EQ]   = OP_NEQ,
    [TOK_LT]        = OP_LT,
    [TOK_LT_EQ]     = OP_LTE,
    [TOK_GT]        = OP_GT,
    [TOK_GT_EQ]     = OP_GTE,
};;

static bool compile_expr(VirtualMachine* vm, VmChunk* chunk, File* file, AstNodeId id);
static bool compile_binary_op(VirtualMachine* vm, VmChunk* chunk, File* file, AstNode* node);
static bool compile_unary_op(VirtualMachine* vm, VmChunk* chunk, File* file, AstNode* node);
static bool compile_literal(VirtualMachine* vm, VmChunk* chunk, File* file, AstNode* node);
static bool compile_identifier(VirtualMachine* vm, VmChunk* chunk, File* file, AstNode* node);
static bool compile_struct_literal(VirtualMachine* vm, VmChunk* chunk, File* file, AstNode* node);
static bool compile_function_call(VirtualMachine* vm, VmChunk* chunk, File* file, AstNode* node);

static void emit_u16_operand(VirtualMachine* vm, VmChunk* chunk, u16 value, AstNodeId node_id) {
    vm_emit_u16(vm, chunk, (u8)(value >> 8), (u8)(value & 0xFF), node_id);
}

VmResult evaluate_const_expr(File* file, AstNodeId id) {
    VmChunk chunk = {0};

    vm_chunk_init(&driver.vm, &chunk);

    if (!compile_expr(&driver.vm, &chunk, file, id)) {
        return vm_error(VM_ERR_NOT_CONST_EVALUABLE);
    }

    vm_emit_u8(&driver.vm, &chunk, OP_RETURN, id);

    if (!vm_push_frame(&driver.vm, &chunk)) {
        return vm_error(VM_ERR_STACK_OVERFLOW);
    }

    return vm_run(&driver.vm);
}

static bool compile_expr(VirtualMachine* vm, VmChunk* chunk, File* file, AstNodeId id) {
    AstNode* node = &file -> ast.nodes[id];

    switch (node -> kind) {
        case AST_BINARY_OP:
            return compile_binary_op(vm, chunk, file, node);

        case AST_UNARY_OP:
            return compile_unary_op(vm, chunk, file, node);

        case AST_LITERAL:
            return compile_literal(vm, chunk, file, node);

        case AST_IDENTIFIER:
            return compile_identifier(vm, chunk, file, node);

        case AST_STRUCT_LITERAL:
            return compile_struct_literal(vm, chunk, file, node);

        case AST_FUNCTION_CALL:
            return compile_function_call(vm, chunk, file, node);

        // TODO: a lot. need identifiers, calls etc. need a globals map as well

        default:
            return false;
    }
}

static bool compile_binary_op(VirtualMachine* vm, VmChunk* chunk, File* file, AstNode* node) {
    if (!compile_expr(vm, chunk, file, node -> as.binary_op.left)) {
        return false;
    }

    if (!compile_expr(vm, chunk, file, node -> as.binary_op.right)) {
        return false;
    }

    VmOpCode op = operator_op_code_lut[node -> as.binary_op.op];

    if (op == OP_INVALID) {
        return false;
    }

    vm_emit_u8(vm, chunk, op, node -> id);

    return true;
}

static bool compile_unary_op(VirtualMachine* vm, VmChunk* chunk, File* file, AstNode* node) {
    if (!compile_expr(vm, chunk, file, node -> as.unary_op.operand)) {
        return false;
    }

    switch (node -> as.unary_op.op) {
        case TOK_MINUS: 
            vm_emit_u8(vm, chunk, OP_NEG, node -> id); 
            return true;

        case TOK_BANG:  
            vm_emit_u8(vm, chunk, OP_NOT, node -> id); 
            return true;

        default:        
            return false;
    }
}

static bool compile_literal(VirtualMachine* vm, VmChunk* chunk, File* file, AstNode* node) {
    UNUSED(file);

    AstLiteral* literal = &node -> as.literal;
    VmValue value = {0};

    switch (literal-> kind) {
        // TODO: switch on the literal's type
        case LITERAL_INTEGER: {
            value = (VmValue) { .kind = VM_VALUE_I64, .as.u64 = literal -> as.integer };
            break;

            if (is_type_unsigned_int(node -> resolved_type)) {
                value = (VmValue) {
                    .kind = VM_VALUE_U64,
                    .as.u64 = literal -> as.integer, // w union hack
                };
            } else if (is_type_signed_int(node -> resolved_type)) {
                value = (VmValue) {
                    .kind = VM_VALUE_I64,
                    .as.i64 = literal -> as.integer, // w union hack
                };
            } 
        } break;

        case LITERAL_FLOAT:
            value = (VmValue){ .kind = VM_VALUE_F64, .as.f64 = literal-> as.floating };
            break;

        case LITERAL_BOOL:
            value = (VmValue){ .kind = VM_VALUE_BOOL, .as.boolean = literal-> as.boolean };
            break;

        case LITERAL_CHAR:
            value = (VmValue){ .kind = VM_VALUE_CHAR, .as.character = (char) literal-> as.character };
            break;

        // TODO: strings & pointers
        default:
            return false;
    }

    u16 index = vm_chunk_add_constant(vm, chunk, value);

    vm_emit_u8(vm, chunk, OP_PUSH_CONST, node -> id);

    emit_u16_operand(vm, chunk, index, node -> id);

    return true;
}

static bool compile_identifier(VirtualMachine* vm, VmChunk* chunk, File* file, AstNode* node) {
    UNUSED(file);

    SymbolId symbol_id = node -> resolved_symbol;

    if (symbol_id == SYMBOL_ID_NONE) {
        return false;
    }

    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(symbol_id);

    if (symbol -> kind != SYMBOL_VARIABLE) {
        return false;
    }

    VmValue value = symbol -> as.variable_symbol.compile_time_const_value;

    if (value.kind == VM_VALUE_VOID) {
        return false;
    }

    u16 index = vm_chunk_add_constant(vm, chunk, value);

    vm_emit_u8(vm, chunk, OP_PUSH_CONST, node -> id);

    emit_u16_operand(vm, chunk, index, node -> id);

    return true;
}

static bool compile_struct_literal(VirtualMachine* vm, VmChunk* chunk, File* file, AstNode* node) {
    for (u32 i = 0; i < node -> as.struct_literal.inits.count; i++) {
        AstNodeId init_id  = node -> as.struct_literal.inits.ids[i];
        AstNode* init_node = &file -> ast.nodes[init_id];

        if (!compile_expr(vm, chunk, file, init_node -> as.field_init.value)) {
            return false;
        }
    }

    return true;
}

static bool compile_function_call(VirtualMachine* vm, VmChunk* chunk, File* file, AstNode* node) {
    SymbolId symbol_id = node -> resolved_symbol;

    if (symbol_id == SYMBOL_ID_NONE) {
        return false;
    }

    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(symbol_id);

    if (symbol -> kind != SYMBOL_FUNCTION) {
        return false;
    }

    u32 arg_count = node -> as.function_call.arguments.count;

    for (u32 i = 0; i < arg_count; i++) {
        AstNodeId arg_id = node -> as.function_call.arguments.ids[i];

        if (!compile_expr(vm, chunk, file, arg_id)) {
            return false;
        }
    }

    return true;
}
