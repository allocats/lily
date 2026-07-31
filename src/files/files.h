#ifndef LILY_FILES_H
#define LILY_FILES_H

#include "files/types.h"

void file_registry_init(u32 count);
void files_load_stdlib(str8 path);

// Interns the file, allocates the file's contents into buffer_arena. 
// path MUST be a null terminated cstring 
FileId files_intern(str8 path);
FileId files_lookup_path(str8 path);
File*  files_lookup_id(FileId);

#endif // !LILY_FILES_H
