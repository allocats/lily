#ifndef LILY_DRIVER_TYPES_H
#define LILY_DRIVER_TYPES_H

#include "diagnostics/types.h"
#include "files/types.h"

typedef struct {
    FileInterner file_interner;
    DiagnosticEngine diagnostic_engine;
} DriverCtx;

#endif // !LILY_DRIVER_TYPES_H
