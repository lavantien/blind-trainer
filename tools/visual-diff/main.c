#include "lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/alloc.h"

#ifdef _WIN32
#include <windows.h>

static void arm_alloc_seam_from_env(void) {
    const char *raw = getenv("BTVD_ALLOC_FAIL_OK");
    if (raw != NULL && raw[0] != '\0') {
        char *end = NULL;
        unsigned long long count = strtoull(raw, &end, 10);
        if (end != raw) {
            bt_alloc_fail_arm((size_t)count);
        }
    }
}

static char *wide_to_utf8(const wchar_t *text) {
    int len = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (len <= 0) {
        return NULL;
    }
    char *out = bt_malloc((size_t)len);
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
    arm_alloc_seam_from_env();
    size_t count = argc > 1 ? (size_t)(argc - 1) : 0;
    char **utf8 = NULL;
    if (count > 0) {
        utf8 = bt_calloc(count, sizeof *utf8);
        if (utf8 == NULL) {
            fputs("bt-visual-diff: out of memory\n", stderr);
            return BTVD_EXIT_USAGE;
        }
        for (size_t at = 0; at < count; at++) {
            utf8[at] = wide_to_utf8(argv[at + 1]);
            if (utf8[at] == NULL) {
                for (size_t done = 0; done < at; done++) {
                    free(utf8[done]);
                }
                fputs("bt-visual-diff: out of memory\n", stderr);
                free(utf8);
                return BTVD_EXIT_USAGE;
            }
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
