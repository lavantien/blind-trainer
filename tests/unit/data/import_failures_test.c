#include "common/bt_harness.h"
#include "data/db.h"
#include "data/import.h"
#include "data/paths.h"
#include "data/repo_settings.h"
#include "unit/data/test_util.h"

#include <stdio.h>
#include <string.h>

#include "sqlite3.h"

static const char *SETTINGS_JSON =
    "{\"memo_cadence_ms\":640,\"memo_mode\":\"per-orbit\",\"drill_reps\":6}";

static const char *SRS_JSON =
    "{\"cards\":{\"edge/UF\":{\"memory\":null,\"interval_days\":6,\"due_day\":10,\"reps\":2,"
    "\"lapses\":1,\"reviews\":[{\"ts_days\":4,\"quality\":4,\"elapsed_days\":0,"
    "\"scheduled_days\":6}]}}}";

static const char *SOLVES_JSON =
    "{\"solves\":[{\"started_ms\":5,\"memo_ms\":40000,\"exec_ms\":25000,\"success\":true}]}";

static const char *ALGS_JSON = "{\"entries\":{\"corner/URB\":{\"alg\":\"R U' R'\"}}}";

static int deny_all(void *ctx, int code, const char *arg1, const char *arg2, const char *db,
                    const char *trigger) {
    (void)ctx;
    (void)code;
    (void)arg1;
    (void)arg2;
    (void)db;
    (void)trigger;
    return SQLITE_DENY;
}

static const char *deny_table;

static int deny_insert_on_table(void *ctx, int code, const char *arg1, const char *arg2,
                                const char *db, const char *trigger) {
    (void)ctx;
    (void)arg2;
    (void)db;
    (void)trigger;
    if (code == SQLITE_INSERT && arg1 != NULL && strcmp(arg1, deny_table) == 0) {
        return SQLITE_DENY;
    }
    return SQLITE_OK;
}

static void open_at(bt_db *db, const char *root) {
    char db_path[BT_PATH_MAX];
    bt_data_db_path_at(root, db_path);
    bt_db_error err;
    BT_CHECK(bt_db_open_at(db_path, db, &err) == 0);
}

static int path_exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    fclose(file);
    return 1;
}

static void assert_still_fresh(bt_db *db) {
    int fresh = 0;
    bt_db_error err;
    BT_CHECK(bt_db_is_fresh(db, &fresh, &err) == 0);
    BT_CHECK_EQ(fresh, 1);
}

static void assert_settings_absent(bt_db *db) {
    char *settings = NULL;
    bt_db_error err;
    BT_CHECK(bt_repo_settings_load_text(db, BT_REPO_SETTINGS_APP_KEY, &settings, &err) == 0);
    BT_CHECK(settings == NULL);
}

BT_TEST(import_failures, a_directory_in_place_of_the_srs_file_fails_the_read) {
    const char *root = tu_temp_dir("bt-import-fail", "dir-srs");
    char path[BT_TU_PATH_MAX];
    bt_data_settings_path_at(root, path);
    tu_write_file(path, SETTINGS_JSON);
    bt_data_srs_path_at(root, path);
    tu_mkdir_p(path);
    bt_db db;
    open_at(&db, root);
    char err[512];
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "srs.json") != NULL);
    BT_CHECK(strstr(err, "read ") == err);
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(import_failures, a_directory_in_place_of_settings_fails_the_read) {
    const char *root = tu_temp_dir("bt-import-fail", "dir-settings");
    char path[BT_TU_PATH_MAX];
    bt_data_srs_path_at(root, path);
    tu_write_file(path, SRS_JSON);
    bt_data_settings_path_at(root, path);
    tu_mkdir_p(path);
    bt_db db;
    open_at(&db, root);
    char err[512];
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "settings.json") != NULL);
    BT_CHECK(strstr(err, "read ") == err);
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(import_failures, an_authorizer_denial_fails_is_fresh_and_leads_with_the_db_path) {
    const char *root = tu_temp_dir("bt-import-fail", "deny-fresh");
    char path[BT_TU_PATH_MAX];
    bt_data_settings_path_at(root, path);
    tu_write_file(path, SETTINGS_JSON);
    bt_db db;
    open_at(&db, root);
    sqlite3_set_authorizer(bt_db_conn(&db), deny_all, NULL);
    char err[512];
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) != 0);
    sqlite3_set_authorizer(bt_db_conn(&db), NULL, NULL);
    BT_CHECK(strstr(err, "not authorized") != NULL);
    char db_path[BT_PATH_MAX];
    bt_data_db_path_at(root, db_path);
    BT_CHECK(strstr(err, db_path) == err);
    BT_CHECK(path_exists(path));
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(import_failures, an_open_transaction_fails_the_import_begin) {
    const char *root = tu_temp_dir("bt-import-fail", "begin");
    char path[BT_TU_PATH_MAX];
    bt_data_settings_path_at(root, path);
    tu_write_file(path, SETTINGS_JSON);
    bt_db db;
    open_at(&db, root);
    BT_CHECK_EQ(sqlite3_exec(bt_db_conn(&db), "BEGIN", NULL, NULL, NULL), SQLITE_OK);
    char err[512];
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "transaction") != NULL);
    sqlite3_exec(bt_db_conn(&db), "ROLLBACK", NULL, NULL, NULL);
    BT_CHECK(path_exists(path));
    assert_still_fresh(&db);
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(import_failures, an_insert_denial_on_settings_rolls_the_import_back) {
    const char *root = tu_temp_dir("bt-import-fail", "deny-settings");
    char path[BT_TU_PATH_MAX];
    bt_data_settings_path_at(root, path);
    tu_write_file(path, SETTINGS_JSON);
    bt_db db;
    open_at(&db, root);
    deny_table = "settings";
    sqlite3_set_authorizer(bt_db_conn(&db), deny_insert_on_table, NULL);
    char err[512];
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) != 0);
    sqlite3_set_authorizer(bt_db_conn(&db), NULL, NULL);
    BT_CHECK(strstr(err, "not authorized") != NULL);
    BT_CHECK(path_exists(path));
    assert_settings_absent(&db);
    assert_still_fresh(&db);
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(import_failures, an_insert_denial_on_card_rolls_the_srs_import_back) {
    const char *root = tu_temp_dir("bt-import-fail", "deny-srs");
    char path[BT_TU_PATH_MAX];
    bt_data_srs_path_at(root, path);
    tu_write_file(path, SRS_JSON);
    bt_db db;
    open_at(&db, root);
    deny_table = "card";
    sqlite3_set_authorizer(bt_db_conn(&db), deny_insert_on_table, NULL);
    char err[512];
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) != 0);
    sqlite3_set_authorizer(bt_db_conn(&db), NULL, NULL);
    BT_CHECK(strstr(err, "not authorized") != NULL);
    BT_CHECK(path_exists(path));
    assert_still_fresh(&db);
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(import_failures, an_insert_denial_on_solves_rolls_the_import_back) {
    const char *root = tu_temp_dir("bt-import-fail", "deny-solves");
    char path[BT_TU_PATH_MAX];
    bt_data_solves_path_at(root, path);
    tu_write_file(path, SOLVES_JSON);
    bt_db db;
    open_at(&db, root);
    deny_table = "solves";
    sqlite3_set_authorizer(bt_db_conn(&db), deny_insert_on_table, NULL);
    char err[512];
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) != 0);
    sqlite3_set_authorizer(bt_db_conn(&db), NULL, NULL);
    BT_CHECK(strstr(err, "not authorized") != NULL);
    BT_CHECK(path_exists(path));
    assert_still_fresh(&db);
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(import_failures, an_insert_denial_on_user_alg_rolls_the_import_back) {
    const char *root = tu_temp_dir("bt-import-fail", "deny-algs");
    char path[BT_TU_PATH_MAX];
    bt_data_user_algs_path_at(root, path);
    tu_write_file(path, ALGS_JSON);
    bt_db db;
    open_at(&db, root);
    deny_table = "user_alg";
    sqlite3_set_authorizer(bt_db_conn(&db), deny_insert_on_table, NULL);
    char err[512];
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) != 0);
    sqlite3_set_authorizer(bt_db_conn(&db), NULL, NULL);
    BT_CHECK(strstr(err, "not authorized") != NULL);
    BT_CHECK(path_exists(path));
    assert_still_fresh(&db);
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(import_failures, a_held_reader_fails_the_import_commit_and_rolls_back) {
    const char *root = tu_temp_dir("bt-import-fail", "busy-commit");
    char path[BT_TU_PATH_MAX];
    bt_data_settings_path_at(root, path);
    tu_write_file(path, SETTINGS_JSON);
    bt_db db;
    open_at(&db, root);
    char db_path[BT_PATH_MAX];
    bt_data_db_path_at(root, db_path);
    sqlite3 *reader = NULL;
    BT_CHECK_EQ(sqlite3_open(db_path, &reader), SQLITE_OK);
    sqlite3_stmt *stmt = NULL;
    BT_CHECK_EQ(sqlite3_prepare_v2(reader, "SELECT COUNT(*) FROM sqlite_master", -1, &stmt, NULL),
                SQLITE_OK);
    BT_CHECK_EQ(sqlite3_step(stmt), SQLITE_ROW);
    char err[512];
    BT_CHECK(bt_import_run(&db, root, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "commit import") != NULL);
    BT_CHECK(strstr(err, "locked") != NULL);
    sqlite3_finalize(stmt);
    sqlite3_close(reader);
    BT_CHECK(path_exists(path));
    assert_settings_absent(&db);
    assert_still_fresh(&db);
    bt_db_close(&db);
    tu_remove_tree(root);
}
