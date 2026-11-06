#include "common/alloc.h"
#include "common/bt_harness.h"
#include "common/json.h"
#include "data/paths.h"
#include "unit/data/test_util.h"

#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned paths_fail_counter;

static const char *paths_fail_dir(const char *tag) {
    static char dir[BT_TU_PATH_MAX];
    const char *base = getenv("TEMP");
    snprintf(dir, sizeof dir, "%s/bt-paths-failures/%s-%d-%u", base ? base : ".", tag, _getpid(),
             paths_fail_counter++);
    tu_remove_tree(dir);
    tu_mkdir_p(dir);
    return dir;
}

typedef struct {
    char name[32];
    char value[BT_TU_PATH_MAX];
    int was_set;
    char old[BT_TU_PATH_MAX];
} env_guard;

static void env_set(env_guard *guard, const char *name, const char *value) {
    snprintf(guard->name, sizeof guard->name, "%s", name);
    const char *current = getenv(name);
    guard->was_set = current != NULL;
    snprintf(guard->old, sizeof guard->old, "%s", current ? current : "");
    _putenv_s(name, value);
}

static void env_restore(const env_guard *guard) {
    if (guard->was_set) {
        _putenv_s(guard->name, guard->old);
    } else {
        _putenv_s(guard->name, "");
    }
}

BT_TEST(paths_failures, error_string_renders_each_paths_error_kind) {
    bt_paths_error err;
    char text[256];
    memset(&err, 0, sizeof err);
    err.kind = BT_PATHS_ERR_READ;
    snprintf(err.path, sizeof err.path, "%s", "root/a.json");
    snprintf(err.detail, sizeof err.detail, "%s", "Permission denied");
    bt_paths_error_string(&err, text, sizeof text);
    BT_CHECK_EQ(text, "read root/a.json: Permission denied");
    err.kind = BT_PATHS_ERR_WRITE;
    bt_paths_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "write root/a.json") == text);
    err.kind = BT_PATHS_ERR_SERIALIZE;
    bt_paths_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "serialize root/a.json") == text);
    err.kind = BT_PATHS_ERR_DESERIALIZE;
    bt_paths_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "deserialize root/a.json") == text);
    err.kind = BT_PATHS_ERR_MISSING;
    snprintf(err.section, sizeof err.section, "%s", "paths.packs");
    bt_paths_error_string(&err, text, sizeof text);
    BT_CHECK_EQ(text, "config section missing: paths.packs");
}

BT_TEST(paths_failures, roots_go_null_without_a_config_or_a_section) {
    const char *dir = paths_fail_dir("null-roots");
    char missing[BT_TU_PATH_MAX];
    snprintf(missing, sizeof missing, "%s/absent-config.json", dir);
    env_guard guard;
    env_set(&guard, "BT_CONFIG", missing);
    env_guard ground;
    env_set(&ground, "BT_REF_GROUND", "");
    char out[BT_PATH_MAX];
    BT_CHECK(bt_data_vendor_root_or_null(out) == NULL);
    BT_CHECK(bt_data_packs_root_or_null(out) == NULL);
    char bare[BT_TU_PATH_MAX];
    snprintf(bare, sizeof bare, "%s/bare-config.json", dir);
    tu_write_file(bare, "{}");
    env_guard swap;
    env_set(&swap, "BT_CONFIG", bare);
    BT_CHECK(bt_data_vendor_root_or_null(out) == NULL);
    BT_CHECK(bt_data_packs_root_or_null(out) == NULL);
    env_restore(&swap);
    env_restore(&ground);
    env_restore(&guard);
    tu_remove_tree(dir);
}

BT_TEST(paths_failures, db_file_name_falls_back_to_default_and_honors_the_hub) {
    char name[BT_PATH_MAX];
    bt_data_db_file_name(name);
    BT_CHECK_EQ(name, "blind-trainer.db");
    const char *dir = paths_fail_dir("db-name");
    char config[BT_TU_PATH_MAX];
    snprintf(config, sizeof config, "%s/config.json", dir);
    tu_write_file(config, "{\"paths\":{\"db_file\":\"custom-store.db\"}}");
    env_guard guard;
    env_set(&guard, "BT_CONFIG", config);
    bt_data_db_file_name(name);
    BT_CHECK_EQ(name, "custom-store.db");
    char missing[BT_TU_PATH_MAX];
    snprintf(missing, sizeof missing, "%s/absent.json", dir);
    env_set(&guard, "BT_CONFIG", missing);
    bt_data_db_file_name(name);
    BT_CHECK_EQ(name, "blind-trainer.db");
    env_restore(&guard);
    tu_remove_tree(dir);
}

BT_TEST(paths_failures, user_root_and_dir_kinds_cover_the_unknown_kind) {
    char root[BT_PATH_MAX];
    bt_data_user_root(root);
    const char *appdata = getenv("APPDATA");
    BT_CHECK(appdata != NULL && appdata[0] != '\0');
    BT_CHECK(strstr(root, appdata) == root);
    BT_CHECK(strstr(root, "/blind-trainer") != NULL);
    char out[BT_PATH_MAX];
    bt_data_user_dir_path_at(root, "bogus", out);
    BT_CHECK_EQ(out, "");
    bt_data_user_dir_path_at(root, "algs", out);
    BT_CHECK(strstr(out, "/algs") != NULL);
}

BT_TEST(paths_failures, write_json_atomic_reports_each_starvation_step) {
    const char *dir = paths_fail_dir("atomic-oom");
    char path[BT_TU_PATH_MAX];
    snprintf(path, sizeof path, "%s/out.json", dir);
    bt_json_value *root = bt_json_new_object();
    bt_json_value *text = bt_json_new_string("value");
    bt_json_object_set(root, "k", text);
    bt_paths_error err;
    bt_alloc_fail_arm(0);
    int rc = bt_data_write_json_atomic(path, root, &err);
    bt_alloc_fail_disarm();
    BT_CHECK(rc != 0);
    BT_CHECK_EQ(err.kind, BT_PATHS_ERR_SERIALIZE);
    BT_CHECK_EQ(err.detail, "out of memory");
    bt_alloc_fail_arm(1);
    rc = bt_data_write_json_atomic(path, root, &err);
    bt_alloc_fail_disarm();
    BT_CHECK(rc != 0);
    BT_CHECK_EQ(err.kind, BT_PATHS_ERR_SERIALIZE);
    BT_CHECK_EQ(err.detail, "serialization failed");
    bt_json_free(root);
    tu_remove_tree(dir);
}

BT_TEST(paths_failures, user_algs_survive_each_interior_allocation_failure) {
    bt_user_algs algs;
    bt_user_algs_init(&algs);
    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof key, "sheet-%d/pos", 9 - i);
        bt_user_algs_set(&algs, "sheet", key, "R U R'");
    }
    BT_CHECK((int)bt_user_algs_len(&algs) == 10);
    for (size_t ok = 0; ok < 26; ok++) {
        bt_user_algs clone;
        bt_alloc_fail_arm(ok);
        int rc = bt_user_algs_clone(&algs, &clone);
        bt_alloc_fail_disarm();
        if (rc == 0) {
            BT_CHECK(bt_user_algs_eq(&algs, &clone) == 1);
            bt_user_algs_free(&clone);
        }
    }
    bt_user_algs clone;
    BT_CHECK(bt_user_algs_clone(&algs, &clone) == 0);
    BT_CHECK(bt_user_algs_eq(&algs, &clone) == 1);
    bt_user_algs_free(&clone);
    bt_user_algs_free(&algs);
}

BT_TEST(paths_failures, user_algs_set_replaces_an_existing_key) {
    bt_user_algs algs;
    bt_user_algs_init(&algs);
    bt_user_algs_set(&algs, "sheet", "UF", "R U R'");
    bt_user_algs_set(&algs, "sheet", "UF", "M2");
    BT_CHECK((int)bt_user_algs_len(&algs) == 1);
    BT_CHECK_EQ(bt_user_algs_alg_for(&algs, "sheet", "UF"), "M2");
    BT_CHECK(bt_user_algs_remove(&algs, "sheet", "UF") == 1);
    BT_CHECK(bt_user_algs_remove(&algs, "sheet", "UF") == 0);
    BT_CHECK(bt_user_algs_is_empty(&algs) == 1);
    bt_user_algs_free(&algs);
}

BT_TEST(paths_failures, user_algs_save_and_load_report_each_failure_shape) {
    const char *dir = paths_fail_dir("algs-io");
    bt_user_algs algs;
    bt_user_algs_init(&algs);
    bt_user_algs_set(&algs, "sheet", "UF", "R U R'");
    char algs_path[BT_TU_PATH_MAX];
    bt_data_user_algs_path_at(dir, algs_path);
    tu_mkdir_p(algs_path);
    bt_paths_error err;
    BT_CHECK(bt_user_algs_save_at(&algs, dir, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_PATHS_ERR_WRITE);
    _rmdir(algs_path);
    for (size_t ok = 0; ok < 8; ok++) {
        bt_alloc_fail_arm(ok);
        int rc = bt_user_algs_save_at(&algs, dir, &err);
        bt_alloc_fail_disarm();
        if (rc == 0) {
            bt_user_algs loaded;
            BT_CHECK(bt_user_algs_load_at(&loaded, dir, &err) == BT_DATA_READ_OK);
            BT_CHECK_EQ(bt_user_algs_alg_for(&loaded, "sheet", "UF"), "R U R'");
            bt_user_algs_free(&loaded);
        }
    }
    BT_CHECK(bt_user_algs_save_at(&algs, dir, &err) == 0);
    bt_user_algs loaded;
    BT_CHECK(bt_user_algs_load_at(&loaded, dir, &err) == BT_DATA_READ_OK);
    bt_user_algs_free(&loaded);
    tu_write_file(algs_path, "{\"entries\":{\"sheet/UF\":5}}");
    BT_CHECK(bt_user_algs_load_at(&loaded, dir, &err) == BT_DATA_READ_ERROR);
    BT_CHECK_EQ(err.kind, BT_PATHS_ERR_DESERIALIZE);
    tu_remove_tree(algs_path);
    tu_mkdir_p(algs_path);
    BT_CHECK(bt_user_algs_load_at(&loaded, dir, &err) == BT_DATA_READ_ERROR);
    BT_CHECK_EQ(err.kind, BT_PATHS_ERR_DESERIALIZE);
    _rmdir(algs_path);
    bt_user_algs_free(&algs);
    tu_remove_tree(dir);
}
