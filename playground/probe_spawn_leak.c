#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define PROBE_PATH_MAX 1024

static char g_diag[256];

static int quote_arg(const char *arg, char *out, size_t cap) {
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
        int is_quote = arg[i] == '"';
        size_t emit = is_quote ? backslashes * 2 + 1 : backslashes;
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

static int build_command(const char *exe, const char *const argv[], char *out, size_t cap) {
    size_t used = 0;
    out[0] = '\0';
    const char *parts[32];
    size_t count = 0;
    parts[count++] = exe;
    for (size_t i = 0; argv[i] != NULL; i++) {
        parts[count++] = argv[i];
    }
    for (size_t i = 0; i < count; i++) {
        char quoted[PROBE_PATH_MAX * 2];
        if (quote_arg(parts[i], quoted, sizeof quoted) != 0) {
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

static void append_chunk(char *buffer, size_t cap, const char *chunk, DWORD len) {
    size_t used = strlen(buffer);
    for (DWORD i = 0; i < len && used + 1 < cap; i++) {
        buffer[used++] = chunk[i];
    }
    buffer[used] = '\0';
}

static int capture(const char *exe, const char *const argv[], const char *cwd, unsigned timeout_ms,
                   int *exit_code, char *out, size_t out_cap, char *err, size_t err_cap) {
    *exit_code = -1;
    out[0] = '\0';
    err[0] = '\0';
    char exe_native[PROBE_PATH_MAX];
    snprintf(exe_native, sizeof exe_native, "%s", exe);
    for (char *cursor = exe_native; *cursor != '\0'; cursor++) {
        if (*cursor == '/') {
            *cursor = '\\';
        }
    }
    char command[PROBE_PATH_MAX * 4];
    if (build_command(exe_native, argv, command, sizeof command) != 0) {
        snprintf(g_diag, sizeof g_diag, "command too long");
        return -1;
    }
    wchar_t wide_cmd[PROBE_PATH_MAX * 4];
    wchar_t wide_cwd[PROBE_PATH_MAX] = L"";
    MultiByteToWideChar(CP_UTF8, 0, command, -1, wide_cmd, PROBE_PATH_MAX * 4);
    if (cwd != NULL) {
        char cwd_native[PROBE_PATH_MAX];
        snprintf(cwd_native, sizeof cwd_native, "%s", cwd);
        for (char *cursor = cwd_native; *cursor != '\0'; cursor++) {
            if (*cursor == '/') {
                *cursor = '\\';
            }
        }
        MultiByteToWideChar(CP_UTF8, 0, cwd_native, -1, wide_cwd, PROBE_PATH_MAX);
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
        snprintf(g_diag, sizeof g_diag, "pipe failed: %lu", GetLastError());
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
        snprintf(g_diag, sizeof g_diag, "CreateProcessW failed: %lu", GetLastError());
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
                    append_chunk(slot == 0 ? out : err, slot == 0 ? out_cap : err_cap, chunk, got);
                    continue;
                }
                continue;
            }
            if (!ReadFile(read_end, chunk, available < sizeof chunk ? available : sizeof chunk,
                          &got, NULL)) {
                *done = 1;
                continue;
            }
            append_chunk(slot == 0 ? out : err, slot == 0 ? out_cap : err_cap, chunk, got);
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

static DWORD handle_count(void) {
    DWORD count = 0;
    GetProcessHandleCount(GetCurrentProcess(), &count);
    return count;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: probe_spawn_leak.exe <exe> <count> [mode]\n");
        return 2;
    }
    const char *exe = argv[1];
    int count = atoi(argv[2]);
    const char *mode = argc > 3 ? argv[3] : "nonsense";
    fprintf(stderr, "probe enter exe %s count %d mode %s handles %lu\n", exe, count, mode,
            handle_count());
    for (int i = 0; i < count; i++) {
        const char *child_argv[2] = {mode, NULL};
        char out[16384];
        char err[4096];
        int code = -1;
        unsigned long long t0 = GetTickCount64();
        int rc = capture(exe, child_argv, NULL, 120000, &code, out, sizeof out, err, sizeof err);
        unsigned long long ms = GetTickCount64() - t0;
        fprintf(
            stderr,
            "probe spawn %d rc %d code %d handles %lu ms %llu out_len %zu err_len %zu diag %s\n", i,
            rc, code, handle_count(), ms, strlen(out), strlen(err), g_diag);
        fflush(stderr);
        if (rc != 0) {
            fprintf(stderr, "probe exit early at %d\n", i);
            return 1;
        }
    }
    fprintf(stderr, "probe exit ok handles %lu\n", handle_count());
    return 0;
}
