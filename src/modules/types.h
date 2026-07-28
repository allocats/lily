#ifndef LILY_MODULES_TYPES_H
#define LILY_MODULES_TYPES_H

#include "ast/tree/types.h"
#include "files/types.h"
#include "namespacing/types.h"
#include "string_interner/types.h"
#include "symbols/types.h"

#define MODULE_ID_NONE U32_MAX

typedef NamespaceId ModuleId;

typedef struct {
    NamespaceId namespace_id;
    u32 hash;

    FileId* files;
    u32 file_count;
    u32 file_capacity;

    Ast ast;

    SymbolTable symbol_table;
} Module;

typedef struct {
    Arena arena;

    ModuleId* buckets;
    Module* entries;

    u32 count;

    u32 bucket_capacity;
    u32 entry_capacity;
} ModuleRegistry;

#endif // !LILY_MODULES_TYPES_H
