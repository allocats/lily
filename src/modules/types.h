#ifndef LILY_MODULES_TYPES_H
#define LILY_MODULES_TYPES_H

#include "ids.h"
#include "ast/tree/types.h"
#include "meowrena/meowrena.h"
#include "symbols/types.h"

typedef struct {
    // general purpose arena allocator
    Arena gpa;

    ModuleId id;

    NamespaceId namespace_id;
    u32 hash;

    FileId* files;
    AstNodeId* ast_offsets;
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
