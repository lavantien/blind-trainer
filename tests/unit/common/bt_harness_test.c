#include "common/bt_harness.h"

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

#ifdef __LLVM_PROFILE_GENERATE
#include <profile/InstrProf.h>
#endif

static bool coverage_instrumented(void) {
#ifdef __LLVM_PROFILE_GENERATE
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
