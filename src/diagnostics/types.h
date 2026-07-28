#ifndef LILY_DIAGNOSTICS_TYPES_H
#define LILY_DIAGNOSTICS_TYPES_H

#include "files/types.h"
#include "meowrena/meowrena.h"
#include "utils/types.h"

#define DIAG_DEFAULT_THRESHOLD 32

#define DIAG_LOC_START_OF_TOK   (1 << 0)
#define DIAG_LOC_END_OF_TOK     (1 << 1)
#define DIAG_LOC_WHOLE_TOK      (1 << 2)

typedef enum {
    DIAG_ERROR,
    DIAG_WARNING,
    DIAG_NOTE
} DiagKind;

typedef struct {
    DiagKind kind;

    FileId file_id;

    bool is_generic;

    u32 line;
    u32 col;
    u32 len;

    str8 msg;
    str8 help;
} Diagnostic;

typedef struct {
    // mostly for strings and array
    Arena arena;

    // TODO: Add support to read from flags
    u32 threshold_value;

    // maybe bit flags later?
    u8  __padding[4];

    // TODO: path to dump output, null if none which defaults to stdout/stderr
    u8* dump_path;

    u32 count;
    u32 capacity;
    Diagnostic* diags;
} DiagnosticEngine;

#endif // !LILY_DIAGNOSTICS_TYPES_H
