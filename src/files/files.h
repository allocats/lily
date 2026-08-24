#ifndef LILY_FILES_H
#define LILY_FILES_H

#include "files/types.h"
#include "ids.h"
#include "utils/types.h"

void file_interner_init(u32 count);

FileId file_intern(str8 path);
FileId file_lookup(str8 path);

File*  file_lookup_id(FileId id);

void path_normalizer_init(void);
str8 get_absolute_path(str8 input_path);

#endif // !LILY_FILES_H
