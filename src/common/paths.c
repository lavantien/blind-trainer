#include "common/paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "common/bt_trace.h"

static int path_exists(const char *path) {
    struct stat info;
    return stat(path, &info) == 0;
}

static int dir_contains_config(const char *path) {
    char probe[BT_PATH_MAX];
    snprintf(probe, sizeof probe, "%s/config/config.json", path);
    return path_exists(probe);
}

static void join_path(char out[BT_PATH_MAX], const char *base, const char *leaf) {
    snprintf(out, BT_PATH_MAX, "%s/%s", base, leaf);
}

void bt_paths_workspace_root_from(const char *start_dir, char out[BT_PATH_MAX]) {
    BT_TRACE_ENTER("paths.workspace_root_from");
    char current[BT_PATH_MAX];
    snprintf(current, sizeof current, "%s", start_dir);
    while (1) {
        if (dir_contains_config(current)) {
            snprintf(out, BT_PATH_MAX, "%s", current);
            BT_TRACE_EXIT("paths.workspace_root_from");
            return;
        }
        char *slash = strrchr(current, '/');
        char *backslash = strrchr(current, '\\');
        char *cut = slash > backslash ? slash : backslash;
        if (cut == NULL || cut == current) {
            snprintf(out, BT_PATH_MAX, "%s", start_dir);
            BT_TRACE_EXIT("paths.workspace_root_from");
            return;
        }
        *cut = '\0';
    }
}

void bt_paths_exe_dir(char out[BT_PATH_MAX]) {
    BT_TRACE_ENTER("paths.exe_dir");
    char exe[BT_PATH_MAX];
#ifdef _WIN32
    DWORD len = GetModuleFileNameA(NULL, exe, sizeof exe);
    if (len == 0 || len >= sizeof exe) {
        snprintf(out, BT_PATH_MAX, ".");
        BT_TRACE_FAIL("paths.exe_dir", "GetModuleFileNameA failed");
        BT_TRACE_EXIT("paths.exe_dir");
        return;
    }
#else
    ssize_t len = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (len <= 0) {
        snprintf(out, BT_PATH_MAX, ".");
        BT_TRACE_FAIL("paths.exe_dir", "readlink failed");
        BT_TRACE_EXIT("paths.exe_dir");
        return;
    }
    exe[len] = '\0';
#endif
    char *slash = strrchr(exe, '/');
    char *backslash = strrchr(exe, '\\');
    char *cut = slash > backslash ? slash : backslash;
    if (cut == NULL) {
        snprintf(out, BT_PATH_MAX, ".");
    } else {
        *cut = '\0';
        snprintf(out, BT_PATH_MAX, "%s", exe);
    }
    BT_TRACE_EXIT("paths.exe_dir");
}

void bt_paths_workspace_root(char out[BT_PATH_MAX]) {
    char exe_dir[BT_PATH_MAX];
    bt_paths_exe_dir(exe_dir);
    bt_paths_workspace_root_from(exe_dir, out);
}

void bt_paths_user_root_from(const char *appdata, const char *xdg, const char *home,
                             char out[BT_PATH_MAX]) {
    BT_TRACE_ENTER("paths.user_root_from");
    if (appdata != NULL && appdata[0] != '\0') {
        join_path(out, appdata, BT_APP_DIR_NAME);
    } else if (xdg != NULL && xdg[0] != '\0') {
        join_path(out, xdg, BT_APP_DIR_NAME);
    } else if (home != NULL && home[0] != '\0') {
        char base[BT_PATH_MAX];
        join_path(base, home, ".local/share");
        join_path(out, base, BT_APP_DIR_NAME);
    } else {
        const char *temp = getenv("TEMP");
        if (temp == NULL || temp[0] == '\0') {
            temp = getenv("TMP");
        }
        if (temp == NULL || temp[0] == '\0') {
            temp = "/tmp";
        }
        join_path(out, temp, BT_APP_DIR_NAME);
    }
    BT_TRACE_EXIT("paths.user_root_from");
}

void bt_paths_user_root(char out[BT_PATH_MAX]) {
    bt_paths_user_root_from(getenv("APPDATA"), getenv("XDG_DATA_HOME"), getenv("HOME"), out);
}

void bt_paths_config_path_from(const char *env_value, const char *workspace_root,
                               char out[BT_PATH_MAX]) {
    BT_TRACE_ENTER("paths.config_path_from");
    if (env_value != NULL && env_value[0] != '\0') {
        int absolute = env_value[0] == '/' || env_value[0] == '\\' || env_value[1] == ':';
        if (absolute) {
            snprintf(out, BT_PATH_MAX, "%s", env_value);
        } else {
            join_path(out, workspace_root, env_value);
        }
    } else {
        snprintf(out, BT_PATH_MAX, "%s/config/config.json", workspace_root);
    }
    BT_TRACE_EXIT("paths.config_path_from");
}

void bt_paths_config_path(char out[BT_PATH_MAX]) {
    char root[BT_PATH_MAX];
    bt_paths_workspace_root(root);
    bt_paths_config_path_from(getenv("BT_CONFIG"), root, out);
}

void bt_paths_resolve_root(char out[BT_PATH_MAX], const char *primary,
                           const char *const *candidates, size_t count) {
    BT_TRACE_ENTER("paths.resolve_root");
    if (path_exists(primary)) {
        snprintf(out, BT_PATH_MAX, "%s", primary);
        BT_TRACE_EXIT("paths.resolve_root");
        return;
    }
    for (size_t i = 0; i < count; i++) {
        if (path_exists(candidates[i])) {
            snprintf(out, BT_PATH_MAX, "%s", candidates[i]);
            BT_TRACE_EXIT("paths.resolve_root");
            return;
        }
    }
    snprintf(out, BT_PATH_MAX, "%s", primary);
    BT_TRACE_EXIT("paths.resolve_root");
}

int bt_paths_write_atomic(const char *path, const void *data, size_t len) {
    BT_TRACE_ENTER("paths.write_atomic");
    char tmp[BT_PATH_MAX];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *file = fopen(tmp, "wb");
    if (file == NULL) {
        BT_TRACE_FAIL("paths.write_atomic", "cannot open %s", tmp);
        BT_TRACE_EXIT("paths.write_atomic");
        return -1;
    }
    size_t wrote = fwrite(data, 1, len, file);
    if (wrote != len || fclose(file) != 0) {
        remove(tmp);
        BT_TRACE_FAIL("paths.write_atomic", "short write on %s", tmp);
        BT_TRACE_EXIT("paths.write_atomic");
        return -1;
    }
#ifdef _WIN32
    wchar_t wide_path[BT_PATH_MAX];
    wchar_t wide_tmp[BT_PATH_MAX];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path, BT_PATH_MAX);
    MultiByteToWideChar(CP_UTF8, 0, tmp, -1, wide_tmp, BT_PATH_MAX);
    if (MoveFileExW(wide_tmp, wide_path, MOVEFILE_REPLACE_EXISTING) == 0) {
        remove(tmp);
        BT_TRACE_FAIL("paths.write_atomic", "rename onto %s failed", path);
        BT_TRACE_EXIT("paths.write_atomic");
        return -1;
    }
#else
    if (rename(tmp, path) != 0) {
        remove(tmp);
        BT_TRACE_FAIL("paths.write_atomic", "rename onto %s failed", path);
        BT_TRACE_EXIT("paths.write_atomic");
        return -1;
    }
#endif
    BT_TRACE_EXIT("paths.write_atomic");
    return 0;
}
