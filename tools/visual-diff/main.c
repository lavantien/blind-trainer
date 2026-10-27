#include "lib.h"

#include <stdio.h>

int main(int argc, char **argv) {
    btvd_env env;
    btvd_env_from_process(&env, argv + 1, argc > 1 ? (size_t)(argc - 1) : 0);
    char err[BTVD_ERR_MAX];
    int code = btvd_run(&env, err, sizeof err);
    if (code < 0) {
        fprintf(stderr, "bt-visual-diff: %s\n", err);
        return BTVD_EXIT_USAGE;
    }
    return code;
}
