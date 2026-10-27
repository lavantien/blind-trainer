#ifndef BT_APP_TEST_COMMON_H
#define BT_APP_TEST_COMMON_H

#include "common/bt_harness.h"
#include "common/paths.h"

#include <direct.h>
#include <io.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define TA_PATH_MAX 1024

static void ta_mkdir_p(const char *path) {
    char buf[TA_PATH_MAX];
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

static void ta_remove_tree(const char *path) {
    char pattern[TA_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", path);
    struct _finddata_t entry;
    intptr_t handle = _findfirst(pattern, &entry);
    if (handle != -1) {
        do {
            if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
                continue;
            }
            char child[TA_PATH_MAX];
            snprintf(child, sizeof child, "%s/%s", path, entry.name);
            if (entry.attrib & _A_SUBDIR) {
                ta_remove_tree(child);
            } else {
                remove(child);
            }
        } while (_findnext(handle, &entry) == 0);
        _findclose(handle);
    }
    _rmdir(path);
}

static unsigned ta_dir_counter(void) {
    static unsigned counter = 0;
    counter += 1;
    return counter;
}

static const char *ta_temp_dir(const char *suite, const char *tag) {
    static char dir[TA_PATH_MAX];
    const char *base = getenv("TEMP");
    snprintf(dir, sizeof dir, "%s/bt-app-%s-tests/%s-%d-%u", base ? base : ".", suite, tag,
             _getpid(), ta_dir_counter());
    ta_remove_tree(dir);
    ta_mkdir_p(dir);
    return dir;
}

static void ta_write_file(const char *path, const char *text) {
    char parent[TA_PATH_MAX];
    snprintf(parent, sizeof parent, "%s", path);
    char *slash = strrchr(parent, '/');
    char *back = strrchr(parent, '\\');
    char *cut = back > slash ? back : slash;
    if (cut != NULL) {
        *cut = '\0';
        ta_mkdir_p(parent);
    }
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        BT_ABORT("cannot write %s", path);
    }
    fputs(text, file);
    fclose(file);
}

#endif
