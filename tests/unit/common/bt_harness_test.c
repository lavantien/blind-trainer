#include "common/bt_harness.h"
#include "unit/core/death_test.h"

#include <direct.h>
#include <process.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

int bt_harness_noreturn_pin(void) {
    bt_harness_exit_failure();
}

static void inner_pass(void) {
    BT_CHECK(1);
}

BT_TEST(bt_harness, run_reports_green_suite) {
    bt_test_entry entries[] = {{"inner_pass", inner_pass}};
    BT_CHECK_EQ(bt_harness_run("bt_harness_inner", entries, 1), 0);
}

static void inner_fail_check(void) {
    BT_CHECK(1 == 2);
}

static void inner_fail_eq_int(void) {
    BT_CHECK_EQ(1, 2);
}

static void inner_fail_eq_u64(void) {
    BT_CHECK_EQ(1ULL, 2ULL);
}

static void inner_fail_eq_f64(void) {
    BT_CHECK_EQ(1.5, 2.5);
}

static void inner_fail_eq_str(void) {
    const char *actual = "a";
    const char *expected = "b";
    BT_CHECK_EQ(actual, expected);
}

static void inner_fail_eq_str_null_actual(void) {
    const char *actual = NULL;
    const char *expected = "b";
    BT_CHECK_EQ(actual, expected);
}

static void inner_fail_eq_str_null_expected(void) {
    const char *actual = "a";
    const char *expected = NULL;
    BT_CHECK_EQ(actual, expected);
}

static void inner_fail_near(void) {
    BT_CHECK_F64_NEAR(1.0, 2.0, 0.5);
}

static void inner_fail_check_msg(void) {
    bt_harness_check_msg(__FILE__, __LINE__, 0, "msg %s", "pin");
}

BT_TEST(bt_harness, run_reports_failing_checks_per_kind_and_suite_status) {
    bt_test_entry entries[] = {
        {"inner_fail_check", inner_fail_check},
        {"inner_fail_eq_int", inner_fail_eq_int},
        {"inner_fail_eq_u64", inner_fail_eq_u64},
        {"inner_fail_eq_f64", inner_fail_eq_f64},
        {"inner_fail_eq_str", inner_fail_eq_str},
        {"inner_fail_eq_str_null_actual", inner_fail_eq_str_null_actual},
        {"inner_fail_eq_str_null_expected", inner_fail_eq_str_null_expected},
        {"inner_fail_near", inner_fail_near},
        {"inner_fail_check_msg", inner_fail_check_msg},
    };
    size_t count = sizeof entries / sizeof entries[0];
    BT_CHECK_EQ(bt_harness_run("bt_harness_inner_fail", entries, count), 1);
}

static bool coverage_instrumented(void) {
#ifdef __LLVM_INSTR_PROFILE_GENERATE
    return true;
#else
    return false;
#endif
}

static void abort_child(void) {
    abort();
}

BT_TEST(bt_harness, an_aborting_child_still_writes_its_coverage_profile) {
    const char *child = getenv("BT_HARNESS_ABORT_CHILD");
    if (child != NULL) {
        abort_child();
    }
    bool instrumented = coverage_instrumented();
    _putenv("BT_HARNESS_ABORT_CHILD=1");
    _putenv("LLVM_PROFILE_FILE=build/test-tmp/harness-abort-%p.profraw");
    _mkdir("build");
    _mkdir("build/test-tmp");
    WIN32_FIND_DATAA entry;
    HANDLE find = FindFirstFileA("build/test-tmp/harness-abort-*.profraw", &entry);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            char path[MAX_PATH];
            snprintf(path, sizeof path, "build/test-tmp/%s", entry.cFileName);
            remove(path);
        } while (FindNextFileA(find, &entry));
        FindClose(find);
    }
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    intptr_t code = _wspawnl(_P_WAIT, exe, exe, NULL);
    _putenv("BT_HARNESS_ABORT_CHILD=");
    _putenv("LLVM_PROFILE_FILE=");
    BT_CHECK_EQ((long long)code, -1073740791LL);
    find = FindFirstFileA("build/test-tmp/harness-abort-*.profraw", &entry);
    if (instrumented) {
        BT_CHECK(find != INVALID_HANDLE_VALUE);
        if (find != INVALID_HANDLE_VALUE) {
            char path[MAX_PATH];
            snprintf(path, sizeof path, "build/test-tmp/%s", entry.cFileName);
            WIN32_FILE_ATTRIBUTE_DATA info;
            BT_CHECK(GetFileAttributesExA(path, GetFileExInfoStandard, &info) != 0);
            BT_CHECK(info.nFileSizeLow > 0 || info.nFileSizeHigh > 0);
            FindClose(find);
            remove(path);
        }
        return;
    }
    BT_CHECK(find == INVALID_HANDLE_VALUE);
    if (find != INVALID_HANDLE_VALUE) {
        FindClose(find);
    }
}

static void harness_death_abort_pin(void) {
    BT_ABORT("pin");
}

BT_TEST(bt_harness, abort_macro_fails_then_exits_with_the_suite_code) {
    bt_death_expect_code("harness.abort_pin", "abort: pin", 134, harness_death_abort_pin);
}
