#ifndef LILY_DRIVER_TYPES_H
#define LILY_DRIVER_TYPES_H

#include "diagnostics/types.h"
#include "files/types.h"
#include "namespacing/types.h"
#include "string_interner/types.h"

typedef struct {
    u64 flags;

    FileInterner file_interner;
    DiagnosticEngine diagnostic_engine;
    StringInterner string_interner;
    NamespaceInterner namespace_interner;
} DriverCtx;

#endif // !LILY_DRIVER_TYPES_H
