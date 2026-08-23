#ifndef LILY_FILES_TYPES_H
#define LILY_FILES_TYPES_H

#include "ast/tree/types.h"
#include "ids.h"
#include "token/types.h"
#include "utils/types.h"

typedef enum {
    FILE_ALLOCATED,
    FILE_LEXED,
    FILE_PARSED,
    FILE_ERROR
} FileStage;

typedef struct {
    FileId id;
    u32 hash;
} FileBucket;

typedef struct {
    Ast ast;

    TokenArray tokens;

    str8 buffer;
    str8 path;
    u32  hash;

    StringId path_string_id;

    FileStage stage;
    
    FileId id;
} File;

typedef struct {
    // source buffers, lifetime as long as the program, all refs will live
    // File.buffer.pointer points into this arena
    Arena buffer_arena;

    // arena for buckets and entries
    Arena interner_arena;

    FileBucket* buckets;
    File* entries;

    u32 count;

    u32 bucket_capacity;
    u32 entry_capacity;

    u32 resize_threshold_as_u32;
} FileInterner;

#endif // !LILY_FILES_TYPES_H
