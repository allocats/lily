#ifndef LILY_DIAGNOSTICS_H
#define LILY_DIAGNOSTICS_H

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
    char* msg,
    char* help
);
void diagnostics_print(DiagnosticEngine* engine);

#endif // !LILY_DIAGNOSTICS_H
