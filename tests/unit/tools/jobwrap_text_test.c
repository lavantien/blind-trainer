#include "jobwrap/lib.h"

#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

#include "common/bt_harness.h"
#include "common/json.h"
#include "common/paths.h"
#include "jobwrap/ffi.h"

static bt_json_value *config_of(const char *body) {
    bt_json_value *config = NULL;
    bt_json_error error;
    if (bt_json_parse(body, strlen(body), &config, &error) != 0) {
        BT_ABORT("bad test json: %s", error.message);
    }
    return config;
}

static char g_config_saved[BT_PATH_MAX];
static int g_config_had = -1;

static void set_config_env(const char *value) {
    if (g_config_had < 0) {
        const char *raw = getenv("BT_CONFIG");
        g_config_had = raw != NULL && raw[0] != '\0';
        snprintf(g_config_saved, sizeof g_config_saved, "%s", raw != NULL ? raw : "");
    }
    _putenv_s("BT_CONFIG", value);
}

static void restore_config_env(void) {
    if (g_config_had > 0) {
        _putenv_s("BT_CONFIG", g_config_saved);
    } else {
        _putenv("BT_CONFIG=");
    }
}

static void write_config_pair(char dir[BT_PATH_MAX], const char *tag, const char *body) {
    snprintf(dir, BT_PATH_MAX, "%s\\bt-jobwrap-text-cfg-%s-%d", getenv("TEMP"), tag, _getpid());
    CreateDirectoryA(dir, NULL);
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s\\config.json", dir);
    FILE *file = fopen(path, "wb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        fputs(body, file);
        fclose(file);
    }
}

static void config_file_path(const char *dir, char out[BT_PATH_MAX]) {
    snprintf(out, BT_PATH_MAX, "%s\\config.json", dir);
}

static void drop_config_dir(const char *dir) {
    char path[BT_PATH_MAX];
    config_file_path(dir, path);
    DeleteFileA(path);
    RemoveDirectoryA(dir);
}

BT_TEST(jobwrap, pid_dir_blank_check_covers_each_unicode_whitespace_class) {
    char out[BT_PATH_MAX];
    char err[BT_JOBWRAP_ERR_MAX];
    err[0] = '\0';
    static const char *blanks[] = {
        "\"\xc2\x85\"",     "\"\xe1\x9a\x80\"", "\"\xe2\x80\xa8\"",
        "\"\xe2\x80\x83\"", "\"\xe2\x80\xaf\"", "\"\xe2\x81\x9f\"",
    };
    for (size_t at = 0; at < sizeof blanks / sizeof blanks[0]; at++) {
        char body[160];
        snprintf(body, sizeof body, "{\"memguard\":{\"pid_dir\":%s}}", blanks[at]);
        bt_json_value *config = config_of(body);
        BT_CHECK(bt_jobwrap_pid_dir_from(config, out, err, sizeof err) != 0);
        BT_CHECK_EQ(strcmp(err, "config value invalid: memguard.pid_dir"), 0);
        bt_json_free(config);
    }
    bt_json_value *mixed = config_of("{\"memguard\":{\"pid_dir\":\"\xc2\x85x\xe2\x80\xa9\"}}");
    BT_CHECK_EQ(bt_jobwrap_pid_dir_from(mixed, out, err, sizeof err), 0);
    bt_json_free(mixed);
}

BT_TEST(jobwrap, pid_dir_passes_wide_utf8_through_as_path_bytes) {
    char out[BT_PATH_MAX];
    char err[BT_JOBWRAP_ERR_MAX];
    err[0] = '\0';
    bt_json_value *wide = config_of("{\"memguard\":{\"pid_dir\":\"\xf0\x9f\x98\x80ghi\"}}");
    BT_CHECK_EQ(bt_jobwrap_pid_dir_from(wide, out, err, sizeof err), 0);
    BT_CHECK(strstr(out, "ghi") != NULL);
    bt_json_free(wide);
    bt_json_value *accented = config_of("{\"memguard\":{\"pid_dir\":\"\xc3\xa9jkl\"}}");
    BT_CHECK_EQ(bt_jobwrap_pid_dir_from(accented, out, err, sizeof err), 0);
    BT_CHECK(strstr(out, "jkl") != NULL);
    bt_json_free(accented);
}

BT_TEST(jobwrap, run_reports_a_blocked_anchor_pid_file) {
    char dir[BT_PATH_MAX];
    char body[BT_PATH_MAX + 128];
    char pid_dir[BT_PATH_MAX];
    char temp[BT_PATH_MAX];
    snprintf(temp, sizeof temp, "%s", getenv("TEMP"));
    for (char *cursor = temp; *cursor != '\0'; cursor++) {
        if (*cursor == '\\') {
            *cursor = '/';
        }
    }
    snprintf(pid_dir, sizeof pid_dir, "%s/bt-jobwrap-text-blocked-%d", temp, _getpid());
    CreateDirectoryA(pid_dir, NULL);
    char blocker[BT_PATH_MAX];
    snprintf(blocker, sizeof blocker, "%s/blk-%lu.pid", pid_dir, GetCurrentProcessId());
    CreateDirectoryA(blocker, NULL);
    snprintf(body, sizeof body,
             "{\"job\":{\"per_process_bytes\":4294967296},\"memguard\":{\"pid_dir\":\"%s\"}}",
             pid_dir);
    write_config_pair(dir, "blocked", body);
    char config_path[BT_PATH_MAX];
    config_file_path(dir, config_path);
    set_config_env(config_path);
    bt_jobwrap_invocation invocation;
    memset(&invocation, 0, sizeof invocation);
    invocation.kind = BT_JOBWRAP_INVOCATION_WRAP;
    invocation.anchor = L"blk";
    const wchar_t *payload[] = {L"cmd", L"/c", L"exit 0"};
    invocation.payload = payload;
    invocation.payload_count = 3;
    unsigned int code = 0;
    char err[BT_JOBWRAP_ERR_MAX];
    err[0] = '\0';
    BT_CHECK_EQ(bt_jobwrap_run(&invocation, &code, err, sizeof err), BT_JOBWRAP_RUN_FAILURE);
    BT_CHECK(strstr(err, "write ") != NULL);
    restore_config_env();
    drop_config_dir(dir);
    RemoveDirectoryA(blocker);
    RemoveDirectoryA(pid_dir);
}

BT_TEST(jobwrap, run_names_an_empty_payload_and_fails_to_spawn_it) {
    char dir[BT_PATH_MAX];
    write_config_pair(dir, "emptypay",
                      "{\"job\":{\"per_process_bytes\":4294967296},"
                      "\"memguard\":{\"pid_dir\":\"artifacts/progress/pids\"}}");
    char config_path[BT_PATH_MAX];
    config_file_path(dir, config_path);
    set_config_env(config_path);
    bt_jobwrap_invocation invocation;
    memset(&invocation, 0, sizeof invocation);
    invocation.kind = BT_JOBWRAP_INVOCATION_WRAP;
    const wchar_t *payload[] = {NULL};
    invocation.payload = payload;
    invocation.payload_count = 0;
    unsigned int code = 0;
    char err[BT_JOBWRAP_ERR_MAX];
    err[0] = '\0';
    BT_CHECK_EQ(bt_jobwrap_run(&invocation, &code, err, sizeof err), BT_JOBWRAP_RUN_FAILURE);
    BT_CHECK(strstr(err, "spawn failed") != NULL);
    restore_config_env();
    drop_config_dir(dir);
}

BT_TEST(jobwrap, create_ceiling_job_rejects_a_subpage_limit) {
    HANDLE job = NULL;
    unsigned long rc = bt_jobwrap_create_ceiling_job(1, &job);
    if (rc == 0) {
        bt_jobwrap_close_handle(job);
    }
    BT_CHECK_EQ(rc, 87UL);
}
