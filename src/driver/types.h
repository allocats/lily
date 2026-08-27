#ifndef LILY_DRIVER_TYPES_H
#define LILY_DRIVER_TYPES_H

#include "diagnostics/types.h"
#include "files/types.h"
#include "string_interner/types.h"
#include "symbols/table/types.h"
#include "types/table/types.h"

typedef struct {
    SymbolTable symbol_table;
    TypeTable type_table;
    FileInterner file_interner;
    DiagnosticEngine diagnostic_engine;
    StringInterner string_interner;

    u64 flags;

    const char* stdlib_path;
} DriverCtx;

typedef struct {
    str8* paths;
    u32 count;
    u32 capacity;
} StdlibFiles;

#endif // !LILY_DRIVER_TYPES_H
