#ifndef LILY_DIAGNOSTICS_H
#define LILY_DIAGNOSTICS_H

#include "ids.h"
#include "diagnostics/types.h"

void diagnostic_engine_init(void);

void diagnostic_add_generic(DiagKind kind, char* fmt, ...);
void diagnostic_add_token(FileId file_id, DiagKind kind, Token* tok, u8 loc, const char* msg, const char* help);

bool diagnostics_print();

#endif // !LILY_DIAGNOSTICS_H
