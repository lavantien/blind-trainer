#include "mutate/mutate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifdef _WIN32

static wchar_t *to_wide(const char *text) {
    size_t len = strlen(text);
    wchar_t *wide = malloc((len + 1) * sizeof(wchar_t));
    if (wide == NULL) {
        return NULL;
    }
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, (int)(len + 1));
    return wide;
}

static int quote_arg(const char *arg, char *out, size_t cap) {
    int needs = arg[0] == '\0' || strpbrk(arg, " \t\"") != NULL;
    size_t used = 0;
    if (needs && used + 1 < cap) {
        out[used++] = '"';
    }
    for (size_t i = 0; arg[i] != '\0'; i++) {
        size_t backslashes = 0;
        while (arg[i] == '\\') {
            backslashes++;
            i++;
        }
        if (arg[i] == '\0') {
            for (size_t b = 0; b < backslashes * 2 && used + 1 < cap; b++) {
                out[used++] = '\\';
            }
            break;
        }
        int is_quote = arg[i] == '"';
        size_t emit = is_quote ? backslashes * 2 + 1 : backslashes;
        for (size_t b = 0; b < emit && used + 1 < cap; b++) {
            out[used++] = '\\';
        }
        if (used + 1 >= cap) {
            return -1;
        }
        out[used++] = arg[i];
    }
    if (needs && used + 1 < cap) {
        out[used++] = '"';
    }
    if (used >= cap) {
        return -1;
    }
    out[used] = '\0';
    return 0;
}

static int build_command(char *const argv[], char *out, size_t cap) {
    size_t used = 0;
    out[0] = '\0';
    for (size_t i = 0; argv[i] != NULL; i++) {
        char quoted[BT_MUT_PATH_MAX * 2];
        if (quote_arg(argv[i], quoted, sizeof quoted) != 0) {
            return -1;
        }
        size_t len = strlen(quoted);
        if (used + len + 2 >= cap) {
            return -1;
        }
        if (used > 0) {
            out[used++] = ' ';
        }
        memcpy(out + used, quoted, len);
        used += len;
        out[used] = '\0';
    }
    return 0;
}

#endif

int bt_mut_spawn(const char *exe, char *const argv[], const char *cwd, unsigned timeout_ms,
                 bt_mut_proc *out) {
    BT_TRACE_ENTER("mutate.spawn");
    out->rc = -1;
    out->exit_code = -1;
    out->timed_out = 0;
    out->output = NULL;
    out->output_len = 0;
#ifdef _WIN32
    char exe_native[BT_MUT_PATH_MAX];
    snprintf(exe_native, sizeof exe_native, "%s", exe);
    for (char *cursor = exe_native; *cursor != '\0'; cursor++) {
        if (*cursor == '/') {
            *cursor = '\\';
        }
    }
    char command[BT_MUT_PATH_MAX * 8];
    if (build_command(argv, command, sizeof command) != 0) {
        BT_TRACE_FAIL("mutate.spawn", "command line too long");
        BT_TRACE_EXIT("mutate.spawn");
        return -1;
    }
    wchar_t *wide_cmd = to_wide(command);
    wchar_t *wide_cwd = NULL;
    if (cwd != NULL) {
        char cwd_native[BT_MUT_PATH_MAX];
        snprintf(cwd_native, sizeof cwd_native, "%s", cwd);
        for (char *cursor = cwd_native; *cursor != '\0'; cursor++) {
            if (*cursor == '/') {
                *cursor = '\\';
            }
        }
        wide_cwd = to_wide(cwd_native);
    }
    if (wide_cmd == NULL || (cwd != NULL && wide_cwd == NULL)) {
        free(wide_cmd);
        free(wide_cwd);
        BT_TRACE_FAIL("mutate.spawn", "oom");
        BT_TRACE_EXIT("mutate.spawn");
        return -1;
    }
    SECURITY_ATTRIBUTES inheritable = {
        .nLength = sizeof inheritable,
        .lpSecurityDescriptor = NULL,
        .bInheritHandle = TRUE,
    };
    HANDLE read_end = NULL;
    HANDLE write_end = NULL;
    if (!CreatePipe(&read_end, &write_end, &inheritable, 0)) {
        free(wide_cmd);
        free(wide_cwd);
        BT_TRACE_FAIL("mutate.spawn", "pipe failed");
        BT_TRACE_EXIT("mutate.spawn");
        return -1;
    }
    SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);
    HANDLE duplicate = NULL;
    DuplicateHandle(GetCurrentProcess(), write_end, GetCurrentProcess(), &duplicate, 0, TRUE,
                    DUPLICATE_SAME_ACCESS);
    STARTUPINFOW startup = {
        .cb = sizeof startup,
        .dwFlags = STARTF_USESTDHANDLES,
        .hStdOutput = write_end,
        .hStdError = duplicate != NULL ? duplicate : write_end,
    };
    PROCESS_INFORMATION info = {0};
    if (!CreateProcessW(NULL, wide_cmd, NULL, NULL, TRUE, 0, NULL, wide_cwd, &startup, &info)) {
        CloseHandle(read_end);
        CloseHandle(write_end);
        if (duplicate != NULL) {
            CloseHandle(duplicate);
        }
        free(wide_cmd);
        free(wide_cwd);
        BT_TRACE_FAIL("mutate.spawn", "CreateProcessW failed for %s", exe);
        BT_TRACE_EXIT("mutate.spawn");
        return -1;
    }
    CloseHandle(write_end);
    if (duplicate != NULL) {
        CloseHandle(duplicate);
    }
    free(wide_cmd);
    free(wide_cwd);
    HANDLE job = CreateJobObjectW(NULL, NULL);
    if (job != NULL) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
        memset(&limits, 0, sizeof limits);
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof limits);
        AssignProcessToJobObject(job, info.hProcess);
    }
    size_t cap = 8192;
    char *buffer = malloc(cap);
    size_t used = 0;
    if (buffer != NULL) {
        buffer[0] = '\0';
    }
    ULONGLONG deadline = GetTickCount64() + timeout_ms;
    int done = 0;
    while (!done) {
        if (GetTickCount64() > deadline) {
            TerminateProcess(info.hProcess, 124);
            WaitForSingleObject(info.hProcess, 5000);
            out->rc = -2;
            out->timed_out = 1;
            done = 1;
            break;
        }
        DWORD available = 0;
        if (!PeekNamedPipe(read_end, NULL, 0, NULL, &available, NULL)) {
            done = 1;
            continue;
        }
        if (available > 0) {
            char chunk[4096];
            DWORD got = 0;
            DWORD want = available < sizeof chunk ? available : (DWORD)sizeof chunk;
            if (!ReadFile(read_end, chunk, want, &got, NULL) || got == 0) {
                done = 1;
                continue;
            }
            if (buffer != NULL && used + (size_t)got < BT_MUT_CAPTURE_MAX) {
                if (used + got + 1 > cap) {
                    size_t want_cap = cap * 2;
                    while (used + got + 1 > want_cap) {
                        want_cap *= 2;
                    }
                    char *grown = realloc(buffer, want_cap);
                    if (grown != NULL) {
                        buffer = grown;
                        cap = want_cap;
                    }
                }
                if (used + got + 1 <= cap) {
                    memcpy(buffer + used, chunk, got);
                    used += got;
                    buffer[used] = '\0';
                }
            }
            continue;
        }
        if (WaitForSingleObject(info.hProcess, 15) == WAIT_OBJECT_0) {
            DWORD more = 0;
            if (!PeekNamedPipe(read_end, NULL, 0, NULL, &more, NULL) || more == 0) {
                done = 1;
            }
        }
    }
    DWORD code = 0;
    GetExitCodeProcess(info.hProcess, &code);
    if (job != NULL) {
        CloseHandle(job);
    }
    CloseHandle(read_end);
    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);
    if (out->rc == -2) {
        free(buffer);
        BT_TRACE_FAIL("mutate.spawn", "timeout after %ums on %s", timeout_ms, exe);
        BT_TRACE_EXIT("mutate.spawn");
        return -2;
    }
    out->rc = 0;
    out->exit_code = (int)code;
    out->output = buffer;
    out->output_len = used;
    BT_TRACE_EXIT("mutate.spawn");
    return 0;
#else
    BT_TRACE_FAIL("mutate.spawn", "unsupported platform");
    BT_TRACE_EXIT("mutate.spawn");
    return -1;
#endif
}

void bt_mut_proc_free(bt_mut_proc *proc) {
    free(proc->output);
    proc->output = NULL;
    proc->output_len = 0;
}

int bt_mut_compile_file(const bt_mut_build *build, const char *dir_include,
                        const char *extra_include, const char *source, const char *obj, char *diag,
                        size_t diag_cap) {
    BT_TRACE_ENTER("mutate.compile_file");
    char *argv[64];
    size_t count = bt_mut_compile_argv(build, dir_include, extra_include, source, obj, argv, 64);
    if (count == 0 || count + 1 > 64) {
        if (diag != NULL) {
            snprintf(diag, diag_cap, "argv assembly failed");
        }
        BT_TRACE_FAIL("mutate.compile_file", "argv assembly failed");
        BT_TRACE_EXIT("mutate.compile_file");
        return -1;
    }
    argv[count] = NULL;
    bt_mut_proc proc;
    int rc = bt_mut_spawn(build->clang, argv, NULL, 300000, &proc);
    if (rc == -2) {
        if (diag != NULL) {
            snprintf(diag, diag_cap, "compile timed out on %s", build->clang);
        }
        BT_TRACE_FAIL("mutate.compile_file", "timeout on %s", build->clang);
        BT_TRACE_EXIT("mutate.compile_file");
        return -2;
    }
    if (rc != 0) {
        if (diag != NULL) {
            snprintf(diag, diag_cap, "spawn failed: %s", build->clang);
        }
        BT_TRACE_FAIL("mutate.compile_file", "spawn failed for %s", build->clang);
        BT_TRACE_EXIT("mutate.compile_file");
        return -1;
    }
    if (diag != NULL) {
        snprintf(diag, diag_cap, "%s", proc.output != NULL ? proc.output : "");
    }
    int code = proc.exit_code;
    bt_mut_proc_free(&proc);
    BT_TRACE_EXIT("mutate.compile_file");
    return code;
}
