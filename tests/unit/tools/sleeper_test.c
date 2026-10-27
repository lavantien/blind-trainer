#include "sleeper/lib.h"

#include <math.h>
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
