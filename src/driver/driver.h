#ifndef LILY_DRIVER_H
#define LILY_DRIVER_H

#include "driver/types.h"

static constexpr u64 DRIVER_FLAGS_DUMP_TOKENS = 1 << 0;
static constexpr u64 DRIVER_FLAGS_DUMP_AST    = 1 << 1;

void driver_init(DriverCtx* driver, i32 argc, char** argv);

#endif // !LILY_DRIVER_H
