#include "e2e/proc.h"

#include "common/bt_trace.h"

#include <errno.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

typedef struct {
    HANDLE pipe;
    unsigned char **buffer;
    size_t *len;
} bt_e2e_reader;

static DWORD WINAPI read_pipe(LPVOID raw) {
    bt_e2e_reader *reader = raw;
    unsigned char chunk[4096];
    for (;;) {
        DWORD read_len = 0;
        if (!ReadFile(reader->pipe, chunk, sizeof chunk, &read_len, NULL) || read_len == 0) {
            break;
        }
        unsigned char *grown = realloc(*reader->buffer, *reader->len + read_len + 1);
        if (grown == NULL) {
            break;
        }
        *reader->buffer = grown;
        memcpy(*reader->buffer + *reader->len, chunk, read_len);
        *reader->len += read_len;
    }
    return 0;
}

int bt_e2e_utf8_to_wide(const char *text, wchar_t *out, size_t cap) {
    int written = MultiByteToWideChar(CP_UTF8, 0, text, -1, out, (int)cap);
    return written > 0 ? 0 : -1;
}

int bt_e2e_wide_to_utf8(const wchar_t *text, char *out, size_t cap) {
    int written = WideCharToMultiByte(CP_UTF8, 0, text, -1, out, (int)cap, NULL, NULL);
    return written > 0 ? 0 : -1;
}

int bt_e2e_wide_to_utf8_alloc(const wchar_t *text, char **out) {
    int needed = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (needed <= 0) {
        return -1;
    }
    *out = malloc((size_t)needed);
    if (*out == NULL) {
        return -1;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, text, -1, *out, needed, NULL, NULL) <= 0) {
        free(*out);
        *out = NULL;
        return -1;
    }
    return 0;
}

int bt_e2e_file_exists(const char *path) {
    wchar_t path_w[BT_E2E_PATH_WMAX];
    if (bt_e2e_utf8_to_wide(path, path_w, BT_E2E_PATH_WMAX) != 0) {
        return 0;
    }
    DWORD attributes = GetFileAttributesW(path_w);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

unsigned long long bt_e2e_now_ms(void) {
    return (unsigned long long)GetTickCount64();
}

static void last_error_text(char *out, size_t cap) {
    wchar_t *message = NULL;
    DWORD written = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&message, 0, NULL);
    if (written == 0 || message == NULL || bt_e2e_wide_to_utf8(message, out, cap) != 0) {
        snprintf(out, cap, "os error %lu", GetLastError());
    }
    LocalFree(message);
}

int bt_e2e_env_block(const char *const *env_pairs, size_t env_count, wchar_t **out) {
    *out = NULL;
    wchar_t **pairs_w = calloc(env_count > 0 ? env_count : 1, sizeof(wchar_t *));
    if (pairs_w == NULL) {
        return -1;
    }
    for (size_t i = 0; i < env_count; i++) {
        size_t needed = strlen(env_pairs[i]) + 1;
        pairs_w[i] = malloc(needed * sizeof(wchar_t));
        if (pairs_w[i] == NULL || bt_e2e_utf8_to_wide(env_pairs[i], pairs_w[i], needed) != 0) {
            for (size_t j = 0; j < env_count; j++) {
                free(pairs_w[j]);
            }
            free(pairs_w);
            return -1;
        }
    }
    size_t block_len = 0;
    size_t block_cap = 256;
    wchar_t *block = malloc(block_cap * sizeof(wchar_t));
    if (block == NULL) {
        for (size_t j = 0; j < env_count; j++) {
            free(pairs_w[j]);
        }
        free(pairs_w);
        return -1;
    }
    wchar_t *environment = GetEnvironmentStringsW();
    if (environment != NULL) {
        for (const wchar_t *cursor = environment; *cursor != L'\0';) {
            size_t entry_len = wcslen(cursor);
            int overridden = 0;
            for (size_t i = 0; i < env_count; i++) {
                size_t key_len = wcscspn(pairs_w[i], L"=");
                if (entry_len > key_len && _wcsnicmp(cursor, pairs_w[i], key_len) == 0 &&
                    cursor[key_len] == L'=') {
                    overridden = 1;
                }
            }
            if (!overridden) {
                if (block_len + entry_len + 2 > block_cap) {
                    block_cap = block_len + entry_len + 2 + block_cap;
                    wchar_t *grown = realloc(block, block_cap * sizeof(wchar_t));
                    if (grown == NULL) {
                        free(block);
                        FreeEnvironmentStringsW(environment);
                        for (size_t j = 0; j < env_count; j++) {
                            free(pairs_w[j]);
                        }
                        free(pairs_w);
                        return -1;
                    }
                    block = grown;
                }
                wcscpy(block + block_len, cursor);
                block_len += entry_len + 1;
            }
            cursor += entry_len + 1;
        }
        FreeEnvironmentStringsW(environment);
    }
    for (size_t i = 0; i < env_count; i++) {
        size_t pair_len = wcslen(pairs_w[i]);
        if (block_len + pair_len + 2 > block_cap) {
            block_cap = block_len + pair_len + 2 + block_cap;
            wchar_t *grown = realloc(block, block_cap * sizeof(wchar_t));
            if (grown == NULL) {
                free(block);
                for (size_t j = 0; j < env_count; j++) {
                    free(pairs_w[j]);
                }
                free(pairs_w);
                return -1;
            }
            block = grown;
        }
        wcscpy(block + block_len, pairs_w[i]);
        block_len += pair_len + 1;
    }
    block[block_len] = L'\0';
    for (size_t j = 0; j < env_count; j++) {
        free(pairs_w[j]);
    }
    free(pairs_w);
    *out = block;
    return 0;
}

static void quote_into(wchar_t *command, size_t cap, const wchar_t *arg) {
    size_t used = wcslen(command);
    if (used > 0 && used + 1 < cap) {
        command[used] = L' ';
        command[used + 1] = L'\0';
        used += 1;
    }
    if (used + wcslen(arg) + 3 < cap) {
        wcscat(command, L"\"");
        wcscat(command, arg);
        wcscat(command, L"\"");
    }
}

static void normalize_slashes(wchar_t *text) {
    for (wchar_t *p = text; *p != L'\0'; p++) {
        if (*p == L'/') {
            *p = L'\\';
        }
    }
}

int bt_e2e_run_smoke(const char *binary, const char *out_dir, const char *const *env_pairs,
                     size_t env_count, unsigned long long deadline_ms, bt_e2e_smoke_run *out,
                     char *error, size_t error_cap) {
    BT_TRACE_ENTER("e2e.run_smoke");
    memset(out, 0, sizeof *out);
    wchar_t binary_w[BT_E2E_PATH_WMAX];
    wchar_t out_dir_w[BT_E2E_PATH_WMAX];
    if (bt_e2e_utf8_to_wide(binary, binary_w, BT_E2E_PATH_WMAX) != 0 ||
        bt_e2e_utf8_to_wide(out_dir, out_dir_w, BT_E2E_PATH_WMAX) != 0) {
        snprintf(error, error_cap, "cannot spawn %s: %s", binary, strerror(errno));
        BT_TRACE_FAIL("e2e.run_smoke", "utf8 conversion failed");
        BT_TRACE_EXIT("e2e.run_smoke");
        return -1;
    }
    normalize_slashes(binary_w);
    normalize_slashes(out_dir_w);
    wchar_t command[BT_E2E_PATH_WMAX * 2 + 32];
    command[0] = L'\0';
    quote_into(command, BT_E2E_PATH_WMAX * 2 + 32, binary_w);
    quote_into(command, BT_E2E_PATH_WMAX * 2 + 32, L"--smoke");
    quote_into(command, BT_E2E_PATH_WMAX * 2 + 32, out_dir_w);

    SECURITY_ATTRIBUTES inheritable = {sizeof inheritable, NULL, TRUE};
    HANDLE stdout_read = NULL;
    HANDLE stdout_write = NULL;
    HANDLE stderr_read = NULL;
    HANDLE stderr_write = NULL;
    if (!CreatePipe(&stdout_read, &stdout_write, &inheritable, 0) ||
        !CreatePipe(&stderr_read, &stderr_write, &inheritable, 0)) {
        char os_error[256];
        last_error_text(os_error, sizeof os_error);
        CloseHandle(stdout_read);
        CloseHandle(stdout_write);
        CloseHandle(stderr_read);
        CloseHandle(stderr_write);
        snprintf(error, error_cap, "cannot spawn %s: %s", binary, os_error);
        BT_TRACE_FAIL("e2e.run_smoke", "pipe creation failed");
        BT_TRACE_EXIT("e2e.run_smoke");
        return -1;
    }
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup;
    memset(&startup, 0, sizeof startup);
    startup.cb = sizeof startup;
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = stdout_write;
    startup.hStdError = stderr_write;
    PROCESS_INFORMATION process;
    memset(&process, 0, sizeof process);
    wchar_t *env_block = NULL;
    DWORD creation_flags = 0;
    if (env_count > 0) {
        if (bt_e2e_env_block(env_pairs, env_count, &env_block) != 0) {
            CloseHandle(stdout_read);
            CloseHandle(stdout_write);
            CloseHandle(stderr_read);
            CloseHandle(stderr_write);
            snprintf(error, error_cap, "cannot spawn %s: env block allocation failed", binary);
            BT_TRACE_FAIL("e2e.run_smoke", "env block build failed");
            BT_TRACE_EXIT("e2e.run_smoke");
            return -1;
        }
        creation_flags |= CREATE_UNICODE_ENVIRONMENT;
    }
    if (!CreateProcessW(NULL, command, NULL, NULL, TRUE, creation_flags, env_block, NULL, &startup,
                        &process)) {
        char os_error[256];
        last_error_text(os_error, sizeof os_error);
        free(env_block);
        CloseHandle(stdout_read);
        CloseHandle(stdout_write);
        CloseHandle(stderr_read);
        CloseHandle(stderr_write);
        snprintf(error, error_cap, "cannot spawn %s: %s", binary, os_error);
        BT_TRACE_FAIL("e2e.run_smoke", "CreateProcessW failed");
        BT_TRACE_EXIT("e2e.run_smoke");
        return -1;
    }
    free(env_block);
    CloseHandle(stdout_write);
    CloseHandle(stderr_write);
    bt_e2e_reader stdout_reader = {stdout_read, &out->stdout_bytes, &out->stdout_len};
    bt_e2e_reader stderr_reader = {stderr_read, &out->stderr_bytes, &out->stderr_len};
    HANDLE stdout_thread = CreateThread(NULL, 0, read_pipe, &stdout_reader, 0, NULL);
    HANDLE stderr_thread = CreateThread(NULL, 0, read_pipe, &stderr_reader, 0, NULL);
    if (stdout_thread == NULL || stderr_thread == NULL) {
        char os_error[256];
        last_error_text(os_error, sizeof os_error);
        if (stdout_thread != NULL) {
            WaitForSingleObject(stdout_thread, INFINITE);
        }
        if (stderr_thread != NULL) {
            WaitForSingleObject(stderr_thread, INFINITE);
        }
        CloseHandle(stdout_thread);
        CloseHandle(stderr_thread);
        CloseHandle(stdout_read);
        CloseHandle(stderr_read);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        snprintf(error, error_cap, "cannot spawn %s: %s", binary, os_error);
        BT_TRACE_FAIL("e2e.run_smoke", "reader thread creation failed");
        BT_TRACE_EXIT("e2e.run_smoke");
        return -1;
    }
    int timed_out = 0;
    int wait_failed = 0;
    for (;;) {
        if (bt_e2e_now_ms() >= deadline_ms) {
            TerminateProcess(process.hProcess, 1);
            WaitForSingleObject(process.hProcess, INFINITE);
            timed_out = 1;
            break;
        }
        DWORD wait_code = WaitForSingleObject(process.hProcess, 100);
        if (wait_code == WAIT_FAILED) {
            wait_failed = 1;
            break;
        }
        if (wait_code == WAIT_OBJECT_0) {
            break;
        }
    }
    DWORD exit_code = 0;
    if (!timed_out && !wait_failed && !GetExitCodeProcess(process.hProcess, &exit_code)) {
        wait_failed = 1;
    }
    WaitForSingleObject(stdout_thread, INFINITE);
    WaitForSingleObject(stderr_thread, INFINITE);
    CloseHandle(stdout_thread);
    CloseHandle(stderr_thread);
    CloseHandle(stdout_read);
    CloseHandle(stderr_read);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    int verdict = 0;
    if (timed_out) {
        snprintf(error, error_cap, "smoke run timed out");
        verdict = -1;
    } else if (wait_failed) {
        char os_error[256];
        last_error_text(os_error, sizeof os_error);
        snprintf(error, error_cap, "cannot wait for smoke process: %s", os_error);
        verdict = -1;
    } else {
        out->has_code = 1;
        out->code = (long)exit_code;
    }
    if (verdict == 0) {
        BT_TRACE_EXIT("e2e.run_smoke");
    } else {
        BT_TRACE_FAIL("e2e.run_smoke", "%s", error);
        BT_TRACE_EXIT("e2e.run_smoke");
    }
    return verdict;
}

void bt_e2e_smoke_run_free(bt_e2e_smoke_run *run) {
    BT_TRACE_ENTER("e2e.smoke_run_free");
    free(run->stdout_bytes);
    free(run->stderr_bytes);
    run->stdout_bytes = NULL;
    run->stderr_bytes = NULL;
    run->stdout_len = 0;
    run->stderr_len = 0;
    BT_TRACE_EXIT("e2e.smoke_run_free");
}
