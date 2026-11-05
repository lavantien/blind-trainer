#include "policy_test_util.h"

#include <string.h>

#include "policy/fsutil.h"
#include "policy/lib.h"
#include "policy/selftest.h"

BT_TEST(policy_fsutil, strvec_push_sort_free_roundtrip) {
    policy_strvec vec = {0};
    policy_strvec_push(&vec, policy_xstrdup("b"));
    policy_strvec_push(&vec, policy_xstrdup("c"));
    policy_strvec_push(&vec, policy_xstrdup("a"));
    policy_strvec_sort(&vec);
    BT_CHECK_EQ(vec.count, 3U);
    BT_CHECK(strcmp(vec.items[0], "a") == 0);
    BT_CHECK(strcmp(vec.items[1], "b") == 0);
    BT_CHECK(strcmp(vec.items[2], "c") == 0);
    policy_strvec_free(&vec);
    BT_CHECK(vec.items == NULL);
    BT_CHECK_EQ(vec.count, 0U);
    BT_CHECK_EQ(vec.cap, 0U);
}

BT_TEST(policy_fsutil, join_checked_accepts_short_and_rejects_oversize) {
    char out[BT_PATH_MAX];
    char err[POLICY_ERR_MAX];
    BT_CHECK_EQ(policy_join_checked(out, "C:/x", "y.c", err, sizeof err), 0);
    BT_CHECK(strcmp(out, "C:/x/y.c") == 0);
    static char huge[BT_PATH_MAX + 64];
    memset(huge, 'a', sizeof huge - 1);
    huge[sizeof huge - 1] = '\0';
    BT_CHECK_EQ(policy_join_checked(out, "C:/x", huge, err, sizeof err), -1);
    BT_CHECK(strcmp(err, "path too long under C:/x") == 0);
}

BT_TEST(policy_fsutil, list_dir_rejects_a_dir_past_the_pattern_limit) {
    static char huge[BT_PATH_MAX];
    memset(huge, 'd', sizeof huge - 1);
    huge[0] = 'C';
    huge[1] = ':';
    huge[2] = '/';
    huge[sizeof huge - 1] = '\0';
    policy_strvec names = {0};
    char err[POLICY_ERR_MAX];
    BT_CHECK_EQ(policy_list_dir(huge, &names, err, sizeof err), -1);
    BT_CHECK(strncmp(err, "path too long under", 19) == 0);
    policy_strvec_free(&names);
}

BT_TEST(policy_fsutil, path_predicates_classify_dir_file_and_missing) {
    char root[POLICY_UTIL_PATH_MAX];
    policy_util_scratch("fsutil-pred", root);
    char dir_path[POLICY_UTIL_PATH_MAX];
    policy_util_join(dir_path, root, "sub");
    policy_util_mkdir_p(dir_path);
    char file_path[POLICY_UTIL_PATH_MAX];
    policy_util_join(file_path, root, "x.c");
    policy_util_write(file_path, "int x;\n");
    char missing[POLICY_UTIL_PATH_MAX];
    policy_util_join(missing, root, "gone");
    BT_CHECK_EQ(policy_path_is_dir(dir_path), 1);
    BT_CHECK_EQ(policy_path_is_file(dir_path), 0);
    BT_CHECK_EQ(policy_path_is_file(file_path), 1);
    BT_CHECK_EQ(policy_path_is_dir(file_path), 0);
    BT_CHECK_EQ(policy_path_is_dir(missing), 0);
    BT_CHECK_EQ(policy_path_is_file(missing), 0);
    BT_CHECK_EQ(policy_path_is_reparse_or_link(dir_path), 0);
    BT_CHECK_EQ(policy_path_is_reparse_or_link(missing), 0);
    policy_util_rm_rf(root);
}

BT_TEST(policy_fsutil, rm_rf_removes_nested_trees_files_and_tolerates_missing) {
    char root[POLICY_UTIL_PATH_MAX];
    policy_util_scratch("fsutil-rmrf", root);
    char leaf_a[POLICY_UTIL_PATH_MAX];
    char leaf_b[POLICY_UTIL_PATH_MAX];
    char deep[POLICY_UTIL_PATH_MAX];
    policy_util_join(leaf_a, root, "a");
    policy_util_join(leaf_b, root, "b/c");
    policy_util_mkdir_p(leaf_b);
    policy_util_join(deep, leaf_b, "y.txt");
    policy_util_write(deep, "y\n");
    policy_util_join(leaf_a, root, "a");
    policy_util_write(leaf_a, "x\n");
    policy_rm_rf(leaf_a);
    BT_CHECK_EQ(policy_path_is_file(leaf_a), 0);
    char branch[POLICY_UTIL_PATH_MAX];
    policy_util_join(branch, root, "b");
    policy_rm_rf(branch);
    BT_CHECK_EQ(policy_path_is_dir(branch), 0);
    char absent[POLICY_UTIL_PATH_MAX];
    policy_util_join(absent, root, "never");
    policy_rm_rf(absent);
    BT_CHECK_EQ(policy_path_is_dir(root), 1);
    policy_util_rm_rf(root);
}

static int make_junction(const char *link, const char *target) {
    if (!CreateDirectoryA(link, NULL)) {
        return 0;
    }
    HANDLE handle = CreateFileA(link, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                                FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        return 0;
    }
    char full[BT_PATH_MAX];
    GetFullPathNameA(target, sizeof full, full, NULL);
    char prefixed[BT_PATH_MAX];
    snprintf(prefixed, sizeof prefixed, "\\??\\%s", full);
    for (char *cursor = prefixed; *cursor != '\0'; cursor++) {
        if (*cursor == '/') {
            *cursor = '\\';
        }
    }
    wchar_t name[BT_PATH_MAX];
    int name_len = MultiByteToWideChar(CP_UTF8, 0, prefixed, -1, name, BT_PATH_MAX);
    size_t bytes = (size_t)(name_len - 1) * sizeof(wchar_t);
    unsigned short data_len = (unsigned short)(12 + bytes);
    unsigned char buffer[8192];
    memset(buffer, 0, sizeof buffer);
    *(unsigned long *)(buffer + 0) = 0xA0000003u;
    *(unsigned short *)(buffer + 4) = data_len;
    *(unsigned short *)(buffer + 6) = 0;
    *(unsigned short *)(buffer + 8) = 0;
    *(unsigned short *)(buffer + 10) = (unsigned short)bytes;
    *(unsigned short *)(buffer + 12) = (unsigned short)(bytes + 2);
    *(unsigned short *)(buffer + 14) = 0;
    memcpy(buffer + 16, name, bytes);
    DWORD returned = 0;
    int ok = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, buffer, (DWORD)(8 + data_len), NULL,
                             0, &returned, NULL)
                 ? 1
                 : 0;
    CloseHandle(handle);
    if (!ok) {
        RemoveDirectoryA(link);
    }
    return ok;
}

BT_TEST(policy_fsutil, reparse_directories_are_detected_and_skipped_by_scans) {
    char root[POLICY_UTIL_PATH_MAX];
    policy_util_scratch("fsutil-reparse", root);
    char target[POLICY_UTIL_PATH_MAX];
    policy_util_join(target, root, "real");
    policy_util_mkdir_p(target);
    char stray[POLICY_UTIL_PATH_MAX];
    policy_util_join(stray, target, "stray.cpp");
    policy_util_write(stray, "int x;\n");
    char commented[POLICY_UTIL_PATH_MAX];
    policy_util_join(commented, target, "note.c");
    policy_util_write(commented, "// hidden\nint y;\n");
    char src_dir[POLICY_UTIL_PATH_MAX];
    policy_util_join(src_dir, root, "src");
    policy_util_mkdir_p(src_dir);
    char clean[POLICY_UTIL_PATH_MAX];
    policy_util_join(clean, src_dir, "clean.c");
    policy_util_write(clean, "int z;\n");
    char link[POLICY_UTIL_PATH_MAX];
    policy_util_join(link, src_dir, "linked");
    BT_CHECK_EQ(make_junction(link, target), 1);
    BT_CHECK_EQ(policy_path_is_reparse_or_link(link), 1);
    BT_CHECK_EQ(policy_path_is_dir(link), 1);
    policy_outcome outcome = {0};
    char err[POLICY_ERR_MAX];
    char allow[POLICY_UTIL_PATH_MAX];
    policy_util_join(allow, root, "allow.txt");
    policy_util_write(allow, "# allow\n");
    bt_json_value *config = NULL;
    bt_json_error json_err;
    int parsed = bt_json_parse(POLICY_SELFTEST_CONFIG, strlen(POLICY_SELFTEST_CONFIG), &config,
                               &json_err);
    BT_CHECK_EQ(parsed, 0);
    int rc = policy_run_checks(root, POLICY_MODE_BOTH, config, allow, &outcome, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(outcome.finding_count, 0);
    policy_outcome_free(&outcome);
    bt_json_free(config);
    RemoveDirectoryA(link);
    policy_util_rm_rf(root);
}

BT_TEST(policy_fsutil, selftest_cleans_stale_run_dirs_before_planting) {
    char workdir[POLICY_UTIL_PATH_MAX];
    policy_util_scratch("fsutil-stale", workdir);
    char stale[POLICY_UTIL_PATH_MAX];
    snprintf(stale, POLICY_UTIL_PATH_MAX, "%s/pol-leftover", workdir);
    policy_util_mkdir_p(stale);
    char junk[POLICY_UTIL_PATH_MAX];
    policy_util_join(junk, stale, "junk.txt");
    policy_util_write(junk, "junk\n");
    char found[POLICY_UTIL_PATH_MAX];
    BT_CHECK_EQ(policy_util_find_child(workdir, "pol-leftover", found), 1);
    char err[POLICY_ERR_MAX];
    BT_CHECK_EQ(policy_selftest(workdir, err, sizeof err), 1);
    BT_CHECK_EQ(policy_path_is_dir(stale), 0);
    policy_util_rm_rf(workdir);
}
