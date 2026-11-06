#include "unit/tools/spawn_util.h"

#include <string.h>

#include "common/bt_harness.h"

BT_TEST(spawn_util, error_classes_split_transient_from_permanent) {
    BT_CHECK_EQ(bt_spawn_error_is_transient("stdout pipe failed: 231"), 1);
    BT_CHECK_EQ(bt_spawn_error_is_transient("stderr pipe failed: 1455"), 1);
    BT_CHECK_EQ(bt_spawn_error_is_transient("CreateProcessW failed: 231"), 1);
    BT_CHECK_EQ(bt_spawn_error_is_transient("CreateProcessW failed: 8"), 1);
    BT_CHECK_EQ(bt_spawn_error_is_transient("CreateProcessW failed: 2"), 0);
    BT_CHECK_EQ(bt_spawn_error_is_transient("CreateProcessW failed: 3"), 0);
    BT_CHECK_EQ(bt_spawn_error_is_transient("command too long: x"), 0);
    BT_CHECK_EQ(bt_spawn_error_is_transient(""), 0);
}

BT_TEST(spawn_util, command_rejects_more_arguments_than_the_part_table_holds) {
    const char *many[40];
    for (size_t at = 0; at < 39; at++) {
        many[at] = "x";
    }
    many[39] = NULL;
    char out[64];
    BT_CHECK(bt_spawn_command("exe", many, out, sizeof out) != 0);
    const char *few[] = {"a", "b", NULL};
    BT_CHECK_EQ(bt_spawn_command("exe", few, out, sizeof out), 0);
    BT_CHECK_EQ(strcmp(out, "exe a b"), 0);
}

BT_TEST(spawn_util, a_missing_exe_fails_once_without_the_retry_sleeps) {
    const char *argv[] = {NULL};
    int code = -1;
    char out[64];
    char err[64];
    ULONGLONG before = GetTickCount64();
    int rc = bt_spawn_capture("bt-no-such-spawn-target.exe", argv, NULL, 5000, &code, out,
                              sizeof out, err, sizeof err);
    ULONGLONG elapsed = GetTickCount64() - before;
    BT_CHECK_EQ(rc, -1);
    BT_CHECK(strstr(bt_spawn_diag(), "CreateProcessW failed: 2") == bt_spawn_diag());
    BT_CHECK(strstr(bt_spawn_diag(), "retried") == NULL);
    BT_CHECK(elapsed < 1000);
}
