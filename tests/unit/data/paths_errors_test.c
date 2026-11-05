#include "common/bt_harness.h"
#include "common/json.h"
#include "data/db.h"
#include "data/notation.h"
#include "data/paths.h"
#include "unit/data/test_util.h"

#include <direct.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static unsigned paths_err_counter;

static const char *paths_err_dir(const char *tag) {
    static char dir[BT_TU_PATH_MAX];
    const char *base = getenv("TEMP");
    snprintf(dir, sizeof dir, "%s/bt-paths-errors/%s-%d-%u", base ? base : ".", tag, _getpid(),
             paths_err_counter++);
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

static char *write_config(const char *tag, const char *text) {
    const char *dir = paths_err_dir(tag);
    static char path[BT_TU_PATH_MAX];
    snprintf(path, sizeof path, "%s/config.json", dir);
    tu_write_file(path, text);
    return path;
}

BT_TEST(paths_errors, error_string_renders_every_kind) {
    struct {
        bt_paths_error_kind kind;
        const char *needle;
    } cases[] = {
        {BT_PATHS_ERR_READ, "read"},
        {BT_PATHS_ERR_WRITE, "write"},
        {BT_PATHS_ERR_SERIALIZE, "serialize"},
        {BT_PATHS_ERR_DESERIALIZE, "deserialize"},
        {BT_PATHS_ERR_MISSING, "config section missing"},
    };
    for (int i = 0; i < 5; i++) {
        bt_paths_error err;
        memset(&err, 0, sizeof err);
        err.kind = cases[i].kind;
        snprintf(err.path, sizeof err.path, "%s", "p.json");
        snprintf(err.detail, sizeof err.detail, "%s", "the detail");
        snprintf(err.section, sizeof err.section, "%s", "paths.section");
        char text[256];
        bt_paths_error_string(&err, text, sizeof text);
        BT_CHECK(strstr(text, cases[i].needle) != NULL);
    }
}

BT_TEST(paths_errors, config_path_honors_the_env_and_falls_back_to_the_hub) {
    char out[BT_PATH_MAX];
    bt_data_config_path_from("", out);
    BT_CHECK(strstr(out, "config/config.json") != NULL);
    bt_data_config_path_from("custom/hub.json", out);
    BT_CHECK(strstr(out, "custom/hub.json") != NULL);
    env_guard guard;
    env_set(&guard, "BT_CONFIG", "env/config.json");
    bt_data_config_path(out);
    BT_CHECK(strstr(out, "env/config.json") != NULL);
    env_restore(&guard);
}

BT_TEST(paths_errors, load_config_fails_on_missing_and_malformed_files) {
    const char *dir = paths_err_dir("config-bad");
    char missing[BT_TU_PATH_MAX];
    snprintf(missing, sizeof missing, "%s/missing.json", dir);
    char malformed[BT_TU_PATH_MAX];
    snprintf(malformed, sizeof malformed, "%s/malformed.json", dir);
    tu_write_file(malformed, "{not json");
    env_guard guard;
    env_set(&guard, "BT_CONFIG", missing);
    BT_CHECK(bt_data_load_config() == NULL);
    env_restore(&guard);
    env_guard second;
    env_set(&second, "BT_CONFIG", malformed);
    BT_CHECK(bt_data_load_config() == NULL);
    env_restore(&second);
}

BT_TEST(paths_errors, vendor_and_packs_roots_resolve_from_env_config_or_fail) {
    bt_json_value *with_paths = NULL;
    bt_json_error parse_err;
    const char *config_text = "{\"paths\":{\"ref_ground\":\"rg\",\"packs\":\"pk\"}}";
    BT_CHECK(bt_json_parse(config_text, strlen(config_text), &with_paths, &parse_err) == 0);
    char out[BT_PATH_MAX];
    bt_paths_error err;
    BT_CHECK(bt_data_vendor_root_from("env-root", with_paths, out, &err) == 0);
    BT_CHECK(strstr(out, "env-root") != NULL);
    BT_CHECK(bt_data_vendor_root_from("", with_paths, out, &err) == 0);
    BT_CHECK(strstr(out, "rg") != NULL);
    BT_CHECK(bt_data_packs_root_from("env-root", with_paths, out, &err) == 0);
    BT_CHECK(strstr(out, "env-root/packs") != NULL);
    BT_CHECK(bt_data_packs_root_from("", with_paths, out, &err) == 0);
    BT_CHECK(strstr(out, "pk") != NULL);
    bt_json_free(with_paths);
    bt_json_value *bare = NULL;
    BT_CHECK(bt_json_parse("{}", 2, &bare, &parse_err) == 0);
    BT_CHECK(bt_data_vendor_root_from("", bare, out, &err) != 0);
    BT_CHECK(err.kind == BT_PATHS_ERR_MISSING);
    BT_CHECK(strcmp(err.section, "paths.ref_ground") == 0);
    BT_CHECK(bt_data_packs_root_from("", bare, out, &err) != 0);
    BT_CHECK(err.kind == BT_PATHS_ERR_MISSING);
    BT_CHECK(strcmp(err.section, "paths.packs") == 0);
    bt_json_value *typed = NULL;
    const char *typed_text = "{\"paths\":{\"ref_ground\":1,\"packs\":[]}}";
    BT_CHECK(bt_json_parse(typed_text, strlen(typed_text), &typed, &parse_err) == 0);
    BT_CHECK(bt_data_vendor_root_from("", typed, out, &err) != 0);
    BT_CHECK(bt_data_packs_root_from("", typed, out, &err) != 0);
    bt_json_free(typed);
    bt_json_free(bare);
}

BT_TEST(paths_errors, root_or_null_follows_the_config_load) {
    char out[BT_PATH_MAX];
    env_guard ref_ground;
    env_set(&ref_ground, "BT_REF_GROUND", "");
    env_guard guard;
    env_set(&guard, "BT_CONFIG", write_config("or-null-missing", "{}"));
    BT_CHECK(bt_data_vendor_root_or_null(out) == NULL);
    BT_CHECK(bt_data_packs_root_or_null(out) == NULL);
    env_restore(&guard);
    env_guard second;
    env_set(&second, "BT_CONFIG",
            write_config("or-null-present", "{\"paths\":{\"ref_ground\":\"rg\",\"packs\":\"pk\"}}"));
    bt_json_value *vendor = bt_data_vendor_root_or_null(out);
    BT_CHECK(vendor != NULL);
    BT_CHECK(strstr(out, "rg") != NULL);
    bt_json_free(vendor);
    bt_json_value *packs = bt_data_packs_root_or_null(out);
    BT_CHECK(packs != NULL);
    BT_CHECK(strstr(out, "pk") != NULL);
    bt_json_free(packs);
    env_restore(&second);
    env_restore(&ref_ground);
}

BT_TEST(paths_errors, user_root_picks_each_base_in_order) {
    char out[BT_PATH_MAX];
    bt_data_user_root_from("appdata-root", NULL, NULL, out);
    BT_CHECK(strstr(out, "appdata-root/blind-trainer") != NULL);
    bt_data_user_root_from("", "xdg-root", NULL, out);
    BT_CHECK(strstr(out, "xdg-root/blind-trainer") != NULL);
    bt_data_user_root_from(NULL, "", "home-root", out);
    BT_CHECK(strstr(out, "home-root/.local/share/blind-trainer") != NULL);
    env_guard tmp;
    env_set(&tmp, "TMP", "tmp-base");
    env_guard temp;
    env_set(&temp, "TEMP", "");
    bt_data_user_root_from(NULL, NULL, NULL, out);
    BT_CHECK(strstr(out, "tmp-base/blind-trainer") != NULL);
    env_restore(&temp);
    env_restore(&tmp);
    env_guard tmp_empty;
    env_set(&tmp_empty, "TMP", "");
    env_guard temp_set;
    env_set(&temp_set, "TEMP", "temp-base");
    bt_data_user_root_from(NULL, NULL, NULL, out);
    BT_CHECK(strstr(out, "temp-base/blind-trainer") != NULL);
    env_restore(&temp_set);
    env_restore(&tmp_empty);
    env_guard tmp_gone;
    env_set(&tmp_gone, "TMP", "");
    env_guard temp_gone;
    env_set(&temp_gone, "TEMP", "");
    bt_data_user_root_from(NULL, NULL, NULL, out);
    BT_CHECK(strstr(out, "/blind-trainer") != NULL && out[0] != '\0');
    env_restore(&temp_gone);
    env_restore(&tmp_gone);
}

BT_TEST(paths_errors, user_dir_paths_cover_every_kind_and_reject_unknown) {
    char out[BT_PATH_MAX];
    const char *kinds[4] = {"algs", "srs", "solves", "settings"};
    for (int i = 0; i < 4; i++) {
        bt_data_user_dir_path_at("root", kinds[i], out);
        char want[96];
        snprintf(want, sizeof want, "root/%s", kinds[i]);
        BT_CHECK(strcmp(out, want) == 0);
    }
    bt_data_user_dir_path_at("root", "nope", out);
    BT_CHECK(out[0] == '\0');
    bt_data_user_algs_path_at("root", out);
    BT_CHECK(strstr(out, "algs/user-algs.json") != NULL);
    bt_data_settings_path_at("root", out);
    BT_CHECK(strstr(out, "settings/settings.json") != NULL);
    bt_data_srs_path_at("root", out);
    BT_CHECK(strstr(out, "srs/srs.json") != NULL);
    bt_data_solves_path_at("root", out);
    BT_CHECK(strstr(out, "solves/solves.json") != NULL);
    bt_data_db_path_at("root", out);
    BT_CHECK(strstr(out, "root/") != NULL);
}

BT_TEST(paths_errors, db_file_name_honors_the_hub_and_falls_back) {
    char out[BT_PATH_MAX];
    env_guard guard;
    env_set(&guard, "BT_CONFIG", write_config("db-name", "{\"paths\":{\"db_file\":\"x.db\"}}"));
    bt_data_db_file_name(out);
    BT_CHECK(strcmp(out, "x.db") == 0);
    env_restore(&guard);
    env_guard second;
    env_set(&second, "BT_CONFIG", write_config("db-name-default", "{\"paths\":{}}"));
    bt_data_db_file_name(out);
    BT_CHECK(strcmp(out, "blind-trainer.db") == 0);
    env_restore(&second);
    env_guard third;
    env_set(&third, "BT_CONFIG", "definitely/not/there/config.json");
    bt_data_db_file_name(out);
    BT_CHECK(strcmp(out, "blind-trainer.db") == 0);
    env_restore(&third);
}

BT_TEST(paths_errors, read_json_reports_missing_and_error_and_ok) {
    const char *dir = paths_err_dir("read-json");
    char missing[BT_TU_PATH_MAX];
    snprintf(missing, sizeof missing, "%s/missing.json", dir);
    char broken[BT_TU_PATH_MAX];
    snprintf(broken, sizeof broken, "%s/broken.json", dir);
    tu_write_file(broken, "[1,");
    char good[BT_TU_PATH_MAX];
    snprintf(good, sizeof good, "%s/good.json", dir);
    tu_write_file(good, "{\"a\":1}");
    bt_json_value *value = NULL;
    BT_CHECK(bt_data_read_json(missing, &value) == BT_DATA_READ_MISSING);
    BT_CHECK(bt_data_read_json(broken, &value) == BT_DATA_READ_ERROR);
    BT_CHECK(bt_data_read_json(good, &value) == BT_DATA_READ_OK);
    BT_CHECK(value != NULL);
    bt_json_free(value);
}

BT_TEST(paths_errors, write_json_atomic_fails_when_the_parent_cannot_exist) {
    const char *dir = paths_err_dir("write-blocked");
    char blocker[BT_TU_PATH_MAX];
    snprintf(blocker, sizeof blocker, "%s/blocker", dir);
    tu_write_file(blocker, "occupied");
    char target[BT_TU_PATH_MAX];
    snprintf(target, sizeof target, "%s/under/x.json", blocker);
    bt_json_value *root = NULL;
    bt_json_error parse_err;
    BT_CHECK(bt_json_parse("{}", 2, &root, &parse_err) == 0);
    bt_paths_error err;
    BT_CHECK(bt_data_write_json_atomic(target, root, &err) != 0);
    BT_CHECK(err.kind == BT_PATHS_ERR_WRITE);
    char text[256];
    bt_paths_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "write") != NULL);
    bt_json_free(root);
}

BT_TEST(paths_errors, user_algs_from_json_rejects_each_bad_shape) {
    const char *bad[] = {
        "{}",
        "{\"entries\":[]}",
        "{\"entries\":{\"k\":1}}",
        "{\"entries\":{\"k\":{\"alg\":1}}}",
    };
    bt_json_error parse_err;
    for (int i = 0; i < 4; i++) {
        bt_json_value *root = NULL;
        BT_CHECK(bt_json_parse(bad[i], strlen(bad[i]), &root, &parse_err) == 0);
        bt_user_algs algs;
        bt_paths_error err;
        BT_CHECK(bt_user_algs_from_json(root, &algs, &err) != 0);
        BT_CHECK(err.kind == BT_PATHS_ERR_DESERIALIZE);
        bt_json_free(root);
    }
    const char *good = "{\"entries\":{\"edge/UF\":{\"alg\":\"R U R'\"}}}";
    bt_json_value *root = NULL;
    BT_CHECK(bt_json_parse(good, strlen(good), &root, &parse_err) == 0);
    bt_user_algs algs;
    bt_paths_error err;
    BT_CHECK(bt_user_algs_from_json(root, &algs, &err) == 0);
    BT_CHECK(bt_user_algs_len(&algs) == 1);
    BT_CHECK(strcmp(bt_user_algs_alg_for(&algs, "edge", "UF"), "R U R'") == 0);
    bt_user_algs_free(&algs);
    bt_json_free(root);
}

BT_TEST(paths_errors, user_algs_store_edit_and_round_trip) {
    bt_user_algs algs;
    bt_user_algs_init(&algs);
    BT_CHECK(bt_user_algs_is_empty(&algs) == 1);
    bt_user_algs_set(&algs, "edge", "UF", "R U R'");
    bt_user_algs_set(&algs, "edge", "UR", "R U' R'");
    BT_CHECK(bt_user_algs_len(&algs) == 2);
    BT_CHECK(strcmp(algs.items[0].key, "edge/UF") == 0);
    bt_user_algs_set(&algs, "edge", "UF", "replaced");
    BT_CHECK(strcmp(bt_user_algs_alg_for(&algs, "edge", "UF"), "replaced") == 0);
    BT_CHECK(bt_user_algs_alg_for(&algs, "edge", "missing") == NULL);
    bt_user_algs clone;
    BT_CHECK(bt_user_algs_clone(&algs, &clone) == 0);
    BT_CHECK(bt_user_algs_eq(&algs, &clone) == 1);
    bt_user_algs_set(&clone, "edge", "UL", "extra");
    BT_CHECK(bt_user_algs_eq(&algs, &clone) == 0);
    bt_user_algs_free(&clone);
    BT_CHECK(bt_user_algs_remove(&algs, "edge", "UF") == 1);
    BT_CHECK(bt_user_algs_remove(&algs, "edge", "UF") == 0);
    BT_CHECK(bt_user_algs_len(&algs) == 1);
    const char *root = paths_err_dir("algs-round-trip");
    bt_paths_error err;
    BT_CHECK(bt_user_algs_save_at(&algs, root, &err) == 0);
    bt_user_algs loaded;
    bt_data_read_status read = bt_user_algs_load_at(&loaded, root, &err);
    BT_CHECK(read == BT_DATA_READ_OK);
    BT_CHECK(bt_user_algs_eq(&algs, &loaded) == 1);
    bt_user_algs_free(&loaded);
    char stored_path[BT_TU_PATH_MAX];
    snprintf(stored_path, sizeof stored_path, "%s/algs/user-algs.json", root);
    tu_write_file(stored_path, "{\"entries\":9}");
    bt_data_read_status bad = bt_user_algs_load_at(&loaded, root, &err);
    BT_CHECK(bad == BT_DATA_READ_ERROR);
    BT_CHECK(err.kind == BT_PATHS_ERR_DESERIALIZE);
    tu_write_file(stored_path, "not json");
    bt_data_read_status broken = bt_user_algs_load_at(&loaded, root, &err);
    BT_CHECK(broken == BT_DATA_READ_ERROR);
    const char *empty_root = paths_err_dir("algs-missing");
    bt_data_read_status missing = bt_user_algs_load_at(&loaded, empty_root, &err);
    BT_CHECK(missing == BT_DATA_READ_MISSING);
    bt_user_algs_free(&algs);
}

BT_TEST(paths_errors, db_open_at_trims_trailing_separators_on_the_path) {
    const char *dir = paths_err_dir("trailing-slash");
    char db_path[BT_TU_PATH_MAX];
    snprintf(db_path, sizeof db_path, "%s/slashed/", dir);
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(db_path, &db, &err) != 0);
    BT_CHECK(err.kind == BT_DB_ERR_OPEN);
    struct stat info;
    char made_dir[BT_TU_PATH_MAX];
    snprintf(made_dir, sizeof made_dir, "%s/slashed", dir);
    BT_CHECK(stat(made_dir, &info) == 0);
}

BT_TEST(paths_errors, embedded_migrations_boot_a_repo_free_tree) {
    const char *already_child = getenv("BT_EMBEDDED_BOOT_CHILD");
    if (already_child != NULL && already_child[0] != '\0') {
        return;
    }
    const char *dir = paths_err_dir("embedded-boot");
    char bin_dir[BT_TU_PATH_MAX];
    snprintf(bin_dir, sizeof bin_dir, "%s/bin", dir);
    tu_mkdir_p(bin_dir);
    char config_dir[BT_TU_PATH_MAX];
    snprintf(config_dir, sizeof config_dir, "%s/config", dir);
    tu_mkdir_p(config_dir);
    char marker[BT_TU_PATH_MAX];
    snprintf(marker, sizeof marker, "%s/config/config.json", dir);
    tu_write_file(marker, "{}");
    char self[BT_TU_PATH_MAX];
    DWORD len = GetModuleFileNameA(NULL, self, sizeof self);
    BT_CHECK(len > 0 && len < sizeof self);
    char copied[BT_TU_PATH_MAX];
    snprintf(copied, sizeof copied, "%s/bin/suite.exe", dir);
    BT_CHECK(CopyFileA(self, copied, FALSE) != 0);
    env_guard guard;
    env_set(&guard, "BT_EMBEDDED_BOOT_CHILD", "1");
    intptr_t rc = _spawnl(_P_WAIT, copied, copied, NULL);
    env_restore(&guard);
    BT_CHECK(rc == 0);
}

BT_TEST(paths_errors, notation_strip_rotations_rejects_bad_arguments_and_overflow) {
    char out[64];
    BT_CHECK(bt_notation_strip_rotations(NULL, out, sizeof out) != 0);
    BT_CHECK(bt_notation_strip_rotations("R U", NULL, sizeof out) != 0);
    BT_CHECK(bt_notation_strip_rotations("R U", out, 0) != 0);
    const char *long_token =
        "R R R R R R R R R R R R R R R R R R R R R R R R R R R R R R R R R R R R R R R";
    BT_CHECK(bt_notation_strip_rotations(long_token, out, sizeof out) != 0);
    BT_CHECK(bt_notation_strip_rotations("R U", out, 2) != 0);
    char tiny[4];
    BT_CHECK(bt_notation_strip_rotations("R UR", tiny, sizeof tiny) != 0);
    BT_CHECK(bt_notation_strip_rotations("x y2 z' R U", out, sizeof out) == 0);
    BT_CHECK(strcmp(out, "R U") == 0);
    BT_CHECK(bt_notation_strip_rotations("   ", out, sizeof out) == 0);
    BT_CHECK(out[0] == '\0');
    BT_CHECK(bt_notation_is_rotation("x") == 1);
    BT_CHECK(bt_notation_is_rotation("y2") == 1);
    BT_CHECK(bt_notation_is_rotation("z'") == 1);
    BT_CHECK(bt_notation_is_rotation("w") == 0);
    BT_CHECK(bt_notation_is_rotation("x3") == 0);
    BT_CHECK(bt_notation_is_rotation("") == 0);
    BT_CHECK(bt_notation_is_rotation(NULL) == 0);
}
