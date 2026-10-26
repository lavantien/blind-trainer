#include "common/bt_harness.h"
#include "common/strbuf.h"
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

static bt_json_value *config_with_ref_ground(const char *value) {
    bt_json_value *config = bt_json_new_object();
    bt_json_value *paths = bt_json_new_object();
    bt_json_object_set(paths, "ref_ground", bt_json_new_string(value));
    bt_json_object_set(config, "paths", paths);
    return config;
}

static bt_json_value *read_json_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = malloc((size_t)size + 1);
    size_t read = fread(text, 1, (size_t)size, file);
    fclose(file);
    text[read] = '\0';
    bt_json_value *value = NULL;
    bt_json_error jerr;
    if (bt_json_parse(text, read, &value, &jerr) != 0) {
        free(text);
        return NULL;
    }
    free(text);
    return value;
}

static size_t read_dir_names(const char *dir, char names[8][64]) {
    size_t count = 0;
    char pattern[BT_TU_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", dir);
    struct _finddata_t entry;
    intptr_t handle = _findfirst(pattern, &entry);
    if (handle == -1) {
        return 0;
    }
    do {
        if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
            continue;
        }
        if (count < 8) {
            snprintf(names[count], 64, "%s", entry.name);
            count++;
        }
    } while (_findnext(handle, &entry) == 0);
    _findclose(handle);
    return count;
}

BT_TEST(paths_more, vendor_root_env_wins_over_config_pointer) {
    bt_json_value *config = config_with_ref_ground("from-config");
    char out[BT_PATH_MAX];
    bt_paths_error err;
    BT_CHECK(bt_data_vendor_root_from("C:/bt/rg-override", config, out, &err) == 0);
    BT_CHECK(strcmp(out, "C:/bt/rg-override") == 0);
    bt_json_free(config);
}

BT_TEST(paths_more, vendor_root_relative_env_joins_the_workspace) {
    bt_json_value *config = config_with_ref_ground("from-config");
    char out[BT_PATH_MAX];
    bt_paths_error err;
    BT_CHECK(bt_data_vendor_root_from("rel/rg", config, out, &err) == 0);
    BT_CHECK(ends_with(out, "rel/rg"));
    BT_CHECK(out[1] == ':');
    bt_json_free(config);
}

BT_TEST(paths_more, vendor_root_empty_env_falls_back_to_config_pointer) {
    bt_json_value *config = config_with_ref_ground("ref_ground");
    char out[BT_PATH_MAX];
    bt_paths_error err;
    BT_CHECK(bt_data_vendor_root_from("", config, out, &err) == 0);
    BT_CHECK(ends_with(out, "ref_ground"));
    bt_json_free(config);
}

BT_TEST(paths_more, vendor_root_missing_or_non_string_pointer_is_a_missing_section) {
    const char *texts[] = {
        "{}",
        "{\"paths\":{}}",
        "{\"paths\":{\"ref_ground\":7}}",
        "{\"paths\":{\"ref_ground\":null}}",
    };
    for (int i = 0; i < 4; i++) {
        bt_json_error jerr;
        bt_json_value *config = NULL;
        BT_CHECK(bt_json_parse(texts[i], strlen(texts[i]), &config, &jerr) == 0);
        char out[BT_PATH_MAX];
        bt_paths_error err;
        BT_CHECK(bt_data_vendor_root_from(NULL, config, out, &err) != 0);
        char text[256];
        bt_paths_error_string(&err, text, sizeof text);
        BT_CHECK_EQ(text, "config section missing: paths.ref_ground");
        bt_json_free(config);
    }
}

BT_TEST(paths_more, config_path_empty_env_falls_back_to_the_workspace_default) {
    char out[BT_PATH_MAX];
    bt_data_config_path_from("", out);
    BT_CHECK(ends_with(out, "config/config.json"));
    bt_data_config_path_from(NULL, out);
    BT_CHECK(ends_with(out, "config/config.json"));
}

BT_TEST(paths_more, user_root_ignores_empty_env_values_in_precedence_order) {
    char out[BT_PATH_MAX];
    bt_data_user_root_from("", "/xdg/data", NULL, out);
    BT_CHECK(strcmp(out, "/xdg/data/blind-trainer") == 0);
    bt_data_user_root_from("", "", "/home/u", out);
    BT_CHECK(strcmp(out, "/home/u/.local/share/blind-trainer") == 0);
    bt_data_user_root_from("", "", "", out);
    const char *temp = getenv("TEMP");
    char want[BT_PATH_MAX];
    snprintf(want, sizeof want, "%s/blind-trainer", temp ? temp : ".");
    BT_CHECK(strcmp(out, want) == 0);
}

BT_TEST(paths_more, paths_error_display_pins_write_serialize_deserialize) {
    bt_paths_error err;
    memset(&err, 0, sizeof err);
    err.kind = BT_PATHS_ERR_WRITE;
    snprintf(err.path, sizeof err.path, "%s", "out/state.json");
    snprintf(err.detail, sizeof err.detail, "%s", "disk full");
    char text[256];
    bt_paths_error_string(&err, text, sizeof text);
    BT_CHECK_EQ(text, "write out/state.json: disk full");
    err.kind = BT_PATHS_ERR_SERIALIZE;
    bt_paths_error_string(&err, text, sizeof text);
    BT_CHECK(strncmp(text, "serialize out/state.json: ", 26) == 0);
    err.kind = BT_PATHS_ERR_DESERIALIZE;
    bt_paths_error_string(&err, text, sizeof text);
    BT_CHECK(strncmp(text, "deserialize out/state.json: ", 28) == 0);
}

BT_TEST(paths_more, read_json_deserialize_error_carries_the_path) {
    const char *dir = tu_temp_dir("bt-paths", "more");
    char target[BT_TU_PATH_MAX];
    snprintf(target, sizeof target, "%s/broken.json", dir);
    tu_write_file(target, "{ not json");
    bt_json_value *value = NULL;
    BT_CHECK(bt_data_read_json(target, &value) == BT_DATA_READ_ERROR);
    tu_remove_tree(dir);
}

BT_TEST(paths_more, foreign_json_layout_loads_as_user_algs) {
    const char *dir = tu_temp_dir("bt-paths", "foreign");
    char target[BT_TU_PATH_MAX];
    snprintf(target, sizeof target, "%s/algs/user-algs.json", dir);
    tu_write_file(target,
                  "{\"entries\":{\"edge/UF-UB-UL\":{\"alg\":\"M2 U M U2 M' U M2\"},"
                  "\"flips/UF & UB\":{\"alg\":\"M' U M' U M' U2 M U M U M U2\"}}}");
    bt_user_algs algs;
    bt_paths_error err;
    BT_CHECK(bt_user_algs_load_at(&algs, dir, &err) == BT_DATA_READ_OK);
    BT_CHECK_EQ(bt_user_algs_alg_for(&algs, "edge", "UF-UB-UL"),
                "M2 U M U2 M' U M2");
    BT_CHECK_EQ(bt_user_algs_alg_for(&algs, "flips", "UF & UB"),
                "M' U M' U M' U2 M U M U M U2");
    BT_CHECK(bt_user_algs_len(&algs) == 2);
    bt_user_algs_free(&algs);
    tu_remove_tree(dir);
}

BT_TEST(paths_more, sequential_atomic_writes_leave_no_temp_files_behind) {
    const char *dir = tu_temp_dir("bt-paths", "atomic");
    char first[BT_TU_PATH_MAX];
    char second[BT_TU_PATH_MAX];
    snprintf(first, sizeof first, "%s/a/one.json", dir);
    snprintf(second, sizeof second, "%s/a/two.json", dir);
    bt_paths_error err;
    bt_json_value *one = bt_json_new_object();
    bt_json_object_set(one, "n", bt_json_new_number(1));
    BT_CHECK(bt_data_write_json_atomic(first, one, &err) == 0);
    bt_json_free(one);
    bt_json_value *two = bt_json_new_object();
    bt_json_object_set(two, "n", bt_json_new_number(2));
    BT_CHECK(bt_data_write_json_atomic(second, two, &err) == 0);
    bt_json_free(two);
    one = bt_json_new_object();
    bt_json_object_set(one, "n", bt_json_new_number(3));
    BT_CHECK(bt_data_write_json_atomic(first, one, &err) == 0);
    bt_json_free(one);
    bt_json_value *read = NULL;
    BT_CHECK(bt_data_read_json(first, &read) == BT_DATA_READ_OK);
    bt_json_value *n = bt_json_get(read, "n");
    BT_CHECK(n != NULL && n->as.number == 3);
    bt_json_free(read);
    char names[8][64];
    size_t count = 0;
    char parent[BT_TU_PATH_MAX];
    snprintf(parent, sizeof parent, "%s/a", dir);
    count = read_dir_names(parent, names);
    BT_CHECK(count == 2);
    BT_CHECK((strcmp(names[0], "one.json") == 0 && strcmp(names[1], "two.json") == 0) ||
             (strcmp(names[0], "two.json") == 0 && strcmp(names[1], "one.json") == 0));
    tu_remove_tree(dir);
}

BT_TEST(paths_more, db_paths_follow_the_configured_db_file_and_join_the_root) {
    const char *dir = tu_temp_dir("bt-paths", "db");
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    char repo_config[BT_TU_PATH_MAX];
    snprintf(repo_config, sizeof repo_config, "%s/config/config.json", root);
    bt_json_value *config = read_json_file(repo_config);
    BT_CHECK(config != NULL);
    bt_json_value *paths = bt_json_get(config, "paths");
    BT_CHECK(paths != NULL);
    bt_json_object_set(paths, "db_file", bt_json_new_string("custom.db"));
    char fixture[BT_TU_PATH_MAX];
    snprintf(fixture, sizeof fixture, "%s/config.json", dir);
    bt_strbuf sb;
    bt_strbuf_init(&sb, 512);
    BT_CHECK(bt_json_serialize(config, &sb) == 0);
    tu_write_file(fixture, sb.data);
    bt_strbuf_free(&sb);
    bt_json_free(config);
    set_env("BT_CONFIG", fixture);
    char name[BT_PATH_MAX];
    bt_data_db_file_name(name);
    BT_CHECK_EQ(name, "custom.db");
    char db_path[BT_PATH_MAX];
    bt_data_db_path_at("root-dir", db_path);
    BT_CHECK(strcmp(db_path, "root-dir/custom.db") == 0);
    remove_env("BT_CONFIG");
    tu_remove_tree(dir);
}

BT_TEST(paths_more, packs_root_env_override_joins_packs_on_the_absolute_base) {
    const char *dir = tu_temp_dir("bt-paths", "packs");
    char fixture[BT_TU_PATH_MAX];
    snprintf(fixture, sizeof fixture, "%s/config.json", dir);
    tu_write_file(fixture, "{\"paths\":{}}");
    set_env("BT_CONFIG", fixture);
    set_env("BT_REF_GROUND", dir);
    char out[BT_PATH_MAX];
    bt_json_value *config = bt_data_load_config();
    BT_CHECK(config != NULL);
    bt_paths_error err;
    BT_CHECK(bt_data_packs_root_from(getenv("BT_REF_GROUND"), config, out, &err) == 0);
    bt_json_free(config);
    char want[BT_PATH_MAX];
    snprintf(want, sizeof want, "%s/packs", dir);
    BT_CHECK(strcmp(out, want) == 0);
    remove_env("BT_REF_GROUND");
    remove_env("BT_CONFIG");
    tu_remove_tree(dir);
}
