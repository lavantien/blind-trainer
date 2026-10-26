#ifndef POLICY_TEST_UTIL_H
#define POLICY_TEST_UTIL_H

#include "common/bt_harness.h"

#include <stdio.h>
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

#define POLICY_UTIL_PATH_MAX 512

static void policy_util_mkdir_p(const char *path) {
    char partial[POLICY_UTIL_PATH_MAX];
    snprintf(partial, sizeof partial, "%s", path);
    size_t len = strlen(partial);
    for (size_t at = 1; at < len; at++) {
        if (partial[at] != '/' && partial[at] != '\\') {
            continue;
        }
        char keep = partial[at];
        partial[at] = '\0';
#ifdef _WIN32
        _mkdir(partial);
#else
        mkdir(partial, 0755);
#endif
        partial[at] = keep;
    }
#ifdef _WIN32
    _mkdir(partial);
#else
    mkdir(partial, 0755);
#endif
}

static void policy_util_join(char out[POLICY_UTIL_PATH_MAX], const char *base, const char *leaf) {
    snprintf(out, POLICY_UTIL_PATH_MAX, "%s/%s", base, leaf);
}

static int policy_util_is_dir(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && (info.st_mode & S_IFMT) == S_IFDIR;
}

static void policy_util_rm_rf(const char *path) {
    if (!policy_util_is_dir(path)) {
        remove(path);
        return;
    }
#ifdef _WIN32
    char pattern[POLICY_UTIL_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", path);
    WIN32_FIND_DATAA entry;
    HANDLE handle = FindFirstFileA(pattern, &entry);
    if (handle != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(entry.cFileName, ".") == 0 || strcmp(entry.cFileName, "..") == 0) {
                continue;
            }
            char child[POLICY_UTIL_PATH_MAX];
            policy_util_join(child, path, entry.cFileName);
            policy_util_rm_rf(child);
        } while (FindNextFileA(handle, &entry) != 0);
        FindClose(handle);
    }
    _rmdir(path);
#else
    DIR *dir = opendir(path);
    if (dir != NULL) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            char child[POLICY_UTIL_PATH_MAX];
            policy_util_join(child, path, entry->d_name);
            policy_util_rm_rf(child);
        }
        closedir(dir);
    }
    rmdir(path);
#endif
}

static void policy_util_scratch(const char *tag, char out[POLICY_UTIL_PATH_MAX]) {
    snprintf(out, POLICY_UTIL_PATH_MAX, "build/test-tmp/policy-%s", tag);
    policy_util_rm_rf(out);
    policy_util_mkdir_p(out);
}

static void policy_util_write(const char *path, const char *text) {
    char parent[POLICY_UTIL_PATH_MAX];
    snprintf(parent, sizeof parent, "%s", path);
    char *slash = strrchr(parent, '/');
    if (slash != NULL) {
        *slash = '\0';
        policy_util_mkdir_p(parent);
    }
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "cannot open %s", path);
        return;
    }
    size_t len = strlen(text);
    if (fwrite(text, 1, len, file) != len) {
        bt_harness_fail(__FILE__, __LINE__, "short write on %s", path);
    }
    fclose(file);
}

#endif
