#include "fs.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wchar.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

#include "common/bt_trace.h"

static char *fs_strdup(const char *text) {
    size_t len = strlen(text) + 1;
    char *copy = malloc(len);
    if (copy != NULL) {
        memcpy(copy, text, len);
    }
    return copy;
}

#ifdef _WIN32

static int widen(const char *text, wchar_t *out, size_t out_cap) {
    int len = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (len <= 0 || (size_t)len > out_cap) {
        return -1;
    }
    MultiByteToWideChar(CP_UTF8, 0, text, -1, out, len);
    return 0;
}

static int narrow(const wchar_t *text, char *out, size_t out_cap) {
    int len = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (len <= 0 || (size_t)len > out_cap) {
        return -1;
    }
    WideCharToMultiByte(CP_UTF8, 0, text, -1, out, len, NULL, NULL);
    return 0;
}

static int widen_pattern(const char *dir, wchar_t *out, size_t out_cap) {
    char narrow_pattern[BT_PATH_MAX + 8];
    int wrote = snprintf(narrow_pattern, sizeof narrow_pattern, "%s/*", dir);
    if (wrote < 0 || (size_t)wrote >= sizeof narrow_pattern) {
        return -1;
    }
    return widen(narrow_pattern, out, out_cap);
}

int btvd_fs_is_dir(const char *path) {
    wchar_t wide[BT_PATH_MAX];
    struct _stat info;
    return widen(path, wide, BT_PATH_MAX) == 0 && _wstat(wide, &info) == 0 &&
           (info.st_mode & S_IFMT) == S_IFDIR;
}

int btvd_fs_is_file(const char *path) {
    wchar_t wide[BT_PATH_MAX];
    struct _stat info;
    return widen(path, wide, BT_PATH_MAX) == 0 && _wstat(wide, &info) == 0 &&
           (info.st_mode & S_IFMT) == S_IFREG;
}

void btvd_fs_mkdir_p(const char *path) {
    BT_TRACE_ENTER("visual_diff.fs_mkdir_p");
    char partial[BT_PATH_MAX];
    snprintf(partial, sizeof partial, "%s", path);
    size_t len = strlen(partial);
    for (size_t at = 1; at < len; at++) {
        if (partial[at] != '/' && partial[at] != '\\') {
            continue;
        }
        char keep = partial[at];
        partial[at] = '\0';
        wchar_t wide[BT_PATH_MAX];
        if (widen(partial, wide, BT_PATH_MAX) == 0) {
            _wmkdir(wide);
        }
        partial[at] = keep;
    }
    wchar_t wide[BT_PATH_MAX];
    if (widen(partial, wide, BT_PATH_MAX) == 0) {
        _wmkdir(wide);
    }
    BT_TRACE_EXIT("visual_diff.fs_mkdir_p");
}

void btvd_fs_rm_rf(const char *path) {
    if (!btvd_fs_is_dir(path)) {
        btvd_fs_remove(path, NULL, 0);
        return;
    }
    wchar_t wide[BT_PATH_MAX];
    if (widen(path, wide, BT_PATH_MAX) != 0) {
        return;
    }
    wchar_t pattern[BT_PATH_MAX];
    if (widen_pattern(path, pattern, BT_PATH_MAX) != 0) {
        return;
    }
    struct _wfinddata_t entry;
    intptr_t handle = _wfindfirst(pattern, &entry);
    if (handle != -1) {
        do {
            if (wcscmp(entry.name, L".") == 0 || wcscmp(entry.name, L"..") == 0) {
                continue;
            }
            char child_narrow[BT_PATH_MAX];
            char child[BT_PATH_MAX];
            if (narrow(entry.name, child_narrow, BT_PATH_MAX) == 0) {
                snprintf(child, sizeof child, "%s/%s", path, child_narrow);
                btvd_fs_rm_rf(child);
            }
        } while (_wfindnext(handle, &entry) == 0);
        _findclose(handle);
    }
    _wrmdir(wide);
}

int btvd_fs_list_names(const char *dir, btvd_strvec *names, char *err, size_t err_len) {
    BT_TRACE_ENTER("visual_diff.fs_list_names");
    wchar_t pattern[BT_PATH_MAX];
    if (widen_pattern(dir, pattern, BT_PATH_MAX) != 0) {
        snprintf(err, err_len, "cannot widen %s", dir);
        BT_TRACE_FAIL("visual_diff.fs_list_names", "%s", err);
        BT_TRACE_EXIT("visual_diff.fs_list_names");
        return -1;
    }
    struct _wfinddata_t entry;
    intptr_t handle = _wfindfirst(pattern, &entry);
    if (handle == -1) {
        snprintf(err, err_len, "cannot list %s", dir);
        BT_TRACE_FAIL("visual_diff.fs_list_names", "%s", err);
        BT_TRACE_EXIT("visual_diff.fs_list_names");
        return -1;
    }
    do {
        if (wcscmp(entry.name, L".") == 0 || wcscmp(entry.name, L"..") == 0) {
            continue;
        }
        char narrow_name[BT_PATH_MAX];
        if (narrow(entry.name, narrow_name, BT_PATH_MAX) != 0) {
            continue;
        }
        btvd_strvec_push(names, fs_strdup(narrow_name));
    } while (_wfindnext(handle, &entry) == 0);
    _findclose(handle);
    BT_TRACE_EXIT("visual_diff.fs_list_names");
    return 0;
}

FILE *btvd_fs_open(const char *path, const char *mode) {
    wchar_t wide_path[BT_PATH_MAX];
    wchar_t wide_mode[8];
    if (widen(path, wide_path, BT_PATH_MAX) != 0) {
        return NULL;
    }
    if (mbstowcs(wide_mode, mode, 8) == (size_t)-1) {
        return NULL;
    }
    return _wfopen(wide_path, wide_mode);
}

int btvd_fs_remove(const char *path, char *err, size_t err_len) {
    wchar_t wide[BT_PATH_MAX];
    if (widen(path, wide, BT_PATH_MAX) != 0 || _wremove(wide) != 0) {
        snprintf(err, err_len, "cannot remove %s", path);
        return -1;
    }
    return 0;
}

int btvd_fs_replace(const char *from, const char *to, char *err, size_t err_len) {
    BT_TRACE_ENTER("visual_diff.fs_replace");
    wchar_t wide_from[BT_PATH_MAX];
    wchar_t wide_to[BT_PATH_MAX];
    if (widen(from, wide_from, BT_PATH_MAX) != 0 || widen(to, wide_to, BT_PATH_MAX) != 0 ||
        MoveFileExW(wide_from, wide_to, MOVEFILE_REPLACE_EXISTING) == 0) {
        snprintf(err, err_len, "cannot replace %s", to);
        BT_TRACE_FAIL("visual_diff.fs_replace", "%s error %lu", err, GetLastError());
        BT_TRACE_EXIT("visual_diff.fs_replace");
        return -1;
    }
    BT_TRACE_EXIT("visual_diff.fs_replace");
    return 0;
}

#else

int btvd_fs_is_dir(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && (info.st_mode & S_IFMT) == S_IFDIR;
}

int btvd_fs_is_file(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && (info.st_mode & S_IFMT) == S_IFREG;
}

void btvd_fs_mkdir_p(const char *path) {
    BT_TRACE_ENTER("visual_diff.fs_mkdir_p");
    char partial[BT_PATH_MAX];
    snprintf(partial, sizeof partial, "%s", path);
    size_t len = strlen(partial);
    for (size_t at = 1; at < len; at++) {
        if (partial[at] != '/' && partial[at] != '\\') {
            continue;
        }
        char keep = partial[at];
        partial[at] = '\0';
        mkdir(partial, 0755);
        partial[at] = keep;
    }
    mkdir(partial, 0755);
    BT_TRACE_EXIT("visual_diff.fs_mkdir_p");
}

void btvd_fs_rm_rf(const char *path) {
    if (!btvd_fs_is_dir(path)) {
        btvd_fs_remove(path, NULL, 0);
        return;
    }
    DIR *dir = opendir(path);
    if (dir != NULL) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            char child[BT_PATH_MAX];
            snprintf(child, sizeof child, "%s/%s", path, entry->d_name);
            btvd_fs_rm_rf(child);
        }
        closedir(dir);
    }
    rmdir(path);
}

int btvd_fs_list_names(const char *dir, btvd_strvec *names, char *err, size_t err_len) {
    BT_TRACE_ENTER("visual_diff.fs_list_names");
    DIR *handle = opendir(dir);
    if (handle == NULL) {
        snprintf(err, err_len, "cannot list %s", dir);
        BT_TRACE_FAIL("visual_diff.fs_list_names", "%s", err);
        BT_TRACE_EXIT("visual_diff.fs_list_names");
        return -1;
    }
    struct dirent *entry;
    while ((entry = readdir(handle)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        btvd_strvec_push(names, fs_strdup(entry->d_name));
    }
    closedir(handle);
    BT_TRACE_EXIT("visual_diff.fs_list_names");
    return 0;
}

FILE *btvd_fs_open(const char *path, const char *mode) {
    return fopen(path, mode);
}

int btvd_fs_remove(const char *path, char *err, size_t err_len) {
    if (remove(path) != 0) {
        snprintf(err, err_len, "cannot remove %s", path);
        return -1;
    }
    return 0;
}

int btvd_fs_replace(const char *from, const char *to, char *err, size_t err_len) {
    BT_TRACE_ENTER("visual_diff.fs_replace");
    if (rename(from, to) != 0) {
        snprintf(err, err_len, "cannot replace %s", to);
        BT_TRACE_FAIL("visual_diff.fs_replace", "%s", err);
        BT_TRACE_EXIT("visual_diff.fs_replace");
        return -1;
    }
    BT_TRACE_EXIT("visual_diff.fs_replace");
    return 0;
}

#endif

unsigned char *btvd_fs_read_bytes(const char *path, size_t *len_out, char *err, size_t err_len) {
    BT_TRACE_ENTER("visual_diff.fs_read_bytes");
    *len_out = 0;
    FILE *file = btvd_fs_open(path, "rb");
    if (file == NULL) {
        snprintf(err, err_len, "%s: open failed", path);
        BT_TRACE_FAIL("visual_diff.fs_read_bytes", "%s", err);
        BT_TRACE_EXIT("visual_diff.fs_read_bytes");
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0) {
        fclose(file);
        snprintf(err, err_len, "%s: size failed", path);
        BT_TRACE_FAIL("visual_diff.fs_read_bytes", "%s", err);
        BT_TRACE_EXIT("visual_diff.fs_read_bytes");
        return NULL;
    }
    unsigned char *bytes = malloc((size_t)size + 1);
    if (bytes == NULL) {
        fclose(file);
        snprintf(err, err_len, "%s: out of memory", path);
        BT_TRACE_FAIL("visual_diff.fs_read_bytes", "%s", err);
        BT_TRACE_EXIT("visual_diff.fs_read_bytes");
        return NULL;
    }
    size_t got = fread(bytes, 1, (size_t)size, file);
    fclose(file);
    if (got != (size_t)size) {
        free(bytes);
        snprintf(err, err_len, "%s: short read", path);
        BT_TRACE_FAIL("visual_diff.fs_read_bytes", "%s", err);
        BT_TRACE_EXIT("visual_diff.fs_read_bytes");
        return NULL;
    }
    *len_out = (size_t)size;
    BT_TRACE_EXIT("visual_diff.fs_read_bytes");
    return bytes;
}

int btvd_fs_write_bytes(const char *path, const void *data, size_t len, char *err, size_t err_len) {
    BT_TRACE_ENTER("visual_diff.fs_write_bytes");
    FILE *file = btvd_fs_open(path, "wb");
    if (file == NULL) {
        snprintf(err, err_len, "cannot write %s: %s", path, strerror(errno));
        BT_TRACE_FAIL("visual_diff.fs_write_bytes", "%s", err);
        BT_TRACE_EXIT("visual_diff.fs_write_bytes");
        return -1;
    }
    int broken = fwrite(data, 1, len, file) != len;
    if (fclose(file) != 0) {
        broken = 1;
    }
    if (broken) {
        snprintf(err, err_len, "short write on %s", path);
        BT_TRACE_FAIL("visual_diff.fs_write_bytes", "%s", err);
        BT_TRACE_EXIT("visual_diff.fs_write_bytes");
        return -1;
    }
    BT_TRACE_EXIT("visual_diff.fs_write_bytes");
    return 0;
}

int btvd_fs_copy(const char *from, const char *to, char *err, size_t err_len) {
    BT_TRACE_ENTER("visual_diff.fs_copy");
    char tmp[BT_PATH_MAX];
    int wrote = snprintf(tmp, sizeof tmp, "%s.tmp", to);
    if (wrote < 0 || (size_t)wrote >= sizeof tmp) {
        snprintf(err, err_len, "path too long for %s", to);
        BT_TRACE_FAIL("visual_diff.fs_copy", "%s", err);
        BT_TRACE_EXIT("visual_diff.fs_copy");
        return -1;
    }
    FILE *src = btvd_fs_open(from, "rb");
    if (src == NULL) {
        snprintf(err, err_len, "cannot read %s", from);
        BT_TRACE_FAIL("visual_diff.fs_copy", "%s", err);
        BT_TRACE_EXIT("visual_diff.fs_copy");
        return -1;
    }
    FILE *dst = btvd_fs_open(tmp, "wb");
    if (dst == NULL) {
        fclose(src);
        snprintf(err, err_len, "cannot write %s", tmp);
        BT_TRACE_FAIL("visual_diff.fs_copy", "%s", err);
        BT_TRACE_EXIT("visual_diff.fs_copy");
        return -1;
    }
    char buffer[4096];
    size_t got;
    while ((got = fread(buffer, 1, sizeof buffer, src)) > 0) {
        if (fwrite(buffer, 1, got, dst) != got) {
            snprintf(err, err_len, "short write on %s", tmp);
            fclose(src);
            fclose(dst);
            btvd_fs_remove(tmp, NULL, 0);
            BT_TRACE_FAIL("visual_diff.fs_copy", "%s", err);
            BT_TRACE_EXIT("visual_diff.fs_copy");
            return -1;
        }
    }
    int broken = ferror(src);
    fclose(src);
    fclose(dst);
    if (broken) {
        snprintf(err, err_len, "read failed on %s", from);
        btvd_fs_remove(tmp, NULL, 0);
        BT_TRACE_FAIL("visual_diff.fs_copy", "%s", err);
        BT_TRACE_EXIT("visual_diff.fs_copy");
        return -1;
    }
    if (btvd_fs_replace(tmp, to, err, err_len) != 0) {
        btvd_fs_remove(tmp, NULL, 0);
        BT_TRACE_FAIL("visual_diff.fs_copy", "%s", err);
        BT_TRACE_EXIT("visual_diff.fs_copy");
        return -1;
    }
    BT_TRACE_EXIT("visual_diff.fs_copy");
    return 0;
}

#ifdef _WIN32
static int append_quoted_arg(wchar_t *line, size_t cap, size_t *at, const wchar_t *arg) {
    size_t backslashes = 0;
    if (*at + 2 >= cap) {
        return -1;
    }
    line[(*at)++] = L'"';
    for (const wchar_t *scan = arg; *scan != L'\0'; scan++) {
        if (*scan == L'\\') {
            backslashes++;
            continue;
        }
        if (*scan == L'"') {
            size_t emit = backslashes * 2 + 1;
            if (*at + emit + 2 >= cap) {
                return -1;
            }
            for (size_t rep = 0; rep < emit; rep++) {
                line[(*at)++] = L'\\';
            }
            line[(*at)++] = L'"';
            backslashes = 0;
            continue;
        }
        if (*at + backslashes + 3 >= cap) {
            return -1;
        }
        for (size_t rep = 0; rep < backslashes; rep++) {
            line[(*at)++] = L'\\';
        }
        backslashes = 0;
        line[(*at)++] = *scan;
    }
    if (*at + backslashes + 2 >= cap) {
        return -1;
    }
    for (size_t rep = 0; rep < backslashes; rep++) {
        line[(*at)++] = L'\\';
    }
    line[(*at)++] = L'"';
    line[*at] = L'\0';
    return 0;
}

int btvd_fs_spawn_wait(const char *exe, char *const *argv, size_t argc) {
    BT_TRACE_ENTER("visual_diff.fs_spawn_wait");
    wchar_t wide_exe[BT_PATH_MAX];
    wchar_t wide_full[BT_PATH_MAX];
    wchar_t wide_arg[BT_PATH_MAX];
    wchar_t line[BT_PATH_MAX * 4];
    size_t at = 0;
    int broken = widen(exe, wide_exe, BT_PATH_MAX) != 0 ||
                 _wfullpath(wide_full, wide_exe, BT_PATH_MAX) == NULL ||
                 append_quoted_arg(line, sizeof line / sizeof *line, &at, wide_full) != 0;
    for (size_t idx = 0; broken == 0 && idx < argc; idx++) {
        if (widen(argv[idx], wide_arg, BT_PATH_MAX) != 0 || at >= sizeof line / sizeof *line - 1) {
            broken = 1;
            break;
        }
        line[at++] = L' ';
        broken = append_quoted_arg(line, sizeof line / sizeof *line, &at, wide_arg) != 0;
    }
    int code = -1;
    if (broken == 0) {
        STARTUPINFOW startup;
        PROCESS_INFORMATION info;
        ZeroMemory(&startup, sizeof startup);
        startup.cb = sizeof startup;
        ZeroMemory(&info, sizeof info);
        wchar_t mutable_line[BT_PATH_MAX * 4];
        wmemcpy(mutable_line, line, at + 1);
        if (CreateProcessW(NULL, mutable_line, NULL, NULL, TRUE, 0, NULL, NULL, &startup, &info) !=
            0) {
            WaitForSingleObject(info.hProcess, INFINITE);
            DWORD raw = 0;
            GetExitCodeProcess(info.hProcess, &raw);
            code = (int)raw;
            CloseHandle(info.hProcess);
            CloseHandle(info.hThread);
        } else {
            BT_TRACE_FAIL("visual_diff.fs_spawn_wait", "CreateProcessW error %lu", GetLastError());
        }
    } else {
        BT_TRACE_FAIL("visual_diff.fs_spawn_wait", "command line widen failed");
    }
    BT_TRACE_EXIT("visual_diff.fs_spawn_wait");
    return code;
}
#else
int btvd_fs_spawn_wait(const char *exe, char *const *argv, size_t argc) {
    BT_TRACE_ENTER("visual_diff.fs_spawn_wait");
    char **full = calloc(argc + 2, sizeof *full);
    if (full == NULL) {
        BT_TRACE_EXIT("visual_diff.fs_spawn_wait");
        return -1;
    }
    full[0] = strdup(exe);
    for (size_t at = 0; at < argc; at++) {
        full[at + 1] = strdup(argv[at]);
    }
    pid_t pid = fork();
    if (pid == 0) {
        execv(exe, full);
        _exit(127);
    }
    free(full);
    if (pid < 0) {
        BT_TRACE_FAIL("visual_diff.fs_spawn_wait", "fork failed");
        BT_TRACE_EXIT("visual_diff.fs_spawn_wait");
        return -1;
    }
    int status = 0;
    waitpid(pid, &status, 0);
    BT_TRACE_EXIT("visual_diff.fs_spawn_wait");
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
#endif
