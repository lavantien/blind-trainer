#include "ga-run/lib.h"

#include "common/bt_harness.h"
#include "common/json.h"
#include "common/paths.h"
#include "data/paths.h"
#include "unit/ga/ga_test_util.h"

#include <windows.h>

#include <direct.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *GR_IO_PIPE_BASE = "\\\\.\\pipe\\bt-gr-io";
static const char *GR_IO_MARKDOWN_TAIL = "/ga/report.md";
static const DWORD GR_IO_PIPE_OUT_BUFFER = 4096;
static const DWORD GR_IO_PIPE_IN_BUFFER = 16;
static const char *GR_IO_GA_CONFIG =
    "{\"seed\": 77, \"population\": 30, \"generations\": 20, \"elite\": 2, \"tournament\": 2, "
    "\"pool_top_k\": 1600, \"weights\": {\"confusion\": 1.0, \"flow\": 0.5, \"complexity\": 0.1}, "
    "\"mutation\": {\"swap\": 0.15, \"replace\": 0.05}, \"islands\": {\"count\": 1, "
    "\"migrate_every\": 0, \"migrate_count\": 1}, \"adaptive_mutation\": {\"stagnation_threshold\": "
    "0, \"swap_scale\": 1.0, \"replace_scale\": 1.0}, \"elite_carry\": {\"policy\": \"count\", "
    "\"fraction\": 0.0}, \"restart\": {\"diversity_threshold\": 0.0, \"inject_fraction\": 0.5}}";

static void io_scratch_dir(const char *name, char out[BT_PATH_MAX]) {
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    snprintf(out, BT_PATH_MAX, "%s/build/test-tmp/ga-run-io/%s", root, name);
    for (char *cursor = out; *cursor != '\0'; cursor++) {
        if (*cursor == '\\') {
            *cursor = '/';
        }
    }
    for (char *cursor = out + 1; *cursor != '\0'; cursor++) {
        if (*cursor == '/') {
            *cursor = '\0';
            _mkdir(out);
            *cursor = '/';
        }
    }
    _mkdir(out);
}

static void io_scratch_file(const char *dir, const char *name, const char *bytes) {
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        BT_ABORT("cannot write %s", path);
    }
    fwrite(bytes, 1, strlen(bytes), file);
    fclose(file);
}

static bt_json_value *parse_json(const char *text) {
    bt_json_value *value = NULL;
    bt_json_error error;
    if (bt_json_parse(text, strlen(text), &value, &error) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "fixture json parse failed: %s", error.message);
        return NULL;
    }
    return value;
}

BT_TEST(ga_run_io, cross_check_fails_closed_when_cases_file_size_overflows_ftell) {
    char vendor[BT_PATH_MAX];
    io_scratch_dir("vendor-oversize", vendor);
    char cases_path[BT_PATH_MAX];
    snprintf(cases_path, sizeof cases_path, "%s/cases.json", vendor);
    bt_ga_make_oversize(cases_path);
    char text[BT_PATH_MAX + 128];
    snprintf(text, sizeof text, "{\"paths\": {\"data_vendor\": \"%s\"}}", vendor);
    bt_json_value *config = parse_json(text);
    BT_CHECK(config != NULL);
    bt_gr_offenders offenders = {0};
    BT_CHECK_EQ(bt_gr_cross_check_cases(config, &offenders), -1);
    int saw_read_error = 0;
    for (size_t i = 0; i < offenders.count; i++) {
        if (strncmp(offenders.items[i], "read ", 5) == 0) {
            saw_read_error = 1;
        }
    }
    BT_CHECK(saw_read_error);
    bt_gr_offenders_free(&offenders);
    bt_json_free(config);
}

BT_TEST(ga_run_io, load_freq_data_fails_closed_when_the_file_size_overflows_ftell) {
    char dir[BT_PATH_MAX];
    io_scratch_dir("freq-oversize", dir);
    char freq_path[BT_PATH_MAX];
    snprintf(freq_path, sizeof freq_path, "%s/freq.json", dir);
    bt_ga_make_oversize(freq_path);
    bt_ga_freq_data data;
    char err[BT_GA_ERR_MAX];
    BT_CHECK_EQ(bt_ga_load_freq_data(freq_path, &data, err), 0);
    BT_CHECK(strstr(err, "read ") != NULL);
}

static char *read_bytes(const char *path, size_t *len) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *bytes = malloc((size_t)size + 1);
    if (bytes != NULL) {
        *len = fread(bytes, 1, (size_t)size, file);
        bytes[*len] = '\0';
    }
    fclose(file);
    return bytes;
}

static void copy_repo_freq_json(const char *data_ga) {
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    char freq[BT_PATH_MAX];
    snprintf(freq, sizeof freq, "%s/data/ga/freq.json", root);
    size_t len = 0;
    char *bytes = read_bytes(freq, &len);
    if (bytes == NULL) {
        BT_ABORT("cannot read %s, run python bt.py memofreq first", freq);
    }
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/freq.json", data_ga);
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        free(bytes);
        BT_ABORT("cannot write %s", path);
    }
    fwrite(bytes, 1, len, file);
    fclose(file);
    free(bytes);
}

typedef struct {
    char pipe_name[BT_PATH_MAX];
    int connected;
} markdown_pipe;

static DWORD WINAPI markdown_pipe_server(void *user) {
    markdown_pipe *fixture = user;
    HANDLE pipe = CreateNamedPipeA(fixture->pipe_name, PIPE_ACCESS_INBOUND,
                                   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1,
                                   GR_IO_PIPE_OUT_BUFFER, GR_IO_PIPE_IN_BUFFER, 0, NULL);
    if (pipe == INVALID_HANDLE_VALUE) {
        return 1;
    }
    BOOL connected = ConnectNamedPipe(pipe, NULL);
    if (!connected) {
        connected = GetLastError() == ERROR_PIPE_CONNECTED;
    }
    fixture->connected = connected;
    CloseHandle(pipe);
    return 0;
}

typedef struct {
    int rc;
    size_t offender_count;
    char first_offender[256];
    char markdown_offender[BT_PATH_MAX + 64];
} run_all_ctx;

static void run_all_once(void *user) {
    run_all_ctx *ctx = user;
    bt_gr_offenders offenders = {0};
    ctx->rc = bt_gr_run_all(&offenders);
    ctx->offender_count = offenders.count;
    for (size_t i = 0; i < offenders.count; i++) {
        if (i == 0) {
            snprintf(ctx->first_offender, sizeof ctx->first_offender, "%s", offenders.items[i]);
        }
        if (strncmp(offenders.items[i], "write ", 6) == 0) {
            snprintf(ctx->markdown_offender, sizeof ctx->markdown_offender, "%s",
                     offenders.items[i]);
        }
    }
    bt_gr_offenders_free(&offenders);
}

static void json_escape_path(const char *in, char out[BT_PATH_MAX]) {
    size_t at = 0;
    for (size_t i = 0; in[i] != '\0' && at + 2 < BT_PATH_MAX; i++) {
        if (in[i] == '\\') {
            out[at++] = '\\';
        }
        out[at++] = in[i];
    }
    out[at] = '\0';
}

BT_TEST(ga_run_io, run_all_fails_closed_when_the_markdown_write_breaks) {
    char data_ga[BT_PATH_MAX];
    io_scratch_dir("pipe-write", data_ga);
    char pipe_json[BT_PATH_MAX];
    json_escape_path(GR_IO_PIPE_BASE, pipe_json);
    char text[BT_PATH_MAX * 2 + 2048];
    snprintf(text, sizeof text,
             "{\"paths\": {\"ref_doc\": \"ref_doc\", \"data_vendor\": \"data/vendor\", "
             "\"data_ga\": \"%s\", \"artifacts\": \"%s\"}, \"ga\": %s}",
             data_ga, pipe_json, GR_IO_GA_CONFIG);
    io_scratch_file(data_ga, "config.json", text);
    copy_repo_freq_json(data_ga);
    char config_path[BT_PATH_MAX];
    snprintf(config_path, sizeof config_path, "%s/config.json", data_ga);

    markdown_pipe fixture;
    snprintf(fixture.pipe_name, sizeof fixture.pipe_name, "%s%s", GR_IO_PIPE_BASE,
             GR_IO_MARKDOWN_TAIL);
    HANDLE server = CreateThread(NULL, 0, markdown_pipe_server, &fixture, 0, NULL);
    if (server == NULL) {
        BT_ABORT("cannot start the markdown pipe server, error %lu", GetLastError());
    }

    const char *prior = getenv("BT_CONFIG");
    char saved[BT_PATH_MAX];
    snprintf(saved, sizeof saved, "%s", prior != NULL ? prior : "");
    _putenv_s("BT_CONFIG", config_path);
    run_all_ctx ctx = {0};
    run_all_once(&ctx);
    if (saved[0] != '\0') {
        _putenv_s("BT_CONFIG", saved);
    } else {
        _putenv("BT_CONFIG=");
    }

    char syllables_path[BT_PATH_MAX];
    char report_path[BT_PATH_MAX];
    snprintf(syllables_path, sizeof syllables_path, "%s/syllables.json", data_ga);
    snprintf(report_path, sizeof report_path, "%s/report.json", data_ga);
    if (GetFileAttributesA(syllables_path) == INVALID_FILE_ATTRIBUTES ||
        GetFileAttributesA(report_path) == INVALID_FILE_ATTRIBUTES) {
        bt_harness_fail(
            __FILE__, __LINE__,
            "run_all never reached the markdown write stage, rc %d, first offender '%s'", ctx.rc,
            ctx.offender_count > 0 ? ctx.first_offender : "(none)");
        CloseHandle(server);
        return;
    }
    WaitForSingleObject(server, 10000);
    CloseHandle(server);
    if (!fixture.connected) {
        bt_harness_fail(__FILE__, __LINE__, "the markdown target never accepted the writer");
        return;
    }
    if (ctx.rc != -1) {
        bt_harness_fail(__FILE__, __LINE__, "run_all returned %d with a broken markdown target",
                        ctx.rc);
        return;
    }
    char expected[BT_PATH_MAX + 64];
    snprintf(expected, sizeof expected, "write %s%s", GR_IO_PIPE_BASE, GR_IO_MARKDOWN_TAIL);
    if (strcmp(ctx.markdown_offender, expected) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "markdown offender is '%s', expected '%s'",
                        ctx.markdown_offender, expected);
    }
}
