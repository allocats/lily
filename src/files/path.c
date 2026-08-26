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

static char* normalize_stack[PATH_MAX / 2 + 1];
static char  combine_scratch[PATH_MAX];

void path_normalizer_init(void) {
    assert(getcwd(cwd_ptr, sizeof(cwd_ptr)) != NULL);

    arena_init(&arena, ARENA_KB(2), ALIGN_DEFAULT);
    debug_printf("Init path maker's arena with 2KB");

    cwd_len = strnlen(cwd_ptr, PATH_MAX);

    assert(cwd_len > 0);
}

void path_normalizer_destroy(void) {
    arena_destroy(&arena);
}

static str8 normalize_path(str8 path) {
    assert(path.len + 1 < PATH_MAX && "path too long for normalize_scratch");

    usize top = 0;

    char* result = arena_alloc(&arena, path.len + 2);
    char* cursor = result;

    *cursor++ = '/';

    usize i = 0;

    while (i < path.len) {
        while (i < path.len && path.ptr[i] == '/') { // advance past repeated '/'s
            i++;
        }

        if (i == path.len) {
            break;
        }

        usize start = i;

        while (i < path.len && path.ptr[i] != '/') { // gets end marker of '/'
            i++;
        }

        usize len = i - start;

        if (len == 1 && path.ptr[start] == '.') {
            continue;
        }

        if (len == 2 && path.ptr[start] == '.' && path.ptr[start + 1] == '.') {
            if (top > 0) {
                cursor = normalize_stack[--top];
            }

            continue;
        }

        normalize_stack[top++] = cursor;

        if (top > 1) {
            *cursor++ = '/';
        }

        memcpy(cursor, path.ptr + start, len);

        cursor += len;
    }

    *cursor = '\0';

    return (str8) {
        .ptr = result,
        .len = (u32)(cursor - result),
    };
}

str8 get_absolute_path(str8 input_path) {
    assert(input_path.ptr != NULL);
    assert(input_path.len != 0);

    if (input_path.ptr[0] == '/') {
        return normalize_path(input_path);
    }

    usize len = cwd_len + 1 + input_path.len;
    assert(len + 1 <= PATH_MAX && "combined path too long for combine_scratch");

    memcpy(combine_scratch, cwd_ptr, cwd_len);

    combine_scratch[cwd_len] = '/';

    memcpy(combine_scratch + cwd_len + 1, input_path.ptr, input_path.len);

    combine_scratch[len] = '\0';

    str8 combined = {
        .ptr = combine_scratch,
        .len = len,
    };

    return normalize_path(combined);
}
