#ifndef LILY_DIAGNOSTICS_TYPES_H
#define LILY_DIAGNOSTICS_TYPES_H

#include "files/types.h"
#include "meowrena/meowrena.h"
#include "utils/types.h"

static constexpr u8 DIAG_LOC_START_OF_TOK = (1 << 0);
static constexpr u8 DIAG_LOC_END_OF_TOK   = (1 << 1);
static constexpr u8 DIAG_LOC_WHOLE_TOK    = (1 << 2);

typedef enum {
    DIAG_ERROR,
    DIAG_WARNING,
    DIAG_NOTE
} DiagKind;

typedef enum {
    DIAG_PRESENTATION_GENERIC,
    DIAG_PRESENTATION_SINGLE,
    DIAG_PRESENTATION_MULTILINE
} DiagPresentation;

typedef struct {
    u32 outer_start_line;
    u32 outer_end_line;

    u32 inner_start_line;
    u32 inner_start_col;

    u32 inner_end_line;
    u32 inner_end_col;
} DiagMultilineData;

typedef struct {
    DiagKind kind;

    FileId file_id;

    DiagPresentation presentation;

    u32 line;
    u32 col;
    u32 len;

    str8 msg;
    str8 help;

    // only valid when presentation == DIAG_PRESENTATION_MULTILINE.
    DiagMultilineData multiline;
} Diagnostic;

typedef struct {
    // mostly for strings and array
    Arena arena;

    // TODO: Add support to read from flags
    u32 threshold_value;

    // TODO: path to dump output, null if none which defaults to stdout/stderr
    u8* dump_path;

    u32 error_count;

    u32 count;
    u32 capacity;
    Diagnostic* diags;
} DiagnosticEngine;

#endif // !LILY_DIAGNOSTICS_TYPES_H
