#include "common/bt_harness.h"
#include "visual-diff/fs.h"
#include "visual-diff/lib.h"

#include <direct.h>
#include <stdio.h>
#include <windows.h>

enum { AT_PATH_MAX = 512 };

static void at_join(char out[AT_PATH_MAX], const char *dir, const char *leaf) {
    snprintf(out, AT_PATH_MAX, "%s/%s", dir, leaf);
}

static void at_scratch(const char *tag, char out[AT_PATH_MAX]) {
    char cwd[AT_PATH_MAX];
    _getcwd(cwd, sizeof cwd);
    snprintf(out, AT_PATH_MAX, "%s/build/test-tmp/visual-diff-atomic-%s", cwd, tag);
    btvd_fs_rm_rf(out);
    btvd_fs_mkdir_p(out);
}

BT_TEST(visual_diff_update_atomic, a_held_destination_is_never_swapped_and_leaves_no_temp) {
    char shot_dir[AT_PATH_MAX];
    char base_dir[AT_PATH_MAX];
    at_scratch("shots", shot_dir);
    at_scratch("bases", base_dir);
    char shot_path[AT_PATH_MAX];
    char base_path[AT_PATH_MAX];
    at_join(shot_path, shot_dir, "held.png");
    at_join(base_path, base_dir, "held.png");
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_fs_write_bytes(shot_path, "fresh", 5, err, sizeof err), 0);
    BT_CHECK_EQ(btvd_fs_write_bytes(base_path, "stale", 5, err, sizeof err), 0);
    wchar_t wide_base[AT_PATH_MAX];
    BT_CHECK(MultiByteToWideChar(CP_UTF8, 0, base_path, -1, wide_base, AT_PATH_MAX) > 0);
    HANDLE held = CreateFileW(wide_base, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (held == INVALID_HANDLE_VALUE) {
        bt_harness_fail(__FILE__, __LINE__, "cannot hold %s error %lu", base_path, GetLastError());
        return;
    }
    btvd_args args;
    snprintf(args.screenshots, sizeof args.screenshots, "%s", shot_dir);
    snprintf(args.baselines, sizeof args.baselines, "%s", base_dir);
    args.update = 1;
    btvd_strvec updated = {0};
    BT_CHECK_EQ(btvd_run_update(&args, &updated, err, sizeof err), -1);
    size_t len = 0;
    char read_err[BTVD_ERR_MAX];
    unsigned char *bytes = btvd_fs_read_bytes(base_path, &len, read_err, sizeof read_err);
    BT_CHECK(bytes != NULL);
    if (bytes != NULL) {
        BT_CHECK_EQ(len, 5);
        BT_CHECK(memcmp(bytes, "stale", 5) == 0);
        free(bytes);
    }
    char tmp_path[AT_PATH_MAX];
    at_join(tmp_path, base_dir, "held.png.tmp");
    BT_CHECK(!btvd_fs_is_file(tmp_path));
    btvd_strvec_free(&updated);
    CloseHandle(held);
    btvd_fs_rm_rf(shot_dir);
    btvd_fs_rm_rf(base_dir);
}
