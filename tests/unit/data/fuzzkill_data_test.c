#include "common/bt_harness.h"
#include "data/paths.h"
#include "unit/data/test_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_env(const char *name, const char *value) {
    char entry[256];
    snprintf(entry, sizeof entry, "%s=%s", name, value);
    _putenv(entry);
}

static void remove_env(const char *name) {
    char entry[64];
    snprintf(entry, sizeof entry, "%s=", name);
    _putenv(entry);
}

static int ends_with(const char *text, const char *suffix) {
    size_t len = strlen(text);
    size_t suf = strlen(suffix);
    return len >= suf && strcmp(text + len - suf, suffix) == 0;
}

BT_TEST(fuzzkill_data, path_helpers_are_pinned) {
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    BT_CHECK(root[0] != '\0');
    char out[BT_PATH_MAX];
    bt_data_config_path_from("C:/somewhere/config.json", out);
    BT_CHECK(strcmp(out, "C:/somewhere/config.json") == 0);
    bt_data_config_path_from("rel/config.json", out);
    BT_CHECK(ends_with(out, "rel/config.json"));
    bt_data_config_path_from(NULL, out);
    BT_CHECK(ends_with(out, "config/config.json"));
    bt_json_value *config = bt_json_new_object();
    bt_json_value *paths = bt_json_new_object();
    bt_json_object_set(paths, "ref_ground", bt_json_new_string("rg"));
    bt_json_object_set(config, "paths", paths);
    bt_paths_error err;
    BT_CHECK(bt_data_vendor_root_from("C:/somewhere/config.json", config, out, &err) == 0);
    BT_CHECK(strcmp(out, "C:/somewhere/config.json") == 0);
    BT_CHECK(bt_data_vendor_root_from("", config, out, &err) == 0);
    BT_CHECK(ends_with(out, "rg"));
    bt_json_free(config);
    bt_json_value *loaded = bt_data_load_config();
    BT_CHECK(loaded != NULL);
    bt_json_free(loaded);
    bt_data_user_root_from("C:/users/data", NULL, NULL, out);
    BT_CHECK(strcmp(out, "C:/users/data/blind-trainer") == 0);
    bt_data_user_dir_path_at(out, "srs", out);
    BT_CHECK(ends_with(out, "srs"));
}

BT_TEST(fuzzkill_data, user_algs_round_trip_and_read_json_are_pinned) {
    const char *dir = tu_temp_dir("bt-t14", "algs");
    bt_user_algs algs;
    bt_paths_error err;
    BT_CHECK(bt_user_algs_load_at(&algs, dir, &err) == BT_DATA_READ_MISSING);
    BT_CHECK(bt_user_algs_is_empty(&algs) == 1);
    bt_user_algs_set(&algs, "edge", "UF-FR", "R U R'");
    BT_CHECK(bt_user_algs_is_empty(&algs) == 0);
    BT_CHECK_EQ(bt_user_algs_alg_for(&algs, "edge", "UF-FR"), "R U R'");
    BT_CHECK(bt_user_algs_save_at(&algs, dir, &err) == 0);
    bt_user_algs reloaded;
    bt_data_read_status read = bt_user_algs_load_at(&reloaded, dir, &err);
    BT_CHECK(read == BT_DATA_READ_OK);
    BT_CHECK(bt_user_algs_eq(&algs, &reloaded) == 1);
    bt_user_algs_free(&reloaded);
    BT_CHECK(bt_user_algs_remove(&algs, "edge", "UF-FR") == 1);
    BT_CHECK(bt_user_algs_remove(&algs, "edge", "UF-FR") == 0);
    BT_CHECK(bt_user_algs_is_empty(&algs) == 1);
    bt_user_algs_free(&algs);
    bt_json_value *value = bt_json_new_object();
    bt_json_object_set(value, "k", bt_json_new_number(7));
    char file[BT_TU_PATH_MAX];
    snprintf(file, sizeof file, "%s/round.json", dir);
    BT_CHECK(bt_data_write_json_atomic(file, value, &err) == 0);
    bt_json_free(value);
    bt_json_value *loaded = NULL;
    BT_CHECK(bt_data_read_json(file, &loaded) == BT_DATA_READ_OK);
    bt_json_free(loaded);
    char missing[BT_TU_PATH_MAX];
    snprintf(missing, sizeof missing, "%s/missing.json", dir);
    BT_CHECK(bt_data_read_json(missing, &loaded) == BT_DATA_READ_MISSING);
    bt_user_algs empty;
    bt_user_algs_init(&empty);
    char blocker[BT_TU_PATH_MAX];
    snprintf(blocker, sizeof blocker, "%s/blocker", dir);
    tu_write_file(blocker, "occupied");
    BT_CHECK(bt_user_algs_save_at(&empty, blocker, &err) != 0);
    bt_user_algs_free(&empty);
    tu_remove_tree(dir);
}

BT_TEST(fuzzkill_data, user_algs_load_and_save_use_the_user_root) {
    const char *dir = tu_temp_dir("bt-t14", "appdata");
    const char *stash = getenv("APPDATA");
    set_env("APPDATA", dir);
    bt_user_algs algs;
    bt_user_algs_init(&algs);
    bt_user_algs_set(&algs, "corner", "UF-FR", "R U R'");
    bt_paths_error err;
    BT_CHECK(bt_user_algs_save_at(&algs, dir, &err) == 0);
    bt_user_algs reloaded;
    BT_CHECK(bt_user_algs_load_at(&reloaded, dir, &err) == BT_DATA_READ_OK);
    BT_CHECK_EQ(bt_user_algs_alg_for(&reloaded, "corner", "UF-FR"), "R U R'");
    bt_user_algs_free(&reloaded);
    bt_user_algs_free(&algs);
    if (stash != NULL) {
        set_env("APPDATA", stash);
    } else {
        remove_env("APPDATA");
    }
    tu_remove_tree(dir);
}
