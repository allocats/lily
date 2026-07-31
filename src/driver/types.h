#ifndef lily_driver_types_h
#define lily_driver_types_h

#include "diagnostics/types.h"
#include "files/types.h"
#include "meowrena/meowrena.h"
#include "modules/types.h"
#include "namespacing/types.h"
#include "string_interner/types.h"

#define LILY_FLAGS_NONE         (0 << 0)
#define LILY_FLAGS_DUMP_TOKENS  (1 << 0)
#define LILY_FLAGS_DUMP_AST     (1 << 1)

typedef struct {
    u64 flags;

    Arena* gpa;

    FileRegistry file_registry;

    DiagnosticEngine diagnostics;

    StringInterner string_interner;

    NamespaceInterner namespace_interner;

    ModuleRegistry module_registry;
} LilyCtx;

#endif // !LILY_DRIVER_TYPES_H
