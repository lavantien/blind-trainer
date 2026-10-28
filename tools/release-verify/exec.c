#include "release-verify/exec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "common/bt_trace.h"
#include "common/strbuf.h"

enum {
    BT_RV_POLL_MS = 50,
    BT_RV_KILL_WAIT_MS = 10000,
};

#define BT_RV_CMDLINE_MAX (BT_PATH_MAX * 8)

static void *rv_xmalloc(size_t size) {
    void *block = malloc(size > 0 ? size : 1);
    if (block == NULL) {
        fputs("bt-release-verify: out of memory\n", stderr);
        abort();
    }
    return block;
}

static char *rv_xstrdup(const char *text) {
    size_t len = strlen(text) + 1;
    char *copy = rv_xmalloc(len);
    memcpy(copy, text, len);
    return copy;
}

static int rv_key_is_bt_prefixed(const char *entry) {
    const char *equal = strchr(entry, '=');
    size_t key_len = equal != NULL ? (size_t)(equal - entry) : strlen(entry);
    return key_len >= 3 && _strnicmp(entry, "BT_", 3) == 0;
}

size_t bt_rv_scrub_env(const char *const vars[], char ***out) {
    size_t total = 0;
    for (size_t at = 0; vars != NULL && vars[at] != NULL; at++) {
        if (!rv_key_is_bt_prefixed(vars[at])) {
            total++;
        }
    }
    char **list = rv_xmalloc((total + 1) * sizeof *list);
    size_t used = 0;
    for (size_t at = 0; vars != NULL && vars[at] != NULL; at++) {
        if (!rv_key_is_bt_prefixed(vars[at])) {
            list[used++] = rv_xstrdup(vars[at]);
        }
    }
    list[used] = NULL;
    *out = list;
    return used;
}

void bt_rv_str_list_free(char **list, size_t count) {
    if (list == NULL) {
        return;
    }
    for (size_t at = 0; at < count; at++) {
        free(list[at]);
    }
    free(list);
}

const char *bt_rv_env_get(const bt_rv_child_spec *spec, const char *key) {
    size_t key_len = strlen(key);
    for (size_t at = 0; at < spec->env_count; at++) {
        const char *entry = spec->env[at];
        if (_strnicmp(entry, key, key_len) == 0 && entry[key_len] == '=') {
            return entry + key_len + 1;
        }
    }
    return NULL;
}

static void rv_env_set(char ***env, size_t *count, const char *key, const char *value) {
    size_t key_len = strlen(key);
    for (size_t at = 0; at < *count; at++) {
        const char *entry = (*env)[at];
        if (_strnicmp(entry, key, key_len) == 0 && entry[key_len] == '=') {
            free((*env)[at]);
            size_t need = key_len + 1 + strlen(value) + 1;
            char *merged = rv_xmalloc(need);
            snprintf(merged, need, "%s=%s", key, value);
            (*env)[at] = merged;
            return;
        }
    }
    char **grown = realloc(*env, (*count + 2) * sizeof *grown);
    if (grown == NULL) {
        fputs("bt-release-verify: out of memory\n", stderr);
        abort();
    }
    *env = grown;
    size_t need = key_len + 1 + strlen(value) + 1;
    char *merged = rv_xmalloc(need);
    snprintf(merged, need, "%s=%s", key, value);
    (*env)[*count] = merged;
    (*env)[*count + 1] = NULL;
    (*count)++;
}

static int rv_env_pair_cmp(const void *left, const void *right) {
    return _stricmp(*(const char *const *)left, *(const char *const *)right);
}

static void rv_copy_args(const char *const args[], bt_rv_child_spec *out) {
    size_t count = 0;
    for (size_t at = 0; args != NULL && args[at] != NULL; at++) {
        count++;
    }
    out->args = rv_xmalloc((count + 1) * sizeof *out->args);
    for (size_t at = 0; at < count; at++) {
        out->args[at] = rv_xstrdup(args[at]);
    }
    out->args[count] = NULL;
    out->arg_count = count;
}

void bt_rv_stage_spec(const char *program, const char *const args[], const char *cwd,
                      const char *const vars[], const bt_rv_env_pair *extras, size_t extra_count,
                      bt_rv_child_spec *out) {
    BT_TRACE_ENTER("rv.stage_spec");
    memset(out, 0, sizeof *out);
    snprintf(out->program, sizeof out->program, "%s", program);
    snprintf(out->cwd, sizeof out->cwd, "%s", cwd != NULL ? cwd : "");
    rv_copy_args(args, out);
    out->env_count = bt_rv_scrub_env(vars, &out->env);
    for (size_t at = 0; at < extra_count; at++) {
        rv_env_set(&out->env, &out->env_count, extras[at].key, extras[at].value);
    }
    qsort(out->env, out->env_count, sizeof *out->env, rv_env_pair_cmp);
    BT_TRACE_EXIT("rv.stage_spec");
}

void bt_rv_child_spec_free(bt_rv_child_spec *spec) {
    bt_rv_str_list_free(spec->args, spec->arg_count);
    bt_rv_str_list_free(spec->env, spec->env_count);
    spec->args = NULL;
    spec->env = NULL;
    spec->arg_count = 0;
    spec->env_count = 0;
}

static int rv_quote_wide(const wchar_t *arg, wchar_t *out, size_t cap) {
    size_t used = 0;
    int needs = arg[0] == L'\0' || wcspbrk(arg, L" \t\"") != NULL;
    if (needs) {
        if (used + 1 >= cap) {
            return -1;
        }
        out[used++] = L'"';
    }
    for (size_t i = 0; arg[i] != L'\0'; i++) {
        size_t backslashes = 0;
        while (arg[i] == L'\\') {
            backslashes++;
            i++;
        }
        if (arg[i] == L'\0') {
            for (size_t b = 0; b < backslashes * 2; b++) {
                if (used + 1 >= cap) {
                    return -1;
                }
                out[used++] = L'\\';
            }
            break;
        }
        int quote = arg[i] == L'"';
        size_t emit = quote ? backslashes * 2 + 1 : backslashes;
        for (size_t b = 0; b < emit; b++) {
            if (used + 1 >= cap) {
                return -1;
            }
            out[used++] = L'\\';
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
        out[used++] = L'"';
    }
    out[used] = L'\0';
    return 0;
}

static wchar_t *rv_wide(const char *text) {
    int need = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    wchar_t *wide = rv_xmalloc((size_t)need * sizeof *wide);
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, need);
    return wide;
}

static int rv_build_command_line(const bt_rv_child_spec *spec, wchar_t *out, size_t cap) {
    size_t used = 0;
    out[0] = L'\0';
    for (size_t at = 0; at <= spec->arg_count; at++) {
        const char *part = at == 0 ? spec->program : spec->args[at - 1];
        wchar_t *wide = rv_wide(part);
        wchar_t quoted[BT_PATH_MAX * 2];
        int rc = rv_quote_wide(wide, quoted, sizeof quoted / sizeof *quoted);
        free(wide);
        if (rc != 0) {
            return -1;
        }
        size_t len = wcslen(quoted);
        if (used + len + 2 >= cap) {
            return -1;
        }
        if (at > 0) {
            out[used++] = L' ';
        }
        memcpy(out + used, quoted, len * sizeof *quoted);
        used += len;
        out[used] = L'\0';
    }
    return 0;
}

static int rv_wide_pair_cmp(const void *left, const void *right) {
    return _wcsicmp(*(const wchar_t *const *)left, *(const wchar_t *const *)right);
}

static wchar_t *rv_env_block(const bt_rv_child_spec *spec) {
    wchar_t **wide = rv_xmalloc((spec->env_count + 1) * sizeof *wide);
    for (size_t at = 0; at < spec->env_count; at++) {
        wide[at] = rv_wide(spec->env[at]);
    }
    wide[spec->env_count] = NULL;
    qsort(wide, spec->env_count, sizeof *wide, rv_wide_pair_cmp);
    size_t total = 1;
    for (size_t at = 0; at < spec->env_count; at++) {
        total += wcslen(wide[at]) + 1;
    }
    wchar_t *block = rv_xmalloc((total + 1) * sizeof *block);
    size_t used = 0;
    for (size_t at = 0; at < spec->env_count; at++) {
        size_t len = wcslen(wide[at]);
        memcpy(block + used, wide[at], len * sizeof **wide);
        used += len;
        block[used++] = L'\0';
    }
    block[used] = L'\0';
    block[used + 1] = L'\0';
    for (size_t at = 0; at < spec->env_count; at++) {
        free(wide[at]);
    }
    free(wide);
    return block;
}

typedef struct {
    HANDLE pipe;
    bt_strbuf buffer;
    int ok;
} rv_reader;

static DWORD WINAPI rv_pipe_reader(void *raw) {
    rv_reader *reader = raw;
    char chunk[4096];
    DWORD got = 0;
    for (;;) {
        if (!ReadFile(reader->pipe, chunk, sizeof chunk, &got, NULL) || got == 0) {
            break;
        }
        if (bt_strbuf_append(&reader->buffer, chunk, got) != 0) {
            reader->ok = 0;
            break;
        }
    }
    return 0;
}

static void rv_kill_tree(unsigned long pid, HANDLE process) {
    BT_TRACE_ENTER("rv.kill_tree");
    wchar_t line[96];
    wchar_t pid_text[32];
    _snwprintf_s(pid_text, sizeof pid_text / sizeof *pid_text, _TRUNCATE, L"%lu", pid);
    wcscpy_s(line, sizeof line / sizeof *line, L"taskkill /PID ");
    wcscat_s(line, sizeof line / sizeof *line, pid_text);
    wcscat_s(line, sizeof line / sizeof *line, L" /T /F");
    STARTUPINFOW startup;
    ZeroMemory(&startup, sizeof startup);
    startup.cb = sizeof startup;
    PROCESS_INFORMATION info;
    ZeroMemory(&info, sizeof info);
    int killed = 0;
    if (CreateProcessW(NULL, line, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &startup,
                       &info)) {
        WaitForSingleObject(info.hProcess, BT_RV_KILL_WAIT_MS);
        DWORD code = 0;
        killed = GetExitCodeProcess(info.hProcess, &code) && code == 0;
        CloseHandle(info.hProcess);
        CloseHandle(info.hThread);
    }
    if (!killed) {
        TerminateProcess(process, 1);
    }
    BT_TRACE_EXIT("rv.kill_tree");
}

int bt_rv_run_child(const bt_rv_child_spec *spec, unsigned long long deadline_ms,
                    bt_rv_run_result *out) {
    BT_TRACE_ENTER("rv.run_child");
    ULONGLONG started = GetTickCount64();
    memset(out, 0, sizeof *out);
    out->stdout_text = rv_xstrdup("");
    out->stderr_text = rv_xstrdup("");
    wchar_t command[BT_RV_CMDLINE_MAX];
    wchar_t *cwd_wide = NULL;
    wchar_t *env_wide = NULL;
    SECURITY_ATTRIBUTES inheritable = {
        .nLength = sizeof inheritable,
        .lpSecurityDescriptor = NULL,
        .bInheritHandle = TRUE,
    };
    HANDLE out_read = NULL;
    HANDLE out_write = NULL;
    HANDLE err_read = NULL;
    HANDLE err_write = NULL;
    HANDLE process = NULL;
    HANDLE thread = NULL;
    if (rv_build_command_line(spec, command, sizeof command / sizeof *command) != 0) {
        snprintf(out->outcome.detail, sizeof out->outcome.detail,
                 "cannot spawn %s: command line too long", spec->program);
        out->outcome.kind = BT_RV_OUTCOME_SPAWN;
        goto done;
    }
    cwd_wide = spec->cwd[0] != '\0' ? rv_wide(spec->cwd) : NULL;
    env_wide = rv_env_block(spec);
    if (!CreatePipe(&out_read, &out_write, &inheritable, 0) ||
        !CreatePipe(&err_read, &err_write, &inheritable, 0)) {
        snprintf(out->outcome.detail, sizeof out->outcome.detail,
                 "cannot spawn %s: pipe failed %lu", spec->program, GetLastError());
        out->outcome.kind = BT_RV_OUTCOME_SPAWN;
        goto done;
    }
    SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(err_read, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW startup;
    ZeroMemory(&startup, sizeof startup);
    startup.cb = sizeof startup;
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = out_write;
    startup.hStdError = err_write;
    PROCESS_INFORMATION info;
    ZeroMemory(&info, sizeof info);
    if (!CreateProcessW(NULL, command, NULL, NULL, TRUE, CREATE_UNICODE_ENVIRONMENT, env_wide,
                        cwd_wide, &startup, &info)) {
        snprintf(out->outcome.detail, sizeof out->outcome.detail, "cannot spawn %s: %lu",
                 spec->program, GetLastError());
        out->outcome.kind = BT_RV_OUTCOME_SPAWN;
        goto done;
    }
    process = info.hProcess;
    thread = info.hThread;
    out->pid = info.dwProcessId;
    CloseHandle(out_write);
    CloseHandle(err_write);
    out_write = NULL;
    err_write = NULL;
    rv_reader out_reader = {.pipe = out_read, .ok = 1};
    rv_reader err_reader = {.pipe = err_read, .ok = 1};
    bt_strbuf_init(&out_reader.buffer, 256);
    bt_strbuf_init(&err_reader.buffer, 256);
    HANDLE readers[2];
    readers[0] = CreateThread(NULL, 0, rv_pipe_reader, &out_reader, 0, NULL);
    readers[1] = CreateThread(NULL, 0, rv_pipe_reader, &err_reader, 0, NULL);
    int timed_out = 0;
    for (;;) {
        DWORD wait = WaitForSingleObject(process, BT_RV_POLL_MS);
        if (wait == WAIT_OBJECT_0) {
            break;
        }
        if (GetTickCount64() - started >= deadline_ms) {
            rv_kill_tree(info.dwProcessId, process);
            WaitForSingleObject(process, BT_RV_KILL_WAIT_MS);
            timed_out = 1;
            break;
        }
    }
    WaitForSingleObject(readers[0], INFINITE);
    WaitForSingleObject(readers[1], INFINITE);
    CloseHandle(readers[0]);
    CloseHandle(readers[1]);
    CloseHandle(out_read);
    CloseHandle(err_read);
    out_read = NULL;
    err_read = NULL;
    free(out->stdout_text);
    free(out->stderr_text);
    out->stdout_text = rv_xstrdup(out_reader.buffer.data != NULL ? out_reader.buffer.data : "");
    out->stderr_text = rv_xstrdup(err_reader.buffer.data != NULL ? err_reader.buffer.data : "");
    bt_strbuf_free(&out_reader.buffer);
    bt_strbuf_free(&err_reader.buffer);
    if (timed_out) {
        out->outcome.kind = BT_RV_OUTCOME_TIMEOUT;
    } else {
        DWORD code = 0;
        GetExitCodeProcess(process, &code);
        out->outcome.kind = BT_RV_OUTCOME_EXIT;
        out->outcome.exit_code = (int)code;
    }
done:
    if (out_read != NULL) {
        CloseHandle(out_read);
    }
    if (err_read != NULL) {
        CloseHandle(err_read);
    }
    if (out_write != NULL) {
        CloseHandle(out_write);
    }
    if (err_write != NULL) {
        CloseHandle(err_write);
    }
    if (process != NULL) {
        CloseHandle(process);
    }
    if (thread != NULL) {
        CloseHandle(thread);
    }
    free(cwd_wide);
    free(env_wide);
    out->elapsed_ms = GetTickCount64() - started;
    if (out->outcome.kind == BT_RV_OUTCOME_SPAWN) {
        BT_TRACE_FAIL("rv.run_child", "%s", out->outcome.detail);
    }
    BT_TRACE_EXIT("rv.run_child");
    return 0;
}

void bt_rv_run_result_free(bt_rv_run_result *result) {
    free(result->stdout_text);
    free(result->stderr_text);
    result->stdout_text = NULL;
    result->stderr_text = NULL;
}
