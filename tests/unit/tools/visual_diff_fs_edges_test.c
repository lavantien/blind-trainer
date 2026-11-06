#include "common/bt_harness.h"
#include "visual-diff/fs.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <wchar.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

enum {
    FS_EDGE_WIDEN_FAIL_LEN = 1200,
    FS_EDGE_PATTERN_FAIL_LEN = 1023,
    FS_EDGE_COPY_FAIL_LEN = 1030,
    FS_EDGE_ARG_LEN = 1022,
};

static void fs_edge_fill(char *out, size_t len) {
    memset(out, 'x', len);
    out[len] = '\0';
}

static void fs_edge_scratch(const char *tag, char out[BT_PATH_MAX]) {
    snprintf(out, BT_PATH_MAX, "%s", "build/test-tmp/visual-fs-edges");
    btvd_fs_mkdir_p(out);
    char dir[BT_PATH_MAX];
    snprintf(dir, sizeof dir, "%s/%s", out, tag);
    btvd_fs_rm_rf(dir);
    btvd_fs_mkdir_p(dir);
    snprintf(out, BT_PATH_MAX, "%s", dir);
}

BT_TEST(visual_diff_fs_edges, stat_helpers_reject_unwidenable_paths) {
    char huge[FS_EDGE_WIDEN_FAIL_LEN + 1];
    fs_edge_fill(huge, FS_EDGE_WIDEN_FAIL_LEN);
    BT_CHECK(!btvd_fs_is_dir(huge));
    BT_CHECK(!btvd_fs_is_file(huge));
    BT_CHECK(btvd_fs_open(huge, "rb") == NULL);
    BT_CHECK(!btvd_fs_is_dir("\xff"));
    BT_CHECK(!btvd_fs_is_file("\xff"));
    char err[BTVD_ERR_MAX];
    BT_CHECK(btvd_fs_remove(huge, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "cannot remove") != NULL);
    BT_CHECK(btvd_fs_replace(huge, huge, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "cannot replace") != NULL);
}

BT_TEST(visual_diff_fs_edges, open_reads_a_written_file_back) {
    char root[BT_PATH_MAX];
    fs_edge_scratch("mode", root);
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/target", root);
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_fs_write_bytes(path, "body", 4, err, sizeof err), 0);
    FILE *file = btvd_fs_open(path, "rb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        fclose(file);
    }
    btvd_fs_rm_rf(root);
}

BT_TEST(visual_diff_fs_edges, list_names_rejects_unlistable_dirs) {
    char boundary[FS_EDGE_PATTERN_FAIL_LEN + 1];
    fs_edge_fill(boundary, FS_EDGE_PATTERN_FAIL_LEN);
    btvd_strvec names = {0};
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_fs_list_names(boundary, &names, err, sizeof err), -1);
    BT_CHECK(strstr(err, "cannot widen") != NULL);
    char missing[BT_PATH_MAX];
    snprintf(missing, sizeof missing, "%s", "build/test-tmp/visual-fs-absent");
    btvd_fs_rm_rf(missing);
    BT_CHECK_EQ(btvd_fs_list_names(missing, &names, err, sizeof err), -1);
    BT_CHECK(strstr(err, "cannot list") != NULL);
}

BT_TEST(visual_diff_fs_edges, rm_rf_stays_silent_on_unwidenable_paths) {
    char root[BT_PATH_MAX];
    fs_edge_scratch("rmrf", root);
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/inside", root);
    btvd_fs_mkdir_p(path);
    BT_CHECK(btvd_fs_is_dir(path));
    char huge[FS_EDGE_WIDEN_FAIL_LEN + 1];
    fs_edge_fill(huge, FS_EDGE_WIDEN_FAIL_LEN);
    btvd_fs_rm_rf(huge);
    char boundary[FS_EDGE_PATTERN_FAIL_LEN + 1];
    fs_edge_fill(boundary, FS_EDGE_PATTERN_FAIL_LEN);
    btvd_fs_rm_rf(boundary);
    BT_CHECK(btvd_fs_is_dir(path));
    btvd_fs_rm_rf(root);
    BT_CHECK(!btvd_fs_is_dir(root));
}

BT_TEST(visual_diff_fs_edges, copy_names_overlong_blocked_and_directory_targets) {
    char root[BT_PATH_MAX];
    fs_edge_scratch("copy", root);
    char src[BT_PATH_MAX];
    snprintf(src, sizeof src, "%s/src", root);
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_fs_write_bytes(src, "body", 4, err, sizeof err), 0);
    char huge[FS_EDGE_COPY_FAIL_LEN + 1];
    fs_edge_fill(huge, FS_EDGE_COPY_FAIL_LEN);
    BT_CHECK(btvd_fs_copy(src, huge, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "path too long") != NULL);
    char blocked[BT_PATH_MAX];
    snprintf(blocked, sizeof blocked, "%s/target", root);
    char tmp_dir[BT_PATH_MAX];
    snprintf(tmp_dir, sizeof tmp_dir, "%s.tmp", blocked);
    btvd_fs_mkdir_p(tmp_dir);
    BT_CHECK(btvd_fs_copy(src, blocked, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "cannot write") != NULL);
    BT_CHECK(btvd_fs_is_dir(tmp_dir));
    btvd_fs_rm_rf(tmp_dir);
    btvd_fs_mkdir_p(blocked);
    BT_CHECK(btvd_fs_copy(src, blocked, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "cannot replace") != NULL);
    char leftover[BT_PATH_MAX];
    snprintf(leftover, sizeof leftover, "%s.tmp", blocked);
    BT_CHECK(!btvd_fs_is_file(leftover));
    btvd_fs_rm_rf(root);
}

BT_TEST(visual_diff_fs_edges, spawn_wait_reports_missing_exes_and_bad_args) {
    BT_CHECK_EQ(btvd_fs_spawn_wait("Z:/no/such/exe.exe", NULL, 0), -1);
    char wide_fail[FS_EDGE_WIDEN_FAIL_LEN + 1];
    fs_edge_fill(wide_fail, FS_EDGE_WIDEN_FAIL_LEN);
    char *bad_argv[1] = {wide_fail};
    BT_CHECK_EQ(btvd_fs_spawn_wait("build/bin/visual-diff.exe", bad_argv, 1), -1);
    char arg[FS_EDGE_ARG_LEN + 1];
    fs_edge_fill(arg, FS_EDGE_ARG_LEN);
    char *long_argv[4] = {arg, arg, arg, arg};
    BT_CHECK_EQ(btvd_fs_spawn_wait("build/bin/visual-diff.exe", long_argv, 4), -1);
}

#ifdef _WIN32

enum {
    FS_EDGE_QUOTE_ARG_CAP = 1023,
    FS_EDGE_QUOTE_SLOTS = 8,
    FS_EDGE_QUOTE_FILLER = 1022,
};

static size_t fs_edge_line_cap(void) {
    return (size_t)BT_PATH_MAX * 4U;
}

static size_t fs_edge_exe_quoted_len(const char *exe) {
    wchar_t wide[BT_PATH_MAX];
    wchar_t full[BT_PATH_MAX];
    if (MultiByteToWideChar(CP_UTF8, 0, exe, -1, wide, BT_PATH_MAX) == 0) {
        return 0;
    }
    if (_wfullpath(full, wide, BT_PATH_MAX) == NULL) {
        return 0;
    }
    return wcslen(full) + 2U;
}

static size_t fs_edge_push_fillers(char args[FS_EDGE_QUOTE_SLOTS][FS_EDGE_QUOTE_ARG_CAP],
                                   size_t *argc, size_t budget) {
    while (budget >= FS_EDGE_QUOTE_FILLER + 3U) {
        fs_edge_fill(args[(*argc)++], FS_EDGE_QUOTE_FILLER);
        budget -= FS_EDGE_QUOTE_FILLER + 3U;
    }
    if (budget >= 4U) {
        fs_edge_fill(args[(*argc)++], budget - 3U);
        return 0;
    }
    return budget;
}

static void fs_edge_quote_arg(char out[FS_EDGE_QUOTE_ARG_CAP], size_t filler, const char *tail) {
    memset(out, 'x', filler);
    memcpy(out + filler, tail, strlen(tail));
    out[filler + strlen(tail)] = '\0';
}

static int fs_edge_spawn_cap(const char *exe, char args[FS_EDGE_QUOTE_SLOTS][FS_EDGE_QUOTE_ARG_CAP],
                             size_t argc) {
    char *argv[FS_EDGE_QUOTE_SLOTS];
    for (size_t at = 0; at < argc; at++) {
        argv[at] = args[at];
    }
    return btvd_fs_spawn_wait(exe, argv, argc);
}

BT_TEST(visual_diff_fs_edges, spawn_wait_hits_each_quote_cap_boundary) {
    const char *exe = "Z:/no/such/exe.exe";
    size_t quoted = fs_edge_exe_quoted_len(exe);
    BT_CHECK(quoted > 0);
    size_t cap = fs_edge_line_cap();
    char args[FS_EDGE_QUOTE_SLOTS][FS_EDGE_QUOTE_ARG_CAP];

    size_t argc = 0;
    size_t at = quoted;
    size_t budget = cap - 2U - at;
    at += budget - fs_edge_push_fillers(args, &argc, budget);
    fs_edge_quote_arg(args[argc++], 0U, "y");
    BT_CHECK_EQ(at, cap - 2U);
    BT_CHECK_EQ(fs_edge_spawn_cap(exe, args, argc), -1);

    argc = 0;
    at = quoted;
    budget = cap - 1015U - at;
    at += budget - fs_edge_push_fillers(args, &argc, budget);
    fs_edge_quote_arg(args[argc++], cap - 9U - at, "x\\\"");
    fs_edge_quote_arg(args[argc++], 0U, "y");
    BT_CHECK_EQ(fs_edge_spawn_cap(exe, args, argc), -1);

    argc = 0;
    at = quoted;
    budget = cap - 1015U - at;
    at += budget - fs_edge_push_fillers(args, &argc, budget);
    fs_edge_quote_arg(args[argc++], cap - 7U - at, "x\\\\");
    BT_CHECK_EQ(fs_edge_spawn_cap(exe, args, argc), -1);

    argc = 0;
    at = quoted;
    budget = cap - 1015U - at;
    at += budget - fs_edge_push_fillers(args, &argc, budget);
    fs_edge_quote_arg(args[argc++], cap - 9U - at, "x\\\\");
    fs_edge_quote_arg(args[argc++], 0U, "y");
    BT_CHECK_EQ(fs_edge_spawn_cap(exe, args, argc), -1);
}

#endif

#ifdef _WIN32

typedef struct {
    char name[BT_PATH_MAX];
    HANDLE pipe;
} fs_edge_pipe;

static DWORD WINAPI fs_edge_pipe_thread(void *user) {
    fs_edge_pipe *fixture = user;
    if (!ConnectNamedPipe(fixture->pipe, NULL) && GetLastError() != ERROR_PIPE_CONNECTED) {
        CloseHandle(fixture->pipe);
        return 1;
    }
    CloseHandle(fixture->pipe);
    return 0;
}

static HANDLE fs_edge_pipe_open(fs_edge_pipe *fixture, const char *tag) {
    snprintf(fixture->name, sizeof fixture->name, "\\\\.\\pipe\\bt-fs-edge-%s-%lu", tag,
             GetCurrentProcessId());
    fixture->pipe =
        CreateNamedPipeA(fixture->name, PIPE_ACCESS_INBOUND,
                         PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 4096, 16, 0, NULL);
    BT_CHECK(fixture->pipe != INVALID_HANDLE_VALUE);
    if (fixture->pipe == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    return CreateThread(NULL, 0, fs_edge_pipe_thread, fixture, 0, NULL);
}

static void fs_edge_pipe_close(HANDLE thread) {
    if (thread == NULL) {
        return;
    }
    WaitForSingleObject(thread, 10000);
    CloseHandle(thread);
}

BT_TEST(visual_diff_fs_edges, write_bytes_names_a_short_write_on_a_broken_pipe) {
    fs_edge_pipe fixture;
    HANDLE thread = fs_edge_pipe_open(&fixture, "write");
    char big[8192];
    memset(big, 'w', sizeof big);
    char err[BTVD_ERR_MAX];
    int rc = btvd_fs_write_bytes(fixture.name, big, sizeof big, err, sizeof err);
    BT_CHECK(rc != 0);
    BT_CHECK(strstr(err, "short write") != NULL);
    fs_edge_pipe_close(thread);
}

#endif
