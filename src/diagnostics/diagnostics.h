#ifndef LILY_DIAGNOSTICS_H
#define LILY_DIAGNOSTICS_H

#include "diagnostics/types.h"
#include "ids.h"
#include "resolver_stack/types.h"
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

void diagnostic_add_node_field(
    FileId file_id,
    DiagKind kind,
    SpanU32 outer,
    SpanU32 inner,
    const char* msg,
    const char* help
);

void diagnostic_add_symbol_redefined(FileId file_id, AstNodeId node_id, SymbolId symbol_id, StringId name_id);
void diagnostic_add_symbol_does_not_exist(FileId file_id, AstNodeId node_id, StringId name_id);
void diagnostic_add_symbol_cycle(ResolveQuery query);

bool diagnostics_print();

#endif // !LILY_DIAGNOSTICS_H
