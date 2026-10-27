#include "common/paths.h"

#include <stdarg.h>
#include <stdatomic.h>
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

static unsigned long current_pid(void) {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return (unsigned long)getpid();
#endif
}

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

static int path_is_sep(char unit) {
    return unit == '/' || unit == '\\';
}

static int path_is_drive_letter(char unit) {
    return (unit >= 'a' && unit <= 'z') || (unit >= 'A' && unit <= 'Z');
}

static int path_is_unc_lead(const char *path) {
    return path[0] != '\0' && path_is_sep(path[0]) && path[1] != '\0' && path_is_sep(path[1]);
}

static int unc_is_absolute(const char *path) {
    size_t at = 2;
    while (path[at] != '\0' && !path_is_sep(path[at])) {
        at++;
    }
    if (path[at] == '\0' || path[at + 1] == '\0' || path_is_sep(path[at + 1])) {
        return 0;
    }
    at++;
    while (path[at] != '\0' && !path_is_sep(path[at])) {
        at++;
    }
    return path_is_sep(path[at]);
}

int bt_paths_is_absolute(const char *path) {
    BT_TRACE_ENTER("paths.is_absolute");
    int absolute = 0;
    if (path != NULL && path[0] != '\0') {
        if (path_is_unc_lead(path)) {
            absolute = unc_is_absolute(path);
        } else if (path_is_drive_letter(path[0]) && path[1] == ':') {
            absolute = path_is_sep(path[2]);
        }
    }
    BT_TRACE_EXIT("paths.is_absolute");
    return absolute;
}

static size_t base_prefix_len(const char *base) {
    if (base[0] == '\0') {
        return 0;
    }
    if (path_is_drive_letter(base[0]) && base[1] == ':') {
        return 2;
    }
    if (!path_is_unc_lead(base)) {
        return 0;
    }
    size_t at = 2;
    while (base[at] != '\0' && !path_is_sep(base[at])) {
        at++;
    }
    if (base[at] == '\0' || base[at + 1] == '\0' || path_is_sep(base[at + 1])) {
        return 0;
    }
    at++;
    while (base[at] != '\0' && !path_is_sep(base[at])) {
        at++;
    }
    return at;
}

static void join_out(char out[BT_PATH_MAX], const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int need = vsnprintf(out, BT_PATH_MAX, fmt, args);
    va_end(args);
    if (need < 0 || (size_t)need >= BT_PATH_MAX) {
        BT_TRACE_FAIL("paths.join", "joined path exceeds %d bytes", BT_PATH_MAX);
    }
}

void bt_paths_join(const char *base, const char *push, char out[BT_PATH_MAX]) {
    BT_TRACE_ENTER("paths.join");
    if (push == NULL || push[0] == '\0') {
        join_out(out, "%s", base != NULL ? base : "");
        BT_TRACE_EXIT("paths.join");
        return;
    }
    int push_prefix = path_is_unc_lead(push) || (path_is_drive_letter(push[0]) && push[1] == ':');
    if (push_prefix) {
        join_out(out, "%s", push);
        BT_TRACE_EXIT("paths.join");
        return;
    }
    if (path_is_sep(push[0])) {
        size_t prefix = base != NULL ? base_prefix_len(base) : 0;
        if (prefix > 0) {
            join_out(out, "%.*s%s", (int)prefix, base, push);
        } else {
            join_out(out, "%s", push);
        }
        BT_TRACE_EXIT("paths.join");
        return;
    }
    if (base != NULL && base[0] != '\0') {
        join_out(out, "%s/%s", base, push);
    } else {
        join_out(out, "%s", push);
    }
    BT_TRACE_EXIT("paths.join");
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
    static atomic_size_t temp_counter = 0;
    size_t sequence = atomic_fetch_add(&temp_counter, 1);
    char tmp[BT_PATH_MAX];
    snprintf(tmp, sizeof tmp, "%s.tmp-%lu-%zu", path, current_pid(), sequence);
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
