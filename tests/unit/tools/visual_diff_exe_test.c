#include "common/bt_harness.h"
#include "common/paths.h"
#include "unit/tools/spawn_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static void scratch_dir(const char *tag, char out[BT_PATH_MAX]) {
    char base[BT_PATH_MAX];
    DWORD n = GetTempPathA(BT_PATH_MAX, base);
    BT_CHECK(n > 0 && n < BT_PATH_MAX);
    snprintf(out, BT_PATH_MAX, "%sbt-visual-diff-exe-%s-%lu", base, tag, GetCurrentProcessId());
    char copy[BT_PATH_MAX];
    snprintf(copy, sizeof copy, "%s", out);
    for (char *cursor = copy + 1; *cursor != '\0'; cursor++) {
        if (*cursor == '/' || *cursor == '\\') {
            *cursor = '\0';
            CreateDirectoryA(copy, NULL);
            *cursor = '\\';
        }
    }
    CreateDirectoryA(copy, NULL);
}

static void rm_scratch(const char *path) {
    char pattern[BT_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", path);
    WIN32_FIND_DATAA found;
    HANDLE scan = FindFirstFileA(pattern, &found);
    if (scan == INVALID_HANDLE_VALUE) {
        RemoveDirectoryA(path);
        return;
    }
    do {
        if (strcmp(found.cFileName, ".") == 0 || strcmp(found.cFileName, "..") == 0) {
            continue;
        }
        char child[BT_PATH_MAX];
        snprintf(child, sizeof child, "%s/%s", path, found.cFileName);
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            rm_scratch(child);
        } else {
            DeleteFileA(child);
        }
    } while (FindNextFileA(scan, &found));
    FindClose(scan);
    RemoveDirectoryA(path);
}

static void copy_file(const char *from, const char *to) {
    CopyFileA(from, to, FALSE);
}

static void baseline_png(char out[BT_PATH_MAX], const char *name) {
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    snprintf(out, BT_PATH_MAX, "%s/tests/visual/baselines/%s", root, name);
}

static void repo_baseline_pair(char shots[BT_PATH_MAX], char bases[BT_PATH_MAX],
                               const char *shot_name, const char *base_name) {
    scratch_dir("pair", shots);
    scratch_dir("pair-bases", bases);
    char source[BT_PATH_MAX];
    char target[BT_PATH_MAX];
    baseline_png(source, shot_name);
    snprintf(target, sizeof target, "%s/%s", shots, shot_name);
    copy_file(source, target);
    baseline_png(source, base_name);
    snprintf(target, sizeof target, "%s/%s", bases, base_name);
    copy_file(source, target);
}

static int spawn_diff(const char *const argv[], const char *artifacts_value,
                      const char *config_value, char *out, size_t out_cap, char *err,
                      size_t err_cap) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "visual-diff.exe");
    char saved_artifacts[BT_PATH_MAX];
    char saved_config[BT_PATH_MAX];
    DWORD had_artifacts = GetEnvironmentVariableA("BT_ARTIFACTS", saved_artifacts, BT_PATH_MAX);
    DWORD had_config = GetEnvironmentVariableA("BT_CONFIG", saved_config, BT_PATH_MAX);
    SetEnvironmentVariableA("BT_ARTIFACTS", artifacts_value);
    SetEnvironmentVariableA("BT_CONFIG", config_value);
    int code = -1;
    int rc = bt_spawn_capture(exe, argv, NULL, 120000, &code, out, out_cap, err, err_cap);
    SetEnvironmentVariableA(
        "BT_ARTIFACTS", had_artifacts > 0 && had_artifacts < BT_PATH_MAX ? saved_artifacts : NULL);
    SetEnvironmentVariableA("BT_CONFIG",
                            had_config > 0 && had_config < BT_PATH_MAX ? saved_config : NULL);
    BT_CHECK_EQ(rc, 0);
    return code;
}

BT_TEST(visual_diff_exe, no_arguments_print_usage_and_exit_2) {
    char out[2048];
    char err[2048];
    const char *argv[] = {NULL};
    int code = spawn_diff(argv, "artifacts", NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 2);
    BT_CHECK(strstr(err, "usage: bt-visual-diff") != NULL);
}

static int spawn_diff_alloc(const char *const argv[], const char *alloc_k, char *out,
                            size_t out_cap, char *err, size_t err_cap) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "visual-diff.exe");
    char saved[16];
    DWORD had = GetEnvironmentVariableA("BTVD_ALLOC_FAIL_OK", saved, sizeof saved);
    SetEnvironmentVariableA("BTVD_ALLOC_FAIL_OK", alloc_k);
    int code = -1;
    int rc = bt_spawn_capture(exe, argv, NULL, 120000, &code, out, out_cap, err, err_cap);
    SetEnvironmentVariableA("BTVD_ALLOC_FAIL_OK", had > 0 && had < sizeof saved ? saved : NULL);
    BT_CHECK_EQ(rc, 0);
    return code;
}

BT_TEST(visual_diff_exe, argv_conversion_starves_at_each_alloc_and_exits_two) {
    char shots[BT_PATH_MAX];
    char bases[BT_PATH_MAX];
    repo_baseline_pair(shots, bases, "menu.png", "menu.png");
    char out[2048];
    char err[2048];
    const char *argv[] = {shots, bases, NULL};
    const char *ks[] = {"0", "1", "2"};
    for (size_t at = 0; at < 3; at++) {
        int code = spawn_diff_alloc(argv, ks[at], out, sizeof out, err, sizeof err);
        BT_CHECK_EQ(code, 2);
        BT_CHECK(strstr(err, "out of memory") != NULL);
    }
    rm_scratch(shots);
    rm_scratch(bases);
}

BT_TEST(visual_diff_exe, selftest_passes_over_its_own_fixtures) {
    char workdir[BT_PATH_MAX];
    scratch_dir("selftest", workdir);
    char out[4096];
    char err[2048];
    const char *argv[] = {"--selftest", workdir, NULL};
    int code = spawn_diff(argv, "artifacts", NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 0);
    BT_CHECK(strstr(out, "selftest: 4/4 checks passed") != NULL);
    rm_scratch(workdir);
}

BT_TEST(visual_diff_exe, selftest_aborts_when_a_png_cannot_be_written) {
    char workdir[BT_PATH_MAX];
    scratch_dir("blocked", workdir);
    char shots[BT_PATH_MAX];
    snprintf(shots, sizeof shots, "%s/screenshots", workdir);
    CreateDirectoryA(shots, NULL);
    char blocked[BT_PATH_MAX];
    snprintf(blocked, sizeof blocked, "%s/identical.png", shots);
    CreateDirectoryA(blocked, NULL);
    BT_CHECK(SetFileAttributesA(blocked, FILE_ATTRIBUTE_READONLY) != 0);
    char out[2048];
    char err[2048];
    const char *argv[] = {"--selftest", workdir, NULL};
    int code = spawn_diff(argv, "artifacts", NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 2);
    BT_CHECK(strstr(err, "cannot write selftest png") != NULL);
    SetFileAttributesA(blocked, FILE_ATTRIBUTE_NORMAL);
    rm_scratch(blocked);
    rm_scratch(shots);
    rm_scratch(workdir);
}

BT_TEST(visual_diff_exe, identical_pair_exits_zero_and_writes_the_report) {
    char shots[BT_PATH_MAX];
    char bases[BT_PATH_MAX];
    repo_baseline_pair(shots, bases, "menu.png", "menu.png");
    char artifacts[BT_PATH_MAX];
    scratch_dir("artifacts", artifacts);
    char out[2048];
    char err[2048];
    const char *argv[] = {shots, bases, NULL};
    int code = spawn_diff(argv, artifacts, NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 0);
    char report[BT_PATH_MAX];
    snprintf(report, sizeof report, "%s/visual/report.md", artifacts);
    WIN32_FIND_DATAA found;
    HANDLE scan = FindFirstFileA(report, &found);
    BT_CHECK(scan != INVALID_HANDLE_VALUE);
    if (scan != INVALID_HANDLE_VALUE) {
        FindClose(scan);
    }
    rm_scratch(shots);
    rm_scratch(bases);
    rm_scratch(artifacts);
}

BT_TEST(visual_diff_exe, drifted_pair_exits_one) {
    char shots[BT_PATH_MAX];
    char bases[BT_PATH_MAX];
    repo_baseline_pair(shots, bases, "menu.png", "drill.png");
    char out[2048];
    char err[2048];
    const char *argv[] = {shots, bases, NULL};
    int code = spawn_diff(argv, "artifacts", NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 1);
    rm_scratch(shots);
    rm_scratch(bases);
}

BT_TEST(visual_diff_exe, missing_baseline_exits_one) {
    char shots[BT_PATH_MAX];
    char bases[BT_PATH_MAX];
    repo_baseline_pair(shots, bases, "menu.png", "menu.png");
    char extra_source[BT_PATH_MAX];
    char extra[BT_PATH_MAX];
    baseline_png(extra_source, "algs.png");
    snprintf(extra, sizeof extra, "%s/algs.png", shots);
    copy_file(extra_source, extra);
    char out[2048];
    char err[2048];
    const char *argv[] = {shots, bases, NULL};
    int code = spawn_diff(argv, "artifacts", NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 1);
    rm_scratch(shots);
    rm_scratch(bases);
}

BT_TEST(visual_diff_exe, unreadable_config_exits_two) {
    char shots[BT_PATH_MAX];
    char bases[BT_PATH_MAX];
    repo_baseline_pair(shots, bases, "menu.png", "menu.png");
    char out[2048];
    char err[2048];
    const char *argv[] = {shots, bases, NULL};
    int code = spawn_diff(argv, "artifacts", "Z:/no-such/visual-diff-config.json", out, sizeof out,
                          err, sizeof err);
    BT_CHECK_EQ(code, 2);
    rm_scratch(shots);
    rm_scratch(bases);
}

BT_TEST(visual_diff_exe, update_mode_copies_shots_into_the_baselines_dir) {
    char shots[BT_PATH_MAX];
    char bases[BT_PATH_MAX];
    repo_baseline_pair(shots, bases, "menu.png", "menu.png");
    char extra_source[BT_PATH_MAX];
    char extra[BT_PATH_MAX];
    baseline_png(extra_source, "algs.png");
    snprintf(extra, sizeof extra, "%s/algs.png", shots);
    copy_file(extra_source, extra);
    char out[4096];
    char err[2048];
    const char *argv[] = {shots, bases, "--update", NULL};
    int code = spawn_diff(argv, "artifacts", NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 0);
    BT_CHECK(strstr(out, "updated") != NULL);
    char copied[BT_PATH_MAX];
    snprintf(copied, sizeof copied, "%s/algs.png", bases);
    WIN32_FIND_DATAA found;
    HANDLE scan = FindFirstFileA(copied, &found);
    BT_CHECK(scan != INVALID_HANDLE_VALUE);
    if (scan != INVALID_HANDLE_VALUE) {
        FindClose(scan);
    }
    rm_scratch(shots);
    rm_scratch(bases);
}
