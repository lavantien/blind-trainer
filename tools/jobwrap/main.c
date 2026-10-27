#include "jobwrap/lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "common/bt_trace.h"
#include "common/config.h"
#include "common/paths.h"

enum {
    BT_JOBWRAP_ALLOC_CHUNK_BYTES = 1048576,
    BT_JOBWRAP_TOUCH_STRIDE_BYTES = 4096,
    BT_JOBWRAP_SLEEP_MS_MAX = 4294967,
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

static int alloc_and_touch(unsigned long long mb) {
    BT_TRACE_ENTER("jobwrap.alloc_and_touch");
    fprintf(stderr, "bt-jobwrap: selftest alloc %llu MiB starting\n", mb);
    fflush(stderr);
    for (unsigned long long block_at = 0; block_at < mb; block_at++) {
        volatile unsigned char *block = calloc((size_t)BT_JOBWRAP_ALLOC_CHUNK_BYTES, 1);
        if (block == NULL) {
            fprintf(stderr, "bt-jobwrap: selftest alloc failed at block %llu\n", block_at);
            fflush(stderr);
            BT_TRACE_FAIL("jobwrap.alloc_and_touch", "calloc failed at block %llu", block_at);
            BT_TRACE_EXIT("jobwrap.alloc_and_touch");
            return 1;
        }
        for (size_t at = 0; at < (size_t)BT_JOBWRAP_ALLOC_CHUNK_BYTES;
             at += (size_t)BT_JOBWRAP_TOUCH_STRIDE_BYTES) {
            block[at] = 1;
        }
    }
    fprintf(stderr, "bt-jobwrap: selftest alloc touched %llu MiB and holds it\n", mb);
    fflush(stderr);
    BT_TRACE_EXIT("jobwrap.alloc_and_touch");
    return 0;
}

static int hold_secs(unsigned long long secs) {
    BT_TRACE_ENTER("jobwrap.hold_secs");
    fprintf(stderr, "bt-jobwrap: selftest hold %llus starting\n", secs);
    fflush(stderr);
    DWORD ms = secs > (unsigned long long)BT_JOBWRAP_SLEEP_MS_MAX ? (DWORD)BT_JOBWRAP_SLEEP_MS_MAX
                                                                  : (DWORD)(secs * 1000ULL);
    Sleep(ms);
    fprintf(stderr, "bt-jobwrap: selftest hold %llus done\n", secs);
    fflush(stderr);
    BT_TRACE_EXIT("jobwrap.hold_secs");
    return 0;
}

static int usage_exit(const char *message) {
    fprintf(stderr, "%s\n", BT_JOBWRAP_USAGE);
    fprintf(stderr, "bt-jobwrap: %s\n", message);
    bt_trace_dump();
    bt_trace_shutdown();
    return 2;
}

int main(int argc, char **argv) {
    trace_from_config();
    bt_jobwrap_invocation invocation;
    char err[BT_JOBWRAP_ERR_MAX];
    err[0] = '\0';
    if (bt_jobwrap_parse_invocation(argc - 1, argv + 1, &invocation, err, sizeof err) != 0) {
        return usage_exit(err);
    }
    if (invocation.kind == BT_JOBWRAP_INVOCATION_ALLOC_MB) {
        int rc = alloc_and_touch(invocation.value);
        if (rc != 0) {
            bt_trace_dump();
        }
        bt_trace_shutdown();
        return rc;
    }
    if (invocation.kind == BT_JOBWRAP_INVOCATION_HOLD_SECS) {
        int rc = hold_secs(invocation.value);
        bt_trace_shutdown();
        return rc;
    }
    unsigned int code = 0;
    int verdict = bt_jobwrap_run(&invocation, &code, err, sizeof err);
    if (verdict == BT_JOBWRAP_RUN_OK) {
        bt_trace_shutdown();
        exit((int)code);
    }
    if (verdict == BT_JOBWRAP_RUN_USAGE) {
        return usage_exit(err);
    }
    bt_trace_dump();
    bt_trace_shutdown();
    return 3;
}
