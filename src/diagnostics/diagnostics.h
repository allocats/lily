#ifndef LILY_DIAGNOSTICS_H
#define LILY_DIAGNOSTICS_H

#include "ast/nodes/types.h"
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
void diagnostic_add_symbol_already_defined(DiagnosticEngine* engine, Module* module, SymbolId symbol, AstNodeId node);
void diagnostic_add_symbol_is_builtin(DiagnosticEngine* engine, Module* module, SymbolId symbol, AstNodeId node);

void diagnostic_add_query_type_cycle(DiagnosticEngine* engine, i32 query_id);
void diagnostic_add_query_symbol_cycle(DiagnosticEngine* engine, i32 query_id);

bool diagnostics_print(DiagnosticEngine* engine);

#endif // !LILY_DIAGNOSTICS_H
