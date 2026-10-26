#ifndef BT_DATA_TEST_UTIL_H
#define BT_DATA_TEST_UTIL_H

#include <direct.h>
#include <io.h>
#include <process.h>
#include <stdio.h>
#include <sys/stat.h>

#define BT_TU_PATH_MAX 1024

static void tu_mkdir_p(const char *path) {
    char buf[BT_TU_PATH_MAX];
    snprintf(buf, sizeof buf, "%s", path);
    for (char *p = buf + (buf[0] && buf[1] == ':' ? 3 : 1); *p; p++) {
        if (*p == '/' || *p == '\\') {
            char save = *p;
            *p = '\0';
            _mkdir(buf);
            *p = save;
        }
    }
    _mkdir(buf);
}

static void tu_remove_tree(const char *path) {
    char pattern[BT_TU_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", path);
    struct _finddata_t entry;
    intptr_t handle = _findfirst(pattern, &entry);
    if (handle != -1) {
        do {
            if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
                continue;
            }
            char child[BT_TU_PATH_MAX];
            snprintf(child, sizeof child, "%s/%s", path, entry.name);
            if (entry.attrib & _A_SUBDIR) {
                tu_remove_tree(child);
            } else {
                remove(child);
            }
        } while (_findnext(handle, &entry) == 0);
        _findclose(handle);
    }
    _rmdir(path);
}

[[maybe_unused]] static const char *tu_temp_dir(const char *prefix, const char *tag) {
    static char dir[BT_TU_PATH_MAX];
    const char *base = getenv("TEMP");
    snprintf(dir, sizeof dir, "%s/%s/%s-%d", base ? base : ".", prefix, tag,
             (int)_getpid());
    tu_remove_tree(dir);
    tu_mkdir_p(dir);
    return dir;
}

[[maybe_unused]] static void tu_write_file(const char *path, const char *text) {
    char parent[BT_TU_PATH_MAX];
    snprintf(parent, sizeof parent, "%s", path);
    char *slash = strrchr(parent, '/');
    if (slash != NULL) {
        *slash = '\0';
        tu_mkdir_p(parent);
    }
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return;
    }
    fputs(text, file);
    fclose(file);
}

#endif
