#include "common.h"
#include "unit/data/test_util.h"

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static void td_env_set(const char *name, const char *value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

static int td_dir_exists(const char *path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

static int td_mkdir_one(const char *path) {
    if (td_dir_exists(path)) {
        return 0;
    }
#ifdef _WIN32
    CreateDirectoryA(path, NULL);
#else
    mkdir(path, 0755);
#endif
    return td_dir_exists(path) ? 0 : -1;
}

static int td_mkdirs(const char *path) {
    char copy[BT_PATH_MAX];
    snprintf(copy, sizeof copy, "%s", path);
    for (char *cursor = copy + 1; *cursor != '\0'; cursor++) {
        if (*cursor == '\\' || *cursor == '/') {
            char saved = *cursor;
            *cursor = '\0';
            if (td_mkdir_one(copy) != 0) {
                return -1;
            }
            *cursor = saved;
        }
    }
    return td_mkdir_one(copy);
}

static void td_drive_root(char root[BT_PATH_MAX]) {
#ifdef _WIN32
    char base[BT_PATH_MAX];
    DWORD n = GetTempPathA(BT_PATH_MAX, base);
    if (n == 0 || n >= BT_PATH_MAX || base[1] != ':') {
        BT_ABORT("no temp drive root");
    }
    snprintf(root, BT_PATH_MAX, "%.2s\\", base);
#else
    snprintf(root, BT_PATH_MAX, "/");
#endif
}

static void td_make_layout(char root[BT_PATH_MAX]) {
    char base[BT_PATH_MAX];
#ifdef _WIN32
    DWORD n = GetTempPathA(BT_PATH_MAX, base);
    if (n == 0 || n >= BT_PATH_MAX) {
        BT_ABORT("no temp dir");
    }
#else
    snprintf(base, sizeof base, "%s", "/tmp");
#endif
    snprintf(root, BT_PATH_MAX, "%sbt-tdrg-%ld", base,
#ifdef _WIN32
             (long)_getpid()
#else
             (long)getpid()
#endif
    );
    char dir[BT_PATH_MAX];
    snprintf(dir, sizeof dir, "%s\\config", root);
    if (td_mkdirs(dir) != 0) {
        BT_ABORT("mkdir %s failed", dir);
    }
    snprintf(dir, sizeof dir, "%s\\ref_ground", root);
    if (td_mkdirs(dir) != 0) {
        BT_ABORT("mkdir %s failed", dir);
    }
    snprintf(dir, sizeof dir, "%s\\config\\config.json", root);
    FILE *file = fopen(dir, "w");
    if (file == NULL) {
        BT_ABORT("write %s failed", dir);
    }
    fputs("{}", file);
    fclose(file);
    snprintf(dir, sizeof dir, "%s\\ref_ground\\marker.tsv", root);
    file = fopen(dir, "w");
    if (file == NULL) {
        BT_ABORT("write %s failed", dir);
    }
    fputs("ok\n", file);
    fclose(file);
}

BT_TEST(td_rg, config_root_beats_exe_location) {
    char root[BT_PATH_MAX];
    td_make_layout(root);
    char config[BT_PATH_MAX];
    snprintf(config, sizeof config, "%s\\config\\config.json", root);
    const char *old_config = getenv("BT_CONFIG");
    const char *old_ref = getenv("BT_REF_GROUND");
    char saved_config[BT_PATH_MAX] = "";
    char saved_ref[BT_PATH_MAX] = "";
    if (old_config != NULL) {
        snprintf(saved_config, sizeof saved_config, "%s", old_config);
    }
    if (old_ref != NULL) {
        snprintf(saved_ref, sizeof saved_ref, "%s", old_ref);
    }
    td_env_set("BT_CONFIG", config);
    td_env_set("BT_REF_GROUND", "ref_ground");
    char resolved[BT_PATH_MAX];
    td_ref_ground(resolved);
    char expected[BT_PATH_MAX];
    snprintf(expected, sizeof expected, "%s\\ref_ground", root);
    if (saved_config[0] != '\0') {
        td_env_set("BT_CONFIG", saved_config);
    }
    if (saved_ref[0] != '\0') {
        td_env_set("BT_REF_GROUND", saved_ref);
    }
    BT_CHECK_EQ(resolved, expected);
    tu_remove_tree(root);
}

BT_TEST(td_rg, absolute_ref_ground_passes_through) {
    char root[BT_PATH_MAX];
    td_make_layout(root);
    char absolute[BT_PATH_MAX];
    snprintf(absolute, sizeof absolute, "%s\\ref_ground", root);
    const char *old_ref = getenv("BT_REF_GROUND");
    char saved_ref[BT_PATH_MAX] = "";
    if (old_ref != NULL) {
        snprintf(saved_ref, sizeof saved_ref, "%s", old_ref);
    }
    td_env_set("BT_REF_GROUND", absolute);
    char resolved[BT_PATH_MAX];
    td_ref_ground(resolved);
    if (saved_ref[0] != '\0') {
        td_env_set("BT_REF_GROUND", saved_ref);
    }
    BT_CHECK_EQ(resolved, absolute);
    tu_remove_tree(root);
}

BT_TEST(td_rg, mkdirs_skip_existing_drive_root) {
    char root[BT_PATH_MAX];
    td_drive_root(root);
    BT_CHECK_EQ(td_mkdirs(root), 0);
}
