#ifndef LILY_DRIVER_H
#define LILY_DRIVER_H

#include "driver/types.h"

void driver_init(LilyCtx* driver, i32 argc, char** argv);
void driver_destroy(LilyCtx* driver);

#endif // !LILY_DRIVER_H
