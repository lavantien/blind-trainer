#include "mutate/mutate.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

int bt_mut_read_file(const char *path, char **out, size_t *out_len) {
    BT_TRACE_ENTER("mutate.read_file");
    *out = NULL;
    *out_len = 0;
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        BT_TRACE_FAIL("mutate.read_file", "cannot open %s", path);
        BT_TRACE_EXIT("mutate.read_file");
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        BT_TRACE_FAIL("mutate.read_file", "seek failed on %s", path);
        BT_TRACE_EXIT("mutate.read_file");
        return -1;
    }
    long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        BT_TRACE_FAIL("mutate.read_file", "tell failed on %s", path);
        BT_TRACE_EXIT("mutate.read_file");
        return -1;
    }
    char *text = malloc((size_t)size + 1);
    if (text == NULL) {
        fclose(file);
        BT_TRACE_FAIL("mutate.read_file", "oom for %s", path);
        BT_TRACE_EXIT("mutate.read_file");
        return -1;
    }
    size_t got = fread(text, 1, (size_t)size, file);
    fclose(file);
    if (got != (size_t)size) {
        free(text);
        BT_TRACE_FAIL("mutate.read_file", "short read on %s", path);
        BT_TRACE_EXIT("mutate.read_file");
        return -1;
    }
    text[size] = '\0';
    *out = text;
    *out_len = (size_t)size;
    BT_TRACE_EXIT("mutate.read_file");
    return 0;
}

int bt_mut_write_file(const char *path, const char *data, size_t len) {
    BT_TRACE_ENTER("mutate.write_file");
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        BT_TRACE_FAIL("mutate.write_file", "cannot open %s", path);
        BT_TRACE_EXIT("mutate.write_file");
        return -1;
    }
    size_t wrote = fwrite(data, 1, len, file);
    int close_rc = fclose(file);
    if (wrote != len || close_rc != 0) {
        BT_TRACE_FAIL("mutate.write_file", "short write on %s", path);
        BT_TRACE_EXIT("mutate.write_file");
        return -1;
    }
    BT_TRACE_EXIT("mutate.write_file");
    return 0;
}

int bt_mut_paths_push(bt_mut_paths *paths, const char *text) {
    BT_TRACE_ENTER("mutate.paths_push");
    if (paths->count == paths->cap) {
        size_t want = paths->cap == 0 ? 16 : paths->cap * 2;
        char **grown = realloc(paths->items, want * sizeof(char *));
        if (grown == NULL) {
            BT_TRACE_FAIL("mutate.paths_push", "oom");
            BT_TRACE_EXIT("mutate.paths_push");
            return -1;
        }
        paths->items = grown;
        paths->cap = want;
    }
    size_t len = strlen(text);
    char *copy = malloc(len + 1);
    if (copy == NULL) {
        BT_TRACE_FAIL("mutate.paths_push", "oom");
        BT_TRACE_EXIT("mutate.paths_push");
        return -1;
    }
    memcpy(copy, text, len + 1);
    paths->items[paths->count++] = copy;
    BT_TRACE_EXIT("mutate.paths_push");
    return 0;
}

void bt_mut_paths_free(bt_mut_paths *paths) {
    BT_TRACE_ENTER("mutate.paths_free");
    for (size_t i = 0; i < paths->count; i++) {
        free(paths->items[i]);
    }
    free(paths->items);
    paths->items = NULL;
    paths->count = 0;
    paths->cap = 0;
    BT_TRACE_EXIT("mutate.paths_free");
}

#ifdef _WIN32

static wchar_t *to_wide(const char *path) {
    size_t len = strlen(path);
    wchar_t *wide = malloc((len + 1) * sizeof(wchar_t));
    if (wide == NULL) {
        return NULL;
    }
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, (int)(len + 1));
    return wide;
}

int bt_mut_mkdirs(const char *path) {
    BT_TRACE_ENTER("mutate.mkdirs");
    char copy[BT_MUT_PATH_MAX];
    snprintf(copy, sizeof copy, "%s", path);
    for (char *cursor = copy + 1; *cursor != '\0'; cursor++) {
        if (*cursor == '/' || *cursor == '\\') {
            *cursor = '\0';
            wchar_t *wide = to_wide(copy);
            if (wide != NULL) {
                CreateDirectoryW(wide, NULL);
                free(wide);
            }
            *cursor = '/';
        }
    }
    wchar_t *wide = to_wide(copy);
    if (wide == NULL) {
        BT_TRACE_FAIL("mutate.mkdirs", "oom for %s", path);
        BT_TRACE_EXIT("mutate.mkdirs");
        return -1;
    }
    CreateDirectoryW(wide, NULL);
    free(wide);
    BT_TRACE_EXIT("mutate.mkdirs");
    return 0;
}

static int remove_tree_w(const wchar_t *wide_dir) {
    wchar_t pattern[BT_MUT_PATH_MAX];
    _snwprintf(pattern, BT_MUT_PATH_MAX, L"%s\\*", wide_dir);
    WIN32_FIND_DATAW entry;
    HANDLE find = FindFirstFileW(pattern, &entry);
    if (find == INVALID_HANDLE_VALUE) {
        return -1;
    }
    int rc = 0;
    do {
        if (wcscmp(entry.cFileName, L".") == 0 || wcscmp(entry.cFileName, L"..") == 0) {
            continue;
        }
        wchar_t child[BT_MUT_PATH_MAX];
        _snwprintf(child, BT_MUT_PATH_MAX, L"%s\\%s", wide_dir, entry.cFileName);
        if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            rc = remove_tree_w(child);
        } else if (!DeleteFileW(child)) {
            rc = -1;
        }
    } while (rc == 0 && FindNextFileW(find, &entry));
    FindClose(find);
    if (rc == 0 && !RemoveDirectoryW(wide_dir)) {
        rc = -1;
    }
    return rc;
}

int bt_mut_rmtree(const char *path) {
    BT_TRACE_ENTER("mutate.rmtree");
    wchar_t *wide = to_wide(path);
    if (wide == NULL) {
        BT_TRACE_FAIL("mutate.rmtree", "oom for %s", path);
        BT_TRACE_EXIT("mutate.rmtree");
        return -1;
    }
    int rc = remove_tree_w(wide);
    free(wide);
    if (rc != 0) {
        BT_TRACE_FAIL("mutate.rmtree", "failed on %s", path);
    }
    BT_TRACE_EXIT("mutate.rmtree");
    return rc;
}

int bt_mut_file_creation(const char *path, uint64_t *out) {
    BT_TRACE_ENTER("mutate.file_creation");
    WIN32_FILE_ATTRIBUTE_DATA info;
    wchar_t *wide = to_wide(path);
    if (wide == NULL || !GetFileAttributesExW(wide, GetFileExInfoStandard, &info)) {
        free(wide);
        BT_TRACE_FAIL("mutate.file_creation", "cannot stat %s", path);
        BT_TRACE_EXIT("mutate.file_creation");
        return -1;
    }
    free(wide);
    ULARGE_INTEGER value;
    value.LowPart = info.ftCreationTime.dwLowDateTime;
    value.HighPart = info.ftCreationTime.dwHighDateTime;
    *out = value.QuadPart;
    BT_TRACE_EXIT("mutate.file_creation");
    return 0;
}

int bt_mut_set_file_creation(const char *path, uint64_t value) {
    BT_TRACE_ENTER("mutate.set_file_creation");
    wchar_t *wide = to_wide(path);
    if (wide == NULL) {
        BT_TRACE_FAIL("mutate.set_file_creation", "oom for %s", path);
        BT_TRACE_EXIT("mutate.set_file_creation");
        return -1;
    }
    HANDLE file = CreateFileW(wide, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    free(wide);
    if (file == INVALID_HANDLE_VALUE) {
        BT_TRACE_FAIL("mutate.set_file_creation", "cannot open %s", path);
        BT_TRACE_EXIT("mutate.set_file_creation");
        return -1;
    }
    FILETIME creation;
    creation.dwLowDateTime = (DWORD)(value & 0xFFFFFFFFULL);
    creation.dwHighDateTime = (DWORD)(value >> 32U);
    int rc = SetFileTime(file, &creation, NULL, NULL) ? 0 : -1;
    CloseHandle(file);
    if (rc != 0) {
        BT_TRACE_FAIL("mutate.set_file_creation", "SetFileTime failed on %s", path);
    }
    BT_TRACE_EXIT("mutate.set_file_creation");
    return rc;
}

uint64_t bt_mut_process_start(void) {
    FILETIME creation;
    FILETIME exit_time;
    FILETIME kernel;
    FILETIME user;
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit_time, &kernel, &user)) {
        return 0;
    }
    ULARGE_INTEGER value;
    value.LowPart = creation.dwLowDateTime;
    value.HighPart = creation.dwHighDateTime;
    return value.QuadPart;
}

#else

int bt_mut_mkdirs(const char *path) {
    BT_TRACE_ENTER("mutate.mkdirs");
    char copy[BT_MUT_PATH_MAX];
    snprintf(copy, sizeof copy, "%s", path);
    for (char *cursor = copy + 1; *cursor != '\0'; cursor++) {
        if (*cursor == '/') {
            *cursor = '\0';
            mkdir(copy, 0755);
            *cursor = '/';
        }
    }
    int rc = mkdir(copy, 0755);
    BT_TRACE_EXIT("mutate.mkdirs");
    return rc == 0 || errno == EEXIST ? 0 : -1;
}

int bt_mut_rmtree(const char *path) {
    return -1;
}

int bt_mut_file_creation(const char *path, uint64_t *out) {
    return -1;
}

int bt_mut_set_file_creation(const char *path, uint64_t value) {
    return -1;
}

uint64_t bt_mut_process_start(void) {
    return 0;
}

#endif
