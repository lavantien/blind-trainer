#include "sleeper/lib.h"

#include <math.h>
#include <process.h>
#include <stdio.h>
#include <string.h>

#include "common/bt_harness.h"
#include "unit/tools/spawn_util.h"

static int plain_ok(const char *raw, double *secs) {
    char *argv[1] = {(char *)raw};
    char err[BT_SLEEPER_ERR_MAX];
    int rc = bt_sleeper_parse_args(1, argv, secs, err, sizeof err);
    if (rc != 0) {
        fprintf(stderr, "parse_args(%s): %s\n", raw, err);
    }
    return rc;
}

static int plain_err(const char *raw) {
    double secs = 0.0;
    char *argv[1] = {(char *)raw};
    char err[BT_SLEEPER_ERR_MAX];
    err[0] = '\0';
    int rc = bt_sleeper_parse_args(1, argv, &secs, err, sizeof err);
    if (rc == 0) {
        fprintf(stderr, "parse_args(%s) unexpectedly ok: %f\n", raw, secs);
    }
    return rc != 0 && err[0] != '\0';
}

static int mode_ok(char **argv, int argc, int *mode, unsigned long long *value) {
    char err[BT_SLEEPER_ERR_MAX];
    err[0] = '\0';
    int rc = bt_sleeper_parse_mode(argc, argv, mode, value, err, sizeof err);
    if (rc != 0) {
        fprintf(stderr, "parse_mode(%s): %s\n", argv[0], err);
    }
    return rc;
}

static int mode_err(char **argv, int argc) {
    int mode = BT_SLEEPER_PLAIN;
    unsigned long long value = 0;
    char err[BT_SLEEPER_ERR_MAX];
    err[0] = '\0';
    int rc = bt_sleeper_parse_mode(argc, argv, &mode, &value, err, sizeof err);
    if (rc == 0) {
        fprintf(stderr, "parse_mode(%s) unexpectedly ok: %d %llu\n", argv[0], mode, value);
    }
    return rc != 0 && err[0] != '\0';
}

BT_TEST(sleeper, parses_positive_decimals) {
    double secs = -1.0;
    BT_CHECK_EQ(plain_ok("3.5", &secs), 0);
    BT_CHECK_EQ(secs, 3.5);
    BT_CHECK_EQ(plain_ok("0.1", &secs), 0);
    BT_CHECK_EQ(secs, 0.1);
    BT_CHECK_EQ(plain_ok("0", &secs), 0);
    BT_CHECK_EQ(secs, 0.0);
}

BT_TEST(sleeper, rejects_non_numeric) {
    BT_CHECK(plain_err("abc"));
    BT_CHECK(plain_err("0x10"));
    BT_CHECK(plain_err(" 1.5"));
    BT_CHECK(plain_err(""));
}

BT_TEST(sleeper, rejects_negative_and_non_finite) {
    BT_CHECK(plain_err("-1"));
    BT_CHECK(plain_err("inf"));
    BT_CHECK(plain_err("NaN"));
}

BT_TEST(sleeper, rejects_missing_and_extra_args) {
    double secs = 0.0;
    char err[BT_SLEEPER_ERR_MAX];
    err[0] = '\0';
    int rc = bt_sleeper_parse_args(0, NULL, &secs, err, sizeof err);
    BT_CHECK(rc != 0 && err[0] != '\0');
    char *two[2] = {(char *)"1", (char *)"2"};
    err[0] = '\0';
    rc = bt_sleeper_parse_args(2, two, &secs, err, sizeof err);
    BT_CHECK(rc != 0 && err[0] != '\0');
}

BT_TEST(sleeper, parses_selftest_modes) {
    int mode = BT_SLEEPER_PLAIN;
    unsigned long long value = 99;
    char *alloc[2] = {(char *)"--selftest-alloc-mb", (char *)"640"};
    BT_CHECK_EQ(mode_ok(alloc, 2, &mode, &value), 0);
    BT_CHECK_EQ(mode, BT_SLEEPER_ALLOC_MB);
    BT_CHECK_EQ(value, 640ULL);
    char *plus[2] = {(char *)"--selftest-alloc-mb", (char *)"+640"};
    BT_CHECK_EQ(mode_ok(plus, 2, &mode, &value), 0);
    BT_CHECK_EQ(mode, BT_SLEEPER_ALLOC_MB);
    BT_CHECK_EQ(value, 640ULL);
    char *hold[2] = {(char *)"--selftest-hold-secs", (char *)"0"};
    BT_CHECK_EQ(mode_ok(hold, 2, &mode, &value), 0);
    BT_CHECK_EQ(mode, BT_SLEEPER_HOLD_SECS);
    BT_CHECK_EQ(value, 0ULL);
    double secs = 0.0;
    BT_CHECK_EQ(plain_ok("1.5", &secs), 0);
    BT_CHECK_EQ(secs, 1.5);
}

BT_TEST(sleeper, rejects_bad_selftest_modes) {
    char *zero[2] = {(char *)"--selftest-alloc-mb", (char *)"0"};
    BT_CHECK(mode_err(zero, 2));
    char *abc[2] = {(char *)"--selftest-alloc-mb", (char *)"abc"};
    BT_CHECK(mode_err(abc, 2));
    char *missing[1] = {(char *)"--selftest-alloc-mb"};
    BT_CHECK(mode_err(missing, 1));
    char *extra[3] = {(char *)"--selftest-hold-secs", (char *)"3", (char *)"4"};
    BT_CHECK(mode_err(extra, 3));
}

BT_TEST(sleeper, rejects_selftest_overflow_and_empty_numbers) {
    char *over[2] = {(char *)"--selftest-alloc-mb", (char *)"18446744073709551616"};
    BT_CHECK(mode_err(over, 2));
    char *plus_empty[2] = {(char *)"--selftest-hold-secs", (char *)"+"};
    BT_CHECK(mode_err(plus_empty, 2));
    char *plus_ok[2] = {(char *)"--selftest-hold-secs", (char *)"+2"};
    int mode = BT_SLEEPER_PLAIN;
    unsigned long long value = 99;
    BT_CHECK_EQ(mode_ok(plus_ok, 2, &mode, &value), 0);
    BT_CHECK_EQ(value, 2ULL);
}

BT_TEST(sleeper, parse_mode_treats_a_null_head_as_plain) {
    char *head_null[1] = {NULL};
    int mode = BT_SLEEPER_ALLOC_MB;
    unsigned long long value = 7;
    char err[BT_SLEEPER_ERR_MAX];
    err[0] = '\0';
    int rc = bt_sleeper_parse_mode(1, head_null, &mode, &value, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(mode, BT_SLEEPER_PLAIN);
}

BT_TEST(sleeper, sleep_ms_zero_chunk_runs_the_total_at_once) {
    ULONGLONG started = GetTickCount64();
    int rc = bt_sleeper_sleep_ms(250, 0);
    ULONGLONG elapsed = GetTickCount64() - started;
    BT_CHECK_EQ(rc, 0);
    BT_CHECK(elapsed >= 250);
}

BT_TEST(sleeper, sleep_ms_covers_the_full_total_across_chunks) {
    ULONGLONG started = GetTickCount64();
    int rc = bt_sleeper_sleep_ms(450, 100);
    ULONGLONG elapsed = GetTickCount64() - started;
    BT_CHECK_EQ(rc, 0);
    BT_CHECK(elapsed >= 450);
    started = GetTickCount64();
    rc = bt_sleeper_sleep_ms(0, 100);
    elapsed = GetTickCount64() - started;
    BT_CHECK_EQ(rc, 0);
    BT_CHECK(elapsed < 1000);
}

BT_TEST(sleeper, exe_hold_mode_runs_the_full_duration) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "sleeper.exe");
    const char *argv[] = {"--selftest-hold-secs", "2", NULL};
    char out[512];
    char err[2048];
    int code = -1;
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    ULONGLONG started = GetTickCount64();
    int rc = bt_spawn_capture(exe, argv, root, 30000, &code, out, sizeof out, err, sizeof err);
    ULONGLONG elapsed = GetTickCount64() - started;
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 0);
    BT_CHECK(elapsed >= 2000);
}

BT_TEST(sleeper, exe_plain_run_exits_zero) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "sleeper.exe");
    const char *argv[] = {"0.2", NULL};
    char out[512];
    char err[512];
    int code = -1;
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    int rc = bt_spawn_capture(exe, argv, root, 30000, &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 0);
}

BT_TEST(sleeper, exe_usage_error_exits_2) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "sleeper.exe");
    const char *argv[] = {"abc", NULL};
    char out[512];
    char err[512];
    int code = -1;
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    int rc = bt_spawn_capture(exe, argv, root, 30000, &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 2);
    BT_CHECK(strstr(err, "usage: bt-sleeper <secs>") != NULL);
}

BT_TEST(sleeper, exe_selftest_mode_rejects_zero) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "sleeper.exe");
    const char *argv[] = {"--selftest-alloc-mb", "0", NULL};
    char out[512];
    char err[512];
    int code = -1;
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    int rc = bt_spawn_capture(exe, argv, root, 30000, &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 2);
    BT_CHECK(strstr(err, "selftest alloc mb must be >= 1") != NULL);
}

static void spawn_with_config(const char *config_value, const char *const argv[], int timeout_ms,
                              int *code, char *err, size_t err_cap) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "sleeper.exe");
    char saved[BT_PATH_MAX];
    DWORD had = GetEnvironmentVariableA("BT_CONFIG", saved, BT_PATH_MAX);
    if (config_value == NULL) {
        SetEnvironmentVariableA("BT_CONFIG", NULL);
    } else {
        SetEnvironmentVariableA("BT_CONFIG", config_value);
    }
    char out[512];
    int rc = bt_spawn_capture(exe, argv, NULL, timeout_ms, code, out, sizeof out, err, err_cap);
    SetEnvironmentVariableA("BT_CONFIG", had > 0 && had < BT_PATH_MAX ? saved : NULL);
    BT_CHECK_EQ(rc, 0);
}

BT_TEST(sleeper, exe_runs_with_an_unreadable_config_on_defaults) {
    char err[512];
    int code = -1;
    const char *argv[] = {"0.15", NULL};
    spawn_with_config("Z:/no-such-dir/no-such-config.json", argv, 30000, &code, err, sizeof err);
    BT_CHECK_EQ(code, 0);
}

BT_TEST(sleeper, exe_runs_with_a_config_that_has_no_trace_keys) {
    char base[BT_PATH_MAX];
    DWORD n = GetTempPathA(BT_PATH_MAX, base);
    BT_CHECK(n > 0 && n < BT_PATH_MAX);
    char dir[BT_PATH_MAX];
    snprintf(dir, sizeof dir, "%sbt-sleeper-cfg-%lu", base, GetCurrentProcessId());
    bt_test_mkdirs(dir);
    char config[BT_PATH_MAX];
    snprintf(config, sizeof config, "%s/minimal.json", dir);
    FILE *file = fopen(config, "wb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        fputs("{}", file);
        fclose(file);
    }
    char err[512];
    int code = -1;
    const char *argv[] = {"0.15", NULL};
    spawn_with_config(config, argv, 30000, &code, err, sizeof err);
    BT_CHECK_EQ(code, 0);
    char config_full[BT_PATH_MAX];
    snprintf(config_full, sizeof config_full, "%s/full.json", dir);
    file = fopen(config_full, "wb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        fputs("{\"trace\": {\"ring_bytes\": 4096, \"max_threads\": 2, \"line_max\": 128}}", file);
        fclose(file);
    }
    code = -1;
    spawn_with_config(config_full, argv, 30000, &code, err, sizeof err);
    BT_CHECK_EQ(code, 0);
    DeleteFileA(config);
    DeleteFileA(config_full);
    RemoveDirectoryA(dir);
}

BT_TEST(sleeper, exe_alloc_mode_holds_and_exits_zero) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "sleeper.exe");
    const char *argv[] = {"--selftest-alloc-mb", "32", NULL};
    char out[512];
    char err[2048];
    int code = -1;
    int rc = bt_spawn_capture(exe, argv, NULL, 60000, &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 0);
    BT_CHECK(strstr(err, "selftest alloc touched 32 MiB and holds it") != NULL);
}

static void restore_bin_dir(const char *saved) {
    if (saved[0] != '\0') {
        _putenv_s("BT_BIN_DIR", saved);
    } else {
        _putenv("BT_BIN_DIR=");
    }
}

BT_TEST(bin_path, override_dir_wins_over_the_plain_build_bin) {
    char saved[BT_PATH_MAX];
    snprintf(saved, sizeof saved, "%s", getenv("BT_BIN_DIR") != NULL ? getenv("BT_BIN_DIR") : "");
    char base[BT_PATH_MAX];
    snprintf(base, sizeof base, "%s\\bt-bin-path-override-%d", getenv("TEMP"), _getpid());
    bt_test_mkdirs(base);
    char exe[BT_PATH_MAX];
    _putenv_s("BT_BIN_DIR", base);
    bt_test_bin_path(exe, "sleeper.exe");
    restore_bin_dir(saved);
    BT_CHECK(strncmp(exe, base, strlen(base)) == 0);
    BT_CHECK(strstr(exe, "sleeper.exe") != NULL);
}

BT_TEST(bin_path, empty_override_falls_back_to_the_plain_build_bin) {
    char saved[BT_PATH_MAX];
    snprintf(saved, sizeof saved, "%s", getenv("BT_BIN_DIR") != NULL ? getenv("BT_BIN_DIR") : "");
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    char expected[BT_PATH_MAX];
    snprintf(expected, sizeof expected, "%s/build/bin/sleeper.exe", root);
    char exe[BT_PATH_MAX];
    _putenv_s("BT_BIN_DIR", "");
    bt_test_bin_path(exe, "sleeper.exe");
    restore_bin_dir(saved);
    BT_CHECK(strcmp(exe, expected) == 0);
}
