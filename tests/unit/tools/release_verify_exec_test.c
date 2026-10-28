#include "release-verify/exec.h"
#include "release-verify/lib.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "common/bt_harness.h"
#include "common/paths.h"
#include "unit/tools/release_verify_test_util.h"
#include "unit/tools/spawn_util.h"

static int env_contains(char **env, size_t count, const char *pair) {
    for (size_t at = 0; at < count; at++) {
        if (strcmp(env[at], pair) == 0) {
            return 1;
        }
    }
    return 0;
}

static void trim_ascii(char *text) {
    size_t len = strlen(text);
    while (len > 0 && (unsigned char)text[len - 1] <= ' ') {
        len--;
    }
    text[len] = '\0';
}

static void canonical_path(const char *in, char out[BT_PATH_MAX]) {
    HANDLE handle = CreateFileA(in, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        snprintf(out, BT_PATH_MAX, "%s", in);
        return;
    }
    char raw[BT_PATH_MAX];
    DWORD len = GetFinalPathNameByHandleA(handle, raw, sizeof raw, FILE_NAME_NORMALIZED);
    CloseHandle(handle);
    if (len == 0 || len >= sizeof raw) {
        snprintf(out, BT_PATH_MAX, "%s", in);
        return;
    }
    const char *front = raw;
    if (strncmp(raw, "\\\\?\\", 4) == 0) {
        front = raw + 4;
    }
    snprintf(out, BT_PATH_MAX, "%s", front);
}

BT_TEST(release_verify, scrub_env_removes_only_bt_prefixed_vars) {
    const char *vars[] = {
        "BT_CONFIG=x",
        "BT_ARTIFACTS=y",
        "bt_probe=z",
        "PATH=p",
        "APPDATA=a",
        "BAT_CONFIG=b",
        "BT=c",
        NULL,
    };
    char **scrubbed = NULL;
    size_t count = bt_rv_scrub_env(vars, &scrubbed);
    BT_CHECK_EQ(count, 4U);
    BT_CHECK(env_contains(scrubbed, count, "APPDATA=a"));
    BT_CHECK(env_contains(scrubbed, count, "BAT_CONFIG=b"));
    BT_CHECK(env_contains(scrubbed, count, "BT=c"));
    BT_CHECK(env_contains(scrubbed, count, "PATH=p"));
    for (size_t at = 0; at < count; at++) {
        BT_CHECK(strncmp(scrubbed[at], "BT_", 3) != 0);
        BT_CHECK(strncmp(scrubbed[at], "bt_", 3) != 0);
    }
    bt_rv_str_list_free(scrubbed, count);
}

static void pwsh_dir(const char *tag, char out[BT_PATH_MAX]) {
    rv_util_scratch(tag, out);
}

BT_TEST(release_verify, run_child_reports_exit_code_and_captured_output) {
    char cwd[BT_PATH_MAX];
    pwsh_dir("exec-exit", cwd);
    const char *args[] = {"-NoProfile", "-Command", "Write-Output hi; exit 7", NULL};
    bt_rv_child_spec spec;
    bt_rv_stage_spec("pwsh", args, cwd, NULL, NULL, 0, &spec);
    bt_rv_run_result result;
    BT_CHECK_EQ(bt_rv_run_child(&spec, 30000, &result), 0);
    BT_CHECK_EQ(result.outcome.kind, BT_RV_OUTCOME_EXIT);
    BT_CHECK_EQ(result.outcome.exit_code, 7);
    trim_ascii(result.stdout_text);
    BT_CHECK_EQ(result.stdout_text, "hi");
    bt_rv_run_result_free(&result);
    bt_rv_child_spec_free(&spec);
}

BT_TEST(release_verify, run_child_honors_cwd_and_env_map) {
    char cwd[BT_PATH_MAX];
    char sandbox[BT_PATH_MAX];
    pwsh_dir("exec-env", cwd);
    pwsh_dir("exec-appdata", sandbox);
    const char *path_value = getenv("PATH");
    char path_pair[BT_PATH_MAX];
    snprintf(path_pair, sizeof path_pair, "PATH=%s", path_value != NULL ? path_value : "");
    const char *vars[] = {"BT_PROBE=leak", path_pair, NULL};
    char appdata_pair[BT_PATH_MAX];
    snprintf(appdata_pair, sizeof appdata_pair, "%s/appdata", sandbox);
    const char *args[] = {
        "-NoProfile",
        "-Command",
        "(Get-Location).Path; $env:APPDATA; if ($env:BT_PROBE) { 'leaked' } else { 'clean' }",
        NULL,
    };
    bt_rv_child_spec spec;
    bt_rv_env_pair extras[] = {{.key = "APPDATA", .value = appdata_pair}};
    bt_rv_stage_spec("pwsh", args, cwd, vars, extras, 1, &spec);
    bt_rv_run_result result;
    BT_CHECK_EQ(bt_rv_run_child(&spec, 30000, &result), 0);
    BT_CHECK_EQ(result.outcome.kind, BT_RV_OUTCOME_EXIT);
    BT_CHECK_EQ(result.outcome.exit_code, 0);
    char *lines[4] = {NULL, NULL, NULL, NULL};
    size_t line_count = 0;
    char *cursor = result.stdout_text;
    while (*cursor != '\0' && line_count < 3) {
        lines[line_count++] = cursor;
        char *end = strpbrk(cursor, "\r\n");
        if (end == NULL) {
            break;
        }
        *end = '\0';
        cursor = end + 1;
        while (*cursor == '\r' || *cursor == '\n') {
            cursor++;
        }
    }
    BT_CHECK_EQ(line_count, 3U);
    if (line_count == 3) {
        char printed_cwd[BT_PATH_MAX];
        char wanted_cwd[BT_PATH_MAX];
        canonical_path(lines[0], printed_cwd);
        canonical_path(cwd, wanted_cwd);
        BT_CHECK(_stricmp(printed_cwd, wanted_cwd) == 0);
        char printed_appdata[BT_PATH_MAX];
        char wanted_appdata[BT_PATH_MAX];
        canonical_path(lines[1], printed_appdata);
        canonical_path(appdata_pair, wanted_appdata);
        BT_CHECK(_stricmp(printed_appdata, wanted_appdata) == 0);
        BT_CHECK_EQ(lines[2], "clean");
    }
    bt_rv_run_result_free(&result);
    bt_rv_child_spec_free(&spec);
}

BT_TEST(release_verify, run_child_reports_spawn_failure) {
    char temp[BT_PATH_MAX];
    GetTempPathA(sizeof temp, temp);
    const char *args[] = {NULL};
    bt_rv_child_spec spec;
    bt_rv_stage_spec("definitely-not-a-program-bt-rv", args, temp, NULL, NULL, 0, &spec);
    bt_rv_run_result result;
    BT_CHECK_EQ(bt_rv_run_child(&spec, 30000, &result), 0);
    BT_CHECK_EQ(result.outcome.kind, BT_RV_OUTCOME_SPAWN);
    bt_rv_run_result_free(&result);
    bt_rv_child_spec_free(&spec);
}

BT_TEST(release_verify, run_child_kills_the_tree_on_timeout) {
    char cwd[BT_PATH_MAX];
    pwsh_dir("exec-timeout", cwd);
    const char *args[] = {"-NoProfile", "-Command", "Start-Sleep -Seconds 30", NULL};
    bt_rv_child_spec spec;
    bt_rv_stage_spec("pwsh", args, cwd, NULL, NULL, 0, &spec);
    ULONGLONG started = GetTickCount64();
    bt_rv_run_result result;
    BT_CHECK_EQ(bt_rv_run_child(&spec, 3000, &result), 0);
    ULONGLONG elapsed = GetTickCount64() - started;
    BT_CHECK_EQ(result.outcome.kind, BT_RV_OUTCOME_TIMEOUT);
    BT_CHECK(result.elapsed_ms < 25000);
    BT_CHECK(elapsed < 25000);
    char pid_text[32];
    snprintf(pid_text, sizeof pid_text, "%lu", result.pid);
    char out[4096];
    char err[512];
    int code = -1;
    char filter[64];
    snprintf(filter, sizeof filter, "PID eq %lu", result.pid);
    const char *tasklist_argv[] = {"/FI", filter, NULL};
    int rc = bt_spawn_capture("tasklist", tasklist_argv, NULL, 30000, &code, out, sizeof out, err,
                              sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK(strstr(out, pid_text) == NULL);
    bt_rv_run_result_free(&result);
    bt_rv_child_spec_free(&spec);
}
