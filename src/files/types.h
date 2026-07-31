#ifndef LILY_FILES_TYPES_H
#define LILY_FILES_TYPES_H

#include "ids.h"
#include "meowrena/meowrena.h"
#include "token/types.h"
#include "utils/types.h"

typedef enum {
    FILE_LOADED  = 0,
    FILE_LEXING  = 1,
    FILE_LEXED   = 2,
    FILE_PARSING = 3,
    FILE_PARSED  = 4,
    FILE_ERROR   = 5,
} FileStage;

typedef struct {
    str8 buffer;
    str8 path;
    u32  hash;

    FileStage stage;
} File;

typedef struct {
    // source buffers, lifetime as long as the program, all refs will live
    // File.buffer.pointer points into this arena
    Arena buffers_arena;

    // arena for buckets and entries
    Arena interner_arena;

    // arena for the array of TokenArrays
    Arena tokens_arena;

    FileId* buckets;
    File*   entries;

    // linked with entries, SoA, after lexing 99% of the refs are to tokens
    TokenArray* tokens;

    u32 count;

    u32 bucket_capacity;
    u32 entry_capacity;
} FileRegistry;

#endif // !LILY_FILES_TYPES_H
