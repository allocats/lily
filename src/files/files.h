#ifndef LILY_FILES_H
#define LILY_FILES_H

#include "ids.h"
#include "files/types.h"
#include "utils/types.h"

void file_interner_init(u32 count);

FileId file_intern(str8 path);
FileId file_lookup(str8 path);

File*  file_lookup_id(FileId id);

#endif // !LILY_FILES_H
