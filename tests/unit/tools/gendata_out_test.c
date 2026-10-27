#include "gendata/out.h"

#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <string.h>

#include "common/bt_harness.h"
#include "common/paths.h"
#include "unit/tools/spawn_util.h"

static void rm_tree(const char *path) {
    char pattern[BT_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", path);
    struct _finddatai64_t entry;
    intptr_t handle = _findfirsti64(pattern, &entry);
    if (handle != -1) {
        do {
            if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
                continue;
            }
            char child[BT_PATH_MAX];
            snprintf(child, sizeof child, "%s/%s", path, entry.name);
            if (entry.attrib & _A_SUBDIR) {
                rm_tree(child);
            } else {
                remove(child);
            }
        } while (_findnexti64(handle, &entry) == 0);
        _findclose(handle);
    }
    _rmdir(path);
}

static void temp_dir(char out[BT_PATH_MAX], const char *tag) {
    const char *base = getenv("TEMP");
    if (base == NULL || base[0] == '\0') {
        base = ".";
    }
    snprintf(out, BT_PATH_MAX, "%s/bt-gendata-out-%s-%lu", base, tag, GetCurrentProcessId());
    rm_tree(out);
}

BT_TEST(gendata_out, write_atomic_creates_parents_and_overwrites) {
    char dir[BT_PATH_MAX];
    temp_dir(dir, "nested");
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/nested/file.json", dir);
    char err[128];
    BT_CHECK_EQ(bt_gd_write_atomic(path, "{\"a\":1}", 7, err, sizeof err), 0);
    BT_CHECK_EQ(bt_gd_write_atomic(path, "{\"a\":2}", 7, err, sizeof err), 0);
    FILE *file = fopen(path, "rb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        char buffer[16] = {0};
        size_t got = fread(buffer, 1, sizeof buffer - 1, file);
        fclose(file);
        BT_CHECK_EQ(got, 7);
        BT_CHECK_EQ(strcmp(buffer, "{\"a\":2}"), 0);
    }
    char nested[BT_PATH_MAX];
    snprintf(nested, sizeof nested, "%s/nested", dir);
    struct _finddatai64_t nested_entry;
    intptr_t handle = _findfirsti64(nested, &nested_entry);
    BT_CHECK(handle != -1);
    if (handle != -1) {
        _findclose(handle);
    }
    rm_tree(dir);
}

BT_TEST(gendata_out, write_atomic_fails_and_cleans_up_when_the_target_is_a_directory) {
    char dir[BT_PATH_MAX];
    temp_dir(dir, "dir");
    char target[BT_PATH_MAX];
    snprintf(target, sizeof target, "%s/file.json", dir);
    bt_test_mkdirs(target);
    char err[128];
    err[0] = '\0';
    BT_CHECK(bt_gd_write_atomic(target, "{}", 2, err, sizeof err) != 0);
    BT_CHECK_EQ(strlen(err) > 0 && strncmp(err, "write ", 6) == 0, 1);
    struct _finddatai64_t entry;
    intptr_t handle = _findfirsti64(dir, &entry);
    BT_CHECK(handle != -1);
    if (handle != -1) {
        _findclose(handle);
    }
    rm_tree(dir);
}

BT_TEST(gendata_out, write_atomic_fails_when_the_parent_exists_as_a_file) {
    char dir[BT_PATH_MAX];
    temp_dir(dir, "file");
    bt_test_mkdirs(dir);
    char blocker[BT_PATH_MAX];
    snprintf(blocker, sizeof blocker, "%s/blocker", dir);
    FILE *file = fopen(blocker, "wb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        fputc('x', file);
        fclose(file);
    }
    char child[BT_PATH_MAX];
    snprintf(child, sizeof child, "%s/child.json", blocker);
    char err[128];
    err[0] = '\0';
    BT_CHECK(bt_gd_write_atomic(child, "{}", 2, err, sizeof err) != 0);
    BT_CHECK_EQ(strlen(err) > 0 && strncmp(err, "write ", 6) == 0, 1);
    FILE *probe = fopen(child, "rb");
    BT_CHECK(probe == NULL);
    if (probe != NULL) {
        fclose(probe);
    }
    rm_tree(dir);
}

BT_TEST(gendata_out, write_atomic_writes_into_an_existing_directory_without_temp_leftovers) {
    char dir[BT_PATH_MAX];
    temp_dir(dir, "flat");
    bt_test_mkdirs(dir);
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/flat.json", dir);
    char err[128];
    BT_CHECK_EQ(bt_gd_write_atomic(path, "payload", 7, err, sizeof err), 0);
    FILE *file = fopen(path, "rb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        char buffer[16] = {0};
        size_t got = fread(buffer, 1, sizeof buffer - 1, file);
        fclose(file);
        BT_CHECK_EQ(got, 7);
        BT_CHECK_EQ(strcmp(buffer, "payload"), 0);
    }
    char pattern[BT_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", dir);
    struct _finddatai64_t entry;
    intptr_t handle = _findfirsti64(pattern, &entry);
    BT_CHECK(handle != -1);
    if (handle != -1) {
        size_t names = 0;
        do {
            if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
                continue;
            }
            BT_CHECK_EQ(strcmp(entry.name, "flat.json"), 0);
            names++;
        } while (_findnexti64(handle, &entry) == 0);
        BT_CHECK_EQ(names, 1u);
        _findclose(handle);
    }
    rm_tree(dir);
}
