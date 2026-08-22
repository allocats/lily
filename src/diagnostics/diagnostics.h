#ifndef LILY_DIAGNOSTICS_H
#define LILY_DIAGNOSTICS_H

#include "diagnostics/types.h"
#include "ids.h"
#include "utils/types.h"

void diagnostic_engine_init(void);

void diagnostic_add_generic(DiagKind kind, char* fmt, ...);

void diagnostic_add_token(
    FileId file_id,
    DiagKind kind,
    Token* tok,
    u8 loc,
    const char* msg,
    const char* help
);

void diagnostic_add_token_span(
    FileId file_id,
    DiagKind kind,
    SpanU32 span,
    const char* msg,
    const char* help
);

bool diagnostics_print();

#endif // !LILY_DIAGNOSTICS_H
