#ifndef LILY_DIAGNOSTICS_H
#define LILY_DIAGNOSTICS_H

#include "ids.h"
#include "modules/types.h"
#include "diagnostics/types.h"

#include "token/types.h"

void diagnostic_engine_init(void);

void diagnostic_add_generic(DiagnosticEngine* engine, DiagKind kind, char* fmt, ...);
void diagnostic_add_token(
    DiagnosticEngine* engine,
    FileId file_id,
    DiagKind kind,
    Token* tok,
    u8 loc,
    const char* msg,
    const char* help
);

void diagnostics_add_unknown_namespace(DiagnosticEngine* engine, Module* module, AstNodeId ident_id);

void diagnostic_add_symbol_already_defined(DiagnosticEngine* engine, Module* module, SymbolId symbol, AstNodeId node);
void diagnostic_add_symbol_is_builtin(DiagnosticEngine* engine, Module* module, SymbolId symbol, AstNodeId node);

void diagnostic_add_use_of_undeclared_identifier(DiagnosticEngine* engine, Module* module, AstNode* node);
void diagnostic_add_use_of_undeclared_member(
    DiagnosticEngine* engine,
    Module* module,
    AstNodeId access_id,
    StringId object_name,
    StringId member
);

void diagnostic_add_resolver_type_cycle(DiagnosticEngine* engine, i32 query_id);
void diagnostic_add_resolver_symbol_cycle(DiagnosticEngine* engine, i32 query_id);
void diagnostic_add_return_type_invalid(DiagnosticEngine* engine, Module* module, SymbolId symbol);

void diagnostic_add_cannot_reassign_constant(DiagnosticEngine* engine, Module* module, AstNodeId binop_id);

void diagnostic_add_pointer_compound_assign_requires_integer(DiagnosticEngine* engine, Module* module, AstNodeId rhs_id);
void diagnostic_add_assignment_type_mismatch(
    DiagnosticEngine* engine,
    Module* module,
    AstNodeId rhs_id,
    TypeId expected_type,
    TypeId found_type
);

void diagnostic_add_pointer_subtraction_type_mismatch(DiagnosticEngine* engine, Module* module, AstNodeId binop_id);
void diagnostic_add_pointer_arithmetic_requires_integer(DiagnosticEngine* engine, Module* module, AstNodeId rhs_id);

void diagnostic_add_comparison_type_mismatch(DiagnosticEngine* engine, Module* module, AstNodeId binop_id);
void diagnostic_add_logical_operator_requires_bool(DiagnosticEngine* engine, Module* module, AstNodeId binop_id);

void diagnostic_add_cannot_reference_rvalue(DiagnosticEngine* engine, Module* module, AstNodeId operand_id);
void diagnostic_add_cannot_dereference_non_pointer(DiagnosticEngine* engine, Module* module, AstNodeId unary_id);

void diagnostic_add_call_argument_count_mismatch(
    DiagnosticEngine* engine,
    Module* module,
    AstNodeId call_id,
    u32 expected,
    u32 found,
    bool is_variadic
);
void diagnostic_add_argument_type_mismatch(
    DiagnosticEngine* engine,
    Module* module,
    AstNodeId arg_id,
    TypeId expected_type,
    TypeId found_type,
    u32 arg_index
);

void diagnostic_add_arrow_access_on_non_pointer(DiagnosticEngine* engine, Module* module, AstNodeId access_id);
void diagnostic_add_dot_access_on_pointer(DiagnosticEngine* engine, Module* module, AstNodeId access_id);
void diagnostic_add_member_access_invalid_base_type(
    DiagnosticEngine* engine,
    Module* module,
    AstNodeId access_id,
    TypeId base_type
);

void diagnostic_add_type_cannot_be_void(DiagnosticEngine* engine, Module* module, AstNodeId type_expr_id);
void diagnostic_add_type_is_not_an_integer(DiagnosticEngine* engine, Module* module, AstNodeId type_expr_id);
void diagnostic_add_type_does_not_exist(DiagnosticEngine* engine, Module* module, AstNodeId type_expr_id);

void diagnostic_add_void_function_returns_value(DiagnosticEngine* engine, Module* module, AstNodeId id);
void diagnostic_add_function_expects_but_returns(
    DiagnosticEngine* engine,
    Module* module,
    StringId fn_name,
    AstNodeId return_stmt_id,
    TypeId return_type,
    TypeId found_type
);

bool diagnostics_print(DiagnosticEngine* engine);

#endif // !LILY_DIAGNOSTICS_H
