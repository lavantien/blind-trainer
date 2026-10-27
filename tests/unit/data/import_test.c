#include "common/bt_harness.h"
#include "common/strbuf.h"
#include "data/import.h"
#include "data/paths.h"
#include "data/repo_algs.h"
#include "data/repo_settings.h"
#include "data/repo_solves.h"
#include "data/repo_srs.h"
#include "unit/data/test_util.h"

#include <stdio.h>
#include <string.h>

static unsigned dir_counter;

static const char *temp_root(const char *tag) {
    static char dir[BT_TU_PATH_MAX];
    const char *base = getenv("TEMP");
    snprintf(dir, sizeof dir, "%s/bt-data-import-tests/%s-%d-%u", base ? base : ".", tag, _getpid(),
             dir_counter++);
    tu_remove_tree(dir);
    tu_mkdir_p(dir);
    return dir;
}

static void open_at(bt_db *db, const char *root) {
    char db_path[BT_PATH_MAX];
    bt_data_db_path_at(root, db_path);
    bt_db_error err;
    BT_CHECK(bt_db_open_at(db_path, db, &err) == 0);
}

static void write_at(const char *path, const char *text) {
    tu_write_file(path, text);
}

static void sample_srs_json(bt_strbuf *sb) {
    bt_strbuf_init(sb, 256);
    bt_strbuf_append_str(sb, "{\"cards\":{\"edge/UF\":{\"memory\":null,\"interval_days\":6,"
                             "\"due_day\":10,\"reps\":2,\"lapses\":1,\"reviews\":[{\"ts_days\":4,"
                             "\"quality\":4,\"elapsed_days\":0,\"scheduled_days\":6}]}}}");
}

static const char *SOLVES_JSON =
    "{\"solves\":[{\"started_ms\":5,\"memo_ms\":40000,\"exec_ms\":25000,"
    "\"success\":true}]}";

static void sample_algs_json(bt_strbuf *sb) {
    bt_strbuf_init(sb, 128);
    bt_strbuf_append_str(sb, "{\"entries\":{\"corner/URB\":{\"alg\":\"R U' R'\"}}}");
}

static void seed_all_legacy(const char *root) {
    char path[BT_TU_PATH_MAX];
    bt_data_settings_path_at(root, path);
    write_at(path, "{\"memo_cadence_ms\":640,\"memo_mode\":\"per-orbit\",\"drill_reps\":6}");
    bt_strbuf srs;
    sample_srs_json(&srs);
    bt_data_srs_path_at(root, path);
    write_at(path, srs.data);
    bt_strbuf_free(&srs);
    bt_data_solves_path_at(root, path);
    write_at(path, SOLVES_JSON);
    bt_strbuf algs;
    sample_algs_json(&algs);
    bt_data_user_algs_path_at(root, path);
    write_at(path, algs.data);
    bt_strbuf_free(&algs);
}

static int path_exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    fclose(file);
    return 1;
}

BT_TEST(import, imports_every_legacy_store_and_renames_each_file) {
    const char *root = temp_root("all");
    seed_all_legacy(root);
    bt_db db;
    open_at(&db, root);
    char err[512];
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) == 0);
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    char *settings = NULL;
    bt_db_error db_err;
    BT_CHECK(bt_repo_settings_load_text(&db, BT_REPO_SETTINGS_APP_KEY, &settings, &db_err) == 0);
    BT_CHECK(settings != NULL);
    BT_CHECK_EQ(settings, "{\"memo_cadence_ms\":640,\"memo_mode\":\"per-orbit\",\"drill_reps\":6}");
    free(settings);
    bt_srs_store srs;
    bt_srs_load_error load_err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &srs, &load_err) == 0);
    const bt_card *card = bt_srs_store_get(&srs, "edge/UF");
    BT_CHECK(card != NULL);
    bt_review_entry history[1] = {{4, 4, 0, 6}};
    bt_fsrs_memory expected;
    int has = 0;
    BT_CHECK(bt_replay_sm2(history, 1, &cfg, &has, &expected) == 0);
    BT_CHECK(has == 1 && card->has_memory == 1);
    BT_CHECK(card->memory.stability == expected.stability);
    BT_CHECK(card->memory.difficulty == expected.difficulty);
    BT_CHECK(card->interval_days == 6);
    BT_CHECK(card->due_day == 10);
    bt_srs_store_free(&srs);
    bt_solve_log solves;
    BT_CHECK(bt_solve_log_load(&db, &solves, &db_err) == 0);
    BT_CHECK(solves.count == 1 && solves.items[0].started_ms == 5 && solves.items[0].success == 1);
    bt_solve_log_free(&solves);
    bt_user_algs algs;
    BT_CHECK(bt_repo_algs_load(&db, &algs, &db_err) == 0);
    BT_CHECK_EQ(bt_user_algs_alg_for(&algs, "corner", "URB"), "R U' R'");
    bt_user_algs_free(&algs);
    char files[4][BT_PATH_MAX];
    bt_data_settings_path_at(root, files[0]);
    bt_data_srs_path_at(root, files[1]);
    bt_data_solves_path_at(root, files[2]);
    bt_data_user_algs_path_at(root, files[3]);
    for (int i = 0; i < 4; i++) {
        BT_CHECK(!path_exists(files[i]));
        char migrated[BT_TU_PATH_MAX];
        bt_import_migrated_path(files[i], migrated, BT_TU_PATH_MAX);
        BT_CHECK(path_exists(migrated));
    }
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(import, legacy_ease_json_imports_and_memory_derives_from_reviews) {
    const char *root = temp_root("legacy-ease");
    char path[BT_TU_PATH_MAX];
    bt_data_srs_path_at(root, path);
    write_at(path, "{\"cards\":{\"edge/UF\":{\"ease\":2.5,\"interval_days\":6,"
                   "\"due_day\":10,\"reps\":2,\"lapses\":1,\"reviews\":[{\"ts_days\":4,"
                   "\"quality\":4,\"elapsed_days\":0,\"scheduled_days\":6}]}}}");
    bt_db db;
    open_at(&db, root);
    char err[512];
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) == 0);
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_srs_store srs;
    bt_srs_load_error load_err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &srs, &load_err) == 0);
    const bt_card *card = bt_srs_store_get(&srs, "edge/UF");
    BT_CHECK(card != NULL);
    bt_fsrs_memory expected;
    int has = 0;
    BT_CHECK(bt_replay_sm2(card->reviews, card->review_count, &cfg, &has, &expected) == 0);
    BT_CHECK(has == 1 && card->has_memory == 1);
    BT_CHECK(card->memory.stability == expected.stability);
    BT_CHECK(card->interval_days == 6);
    BT_CHECK(card->due_day == 10);
    bt_srs_store_free(&srs);
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(import, partial_legacy_only_imports_what_exists) {
    const char *root = temp_root("partial");
    char path[BT_TU_PATH_MAX];
    bt_strbuf algs;
    sample_algs_json(&algs);
    bt_data_user_algs_path_at(root, path);
    write_at(path, algs.data);
    bt_strbuf_free(&algs);
    bt_db db;
    open_at(&db, root);
    char err[512];
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) == 0);
    bt_user_algs loaded;
    bt_db_error db_err;
    BT_CHECK(bt_repo_algs_load(&db, &loaded, &db_err) == 0);
    BT_CHECK_EQ(bt_user_algs_alg_for(&loaded, "corner", "URB"), "R U' R'");
    bt_user_algs_free(&loaded);
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_srs_store srs;
    bt_srs_load_error load_err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &srs, &load_err) == 0);
    BT_CHECK(bt_srs_store_len(&srs) == 0);
    bt_srs_store_free(&srs);
    bt_solve_log solves;
    BT_CHECK(bt_solve_log_load(&db, &solves, &db_err) == 0);
    BT_CHECK(solves.count == 0);
    bt_solve_log_free(&solves);
    bt_data_user_algs_path_at(root, path);
    char migrated[BT_TU_PATH_MAX];
    bt_import_migrated_path(path, migrated, BT_TU_PATH_MAX);
    BT_CHECK(path_exists(migrated));
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(import, skips_import_when_the_db_already_holds_data) {
    const char *root = temp_root("skip");
    seed_all_legacy(root);
    bt_db db;
    open_at(&db, root);
    bt_db_error db_err;
    BT_CHECK(bt_repo_settings_save(&db, BT_REPO_SETTINGS_APP_KEY, "existing", &db_err) == 0);
    char err[512];
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) == 0);
    char *settings = NULL;
    BT_CHECK(bt_repo_settings_load_text(&db, BT_REPO_SETTINGS_APP_KEY, &settings, &db_err) == 0);
    BT_CHECK(settings != NULL && strcmp(settings, "existing") == 0);
    free(settings);
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_srs_store srs;
    bt_srs_load_error load_err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &srs, &load_err) == 0);
    BT_CHECK(bt_srs_store_len(&srs) == 0);
    bt_srs_store_free(&srs);
    char path[BT_TU_PATH_MAX];
    bt_data_settings_path_at(root, path);
    BT_CHECK(!path_exists(path));
    char migrated[BT_TU_PATH_MAX];
    bt_import_migrated_path(path, migrated, BT_TU_PATH_MAX);
    BT_CHECK(path_exists(migrated));
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(import, a_mid_loop_rename_failure_names_the_file_and_says_the_import_committed) {
    const char *root = temp_root("rename-lock");
    seed_all_legacy(root);
    char srs_path[BT_TU_PATH_MAX];
    bt_data_srs_path_at(root, srs_path);
    char blocked[BT_TU_PATH_MAX];
    bt_import_migrated_path(srs_path, blocked, BT_TU_PATH_MAX);
    tu_mkdir_p(blocked);
    bt_db db;
    open_at(&db, root);
    char err[512];
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "srs.json") != NULL);
    BT_CHECK(strstr(err, "already committed") != NULL);
    char path[BT_TU_PATH_MAX];
    bt_data_settings_path_at(root, path);
    char migrated[BT_TU_PATH_MAX];
    bt_import_migrated_path(path, migrated, BT_TU_PATH_MAX);
    BT_CHECK(path_exists(migrated));
    bt_data_solves_path_at(root, path);
    BT_CHECK(path_exists(path));
    bt_data_user_algs_path_at(root, path);
    BT_CHECK(path_exists(path));
    int fresh = 1;
    bt_db_error db_err;
    BT_CHECK(bt_db_is_fresh(&db, &fresh, &db_err) == 0);
    BT_CHECK(fresh == 0);
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(import, a_later_run_finishes_the_residue_renames_and_fails_loud_when_blocked) {
    const char *root = temp_root("residue");
    seed_all_legacy(root);
    char srs_path[BT_TU_PATH_MAX];
    bt_data_srs_path_at(root, srs_path);
    char blocked[BT_TU_PATH_MAX];
    bt_import_migrated_path(srs_path, blocked, BT_TU_PATH_MAX);
    tu_mkdir_p(blocked);
    bt_db db;
    open_at(&db, root);
    char err[512];
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) != 0);
    _rmdir(blocked);
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) == 0);
    char paths[4][BT_TU_PATH_MAX];
    bt_data_settings_path_at(root, paths[0]);
    bt_data_srs_path_at(root, paths[1]);
    bt_data_solves_path_at(root, paths[2]);
    bt_data_user_algs_path_at(root, paths[3]);
    for (int i = 0; i < 4; i++) {
        char migrated[BT_TU_PATH_MAX];
        bt_import_migrated_path(paths[i], migrated, BT_TU_PATH_MAX);
        BT_CHECK(path_exists(migrated));
    }
    char restored[BT_TU_PATH_MAX];
    bt_import_migrated_path(paths[3], restored, BT_TU_PATH_MAX);
    BT_CHECK(rename(restored, paths[3]) == 0);
    tu_mkdir_p(restored);
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "user-algs.json") != NULL);
    BT_CHECK(strstr(err, "already committed") != NULL);
    BT_CHECK(path_exists(paths[3]));
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(import, corrupt_legacy_json_fails_closed_naming_the_file) {
    const char *root = temp_root("corrupt");
    char path[BT_TU_PATH_MAX];
    bt_data_srs_path_at(root, path);
    write_at(path, "{not json");
    bt_db db;
    open_at(&db, root);
    char err[512];
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "srs.json") != NULL);
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(import, corrupt_legacy_settings_fails_closed_naming_the_file) {
    const char *root = temp_root("corrupt-settings");
    char path[BT_TU_PATH_MAX];
    bt_data_settings_path_at(root, path);
    write_at(path, "{not json");
    bt_db db;
    open_at(&db, root);
    char err[512];
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "settings.json") != NULL);
    int fresh = 0;
    bt_db_error db_err;
    BT_CHECK(bt_db_is_fresh(&db, &fresh, &db_err) == 0);
    BT_CHECK(fresh == 1);
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(import, failed_import_leaves_the_db_fresh_for_the_next_run) {
    const char *root = temp_root("atomic");
    char settings_path[BT_TU_PATH_MAX];
    bt_data_settings_path_at(root, settings_path);
    write_at(settings_path, "{\"memo_cadence_ms\":1,\"memo_mode\":\"images\",\"drill_reps\":1}");
    char solves_path[BT_TU_PATH_MAX];
    bt_data_solves_path_at(root, solves_path);
    write_at(solves_path, "{not json");
    bt_db db;
    open_at(&db, root);
    char err[512];
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) != 0);
    int fresh = 0;
    bt_db_error db_err;
    BT_CHECK(bt_db_is_fresh(&db, &fresh, &db_err) == 0);
    BT_CHECK(fresh == 1);
    BT_CHECK(path_exists(settings_path));
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(import, no_legacy_files_is_a_no_op) {
    const char *root = temp_root("noop");
    bt_db db;
    open_at(&db, root);
    char err[512];
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) == 0);
    int fresh = 0;
    bt_db_error db_err;
    BT_CHECK(bt_db_is_fresh(&db, &fresh, &db_err) == 0);
    BT_CHECK(fresh == 1);
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(import, imported_solves_match_the_legacy_json_exactly) {
    const char *root = temp_root("roundtrip");
    char path[BT_TU_PATH_MAX];
    bt_data_solves_path_at(root, path);
    write_at(path, "{\"solves\":[{\"started_ms\":-9,\"memo_ms\":0,"
                   "\"exec_ms\":2305843009213693951,\"success\":false},"
                   "{\"started_ms\":77,\"memo_ms\":12345,\"exec_ms\":67890,"
                   "\"success\":true}]}");
    bt_db db;
    open_at(&db, root);
    char err[512];
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) == 0);
    bt_solve_log loaded;
    bt_db_error db_err;
    BT_CHECK(bt_solve_log_load(&db, &loaded, &db_err) == 0);
    BT_CHECK(loaded.count == 2);
    BT_CHECK(loaded.items[0].started_ms == -9);
    BT_CHECK(loaded.items[0].memo_ms == 0);
    BT_CHECK(loaded.items[0].exec_ms == 9223372036854775807LL / 4);
    BT_CHECK(loaded.items[0].success == 0);
    BT_CHECK(loaded.items[1].started_ms == 77);
    BT_CHECK(loaded.items[1].memo_ms == 12345);
    BT_CHECK(loaded.items[1].exec_ms == 67890);
    BT_CHECK(loaded.items[1].success == 1);
    bt_solve_log_free(&loaded);
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(import, a_reissued_legacy_file_replaces_its_migrated_marker) {
    const char *root = temp_root("reissue");
    char path[BT_TU_PATH_MAX];
    bt_data_settings_path_at(root, path);
    write_at(path, "{\"memo_cadence_ms\":640,\"memo_mode\":\"per-orbit\",\"drill_reps\":6}");
    char migrated[BT_TU_PATH_MAX];
    bt_import_migrated_path(path, migrated, BT_TU_PATH_MAX);
    bt_db db;
    open_at(&db, root);
    char err[512];
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) == 0);
    write_at(path, "{\"memo_cadence_ms\":1,\"memo_mode\":\"images\",\"drill_reps\":1}");
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) == 0);
    char *settings = NULL;
    bt_db_error db_err;
    BT_CHECK(bt_repo_settings_load_text(&db, BT_REPO_SETTINGS_APP_KEY, &settings, &db_err) == 0);
    BT_CHECK(settings != NULL && strstr(settings, "640") != NULL);
    free(settings);
    BT_CHECK(!path_exists(path));
    BT_CHECK(path_exists(migrated));
    bt_db_close(&db);
    tu_remove_tree(root);
}
