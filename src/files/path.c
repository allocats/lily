#include "meowrena/meowrena.h"
#include "utils/debug.h"
#include "utils/types.h"

#include <assert.h>
#include <linux/limits.h>
#include <string.h>
#include <unistd.h>

static Arena arena = {0};

static char cwd_ptr[PATH_MAX] = {0};
static u32  cwd_len = 0;

void path_normalizer_init(void) {
    assert(getcwd(cwd_ptr, sizeof(cwd_ptr)) != NULL);

    arena_init(&arena, ARENA_KB(2), ALIGN_DEFAULT);
    debug_printf("Init path maker's arena with 2KB");

    cwd_len = strnlen(cwd_ptr, PATH_MAX);

    assert(cwd_len > 0);
}

static str8 normalize_path(str8 path) {
    char scratch[PATH_MAX] = {0};
    memcpy(scratch, path.ptr, path.len + 1);

    char** stack = arena_alloc(&arena, sizeof(char*) * (path.len + 1));
    usize top = 0;

    char* saveptr = NULL;
    char* token = strtok_r(scratch, "/", &saveptr);

    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            // no-op
        } else if (strcmp(token, "..") == 0) {
            if (top > 0) {
                top--;
            }
        } else {
            stack[top++] = token;
        }

        token = strtok_r(NULL, "/", &saveptr);
    }

    char* result = arena_alloc(&arena, path.len + 2);
    char* w = result;

    *w++ = '/';

    for (usize i = 0; i < top; i++) {
        usize len = strlen(stack[i]);
        memcpy(w, stack[i], len);
        w += len;

        if (i + 1 < top) {
            *w++ = '/';
        }
    }

    *w = '\0';

    return (str8) { .ptr = result, .len = w - result };
}

str8 get_absolute_path(str8 input_path) {
    assert(input_path.ptr != NULL);
    assert(input_path.len != 0);

    str8 combined = {0};

    if (input_path.ptr[0] == '/') {
        combined.ptr = (char*) input_path.ptr;
        combined.len = input_path.len;
    } else {
        u32 len = cwd_len + 1 + input_path.len + 1;

        combined.ptr = arena_alloc(&arena, len);
        combined.len = len;

        memcpy(combined.ptr, cwd_ptr, cwd_len);
        combined.ptr[cwd_len] = '/';
        memcpy(combined.ptr + cwd_len + 1, input_path.ptr, input_path.len + 1);
    }

    str8 normalized = normalize_path(combined);
    return normalized;
}
