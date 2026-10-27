#include "sleeper/lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "common/bt_trace.h"
#include "common/config.h"
#include "common/paths.h"

enum {
    BT_SLEEPER_ALLOC_CHUNK_BYTES = 1048576,
    BT_SLEEPER_TOUCH_STRIDE_BYTES = 4096,
    BT_SLEEPER_SLEEP_MS_MAX = 4294967,
};

static void trace_from_config(void) {
    char path[BT_PATH_MAX];
    bt_paths_config_path(path);
    bt_json_value *config = NULL;
    if (bt_config_try_load(path, &config) != 0) {
        config = NULL;
    }
    long long ring_bytes = 65536;
    long long max_threads = 8;
    long long line_max = 256;
    if (config != NULL) {
        bt_config_int(config, "trace.ring_bytes", &ring_bytes);
        bt_config_int(config, "trace.max_threads", &max_threads);
        bt_config_int(config, "trace.line_max", &line_max);
        bt_json_free(config);
    }
    bt_trace_init((size_t)ring_bytes, (size_t)max_threads, (size_t)line_max);
    bt_trace_install_crash_handler();
}

static void sleep_secs(double secs) {
    double ms = secs * 1000.0;
    if (ms > (double)BT_SLEEPER_SLEEP_MS_MAX) {
        ms = (double)BT_SLEEPER_SLEEP_MS_MAX;
    }
    Sleep((DWORD)ms);
}

static int alloc_and_touch(unsigned long long mb) {
    BT_TRACE_ENTER("sleeper.alloc_and_touch");
    fprintf(stderr, "bt-sleeper: selftest alloc %llu MiB starting\n", mb);
    fflush(stderr);
    for (unsigned long long block_at = 0; block_at < mb; block_at++) {
        unsigned char *block = calloc((size_t)BT_SLEEPER_ALLOC_CHUNK_BYTES, 1);
        if (block == NULL) {
            fprintf(stderr, "bt-sleeper: selftest alloc failed at block %llu\n", block_at);
            fflush(stderr);
            BT_TRACE_FAIL("sleeper.alloc_and_touch", "calloc failed at block %llu", block_at);
            BT_TRACE_EXIT("sleeper.alloc_and_touch");
            return 1;
        }
        for (size_t at = 0; at < (size_t)BT_SLEEPER_ALLOC_CHUNK_BYTES;
             at += (size_t)BT_SLEEPER_TOUCH_STRIDE_BYTES) {
            block[at] = 1;
        }
    }
    fprintf(stderr, "bt-sleeper: selftest alloc touched %llu MiB and holds it\n", mb);
    fflush(stderr);
    BT_TRACE_EXIT("sleeper.alloc_and_touch");
    return 0;
}

static int hold_secs(unsigned long long secs) {
    BT_TRACE_ENTER("sleeper.hold_secs");
    fprintf(stderr, "bt-sleeper: selftest hold %llus starting\n", secs);
    fflush(stderr);
    DWORD ms = secs > (unsigned long long)BT_SLEEPER_SLEEP_MS_MAX ? (DWORD)BT_SLEEPER_SLEEP_MS_MAX
                                                                  : (DWORD)(secs * 1000ULL);
    Sleep(ms);
    fprintf(stderr, "bt-sleeper: selftest hold %llus done\n", secs);
    fflush(stderr);
    BT_TRACE_EXIT("sleeper.hold_secs");
    return 0;
}

static int usage_exit(const char *message) {
    fprintf(stderr, "usage: bt-sleeper <secs>\n");
    fprintf(stderr, "       bt-sleeper --selftest-alloc-mb <N>\n");
    fprintf(stderr, "       bt-sleeper --selftest-hold-secs <N>\n");
    fprintf(stderr, "bt-sleeper: %s\n", message);
    bt_trace_dump();
    bt_trace_shutdown();
    return 2;
}

int main(int argc, char **argv) {
    trace_from_config();
    int rest = argc - 1;
    char err[BT_SLEEPER_ERR_MAX];
    err[0] = '\0';
    if (rest >= 1 && (strcmp(argv[1], "--selftest-alloc-mb") == 0 ||
                      strcmp(argv[1], "--selftest-hold-secs") == 0)) {
        int mode = BT_SLEEPER_PLAIN;
        unsigned long long value = 0;
        if (bt_sleeper_parse_mode(rest, argv + 1, &mode, &value, err, sizeof err) != 0) {
            return usage_exit(err);
        }
        int rc = mode == BT_SLEEPER_ALLOC_MB ? alloc_and_touch(value) : hold_secs(value);
        if (rc != 0) {
            bt_trace_dump();
        }
        bt_trace_shutdown();
        return rc;
    }
    double secs = 0.0;
    if (bt_sleeper_parse_args(rest, argv + 1, &secs, err, sizeof err) != 0) {
        return usage_exit(err);
    }
    sleep_secs(secs);
    bt_trace_shutdown();
    return 0;
}
