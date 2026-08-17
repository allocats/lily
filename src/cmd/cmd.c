#include "cmd/cmd.h"
#include "utils/types.h"

#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <wait.h>

extern char** environ;

i32 run_command(const char* path, char** args) {
    pid_t pid;

    i32 rc = posix_spawnp(&pid, path, NULL, NULL, args, environ);
    if (rc != 0) {
        fprintf(stderr, "posix_spawnp failed for %s: %s\n", path, strerror(rc));
        return -1;
    }

    i32 status;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return -1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        fprintf(stderr, "%s terminated by signal %d\n", path, WTERMSIG(status));
    }

    return -1;
}

