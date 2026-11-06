#include "mutate/mutate.h"
#include "mutate/tests_shared.h"

#include <string.h>

#include "common/bt_harness.h"
#include "unit/tools/spawn_util.h"

#ifdef _WIN32
#include <windows.h>
#endif

extern char g_self_exe[BT_MUT_PATH_MAX];

static void scratch_case_dir(const char *slot, char dir[BT_MUT_PATH_MAX]) {
    snprintf(dir, BT_MUT_PATH_MAX, "%s/scratch-%s", g_dir, slot);
    bt_mut_rmtree(dir);
    bt_test_mkdirs(dir);
}

void bt_test_mutate_scratch_root_passes_through_and_falls_back_to_temp(void) {
    char out[BT_MUT_PATH_MAX];
    BT_CHECK_EQ(bt_mut_scratch_root("E:/custom/root", out), 0);
    BT_CHECK(strcmp(out, "E:/custom/root") == 0);
    BT_CHECK_EQ(bt_mut_scratch_root(NULL, out), 0);
    size_t len = strlen(out);
    BT_CHECK(len > strlen("/bt-mutants") &&
             strcmp(out + len - strlen("/bt-mutants"), "/bt-mutants") == 0);
    BT_CHECK_EQ(bt_mut_scratch_root("", out), 0);
    len = strlen(out);
    BT_CHECK(len > strlen("/bt-mutants") &&
             strcmp(out + len - strlen("/bt-mutants"), "/bt-mutants") == 0);
}

void bt_test_mutate_orphan_marker_pid_short_circuits_the_creation_clock(void) {
    uint64_t backdated = bt_mut_process_start() - 3600ULL * 10000000ULL;
    uint64_t now = bt_mut_process_start();
    char self_dir[BT_MUT_PATH_MAX];
    scratch_case_dir("pid-self", self_dir);
    char marker[BT_MUT_PATH_MAX];
    snprintf(marker, BT_MUT_PATH_MAX, "%s/marker.txt", self_dir);
    char body[32];
    snprintf(body, sizeof body, "%lu", GetCurrentProcessId());
    bt_mut_write_file(marker, body, strlen(body));
    BT_CHECK_EQ(bt_mut_set_file_creation(marker, backdated), 0);
    BT_CHECK(bt_mut_scratch_is_orphan(self_dir, now) == 0);

    const char *dead_markers[] = {"0", "9999999999", "12345678901", "abc12"};
    for (size_t i = 0; i < sizeof dead_markers / sizeof dead_markers[0]; i++) {
        char dir[BT_MUT_PATH_MAX];
        char name[32];
        snprintf(name, sizeof name, "pid-dead-%zu", i);
        scratch_case_dir(name, dir);
        snprintf(marker, BT_MUT_PATH_MAX, "%s/marker.txt", dir);
        bt_mut_write_file(marker, dead_markers[i], strlen(dead_markers[i]));
        BT_CHECK_EQ(bt_mut_set_file_creation(marker, backdated), 0);
        BT_CHECK(bt_mut_scratch_is_orphan(dir, now) == 1);
    }
}

void bt_test_mutate_orphan_without_marker_uses_the_first_file_creation(void) {
    uint64_t backdated = bt_mut_process_start() - 3600ULL * 10000000ULL;
    uint64_t now = bt_mut_process_start();
    char dir[BT_MUT_PATH_MAX];
    scratch_case_dir("nomarker", dir);
    char planted[BT_MUT_PATH_MAX];
    snprintf(planted, BT_MUT_PATH_MAX, "%s/leftover.obj", dir);
    bt_mut_write_file(planted, "x", 1);
    BT_CHECK_EQ(bt_mut_set_file_creation(planted, backdated), 0);
    BT_CHECK(bt_mut_scratch_is_orphan(dir, now) == 1);
    BT_CHECK(bt_mut_scratch_is_orphan(dir, backdated) == 0);
}

void bt_test_mutate_orphan_and_sweep_refuse_roots_they_cannot_read(void) {
    uint64_t now = bt_mut_process_start();
    char missing[BT_MUT_PATH_MAX];
    snprintf(missing, BT_MUT_PATH_MAX, "%s/absent-scratch-root", g_dir);
    bt_mut_rmtree(missing);
    BT_CHECK_EQ(bt_mut_scratch_is_orphan(missing, now), -1);
    BT_CHECK_EQ(bt_mut_scratch_sweep(missing, now), 0ULL);
}

void bt_test_mutate_orphan_marker_with_a_foreign_live_pid_stays(void) {
    char command[BT_MUT_PATH_MAX * 2];
    snprintf(command, sizeof command, "\"%s\" selftest-flood", g_self_exe);
    wchar_t wide[BT_MUT_PATH_MAX * 2];
    MultiByteToWideChar(CP_UTF8, 0, command, -1, wide, BT_MUT_PATH_MAX * 2);
    STARTUPINFOW startup = {.cb = sizeof startup};
    PROCESS_INFORMATION info = {0};
    BT_CHECK(CreateProcessW(NULL, wide, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &info) != 0);
    char dir[BT_MUT_PATH_MAX];
    scratch_case_dir("pid-foreign", dir);
    char marker[BT_MUT_PATH_MAX];
    snprintf(marker, BT_MUT_PATH_MAX, "%s/marker.txt", dir);
    char body[32];
    snprintf(body, sizeof body, "%lu", info.dwProcessId);
    bt_mut_write_file(marker, body, strlen(body));
    uint64_t backdated = bt_mut_process_start() - 3600ULL * 10000000ULL;
    BT_CHECK_EQ(bt_mut_set_file_creation(marker, backdated), 0);
    int alive_verdict = bt_mut_scratch_is_orphan(dir, bt_mut_process_start());
    TerminateProcess(info.hProcess, 0);
    WaitForSingleObject(info.hProcess, 5000);
    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);
    BT_CHECK(alive_verdict == 0);
    int dead_verdict = bt_mut_scratch_is_orphan(dir, bt_mut_process_start());
    BT_CHECK(dead_verdict == 1);
}
