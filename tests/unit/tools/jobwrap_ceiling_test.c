#include "unit/tools/spawn_util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "common/bt_harness.h"

enum {
    CEILING_WAIT_FILE_MS = 10000,
    CEILING_WAIT_EXIT_MS = 60000,
    CEILING_WAIT_SCAN_MS = 15000,
    CEILING_WAIT_GONE_MS = 20000,
    CEILING_KILL_MS = 30000,
    CEILING_POLL_MS = 150,
};

typedef struct {
    char dir[BT_PATH_MAX];
    char pids[BT_PATH_MAX];
    char config[BT_PATH_MAX];
} ceiling_fixture;

static unsigned g_ceiling_sequence = 0;

static char g_ceiling_diag[256];

static const char *ceiling_diag(void) {
    return g_ceiling_diag;
}

static void to_forward_slashes(char *path) {
    for (char *cursor = path; *cursor != '\0'; cursor++) {
        if (*cursor == '\\') {
            *cursor = '/';
        }
    }
}

static void write_text(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        BT_ABORT("cannot write %s", path);
    }
    fwrite(text, 1, strlen(text), file);
    int rc = fclose(file);
    if (rc != 0) {
        BT_ABORT("cannot close %s", path);
    }
}

static void fixture_init(ceiling_fixture *fx) {
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    snprintf(fx->dir, BT_PATH_MAX, "%s/build/test-tmp/jobwrap-ceiling-%lu-%u", root,
             GetCurrentProcessId(), g_ceiling_sequence);
    g_ceiling_sequence++;
    snprintf(fx->pids, BT_PATH_MAX, "%s/pids", fx->dir);
    snprintf(fx->config, BT_PATH_MAX, "%s/config.json", fx->dir);
    bt_test_mkdirs(fx->pids);
}

static void fixture_ceiling(ceiling_fixture *fx, unsigned long long per_process_bytes) {
    fixture_init(fx);
    char pids[BT_PATH_MAX];
    snprintf(pids, sizeof pids, "%s", fx->pids);
    to_forward_slashes(pids);
    char body[BT_PATH_MAX * 2];
    snprintf(body, sizeof body,
             "{\"job\":{\"per_process_bytes\":%llu},\"memguard\":{\"pid_dir\":\"%s\"}}",
             per_process_bytes, pids);
    write_text(fx->config, body);
}

static void fixture_raw(ceiling_fixture *fx, const char *body) {
    fixture_init(fx);
    write_text(fx->config, body);
}

static char g_ceiling_saved_config[BT_PATH_MAX];
static int g_ceiling_saved_config_set = 0;

static void bt_config_push(const char *path) {
    const char *current = getenv("BT_CONFIG");
    if (current != NULL && current[0] != '\0') {
        snprintf(g_ceiling_saved_config, sizeof g_ceiling_saved_config, "%s", current);
        g_ceiling_saved_config_set = 1;
    } else {
        g_ceiling_saved_config_set = 0;
    }
    char entry[BT_PATH_MAX + 16];
    snprintf(entry, sizeof entry, "BT_CONFIG=%s", path);
    _putenv(entry);
}

static void bt_config_pop(void) {
    if (g_ceiling_saved_config_set != 0) {
        char entry[BT_PATH_MAX + 16];
        snprintf(entry, sizeof entry, "BT_CONFIG=%s", g_ceiling_saved_config);
        _putenv(entry);
    } else {
        _putenv("BT_CONFIG=");
    }
}

static int spawn_jobwrap(const char *bt_config, const char *const argv[],
                         PROCESS_INFORMATION *info) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "jobwrap.exe");
    char exe_native[BT_PATH_MAX];
    snprintf(exe_native, sizeof exe_native, "%s", exe);
    bt_spawn_backslashes(exe_native);
    char command[BT_PATH_MAX * 4];
    if (bt_spawn_command(exe_native, argv, command, sizeof command) != 0) {
        snprintf(g_ceiling_diag, sizeof g_ceiling_diag, "command too long: %s", exe_native);
        return -1;
    }
    wchar_t wide_command[BT_PATH_MAX * 4];
    if (MultiByteToWideChar(CP_UTF8, 0, command, -1, wide_command,
                            (int)(sizeof wide_command / sizeof wide_command[0])) == 0) {
        snprintf(g_ceiling_diag, sizeof g_ceiling_diag, "command widen failed");
        return -1;
    }
    STARTUPINFOW startup;
    ZeroMemory(&startup, sizeof startup);
    startup.cb = sizeof startup;
    ZeroMemory(info, sizeof *info);
    bt_config_push(bt_config);
    int spawned =
        CreateProcessW(NULL, wide_command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, info) != 0;
    bt_config_pop();
    if (spawned == 0) {
        snprintf(g_ceiling_diag, sizeof g_ceiling_diag, "CreateProcessW failed: %lu",
                 GetLastError());
        return -1;
    }
    return 0;
}

static int wait_process(HANDLE process, unsigned timeout_ms, unsigned long *code) {
    if (WaitForSingleObject(process, timeout_ms) != WAIT_OBJECT_0) {
        return -1;
    }
    DWORD raw = 0;
    if (GetExitCodeProcess(process, &raw) == 0) {
        return -1;
    }
    *code = raw;
    return 0;
}

static int run_wrapped(const char *bt_config, const char *const argv[], unsigned timeout_ms,
                       int *exit_code) {
    PROCESS_INFORMATION info;
    if (spawn_jobwrap(bt_config, argv, &info) != 0) {
        return -1;
    }
    unsigned long code = 0;
    int rc = wait_process(info.hProcess, timeout_ms, &code);
    if (rc != 0) {
        TerminateProcess(info.hProcess, 124);
        WaitForSingleObject(info.hProcess, CEILING_KILL_MS);
    }
    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);
    *exit_code = (int)code;
    return rc;
}

static void trim_text(char *text) {
    size_t len = strlen(text);
    while (len > 0 && isspace((unsigned char)text[len - 1]) != 0) {
        text[--len] = '\0';
    }
    char *start = text;
    while (*start != '\0' && isspace((unsigned char)*start) != 0) {
        start++;
    }
    if (start != text) {
        memmove(text, start, strlen(start) + 1);
    }
}

static void pwsh_text(const char *script, char *out, size_t cap) {
    const char *argv[] = {"-NoProfile", "-Command", script, NULL};
    int code = -1;
    char err[512];
    out[0] = '\0';
    int rc =
        bt_spawn_capture("pwsh", argv, NULL, CEILING_KILL_MS * 2, &code, out, cap, err, sizeof err);
    if (rc != 0) {
        BT_ABORT("pwsh spawn failed: %s", bt_spawn_diag());
    }
    trim_text(out);
}

static int pid_alive(unsigned long pid) {
    char script[128];
    char text[32];
    snprintf(script, sizeof script, "[bool](Get-Process -Id %lu -ErrorAction SilentlyContinue)",
             pid);
    pwsh_text(script, text, sizeof text);
    return strcmp(text, "True") == 0;
}

static int wait_for_file(const char *path, unsigned timeout_ms) {
    ULONGLONG deadline = GetTickCount64() + timeout_ms;
    while (GetTickCount64() <= deadline) {
        DWORD attrs = GetFileAttributesA(path);
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            return 1;
        }
        Sleep(100);
    }
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

static unsigned long find_pidfile_pid(const char *dir, const char *prefix) {
    char pattern[BT_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*.pid", dir);
    WIN32_FIND_DATAA data;
    HANDLE find = FindFirstFileA(pattern, &data);
    if (find == INVALID_HANDLE_VALUE) {
        return 0;
    }
    unsigned long found = 0;
    size_t prefix_len = strlen(prefix);
    do {
        size_t name_len = strlen(data.cFileName);
        if (name_len > prefix_len + 4 && strncmp(data.cFileName, prefix, prefix_len) == 0 &&
            strcmp(data.cFileName + name_len - 4, ".pid") == 0) {
            found = strtoul(data.cFileName + prefix_len, NULL, 10);
            break;
        }
    } while (FindNextFileA(find, &data) != 0);
    FindClose(find);
    return found;
}

static void taskkill(unsigned long pid, int tree) {
    char pid_text[32];
    snprintf(pid_text, sizeof pid_text, "%lu", pid);
    const char *argv[5];
    argv[0] = "/PID";
    argv[1] = pid_text;
    argv[2] = "/F";
    argv[3] = tree != 0 ? "/T" : NULL;
    argv[4] = NULL;
    int code = -1;
    char out[256];
    char err[256];
    bt_spawn_capture("taskkill", argv, NULL, CEILING_KILL_MS, &code, out, sizeof out, err,
                     sizeof err);
}

static void jobwrap_exe(char out[BT_PATH_MAX]) {
    bt_test_bin_path(out, "jobwrap.exe");
}

BT_TEST(ceiling, forwards_payload_exit_codes) {
    ceiling_fixture fx;
    fixture_ceiling(&fx, 268435456ULL);
    const char *exit_zero[] = {"--", "pwsh", "-NoProfile", "-Command", "exit 0", NULL};
    int code = -1;
    int rc = run_wrapped(fx.config, exit_zero, CEILING_WAIT_EXIT_MS, &code);
    if (rc != 0) {
        bt_harness_fail(__FILE__, __LINE__, "spawn failed: %s", ceiling_diag());
    }
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 0);
    const char *exit_seven[] = {"--", "pwsh", "-NoProfile", "-Command", "exit 7", NULL};
    rc = run_wrapped(fx.config, exit_seven, CEILING_WAIT_EXIT_MS, &code);
    if (rc != 0) {
        bt_harness_fail(__FILE__, __LINE__, "spawn failed: %s", ceiling_diag());
    }
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 7);
}

BT_TEST(ceiling, payload_under_ceiling_exits_zero) {
    ceiling_fixture fx;
    fixture_ceiling(&fx, 536870912ULL);
    char exe[BT_PATH_MAX];
    jobwrap_exe(exe);
    const char *argv[] = {"--", exe, "--selftest-alloc-mb", "64", NULL};
    int code = -1;
    ULONGLONG started = GetTickCount64();
    int rc = run_wrapped(fx.config, argv, CEILING_WAIT_EXIT_MS, &code);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 0);
    fprintf(stderr, "ceiling: 64 MiB under 512 MiB ceiling exited 0 in %llums\n",
            (unsigned long long)(GetTickCount64() - started));
}

BT_TEST(ceiling, payload_past_ceiling_dies_fast) {
    ceiling_fixture fx;
    fixture_ceiling(&fx, 100663296ULL);
    char exe[BT_PATH_MAX];
    jobwrap_exe(exe);
    const char *argv[] = {"--", exe, "--selftest-alloc-mb", "640", NULL};
    int code = -1;
    ULONGLONG started = GetTickCount64();
    int rc = run_wrapped(fx.config, argv, CEILING_WAIT_EXIT_MS, &code);
    unsigned long long elapsed = GetTickCount64() - started;
    BT_CHECK_EQ(rc, 0);
    BT_CHECK(code != 0);
    BT_CHECK(code != 259);
    BT_CHECK(elapsed < 30000);
    fprintf(stderr, "ceiling: 640 MiB ask under 96 MiB ceiling died in %llums with code %d\n",
            elapsed, code);
}

BT_TEST(ceiling, ceiling_comes_from_the_config_at_runtime) {
    ceiling_fixture tight;
    fixture_ceiling(&tight, 67108864ULL);
    ceiling_fixture loose;
    fixture_ceiling(&loose, 4294967296ULL);
    char exe[BT_PATH_MAX];
    jobwrap_exe(exe);
    const char *argv[] = {"--", exe, "--selftest-alloc-mb", "96", NULL};
    int code = -1;
    int rc = run_wrapped(tight.config, argv, CEILING_WAIT_EXIT_MS, &code);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK(code != 0);
    code = -1;
    rc = run_wrapped(loose.config, argv, CEILING_WAIT_EXIT_MS, &code);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 0);
}

BT_TEST(ceiling, usage_and_config_failures_exit_2_and_3) {
    ceiling_fixture fx;
    fixture_ceiling(&fx, 268435456ULL);
    const char *no_args[] = {NULL};
    const char *marker_only[] = {"--", NULL};
    const char *dangling_anchor[] = {"--anchor", NULL};
    const char *bad_anchor[] = {"--anchor", "bad name", "--", "pwsh", NULL};
    int code = -1;
    int rc = run_wrapped(fx.config, no_args, CEILING_WAIT_EXIT_MS, &code);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 2);
    rc = run_wrapped(fx.config, marker_only, CEILING_WAIT_EXIT_MS, &code);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 2);
    rc = run_wrapped(fx.config, dangling_anchor, CEILING_WAIT_EXIT_MS, &code);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 2);
    rc = run_wrapped(fx.config, bad_anchor, CEILING_WAIT_EXIT_MS, &code);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 2);
    char absent[BT_PATH_MAX];
    snprintf(absent, sizeof absent, "%s/absent.json", fx.dir);
    const char *plain[] = {"--", "pwsh", "-NoProfile", "-Command", "exit 0", NULL};
    rc = run_wrapped(absent, plain, CEILING_WAIT_EXIT_MS, &code);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 3);
    ceiling_fixture nojob;
    fixture_raw(&nojob, "{\"memguard\":{\"pid_dir\":\"unused\"}}");
    rc = run_wrapped(nojob.config, plain, CEILING_WAIT_EXIT_MS, &code);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 3);
}

BT_TEST(ceiling, anchor_pidfile_carries_dotnet_start_ticks_and_clears_on_exit) {
    ceiling_fixture fx;
    fixture_ceiling(&fx, 268435456ULL);
    char exe[BT_PATH_MAX];
    jobwrap_exe(exe);
    const char *argv[] = {"--anchor", "gw-stamp", "--", exe, "--selftest-hold-secs", "12", NULL};
    PROCESS_INFORMATION info;
    if (spawn_jobwrap(fx.config, argv, &info) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "spawn failed: %s", ceiling_diag());
    }
    unsigned long pid = info.dwProcessId;
    char pidfile[BT_PATH_MAX];
    snprintf(pidfile, sizeof pidfile, "%s/gw-stamp-%lu.pid", fx.pids, pid);
    if (!wait_for_file(pidfile, CEILING_WAIT_FILE_MS)) {
        CloseHandle(info.hProcess);
        CloseHandle(info.hThread);
        bt_harness_fail(__FILE__, __LINE__, "pidfile never appeared: %s", pidfile);
    }
    FILE *file = fopen(pidfile, "rb");
    BT_CHECK(file != NULL);
    char raw[128] = {0};
    size_t got = fread(raw, 1, sizeof raw - 1, file);
    fclose(file);
    BT_CHECK(got > 0);
    char *end = NULL;
    unsigned long file_pid = strtoul(raw, &end, 10);
    BT_CHECK(file_pid != 0 && end != NULL && end != raw);
    while (*end == ' ') {
        end++;
    }
    BT_CHECK(*end != '\0');
    unsigned long long ticks = strtoull(end, NULL, 10);
    BT_CHECK(ticks != 0);
    BT_CHECK_EQ(file_pid, pid);
    char script[160];
    snprintf(script, sizeof script, "(Get-Process -Id %lu).StartTime.ToUniversalTime().Ticks", pid);
    char live[64];
    pwsh_text(script, live, sizeof live);
    BT_CHECK(live[0] != '\0');
    BT_CHECK_EQ(ticks, strtoull(live, NULL, 10));
    unsigned long code = 0;
    int rc = wait_process(info.hProcess, CEILING_WAIT_EXIT_MS, &code);
    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 0);
    BT_CHECK(GetFileAttributesA(pidfile) == INVALID_FILE_ATTRIBUTES);
}

BT_TEST(ceiling, killing_the_wrapper_takes_the_grandchild_with_it) {
    ceiling_fixture fx;
    fixture_ceiling(&fx, 4294967296ULL);
    char exe[BT_PATH_MAX];
    jobwrap_exe(exe);
    const char *argv[] = {
        "--anchor", "gw-outer", "--", exe, "--anchor", "gw-mid", "--", exe, "--selftest-hold-secs",
        "90",       NULL,
    };
    PROCESS_INFORMATION info;
    if (spawn_jobwrap(fx.config, argv, &info) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "spawn failed: %s", ceiling_diag());
    }
    unsigned long mid = 0;
    ULONGLONG deadline = GetTickCount64() + CEILING_WAIT_SCAN_MS;
    while (GetTickCount64() <= deadline) {
        mid = find_pidfile_pid(fx.pids, "gw-mid-");
        if (mid != 0) {
            break;
        }
        Sleep(CEILING_POLL_MS);
    }
    if (mid == 0) {
        CloseHandle(info.hProcess);
        CloseHandle(info.hThread);
        bt_harness_fail(__FILE__, __LINE__, "mid wrapper pidfile never appeared");
    }
    unsigned long leaf = 0;
    char script[192];
    snprintf(script, sizeof script,
             "(Get-CimInstance Win32_Process -Filter \"ParentProcessId = %lu\").ProcessId", mid);
    deadline = GetTickCount64() + CEILING_WAIT_SCAN_MS;
    while (GetTickCount64() <= deadline) {
        char text[64];
        pwsh_text(script, text, sizeof text);
        leaf = strtoul(text, NULL, 10);
        if (leaf != 0) {
            break;
        }
        Sleep(CEILING_POLL_MS * 2);
    }
    if (leaf == 0) {
        CloseHandle(info.hProcess);
        CloseHandle(info.hThread);
        bt_harness_fail(__FILE__, __LINE__, "leaf hold process never appeared under mid %lu", mid);
    }
    BT_CHECK(pid_alive(mid));
    BT_CHECK(pid_alive(leaf));
    taskkill(info.dwProcessId, 0);
    int both_gone = 0;
    deadline = GetTickCount64() + CEILING_WAIT_GONE_MS;
    while (GetTickCount64() <= deadline) {
        if (!pid_alive(mid) && !pid_alive(leaf)) {
            both_gone = 1;
            break;
        }
        Sleep(CEILING_POLL_MS * 2);
    }
    if (!both_gone) {
        taskkill(info.dwProcessId, 1);
        taskkill(mid, 1);
    }
    unsigned long code = 0;
    int rc = wait_process(info.hProcess, CEILING_WAIT_EXIT_MS, &code);
    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK(both_gone);
}
