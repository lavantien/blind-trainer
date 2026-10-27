#include "unit/tools/spawn_util.h"

#include <string.h>

#include "common/bt_harness.h"

BT_TEST(mutate_driver, selftest_passes) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "mutate.exe");
    const char *argv[] = {"selftest", NULL};
    char out[32768];
    char err[8192];
    int code = -1;
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    int rc = bt_spawn_capture(exe, argv, root, 110000, &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 0);
    BT_CHECK(strstr(out, "suite mutate: 29 passed, 0 failed") != NULL);
}

BT_TEST(mutate_driver, usage_without_args_exits_2) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "mutate.exe");
    const char *argv[] = {NULL};
    char out[4096];
    char err[4096];
    int code = -1;
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    int rc = bt_spawn_capture(exe, argv, root, 30000, &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 2);
    BT_CHECK(strstr(err, "usage") != NULL);
}

BT_TEST(mutate_driver, list_missing_file_exits_2) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "mutate.exe");
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    const char *argv[] = {"list", "src/does-not-exist.c", NULL};
    char out[4096];
    char err[4096];
    int code = -1;
    int rc = bt_spawn_capture(exe, argv, root, 30000, &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 2);
    BT_CHECK(strstr(err, "cannot read") != NULL);
}

BT_TEST(mutate_driver, list_enumerates_a_fixture) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "mutate.exe");
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    const char *argv[] = {"list", "tests/unit/tools/fixtures/fx_target.c", NULL};
    char out[16384];
    char err[4096];
    int code = -1;
    int rc = bt_spawn_capture(exe, argv, root, 30000, &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 0);
    BT_CHECK(strstr(out, "NumericTweak") != NULL);
    BT_CHECK(strstr(out, "ComparisonSwap") != NULL);
    BT_CHECK(strstr(out, "ConditionNegation") != NULL);
    BT_CHECK(strstr(out, "UnaryDrop") != NULL);
    BT_CHECK(strstr(out, "StatementDelete") != NULL);
}
