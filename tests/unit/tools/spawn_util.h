#ifndef UNIT_TOOLS_SPAWN_UTIL_H
#define UNIT_TOOLS_SPAWN_UTIL_H

#include "common/paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static inline int bt_spawn_quote(const char *arg, char *out, size_t cap) {
    int needs = arg[0] == '\0' || strpbrk(arg, " \t\"") != NULL;
    size_t used = 0;
    if (needs) {
        if (used + 1 >= cap) {
            return -1;
        }
        out[used++] = '"';
    }
    for (size_t i = 0; arg[i] != '\0'; i++) {
        size_t backslashes = 0;
        while (arg[i] == '\\') {
            backslashes++;
            i++;
        }
        if (arg[i] == '\0') {
            for (size_t b = 0; b < backslashes * 2; b++) {
                if (used + 1 >= cap) {
                    return -1;
                }
                out[used++] = '\\';
            }
            break;
        }
        int quote = arg[i] == '"';
        size_t emit = quote ? backslashes * 2 + 1 : backslashes;
        for (size_t b = 0; b < emit; b++) {
            if (used + 1 >= cap) {
                return -1;
            }
            out[used++] = '\\';
        }
        if (used + 1 >= cap) {
            return -1;
        }
        out[used++] = arg[i];
    }
    if (needs) {
        if (used + 1 >= cap) {
            return -1;
        }
        out[used++] = '"';
    }
    out[used] = '\0';
    return 0;
}

static inline int bt_spawn_command(const char *exe, const char *const argv[], char *out,
                                   size_t cap) {
    size_t used = 0;
    out[0] = '\0';
    const char *parts[32];
    size_t count = 0;
    parts[count++] = exe;
    for (size_t i = 0; argv[i] != NULL; i++) {
        parts[count++] = argv[i];
    }
    for (size_t i = 0; i < count; i++) {
        char quoted[BT_PATH_MAX * 2];
        if (bt_spawn_quote(parts[i], quoted, sizeof quoted) != 0) {
            return -1;
        }
        size_t len = strlen(quoted);
        if (used + len + 2 >= cap) {
            return -1;
        }
        if (i > 0) {
            out[used++] = ' ';
        }
        memcpy(out + used, quoted, len);
        used += len;
        out[used] = '\0';
    }
    return 0;
}

static inline void bt_spawn_append(char *buffer, size_t cap, const char *chunk, DWORD len) {
    size_t used = strlen(buffer);
    for (DWORD i = 0; i < len && used + 1 < cap; i++) {
        buffer[used++] = chunk[i];
    }
    buffer[used] = '\0';
}

static inline void bt_spawn_backslashes(char *path) {
    for (char *cursor = path; *cursor != '\0'; cursor++) {
        if (*cursor == '/') {
            *cursor = '\\';
        }
    }
}

static char g_bt_spawn_diag[256];

static inline const char *bt_spawn_diag(void) {
    return g_bt_spawn_diag;
}

static inline int bt_spawn_capture(const char *exe, const char *const argv[], const char *cwd,
                                   unsigned timeout_ms, int *exit_code, char *out, size_t out_cap,
                                   char *err, size_t err_cap) {
    *exit_code = -1;
    out[0] = '\0';
    err[0] = '\0';
    char exe_native[BT_PATH_MAX];
    snprintf(exe_native, sizeof exe_native, "%s", exe);
    bt_spawn_backslashes(exe_native);
    char command[BT_PATH_MAX * 4];
    if (bt_spawn_command(exe_native, argv, command, sizeof command) != 0) {
        snprintf(g_bt_spawn_diag, sizeof g_bt_spawn_diag, "command too long: %s", exe_native);
        return -1;
    }
    wchar_t wide_cmd[BT_PATH_MAX * 4];
    wchar_t wide_cwd[BT_PATH_MAX] = L"";
    MultiByteToWideChar(CP_UTF8, 0, command, -1, wide_cmd, BT_PATH_MAX * 4);
    if (cwd != NULL) {
        char cwd_native[BT_PATH_MAX];
        snprintf(cwd_native, sizeof cwd_native, "%s", cwd);
        bt_spawn_backslashes(cwd_native);
        MultiByteToWideChar(CP_UTF8, 0, cwd_native, -1, wide_cwd, BT_PATH_MAX);
    }
    SECURITY_ATTRIBUTES inheritable = {
        .nLength = sizeof inheritable,
        .lpSecurityDescriptor = NULL,
        .bInheritHandle = TRUE,
    };
    HANDLE out_read = NULL;
    HANDLE out_write = NULL;
    HANDLE err_read = NULL;
    HANDLE err_write = NULL;
    if (!CreatePipe(&out_read, &out_write, &inheritable, 0) ||
        !CreatePipe(&err_read, &err_write, &inheritable, 0)) {
        snprintf(g_bt_spawn_diag, sizeof g_bt_spawn_diag, "pipe failed: %lu", GetLastError());
        return -1;
    }
    SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(err_read, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW startup = {
        .cb = sizeof startup,
        .dwFlags = STARTF_USESTDHANDLES,
        .hStdOutput = out_write,
        .hStdError = err_write,
    };
    PROCESS_INFORMATION info = {0};
    if (!CreateProcessW(NULL, wide_cmd, NULL, NULL, TRUE, 0, NULL, cwd != NULL ? wide_cwd : NULL,
                        &startup, &info)) {
        snprintf(g_bt_spawn_diag, sizeof g_bt_spawn_diag, "CreateProcessW %s failed: %lu",
                 exe_native, GetLastError());
        CloseHandle(out_read);
        CloseHandle(out_write);
        CloseHandle(err_read);
        CloseHandle(err_write);
        return -1;
    }
    CloseHandle(out_write);
    CloseHandle(err_write);
    ULONGLONG deadline = GetTickCount64() + timeout_ms;
    int out_done = 0;
    int err_done = 0;
    while (!out_done || !err_done) {
        DWORD available = 0;
        char chunk[4096];
        DWORD got = 0;
        for (int slot = 0; slot < 2; slot++) {
            HANDLE read_end = slot == 0 ? out_read : err_read;
            int *done = slot == 0 ? &out_done : &err_done;
            if (*done) {
                continue;
            }
            if (!PeekNamedPipe(read_end, NULL, 0, NULL, &available, NULL) || available == 0) {
                if (WaitForSingleObject(info.hProcess, 15) == WAIT_OBJECT_0) {
                    if (!ReadFile(read_end, chunk, sizeof chunk, &got, NULL) || got == 0) {
                        *done = 1;
                        continue;
                    }
                    bt_spawn_append(slot == 0 ? out : err, slot == 0 ? out_cap : err_cap, chunk,
                                    got);
                    continue;
                }
                continue;
            }
            if (!ReadFile(read_end, chunk, available < sizeof chunk ? available : sizeof chunk,
                          &got, NULL)) {
                *done = 1;
                continue;
            }
            bt_spawn_append(slot == 0 ? out : err, slot == 0 ? out_cap : err_cap, chunk, got);
        }
        if (GetTickCount64() > deadline) {
            TerminateProcess(info.hProcess, 124);
            WaitForSingleObject(info.hProcess, 5000);
            CloseHandle(out_read);
            CloseHandle(err_read);
            CloseHandle(info.hProcess);
            CloseHandle(info.hThread);
            return -2;
        }
    }
    WaitForSingleObject(info.hProcess, timeout_ms);
    DWORD code = 0;
    GetExitCodeProcess(info.hProcess, &code);
    *exit_code = (int)code;
    CloseHandle(out_read);
    CloseHandle(err_read);
    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);
    return 0;
}

static inline void bt_test_repo_root(char out[BT_PATH_MAX]) {
    const char *config = getenv("BT_CONFIG");
    if (config != NULL && config[0] != '\0') {
        snprintf(out, BT_PATH_MAX, "%s", config);
        char *slash = strrchr(out, '/');
        char *backslash = strrchr(out, '\\');
        char *cut = slash > backslash ? slash : backslash;
        if (cut != NULL) {
            *cut = '\0';
        }
        slash = strrchr(out, '/');
        backslash = strrchr(out, '\\');
        cut = slash > backslash ? slash : backslash;
        if (cut != NULL) {
            *cut = '\0';
        }
        return;
    }
    bt_paths_workspace_root(out);
}

static inline void bt_test_bin_path(char out[BT_PATH_MAX], const char *exe_name) {
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    snprintf(out, BT_PATH_MAX, "%s/build/bin/%s", root, exe_name);
}

static inline void bt_test_mkdirs(const char *path) {
    char copy[BT_PATH_MAX];
    snprintf(copy, sizeof copy, "%s", path);
    for (char *cursor = copy + 1; *cursor != '\0'; cursor++) {
        if (*cursor == '/' || *cursor == '\\') {
            *cursor = '\0';
            CreateDirectoryA(copy, NULL);
            *cursor = '/';
        }
    }
    CreateDirectoryA(copy, NULL);
}

#endif
