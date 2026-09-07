#include "linker/linker.h"
#include "cmd/cmd.h"
#include "driver/types.h"

extern DriverCtx driver;

i32 link_objects() {
    u32 file_count = driver.file_interner.count;

    arena_reset(&driver.scratch);

    char** args = arena_alloc(&driver.scratch, (file_count + 3) * sizeof(char*));
    u32 arg_count = 0;

    args[arg_count++] = "cc";
    args[arg_count++] = "-fuse-linker-plugin";

    for (u32 i = 0; i < file_count; i++) {
        char* path = driver.file_interner.entries[i].object_path;

        args[arg_count++] = path;
    }

    args[arg_count] = null;

    return run_command("cc", args);
}
