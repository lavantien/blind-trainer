#include "gendata/out.h"

#include <direct.h>
#include <errno.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "common/bt_trace.h"
#include "common/paths.h"

static int mkdirs_for(const char *dir, char *err, size_t err_len) {
    char copy[BT_PATH_MAX];
    snprintf(copy, sizeof copy, "%s", dir);
    for (char *cursor = copy + 1; *cursor != '\0'; cursor++) {
        if (*cursor == '/' || *cursor == '\\') {
            *cursor = '\0';
            if (_mkdir(copy) != 0 && errno != EEXIST) {
                snprintf(err, err_len, "write %s: mkdir failed %d", copy, errno);
                return -1;
            }
            *cursor = '/';
        }
    }
    if (_mkdir(copy) != 0 && errno != EEXIST) {
        snprintf(err, err_len, "write %s: mkdir failed %d", copy, errno);
        return -1;
    }
    return 0;
}

int bt_gd_write_atomic(const char *path, const void *bytes, size_t len, char *err, size_t err_len) {
    BT_TRACE_ENTER("gd.out.write_atomic");
    err[0] = '\0';
    char dir[BT_PATH_MAX];
    char name[BT_PATH_MAX];
    snprintf(name, sizeof name, "%s", path);
    char *cut = strrchr(name, '/');
    char *backslash = strrchr(name, '\\');
    if (backslash > cut) {
        cut = backslash;
    }
    if (cut != NULL) {
        snprintf(dir, sizeof dir, "%.*s", (int)(cut - name), name);
        if (mkdirs_for(dir, err, err_len) != 0) {
            BT_TRACE_FAIL("gd.out.write_atomic", err);
            return -1;
        }
        snprintf(name, sizeof name, "%s", cut + 1);
    } else {
        dir[0] = '\0';
    }
    char temp[BT_PATH_MAX];
    if (dir[0] != '\0') {
        snprintf(temp, sizeof temp, "%s/%s.tmp-%d", dir, name, _getpid());
    } else {
        snprintf(temp, sizeof temp, "%s.tmp-%d", name, _getpid());
    }
    FILE *file = fopen(temp, "wb");
    if (file == NULL) {
        snprintf(err, err_len, "write %s: open failed %d", temp, errno);
        BT_TRACE_FAIL("gd.out.write_atomic", err);
        return -1;
    }
    size_t wrote = fwrite(bytes, 1, len, file);
    int close_rc = fclose(file);
    if (wrote != len || close_rc != 0) {
        remove(temp);
        snprintf(err, err_len, "write %s: short write", temp);
        BT_TRACE_FAIL("gd.out.write_atomic", err);
        return -1;
    }
    wchar_t wide_temp[BT_PATH_MAX];
    wchar_t wide_path[BT_PATH_MAX];
    MultiByteToWideChar(CP_UTF8, 0, temp, -1, wide_temp, BT_PATH_MAX);
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path, BT_PATH_MAX);
    if (!MoveFileExW(wide_temp, wide_path, MOVEFILE_REPLACE_EXISTING)) {
        remove(temp);
        snprintf(err, err_len, "write %s: rename failed %lu", path, GetLastError());
        BT_TRACE_FAIL("gd.out.write_atomic", err);
        return -1;
    }
    BT_TRACE_EXIT("gd.out.write_atomic");
    return 0;
}
