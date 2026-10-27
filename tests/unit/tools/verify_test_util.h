#ifndef VERIFY_TEST_UTIL_H
#define VERIFY_TEST_UTIL_H

#include "common/bt_harness.h"
#include "common/paths.h"

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

#define TV_PATH_MAX 512
#define TV_TEXT_MAX (1 << 20)

static inline void tv_ref_ground(char out[TV_PATH_MAX]) {
    const char *raw = getenv("BT_REF_GROUND");
    if (raw == NULL || raw[0] == '\0') {
        BT_ABORT("BT_REF_GROUND is unset: run tests through bt.py");
    }
    if (raw[0] == '/' || (raw[1] == ':' && (raw[2] == '\\' || raw[2] == '/'))) {
        snprintf(out, TV_PATH_MAX, "%s", raw);
        return;
    }
    char root[BT_PATH_MAX];
    bt_paths_workspace_root(root);
    snprintf(out, TV_PATH_MAX, "%s\\%s", root, raw);
}

static inline void tv_packs_root(char out[TV_PATH_MAX]) {
    tv_ref_ground(out);
    size_t len = strlen(out);
    snprintf(out + len, TV_PATH_MAX - len, "%s", "\\packs");
}

static inline char *tv_read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        BT_ABORT("cannot open %s", path);
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *buffer = malloc((size_t)size + 1);
    if (buffer == NULL) {
        BT_ABORT("out of memory reading %s", path);
    }
    size_t got = fread(buffer, 1, (size_t)size, file);
    fclose(file);
    buffer[got] = '\0';
    return buffer;
}

static inline char *tv_ref_file(const char *relative) {
    char root[TV_PATH_MAX];
    tv_ref_ground(root);
    char path[TV_PATH_MAX];
    snprintf(path, sizeof path, "%s\\%s", root, relative);
    return tv_read_file(path);
}

static inline void tv_mkdir_p(const char *path) {
    char partial[TV_PATH_MAX];
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

static inline void tv_join(char out[TV_PATH_MAX], const char *base, const char *leaf) {
    snprintf(out, TV_PATH_MAX, "%s/%s", base, leaf);
}

static inline int tv_is_dir(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && (info.st_mode & S_IFMT) == S_IFDIR;
}

static inline void tv_rm_rf(const char *path) {
    if (!tv_is_dir(path)) {
        remove(path);
        return;
    }
#ifdef _WIN32
    char pattern[TV_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", path);
    WIN32_FIND_DATAA entry;
    HANDLE handle = FindFirstFileA(pattern, &entry);
    if (handle != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(entry.cFileName, ".") == 0 || strcmp(entry.cFileName, "..") == 0) {
                continue;
            }
            char child[TV_PATH_MAX];
            tv_join(child, path, entry.cFileName);
            tv_rm_rf(child);
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
            char child[TV_PATH_MAX];
            tv_join(child, path, entry->d_name);
            tv_rm_rf(child);
        }
        closedir(dir);
    }
    rmdir(path);
#endif
}

static inline void tv_scratch(const char *tag, char out[TV_PATH_MAX]) {
    snprintf(out, TV_PATH_MAX, "build/test-tmp/verify-%s", tag);
    tv_rm_rf(out);
    tv_mkdir_p(out);
}

static inline void tv_write_file(const char *path, const char *text) {
    char parent[TV_PATH_MAX];
    snprintf(parent, sizeof parent, "%s", path);
    char *slash = strrchr(parent, '/');
    char *back = strrchr(parent, '\\');
    char *cut = back > slash ? back : slash;
    if (cut != NULL) {
        *cut = '\0';
        tv_mkdir_p(parent);
    }
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        BT_ABORT("cannot open %s", path);
    }
    size_t len = strlen(text);
    if (fwrite(text, 1, len, file) != len) {
        BT_ABORT("short write on %s", path);
    }
    fclose(file);
}

#endif
