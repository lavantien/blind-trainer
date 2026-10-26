#include "common/bt_harness.h"
#include "data/paths.h"
#include "unit/data/test_util.h"

#include <stdio.h>
#include <string.h>

static void read_dir_names(const char *dir, char names[8][64], size_t *count) {
    *count = 0;
    char pattern[BT_TU_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", dir);
    struct _finddata_t entry;
    intptr_t handle = _findfirst(pattern, &entry);
    if (handle == -1) {
        return;
    }
    do {
        if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
            continue;
        }
        if (*count < 8) {
            snprintf(names[*count], 64, "%s", entry.name);
            (*count)++;
        }
    } while (_findnext(handle, &entry) == 0);
    _findclose(handle);
}

BT_TEST(paths_layer, user_root_prefers_appdata_then_xdg_then_home) {
    char out[BT_PATH_MAX];
    bt_data_user_root_from("C:/Users/u/AppData/Roaming", NULL, NULL, out);
    BT_CHECK(strcmp(out, "C:/Users/u/AppData/Roaming/blind-trainer") == 0);
    bt_data_user_root_from(NULL, "/home/u/.local/share", NULL, out);
    BT_CHECK(strcmp(out, "/home/u/.local/share/blind-trainer") == 0);
    bt_data_user_root_from(NULL, NULL, "/home/u", out);
    BT_CHECK(strcmp(out, "/home/u/.local/share/blind-trainer") == 0);
}

BT_TEST(paths_layer, user_dirs_are_named_subdirectories) {
    char out[BT_PATH_MAX];
    bt_data_user_dir_path_at("/base", "algs", out);
    BT_CHECK(strcmp(out, "/base/algs") == 0);
    bt_data_user_dir_path_at("/base", "srs", out);
    BT_CHECK(strcmp(out, "/base/srs") == 0);
    bt_data_user_dir_path_at("/base", "solves", out);
    BT_CHECK(strcmp(out, "/base/solves") == 0);
    bt_data_user_dir_path_at("/base", "settings", out);
    BT_CHECK(strcmp(out, "/base/settings") == 0);
    bt_data_user_algs_path_at("/base", out);
    BT_CHECK(strcmp(out, "/base/algs/user-algs.json") == 0);
}

BT_TEST(paths_layer, config_path_resolves_env_then_workspace_default) {
    char out[BT_PATH_MAX];
    bt_data_config_path_from("C:/etc/app.json", out);
    BT_CHECK(strcmp(out, "C:/etc/app.json") == 0);
    bt_data_config_path_from("config/config.json", out);
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    char want[BT_PATH_MAX];
    snprintf(want, sizeof want, "%s/config/config.json", root);
    BT_CHECK(strcmp(out, want) == 0);
    bt_data_config_path_from(NULL, out);
    size_t len = strlen(out);
    BT_CHECK(len >= 18);
    BT_CHECK(strcmp(out + len - 18, "config/config.json") == 0);
}

BT_TEST(paths_layer, atomic_write_creates_reads_and_replaces) {
    const char *dir = tu_temp_dir("bt-data-paths", "atomic");
    char target[BT_TU_PATH_MAX];
    snprintf(target, sizeof target, "%s/settings/state.json", dir);
    bt_json_value *first = bt_json_new_object();
    bt_json_object_set(first, "mode", bt_json_new_string("a"));
    bt_paths_error err;
    BT_CHECK(bt_data_write_json_atomic(target, first, &err) == 0);
    bt_json_value *read = NULL;
    BT_CHECK(bt_data_read_json(target, &read) == BT_DATA_READ_OK);
    bt_json_value *mode = bt_json_get(bt_json_get(read, "mode") ? read : read, "mode");
    BT_CHECK(mode != NULL && strcmp(mode->as.string, "a") == 0);
    bt_json_free(read);
    bt_json_object_set(first, "mode", bt_json_new_string("b"));
    BT_CHECK(bt_data_write_json_atomic(target, first, &err) == 0);
    bt_json_free(first);
    read = NULL;
    BT_CHECK(bt_data_read_json(target, &read) == BT_DATA_READ_OK);
    mode = bt_json_get(read, "mode");
    BT_CHECK(mode != NULL && strcmp(mode->as.string, "b") == 0);
    bt_json_free(read);
    char names[8][64];
    size_t count = 0;
    char parent[BT_TU_PATH_MAX];
    snprintf(parent, sizeof parent, "%s/settings", dir);
    read_dir_names(parent, names, &count);
    BT_CHECK(count == 1);
    BT_CHECK_EQ(names[0], "state.json");
    tu_remove_tree(dir);
}

BT_TEST(paths_layer, failed_write_leaves_target_and_directory_untouched) {
    const char *dir = tu_temp_dir("bt-data-paths", "badwrite");
    char target[BT_TU_PATH_MAX];
    snprintf(target, sizeof target, "%s/state.json", dir);
    bt_json_value *value = bt_json_new_object();
    bt_json_object_set(value, "ok", bt_json_new_number(1));
    bt_paths_error err;
    BT_CHECK(bt_data_write_json_atomic(target, value, &err) == 0);
    bt_json_free(value);
    char blocked[BT_TU_PATH_MAX];
    snprintf(blocked, sizeof blocked, "%s/state.json.tmp", dir);
    tu_mkdir_p(blocked);
    value = bt_json_new_object();
    bt_json_object_set(value, "next", bt_json_new_number(2));
    BT_CHECK(bt_data_write_json_atomic(target, value, &err) != 0);
    bt_json_free(value);
    bt_json_value *read = NULL;
    BT_CHECK(bt_data_read_json(target, &read) == BT_DATA_READ_OK);
    bt_json_value *ok = bt_json_get(read, "ok");
    BT_CHECK(ok != NULL && ok->as.number == 1);
    bt_json_free(read);
    tu_remove_tree(dir);
}

BT_TEST(paths_layer, read_json_reports_missing_as_none_and_corrupt_as_error) {
    const char *dir = tu_temp_dir("bt-data-paths", "readjson");
    char target[BT_TU_PATH_MAX];
    snprintf(target, sizeof target, "%s/x.json", dir);
    bt_json_value *value = NULL;
    BT_CHECK(bt_data_read_json(target, &value) == BT_DATA_READ_MISSING);
    tu_write_file(target, "{ not json");
    BT_CHECK(bt_data_read_json(target, &value) == BT_DATA_READ_ERROR);
    tu_remove_tree(dir);
}
