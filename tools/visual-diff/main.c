#include "lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>

static char *wide_to_utf8(const wchar_t *text) {
    int len = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (len <= 0) {
        return NULL;
    }
    char *out = malloc((size_t)len);
    if (out == NULL) {
        return NULL;
    }
    WideCharToMultiByte(CP_UTF8, 0, text, -1, out, len, NULL, NULL);
    return out;
}

static char *wide_env_utf8(const wchar_t *name) {
    const wchar_t *wide = _wgetenv(name);
    if (wide == NULL) {
        return NULL;
    }
    return wide_to_utf8(wide);
}

static int real_main(char **argv, size_t argc, char *config, char *artifacts) {
    btvd_env env;
    btvd_env_from_process(&env, argv, argc, config, artifacts);
    char err[BTVD_ERR_MAX];
    int code = btvd_run(&env, err, sizeof err);
    free(config);
    free(artifacts);
    if (code < 0) {
        fprintf(stderr, "bt-visual-diff: %s\n", err);
        return BTVD_EXIT_USAGE;
    }
    return code;
}

int wmain(int argc, wchar_t **argv) {
    size_t count = argc > 1 ? (size_t)(argc - 1) : 0;
    char **utf8 = NULL;
    if (count > 0) {
        utf8 = calloc(count, sizeof *utf8);
        if (utf8 == NULL) {
            fputs("bt-visual-diff: out of memory\n", stderr);
            return BTVD_EXIT_USAGE;
        }
        for (size_t at = 0; at < count; at++) {
            utf8[at] = wide_to_utf8(argv[at + 1]);
        }
    }
    char *config = wide_env_utf8(L"BT_CONFIG");
    char *artifacts = wide_env_utf8(L"BT_ARTIFACTS");
    int code = real_main(utf8, count, config, artifacts);
    for (size_t at = 0; at < count; at++) {
        free(utf8[at]);
    }
    free(utf8);
    return code;
}
#else
int main(int argc, char **argv) {
    btvd_env env;
    btvd_env_from_process(&env, argv + 1, argc > 1 ? (size_t)(argc - 1) : 0, getenv("BT_CONFIG"),
                          getenv("BT_ARTIFACTS"));
    char err[BTVD_ERR_MAX];
    int code = btvd_run(&env, err, sizeof err);
    if (code < 0) {
        fprintf(stderr, "bt-visual-diff: %s\n", err);
        return BTVD_EXIT_USAGE;
    }
    return code;
}
#endif
