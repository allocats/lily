#ifndef LILY_DRIVER_TYPES_H
#define LILY_DRIVER_TYPES_H

#include "diagnostics/types.h"
#include "files/types.h"
#include "string_interner/types.h"
#include "symbols/table/types.h"

typedef struct {
    const char* stdlib_path;

    u64 flags;

    FileInterner file_interner;
    DiagnosticEngine diagnostic_engine;
    StringInterner string_interner;

    SymbolTable symbol_table;
} DriverCtx;

typedef struct {
    str8* paths;
    u32 count;
    u32 capacity;
} StdlibFiles;

#endif // !LILY_DRIVER_TYPES_H
