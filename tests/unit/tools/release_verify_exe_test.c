#include "common/bt_harness.h"
#include "common/paths.h"
#include "unit/tools/spawn_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static void exe_path(char out[BT_PATH_MAX]) {
    bt_test_bin_path(out, "release-verify.exe");
}

static int spawn_rv(const char *const argv[], const char *config_value, char *out, size_t out_cap,
                    char *err, size_t err_cap) {
    char exe[BT_PATH_MAX];
    exe_path(exe);
    char saved[BT_PATH_MAX];
    DWORD had = GetEnvironmentVariableA("BT_CONFIG", saved, BT_PATH_MAX);
    SetEnvironmentVariableA("BT_CONFIG", config_value);
    int code = -1;
    int rc = bt_spawn_capture(exe, argv, NULL, 60000, &code, out, out_cap, err, err_cap);
    SetEnvironmentVariableA("BT_CONFIG", had > 0 && had < BT_PATH_MAX ? saved : NULL);
    BT_CHECK_EQ(rc, 0);
    return code;
}

static void write_config(const char *tag, const char *body, char out[BT_PATH_MAX]) {
    char base[BT_PATH_MAX];
    DWORD n = GetTempPathA(BT_PATH_MAX, base);
    BT_CHECK(n > 0 && n < BT_PATH_MAX);
    snprintf(out, BT_PATH_MAX, "%sbt-rv-exe-%s-%lu\\config.json", base, tag, GetCurrentProcessId());
    char dir[BT_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", out);
    char *cut = strrchr(dir, '\\');
    if (cut != NULL) {
        *cut = '\0';
        CreateDirectoryA(dir, NULL);
    }
    FILE *file = fopen(out, "wb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        fputs(body, file);
        fclose(file);
    }
}

static void scratch_rv_staging(char out[BT_PATH_MAX]) {
    char base[BT_PATH_MAX];
    DWORD n = GetTempPathA(BT_PATH_MAX, base);
    BT_CHECK(n > 0 && n < BT_PATH_MAX);
    snprintf(out, BT_PATH_MAX, "%sbt-rv-exe-staging-%lu", base, GetCurrentProcessId());
    CreateDirectoryA(out, NULL);
}

BT_TEST(release_verify_exe, no_arguments_exit_2_with_usage) {
    char out[2048];
    char err[2048];
    const char *argv[] = {NULL};
    int code = spawn_rv(argv, NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 2);
    BT_CHECK(strstr(err, "usage: bt-release-verify") != NULL);
}

BT_TEST(release_verify_exe, alloc_starved_argv_exits_usage) {
    char out[2048];
    char err[2048];
    const char *argv[] = {"staging", "app.zip", NULL};
    char exe[BT_PATH_MAX];
    exe_path(exe);
    const char *settings[3] = {"0", "1", "2"};
    for (size_t at = 0; at < 2; at++) {
        SetEnvironmentVariableA("BT_RV_ALLOC_FAIL_OK", settings[at]);
        int code = -1;
        int rc = bt_spawn_capture(exe, argv, NULL, 60000, &code, out, sizeof out, err, sizeof err);
        SetEnvironmentVariableA("BT_RV_ALLOC_FAIL_OK", NULL);
        BT_CHECK_EQ(rc, 0);
        BT_CHECK_EQ(code, 2);
    }
}

BT_TEST(release_verify_exe, unknown_flag_exit_2) {
    char out[2048];
    char err[2048];
    const char *argv[] = {"staging", "zip", "--bogus", NULL};
    int code = spawn_rv(argv, NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 2);
    BT_CHECK(strstr(err, "unknown flag --bogus") != NULL);
}

BT_TEST(release_verify_exe, unreadable_config_exit_2) {
    char staging[BT_PATH_MAX];
    scratch_rv_staging(staging);
    char out[2048];
    char err[2048];
    const char *argv[] = {staging, "missing.zip", NULL};
    int code =
        spawn_rv(argv, "Z:/no-such/release-verify-config.json", out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 2);
    BT_CHECK(strstr(err, "cannot read config") != NULL);
}

BT_TEST(release_verify_exe, config_without_release_section_exit_2) {
    char staging[BT_PATH_MAX];
    scratch_rv_staging(staging);
    char config[BT_PATH_MAX];
    write_config("empty", "{}", config);
    char out[2048];
    char err[2048];
    const char *argv[] = {staging, "missing.zip", NULL};
    int code = spawn_rv(argv, config, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 2);
    BT_CHECK(strstr(err, "config release section missing") != NULL);
    DeleteFileA(config);
}

BT_TEST(release_verify_exe, missing_staging_dir_reports_findings_exit_1) {
    char config[BT_PATH_MAX];
    write_config("norel",
                 "{\"release\":{\"staging_dir\":\"x\",\"exe\":\"bt-app.exe\","
                 "\"config_file\":\"config.json\",\"fonts_dir\":\"fonts\","
                 "\"db_file\":\"blind-trainer.db\",\"run_timeout_secs\":30,"
                 "\"neg_config_exit\":2,\"usage_exit\":2},\"e2e\":{\"shots\":[]}}",
                 config);
    char staging[BT_PATH_MAX];
    char base[BT_PATH_MAX];
    DWORD n = GetTempPathA(BT_PATH_MAX, base);
    BT_CHECK(n > 0 && n < BT_PATH_MAX);
    snprintf(staging, sizeof staging, "%sbt-rv-exe-absent-%lu", base, GetCurrentProcessId());
    char out[2048];
    char err[2048];
    const char *argv[] = {staging, "missing.zip", NULL};
    int code = spawn_rv(argv, config, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(out, "staging") != NULL || strstr(err, "staging") != NULL);
    DeleteFileA(config);
}
