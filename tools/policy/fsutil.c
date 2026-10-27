#include "policy/fsutil.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

#include "common/bt_trace.h"

static int strvec_cmp(const void *left, const void *right) {
    const char *const *a = left;
    const char *const *b = right;
    return strcmp(*a, *b);
}

void policy_strvec_push(policy_strvec *vec, char *item) {
    if (vec->count == vec->cap) {
        vec->cap = vec->cap == 0 ? 16 : vec->cap * 2;
        char **grown = realloc(vec->items, vec->cap * sizeof *grown);
        if (grown == NULL) {
            BT_TRACE_FAIL("policy.fsutil", "path growth failed at %zu", vec->cap);
            fputs("bt-policy: out of memory\n", stderr);
            exit(2);
        }
        vec->items = grown;
    }
    vec->items[vec->count++] = item;
}

void policy_strvec_sort(policy_strvec *vec) {
    if (vec->count > 1) {
        qsort(vec->items, vec->count, sizeof *vec->items, strvec_cmp);
    }
}

void policy_strvec_free(policy_strvec *vec) {
    for (size_t at = 0; at < vec->count; at++) {
        free(vec->items[at]);
    }
    free(vec->items);
    vec->items = NULL;
    vec->count = 0;
    vec->cap = 0;
}

int policy_path_is_dir(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && (info.st_mode & S_IFMT) == S_IFDIR;
}

int policy_path_is_file(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && (info.st_mode & S_IFMT) == S_IFREG;
}

int policy_path_is_reparse_or_link(const char *path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    struct stat info;
    return lstat(path, &info) == 0 && S_ISLNK(info.st_mode);
#endif
}

int policy_join_checked(char out[BT_PATH_MAX], const char *base, const char *leaf, char *err,
                        size_t err_len) {
    int wrote = snprintf(out, BT_PATH_MAX, "%s/%s", base, leaf);
    if (wrote < 0 || (size_t)wrote >= BT_PATH_MAX) {
        snprintf(err, err_len, "path too long under %s", base);
        BT_TRACE_FAIL("policy.fsutil", "path too long under %s", base);
        return -1;
    }
    return 0;
}

int policy_list_dir(const char *dir, policy_strvec *names, char *err, size_t err_len) {
    policy_strvec raw = {0};
#ifdef _WIN32
    char pattern[BT_PATH_MAX];
    int wrote = snprintf(pattern, sizeof pattern, "%s/*", dir);
    if (wrote < 0 || (size_t)wrote >= sizeof pattern) {
        snprintf(err, err_len, "path too long under %s", dir);
        BT_TRACE_FAIL("policy.fsutil", "pattern too long under %s", dir);
        return -1;
    }
    WIN32_FIND_DATAA entry;
    HANDLE handle = FindFirstFileA(pattern, &entry);
    if (handle == INVALID_HANDLE_VALUE) {
        return 0;
    }
    do {
        if (strcmp(entry.cFileName, ".") == 0 || strcmp(entry.cFileName, "..") == 0) {
            continue;
        }
        policy_strvec_push(&raw, policy_xstrdup(entry.cFileName));
    } while (FindNextFileA(handle, &entry) != 0);
    FindClose(handle);
#else
    DIR *handle = opendir(dir);
    if (handle == NULL) {
        return 0;
    }
    struct dirent *entry;
    while ((entry = readdir(handle)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        policy_strvec_push(&raw, policy_xstrdup(entry->d_name));
    }
    closedir(handle);
#endif
    policy_strvec_sort(&raw);
    for (size_t at = 0; at < raw.count; at++) {
        policy_strvec_push(names, raw.items[at]);
    }
    free(raw.items);
    return 0;
}

void policy_rm_rf(const char *path) {
    if (!policy_path_is_dir(path)) {
        remove(path);
        return;
    }
    policy_strvec names = {0};
    char scratch_err[POLICY_ERR_MAX];
    if (policy_list_dir(path, &names, scratch_err, sizeof scratch_err) != 0) {
        return;
    }
    for (size_t at = 0; at < names.count; at++) {
        char child[BT_PATH_MAX];
        if (policy_join_checked(child, path, names.items[at], scratch_err, sizeof scratch_err) !=
            0) {
            break;
        }
        policy_rm_rf(child);
        free(names.items[at]);
    }
    free(names.items);
#ifdef _WIN32
    _rmdir(path);
#else
    rmdir(path);
#endif
}
