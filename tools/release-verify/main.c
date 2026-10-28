#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

#include "common/bt_trace.h"
#include "common/json.h"
#include "common/paths.h"
#include "release-verify/lib.h"

static void trace_from_config(void) {
    char path[BT_PATH_MAX];
    bt_paths_config_path(path);
    bt_json_value *config = NULL;
    FILE *file = fopen(path, "rb");
    if (file != NULL) {
        char text[1U << 16U];
        size_t len = fread(text, 1, sizeof text - 1, file);
        fclose(file);
        text[len] = '\0';
        bt_json_error json_err;
        bt_json_parse(text, len, &config, &json_err);
    }
    long long ring_bytes = 65536;
    long long max_threads = 8;
    long long line_max = 256;
    if (config != NULL) {
        const bt_json_value *trace = bt_json_get(config, "trace");
        const bt_json_value *ring = trace != NULL ? bt_json_get(trace, "ring_bytes") : NULL;
        const bt_json_value *threads = trace != NULL ? bt_json_get(trace, "max_threads") : NULL;
        const bt_json_value *line = trace != NULL ? bt_json_get(trace, "line_max") : NULL;
        if (ring != NULL && bt_json_is_integer(ring)) {
            ring_bytes = ring->integer;
        }
        if (threads != NULL && bt_json_is_integer(threads)) {
            max_threads = threads->integer;
        }
        if (line != NULL && bt_json_is_integer(line)) {
            line_max = line->integer;
        }
    }
    bt_json_free(config);
    bt_trace_init((size_t)ring_bytes, (size_t)max_threads, (size_t)line_max);
    bt_trace_install_crash_handler();
}

static char *rv_wide_to_utf8(const wchar_t *wide) {
    int need = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    char *text = malloc((size_t)need);
    if (text == NULL) {
        return NULL;
    }
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, text, need, NULL, NULL);
    return text;
}

int wmain(int argc, wchar_t **argv) {
    trace_from_config();
    BT_TRACE_ENTER("rv.main");
    char **argv_utf8 = calloc((size_t)argc, sizeof *argv_utf8);
    if (argv_utf8 == NULL) {
        bt_trace_dump();
        bt_trace_shutdown();
        return BT_RV_EXIT_USAGE;
    }
    for (int at = 1; at < argc; at++) {
        argv_utf8[at - 1] = rv_wide_to_utf8(argv[at]);
    }
    argv_utf8[argc - 1] = NULL;
    const wchar_t *config_wide = _wgetenv(L"BT_CONFIG");
    char *config = NULL;
    if (config_wide != NULL) {
        config = rv_wide_to_utf8(config_wide);
    }
    char root[BT_PATH_MAX];
    bt_paths_workspace_root(root);
    bt_rv_env env = {
        .argv = (const char *const *)argv_utf8,
        .config = config,
        .vars = (const char *const *)environ,
        .root = root,
    };
    int exit_code = 0;
    char err[BT_RV_ERR_MAX];
    if (bt_rv_run(&env, &exit_code, err, sizeof err) != 0) {
        fprintf(stderr, "bt-release-verify: %s\n", err);
        BT_TRACE_FAIL("rv.main", "%s", err);
        bt_trace_dump();
        bt_trace_shutdown();
        return BT_RV_EXIT_USAGE;
    }
    BT_TRACE_EXIT("rv.main");
    bt_trace_shutdown();
    return exit_code;
}
