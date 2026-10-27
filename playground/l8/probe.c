#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

#define BT_PATH_MAX 1024

static char g_root[BT_PATH_MAX];
static char g_pids[BT_PATH_MAX];
static char g_config[BT_PATH_MAX];

static void step(const char *label) {
    fprintf(stderr, "step %s\n", label);
    fflush(stderr);
}

static void mkdirs(const char *path) {
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

static int env_entry_cmp(const void *left, const void *right) {
    const wchar_t *const *a = left;
    const wchar_t *const *b = right;
    return _wcsicmp(*a, *b);
}

static int env_block_with(const char *bt_config, wchar_t *block, size_t cap) {
    step("env get");
    wchar_t *raw = GetEnvironmentStringsW();
    if (raw == NULL) {
        return -1;
    }
    wchar_t override[BT_PATH_MAX + 16];
    int wrote = swprintf(override, sizeof override / sizeof override[0], L"BT_CONFIG=%hs",
                         bt_config);
    if (wrote <= 0) {
        FreeEnvironmentStringsW(raw);
        return -1;
    }
    step("env count");
    size_t count = 0;
    for (wchar_t *entry = raw; *entry != L'\0'; entry += wcslen(entry) + 1) {
        if (wcsncmp(entry, L"BT_CONFIG=", 10) != 0) {
            count++;
        }
    }
    const wchar_t **entries = calloc(count + 1, sizeof *entries);
    if (entries == NULL) {
        FreeEnvironmentStringsW(raw);
        return -1;
    }
    size_t used = 0;
    for (wchar_t *entry = raw; *entry != L'\0'; entry += wcslen(entry) + 1) {
        if (wcsncmp(entry, L"BT_CONFIG=", 10) != 0) {
            entries[used++] = entry;
        }
    }
    entries[used] = override;
    step("env sort");
    qsort(entries, count + 1, sizeof *entries, env_entry_cmp);
    size_t words = 0;
    for (size_t at = 0; at <= count; at++) {
        size_t len = wcslen(entries[at]) + 1;
        if (words + len + 1 > cap) {
            free(entries);
            FreeEnvironmentStringsW(raw);
            return -1;
        }
        wmemcpy(block + words, entries[at], len);
        words += len;
    }
    block[words] = L'\0';
    free(entries);
    FreeEnvironmentStringsW(raw);
    fprintf(stderr, "env words used: %zu\n", words);
    return 0;
}

int main(void) {
    step("start");
    const char *config_env = getenv("BT_CONFIG");
    snprintf(g_root, sizeof g_root, "%s", config_env != NULL ? config_env : ".");
    char *slash = strrchr(g_root, '\\');
    char *backslash = strrchr(g_root, '/');
    char *cut = slash > backslash ? slash : backslash;
    if (cut != NULL) {
        *cut = '\0';
    }
    slash = strrchr(g_root, '\\');
    backslash = strrchr(g_root, '/');
    cut = slash > backslash ? slash : backslash;
    if (cut != NULL) {
        *cut = '\0';
    }
    step(g_root);
    snprintf(g_pids, sizeof g_pids, "%s/build/test-tmp/l8-probe/pids", g_root);
    snprintf(g_config, sizeof g_config, "%s/build/test-tmp/l8-probe/config.json", g_root);
    mkdirs(g_pids);
    step("dirs ok");
    FILE *file = fopen(g_config, "wb");
    fputs("{\"job\":{\"per_process_bytes\":268435456},\"memguard\":{\"pid_dir\":\"PLACEHOLDER\"}}",
          file);
    fclose(file);
    step("config ok");

    char command[BT_PATH_MAX * 4];
    snprintf(command, sizeof command, "%s\\build\\bin\\jobwrap.exe --anchor gw-stamp -- %s\\build\\bin\\jobwrap.exe --selftest-hold-secs 12",
             g_root, g_root);
    wchar_t wide_command[BT_PATH_MAX * 4];
    MultiByteToWideChar(CP_UTF8, 0, command, -1, wide_command,
                        (int)(sizeof wide_command / sizeof wide_command[0]));
    step("command ok");
    wchar_t env[32768];
    if (env_block_with(g_config, env, 32768) != 0) {
        fprintf(stderr, "env block failed\n");
        return 1;
    }
    step("env ok");
    STARTUPINFOW startup;
    ZeroMemory(&startup, sizeof startup);
    startup.cb = sizeof startup;
    PROCESS_INFORMATION info;
    ZeroMemory(&info, sizeof info);
    wchar_t *raw_env = GetEnvironmentStringsW();
    wchar_t *block_used = NULL;
    fprintf(stderr, "variant (1=my block, 2=raw parent block): ");
    int variant = getchar() - '0';
    if (variant == 0) {
        block_used = NULL;
    } else if (variant == 1) {
        block_used = env;
    } else if (variant == 3) {
        wchar_t minimal[BT_PATH_MAX + 16];
        swprintf(minimal, sizeof minimal / sizeof minimal[0], L"BT_CONFIG=%hs", g_config);
        size_t minimal_len = wcslen(minimal);
        minimal[minimal_len + 1] = L'\0';
        block_used = minimal;
    } else if (variant >= 10) {
        size_t keep = (size_t)variant - 10;
        size_t words = 0;
        size_t used_entries = 0;
        for (wchar_t *entry = raw_env; *entry != L'\0' && used_entries < keep;
             entry += wcslen(entry) + 1) {
            size_t len = wcslen(entry) + 1;
            wmemcpy(env + words, entry, len);
            words += len;
            used_entries++;
        }
        swprintf(env + words, 256, L"BT_CONFIG=%hs", g_config);
        size_t tail = wcslen(env + words) + 1;
        env[words + tail] = L'\0';
        block_used = env;
    } else {
        block_used = raw_env;
    }
    DWORD env_flags = block_used != NULL ? CREATE_UNICODE_ENVIRONMENT : 0;
    if (CreateProcessW(NULL, wide_command, NULL, NULL, FALSE, env_flags, block_used, NULL,
                       &startup,
                       &info) == 0) {
        fprintf(stderr, "CreateProcessW variant %d failed: %lu\n", variant, GetLastError());
        FreeEnvironmentStringsW(raw_env);
        return 1;
    }
    FreeEnvironmentStringsW(raw_env);
    step("spawned");
    char pidfile[BT_PATH_MAX];
    snprintf(pidfile, sizeof pidfile, "%s/gw-stamp-%lu.pid", g_pids, info.dwProcessId);
    ULONGLONG deadline = GetTickCount64() + 10000;
    while (GetTickCount64() <= deadline) {
        DWORD attrs = GetFileAttributesA(pidfile);
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            break;
        }
        Sleep(100);
    }
    step("wait file done");
    file = fopen(pidfile, "rb");
    if (file == NULL) {
        fprintf(stderr, "pidfile missing\n");
        return 1;
    }
    char raw[128] = {0};
    fread(raw, 1, sizeof raw - 1, file);
    fclose(file);
    fprintf(stderr, "pidfile content: [%s]\n", raw);
    unsigned long code = 0;
    WaitForSingleObject(info.hProcess, 60000);
    GetExitCodeProcess(info.hProcess, &code);
    fprintf(stderr, "wrapper exit: %lu\n", code);
    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);
    return 0;
}
