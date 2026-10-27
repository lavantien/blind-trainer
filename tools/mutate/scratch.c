#include "mutate/mutate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"

#ifdef _WIN32
#include <windows.h>
#endif

int bt_mut_scratch_root(const char *config_value, char out[BT_MUT_PATH_MAX]) {
    BT_TRACE_ENTER("mutate.scratch_root");
    if (config_value != NULL && config_value[0] != '\0') {
        snprintf(out, BT_MUT_PATH_MAX, "%s", config_value);
        BT_TRACE_EXIT("mutate.scratch_root");
        return 0;
    }
#ifdef _WIN32
    wchar_t wide[BT_MUT_PATH_MAX];
    DWORD len = GetTempPathW(BT_MUT_PATH_MAX, wide);
    if (len == 0 || len >= BT_MUT_PATH_MAX - 32) {
        snprintf(out, BT_MUT_PATH_MAX, ".");
        BT_TRACE_FAIL("mutate.scratch_root", "GetTempPathW failed");
        BT_TRACE_EXIT("mutate.scratch_root");
        return -1;
    }
    char narrow[BT_MUT_PATH_MAX];
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, narrow, BT_MUT_PATH_MAX, NULL, NULL);
    while (narrow[0] != '\0' && narrow[strlen(narrow) - 1] == '\\') {
        narrow[strlen(narrow) - 1] = '\0';
    }
    snprintf(out, BT_MUT_PATH_MAX, "%s/bt-mutants", narrow);
#else
    const char *tmp = getenv("TMPDIR");
    snprintf(out, BT_MUT_PATH_MAX, "%s/bt-mutants", tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
#endif
    BT_TRACE_EXIT("mutate.scratch_root");
    return 0;
}

#ifdef _WIN32

static int dir_first_file_creation(const char *dir, uint64_t *out) {
    char pattern[BT_MUT_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", dir);
    WIN32_FIND_DATAA entry;
    HANDLE find = FindFirstFileA(pattern, &entry);
    if (find == INVALID_HANDLE_VALUE) {
        return -1;
    }
    int rc = -1;
    do {
        if (strcmp(entry.cFileName, ".") == 0 || strcmp(entry.cFileName, "..") == 0) {
            continue;
        }
        char file_path[BT_MUT_PATH_MAX];
        snprintf(file_path, sizeof file_path, "%s/%s", dir, entry.cFileName);
        if (bt_mut_file_creation(file_path, out) == 0) {
            rc = 0;
            break;
        }
    } while (rc != 0 && FindNextFileA(find, &entry));
    FindClose(find);
    return rc;
}

static int pid_alive(unsigned long long pid) {
    if (pid == 0 || pid > 0xFFFFFFFFULL) {
        return 0;
    }
    if (pid == (unsigned long long)GetCurrentProcessId()) {
        return 1;
    }
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (process == NULL) {
        return 0;
    }
    DWORD code = 0;
    int alive = GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
    CloseHandle(process);
    return alive;
}

static int marker_owner_alive(const char *dir) {
    char marker[BT_MUT_PATH_MAX];
    snprintf(marker, sizeof marker, "%s/marker.txt", dir);
    char *text = NULL;
    size_t len = 0;
    if (bt_mut_read_file(marker, &text, &len) != 0) {
        return 0;
    }
    int alive = 0;
    if (len > 0 && len <= 10) {
        int digits = 1;
        for (size_t i = 0; i < len; i++) {
            if (text[i] < '0' || text[i] > '9') {
                digits = 0;
                break;
            }
        }
        if (digits) {
            alive = pid_alive(strtoull(text, NULL, 10));
        }
    }
    free(text);
    return alive;
}

int bt_mut_scratch_is_orphan(const char *dir, uint64_t live_start) {
    BT_TRACE_ENTER("mutate.scratch_is_orphan");
    if (marker_owner_alive(dir)) {
        BT_TRACE_EXIT("mutate.scratch_is_orphan");
        return 0;
    }
    char marker[BT_MUT_PATH_MAX];
    snprintf(marker, sizeof marker, "%s/marker.txt", dir);
    uint64_t creation = 0;
    if (bt_mut_file_creation(marker, &creation) != 0 &&
        dir_first_file_creation(dir, &creation) != 0 && bt_mut_file_creation(dir, &creation) != 0) {
        BT_TRACE_FAIL("mutate.scratch_is_orphan", "no readable creation time in %s", dir);
        BT_TRACE_EXIT("mutate.scratch_is_orphan");
        return -1;
    }
    int orphan = creation < live_start;
    BT_TRACE_EXIT("mutate.scratch_is_orphan");
    return orphan;
}

static int list_subdirs(const char *root, char names[][64], size_t max, size_t *count) {
    char pattern[BT_MUT_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", root);
    WIN32_FIND_DATAA entry;
    HANDLE find = FindFirstFileA(pattern, &entry);
    if (find == INVALID_HANDLE_VALUE) {
        *count = 0;
        return -1;
    }
    size_t index = 0;
    do {
        if (!(entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            continue;
        }
        if (strcmp(entry.cFileName, ".") == 0 || strcmp(entry.cFileName, "..") == 0) {
            continue;
        }
        if (index < max) {
            snprintf(names[index], 64, "%s", entry.cFileName);
            index++;
        }
    } while (FindNextFileA(find, &entry));
    FindClose(find);
    *count = index;
    return 0;
}

#else

int bt_mut_scratch_is_orphan(const char *dir, uint64_t live_start) {
    return 0;
}

static int list_subdirs(const char *root, char names[][64], size_t max, size_t *count) {
    *count = 0;
    return -1;
}

#endif

size_t bt_mut_scratch_sweep(const char *root, uint64_t live_start) {
    BT_TRACE_ENTER("mutate.scratch_sweep");
    static char names[512][64];
    size_t count = 0;
    size_t reclaimed = 0;
    if (list_subdirs(root, names, 512, &count) != 0) {
        BT_TRACE_EXIT("mutate.scratch_sweep");
        return 0;
    }
    for (size_t i = 0; i < count; i++) {
        char dir[BT_MUT_PATH_MAX];
        snprintf(dir, sizeof dir, "%s/%s", root, names[i]);
        int orphan = bt_mut_scratch_is_orphan(dir, live_start);
        if (orphan == 1 && bt_mut_rmtree(dir) == 0) {
            reclaimed++;
        }
    }
    BT_TRACE_EXIT("mutate.scratch_sweep");
    return reclaimed;
}
