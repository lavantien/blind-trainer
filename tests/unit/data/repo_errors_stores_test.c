#include "common/bt_harness.h"
#include "common/json.h"
#include "data/db.h"
#include "data/fsrs.h"
#include "data/repo_algs.h"
#include "data/repo_cards.h"
#include "data/repo_seeds.h"
#include "data/repo_solves.h"
#include "data/repo_user.h"
#include "data/seedpack/mod.h"
#include "unit/data/test_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"

static unsigned repo_stores_counter;

static const char *repo_stores_dir(const char *tag) {
    static char dir[BT_TU_PATH_MAX];
    const char *base = getenv("TEMP");
    snprintf(dir, sizeof dir, "%s/bt-repo-errors/%s-%d-%u", base ? base : ".", tag, _getpid(),
             repo_stores_counter++);
    tu_remove_tree(dir);
    tu_mkdir_p(dir);
    return dir;
}

static void raw_exec(bt_db *db, const char *sql) {
    char *err = NULL;
    BT_CHECK(sqlite3_exec(bt_db_conn(db), sql, NULL, NULL, &err) == SQLITE_OK);
}

static void open_latest_schema(const char *tag, const char *schema_sql, bt_db *db, char *path_out,
                               size_t path_len) {
    const char *dir = repo_stores_dir(tag);
    snprintf(path_out, path_len, "%s/custom.db", dir);
    sqlite3 *conn = NULL;
    BT_CHECK(sqlite3_open(path_out, &conn) == SQLITE_OK);
    char *err = NULL;
    if (schema_sql[0] != '\0') {
        BT_CHECK(sqlite3_exec(conn, schema_sql, NULL, NULL, &err) == SQLITE_OK);
    }
    BT_CHECK(sqlite3_exec(conn, "PRAGMA user_version = 3", NULL, NULL, &err) == SQLITE_OK);
    sqlite3_close(conn);
    bt_db_error open_err;
    BT_CHECK(bt_db_open_at(path_out, db, &open_err) == 0);
}

static void open_memory(bt_db *db) {
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(db, &err) == 0);
}

static bt_card plain_card(void) {
    bt_card card;
    bt_card_init(&card, 0);
    card.due_day = 7;
    card.interval_days = 3;
    return card;
}

BT_TEST(repo_errors_stores, algs_split_and_join_key_edges) {
    char stage[BT_PATH_MAX];
    char case_key[BT_PATH_MAX];
    BT_CHECK(bt_repo_algs_split_key("edge", stage, sizeof stage, case_key, sizeof case_key) == 0);
    BT_CHECK(strcmp(stage, "edge") == 0);
    BT_CHECK(case_key[0] == '\0');
    BT_CHECK(bt_repo_algs_split_key("edge/UF", stage, sizeof stage, case_key, sizeof case_key) ==
             0);
    BT_CHECK(strcmp(stage, "edge") == 0);
    BT_CHECK(strcmp(case_key, "UF") == 0);
    char tiny[4];
    BT_CHECK(bt_repo_algs_split_key("edge", tiny, sizeof tiny, case_key, sizeof case_key) != 0);
    BT_CHECK(bt_repo_algs_split_key("edge", tiny, sizeof tiny, tiny, sizeof tiny) != 0);
    BT_CHECK(bt_repo_algs_split_key("edgeless/UF", tiny, sizeof tiny, case_key, sizeof case_key) !=
             0);
    char joined[BT_REPO_ALGS_KEY_MAX];
    BT_CHECK(bt_repo_algs_join_key("edge", "", joined, sizeof joined) == 0);
    BT_CHECK(strcmp(joined, "edge") == 0);
    BT_CHECK(bt_repo_algs_join_key("edge", "UF", joined, sizeof joined) == 0);
    BT_CHECK(strcmp(joined, "edge/UF") == 0);
    BT_CHECK(bt_repo_algs_join_key("edgeless", "UF", tiny, sizeof tiny) != 0);
    BT_CHECK(bt_repo_algs_join_key("edgeless", "", tiny, sizeof tiny) != 0);
}

BT_TEST(repo_errors_stores, algs_save_fails_without_the_table_and_on_an_oversized_key) {
    bt_db db;
    bt_db_error err;
    char path[BT_TU_PATH_MAX];
    open_latest_schema("algs-no-tables", "", &db, path, sizeof path);
    bt_user_algs algs;
    bt_user_algs_init(&algs);
    BT_CHECK(bt_repo_algs_save_tx(&db, &algs, &err) != 0);
    BT_CHECK(strcmp(err.step, "clear user_alg") == 0);
    BT_CHECK(strstr(err.detail, "no such table") != NULL);
    BT_CHECK(bt_repo_algs_load(&db, &algs, &err) != 0);
    BT_CHECK(err.kind == BT_DB_ERR_QUERY);
    BT_CHECK(strcmp(err.step, "select user_alg") == 0);
    bt_user_algs_free(&algs);
    bt_db_close(&db);
    bt_db full;
    open_memory(&full);
    bt_user_algs huge_algs;
    bt_user_algs_init(&huge_algs);
    size_t huge_len = BT_PATH_MAX + 16;
    char *huge = malloc(huge_len + 1);
    memset(huge, 'k', huge_len);
    huge[huge_len] = '\0';
    BT_CHECK(bt_user_algs_set_entry(&huge_algs, huge, "R U R'") == 0);
    BT_CHECK(bt_repo_algs_save_tx(&full, &huge_algs, &err) != 0);
    BT_CHECK(strcmp(err.step, "insert user_alg") == 0);
    bt_user_algs_free(&huge_algs);
    free(huge);
    bt_db_begin(&full, &err);
    bt_user_algs empty;
    bt_user_algs_init(&empty);
    BT_CHECK(bt_repo_algs_save(&full, &empty, &err) != 0);
    BT_CHECK(strcmp(err.step, "begin") == 0);
    bt_db_rollback(&full);
    bt_user_algs_free(&empty);
    bt_db_close(&full);
}

BT_TEST(repo_errors_stores, algs_load_rejects_null_columns) {
    bt_db db;
    bt_db_error err;
    char path[BT_TU_PATH_MAX];
    open_latest_schema("algs-null-column",
                       "CREATE TABLE user_alg(event TEXT, method TEXT, stage TEXT, case_key TEXT, "
                       "alg TEXT)",
                       &db, path, sizeof path);
    raw_exec(&db, "INSERT INTO user_alg(event, method, stage, case_key, alg) VALUES "
                  "('3bld', '3style', 'edge', 'UF', NULL)");
    bt_user_algs algs;
    BT_CHECK(bt_repo_algs_load(&db, &algs, &err) != 0);
    BT_CHECK(err.kind == BT_DB_ERR_QUERY);
    BT_CHECK(strcmp(err.step, "select user_alg") == 0);
    bt_db_close(&db);
}

BT_TEST(repo_errors_stores, case_ref_cmp_orders_by_every_field) {
    bt_case_ref base;
    bt_case_ref_of(&base, "3bld", "3style", "edge", "UF");
    bt_case_ref same;
    bt_case_ref_of(&same, "3bld", "3style", "edge", "UF");
    bt_case_ref other_event;
    bt_case_ref_of(&other_event, "oh", "3style", "edge", "UF");
    bt_case_ref other_method;
    bt_case_ref_of(&other_method, "3bld", "cfop", "edge", "UF");
    bt_case_ref other_stage;
    bt_case_ref_of(&other_stage, "3bld", "3style", "corner", "UF");
    bt_case_ref other_key;
    bt_case_ref_of(&other_key, "3bld", "3style", "edge", "UR");
    BT_CHECK(bt_case_ref_cmp(&base, &same) == 0);
    BT_CHECK(bt_case_ref_cmp(&base, &other_event) < 0);
    BT_CHECK(bt_case_ref_cmp(&other_event, &base) > 0);
    BT_CHECK(bt_case_ref_cmp(&base, &other_method) < 0);
    BT_CHECK(bt_case_ref_cmp(&base, &other_stage) > 0);
    BT_CHECK(bt_case_ref_cmp(&base, &other_key) < 0);
}

BT_TEST(repo_errors_stores, overrides_replace_remove_and_lookup) {
    bt_overrides ov;
    bt_overrides_init(&ov);
    bt_case_ref ref;
    bt_case_ref_of(&ref, "3x3", "cfop", "pll", "T");
    BT_CHECK(bt_overrides_len(&ov) == 0);
    BT_CHECK(bt_overrides_get(&ov, &ref) == NULL);
    BT_CHECK(bt_overrides_contains(&ov, &ref) == 0);
    BT_CHECK(bt_overrides_remove(&ov, &ref) == 0);
    BT_CHECK(bt_overrides_put(&ov, &ref, "R U R'") == 0);
    BT_CHECK(bt_overrides_put(&ov, &ref, "F R U") == 0);
    BT_CHECK(bt_overrides_len(&ov) == 1);
    BT_CHECK(strcmp(bt_overrides_get(&ov, &ref), "F R U") == 0);
    BT_CHECK(bt_overrides_contains(&ov, &ref) == 1);
    BT_CHECK(bt_overrides_remove(&ov, &ref) == 1);
    BT_CHECK(bt_overrides_len(&ov) == 0);
    bt_overrides_free(&ov);
}

BT_TEST(repo_errors_stores, user_upsert_covers_update_and_insert_paths) {
    bt_db db;
    open_memory(&db);
    bt_case_ref ref;
    bt_case_ref_of(&ref, "3x3", "cfop", "pll", "T");
    bt_db_error err;
    BT_CHECK(bt_repo_user_upsert(&db, &ref, "first alg", &err) == 0);
    BT_CHECK(bt_repo_user_upsert(&db, &ref, "second alg", &err) == 0);
    sqlite3_stmt *stmt = NULL;
    BT_CHECK(sqlite3_prepare_v2(bt_db_conn(&db), "SELECT alg FROM user_alg WHERE case_key = 'T'",
                                -1, &stmt, NULL) == SQLITE_OK);
    BT_CHECK(sqlite3_step(stmt) == SQLITE_ROW);
    BT_CHECK_EQ((const char *)sqlite3_column_text(stmt, 0), "second alg");
    sqlite3_finalize(stmt);
    BT_CHECK(bt_repo_user_remove(&db, &ref, &err) == 1);
    BT_CHECK(bt_repo_user_remove(&db, &ref, &err) == 0);
    bt_db_close(&db);
}

BT_TEST(repo_errors_stores, user_upsert_remove_and_load_failures) {
    bt_db db;
    bt_db_error err;
    char path[BT_TU_PATH_MAX];
    open_latest_schema("user-no-tables", "", &db, path, sizeof path);
    bt_case_ref ref;
    bt_case_ref_of(&ref, "3x3", "cfop", "pll", "T");
    BT_CHECK(bt_repo_user_upsert(&db, &ref, "alg", &err) != 0);
    BT_CHECK(strcmp(err.step, "update user_alg") == 0);
    BT_CHECK(strstr(err.detail, "no such table") != NULL);
    BT_CHECK(bt_repo_user_remove(&db, &ref, &err) != 0);
    BT_CHECK(strcmp(err.step, "delete user_alg") == 0);
    bt_overrides ov;
    BT_CHECK(bt_repo_user_load_all(&db, &ov, &err) != 0);
    BT_CHECK(strcmp(err.step, "select user_alg overrides") == 0);
    bt_db_close(&db);
    bt_db narrow;
    open_latest_schema("user-narrow-table",
                       "CREATE TABLE user_alg(event TEXT, method TEXT, stage TEXT, case_key TEXT, "
                       "alg TEXT, updated_at INTEGER)",
                       &narrow, path, sizeof path);
    raw_exec(&narrow, "INSERT INTO user_alg(event, method, stage, case_key, alg, updated_at) "
                      "VALUES ('3x3', 'cfop', 'pll', 'J', 'seed', 1)");
    BT_CHECK(bt_repo_user_upsert(&narrow, &ref, "alg", &err) != 0);
    BT_CHECK(strcmp(err.step, "insert user_alg") == 0);
    BT_CHECK(strstr(err.detail, "created_at") != NULL);
    bt_db_close(&narrow);
    bt_db blocked;
    open_memory(&blocked);
    raw_exec(&blocked, "INSERT INTO user_alg(event, method, stage, case_key, alg, created_at, "
                       "updated_at) VALUES ('3x3', 'cfop', 'pll', 'J', 'seed', 1, 1)");
    raw_exec(&blocked, "CREATE TRIGGER block_insert BEFORE INSERT ON user_alg "
                       "BEGIN SELECT RAISE(ABORT, 'no inserts'); END;"
                       "CREATE TRIGGER block_delete BEFORE DELETE ON user_alg "
                       "BEGIN SELECT RAISE(ABORT, 'no deletes'); END");
    BT_CHECK(bt_repo_user_upsert(&blocked, &ref, "alg", &err) != 0);
    BT_CHECK(strcmp(err.step, "insert user_alg") == 0);
    BT_CHECK(strstr(err.detail, "no inserts") != NULL);
    bt_case_ref present;
    bt_case_ref_of(&present, "3x3", "cfop", "pll", "J");
    BT_CHECK(bt_repo_user_remove(&blocked, &present, &err) != 0);
    BT_CHECK(strcmp(err.step, "delete user_alg") == 0);
    BT_CHECK(strstr(err.detail, "no deletes") != NULL);
    bt_db_close(&blocked);
}

BT_TEST(repo_errors_stores, user_load_all_rejects_null_columns) {
    bt_db db;
    bt_db_error err;
    char path[BT_TU_PATH_MAX];
    open_latest_schema("user-null-column",
                       "CREATE TABLE user_alg(event TEXT, method TEXT, stage TEXT, case_key TEXT, "
                       "alg TEXT)",
                       &db, path, sizeof path);
    raw_exec(&db, "INSERT INTO user_alg(event, method, stage, case_key, alg) VALUES "
                  "('3x3', 'cfop', 'pll', 'J', NULL)");
    bt_overrides ov;
    BT_CHECK(bt_repo_user_load_all(&db, &ov, &err) != 0);
    BT_CHECK(err.kind == BT_DB_ERR_QUERY);
    bt_db_close(&db);
}

BT_TEST(repo_errors_stores, cards_entry_falls_back_and_stores) {
    bt_case_cards cards;
    bt_case_cards_init(&cards);
    bt_case_ref ref;
    bt_case_ref_of(&ref, "skewb", "beginner", "clamp", "L");
    bt_card fallback = plain_card();
    fallback.reps = 4;
    const bt_card *got = bt_case_cards_entry(&cards, &ref, &fallback);
    BT_CHECK(got != NULL);
    BT_CHECK(got->reps == 4);
    BT_CHECK(bt_case_cards_len(&cards) == 1);
    bt_card updated = plain_card();
    updated.reps = 8;
    BT_CHECK(bt_case_cards_put(&cards, &ref, &updated) == 0);
    BT_CHECK(bt_case_cards_len(&cards) == 1);
    BT_CHECK(bt_case_cards_get(&cards, &ref)->reps == 8);
    BT_CHECK(bt_case_cards_contains(&cards, &ref) == 1);
    bt_case_ref absent;
    bt_case_ref_of(&absent, "skewb", "beginner", "clamp", "R");
    BT_CHECK(bt_case_cards_contains(&cards, &absent) == 0);
    BT_CHECK(bt_case_cards_get(&cards, &absent) == NULL);
    bt_case_cards_free(&cards);
    bt_card_free(&fallback);
    bt_card_free(&updated);
}

BT_TEST(repo_errors_stores, cards_load_and_save_failures) {
    bt_db db;
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_srs_load_error load_err;
    char path[BT_TU_PATH_MAX];
    open_latest_schema("cards-no-tables", "", &db, path, sizeof path);
    bt_case_cards cards;
    BT_CHECK(bt_repo_cards_load(&db, &cfg, &cards, &load_err) != 0);
    BT_CHECK(load_err.kind == BT_SRS_LOAD_DB);
    bt_db_close(&db);
    bt_db blocked;
    open_memory(&blocked);
    bt_case_ref ref;
    bt_case_ref_of(&ref, "skewb", "beginner", "clamp", "L");
    bt_card card = plain_card();
    bt_db_error err;
    BT_CHECK(bt_db_begin(&blocked, &err) == 0);
    BT_CHECK(bt_repo_cards_save_card(&blocked, &ref, &card, &err) != 0);
    BT_CHECK(strcmp(err.step, "begin") == 0);
    bt_db_rollback(&blocked);
    bt_db schemaless;
    open_latest_schema("cards-save-no-tables", "", &schemaless, path, sizeof path);
    BT_CHECK(bt_repo_cards_save_card(&schemaless, &ref, &card, &err) != 0);
    BT_CHECK(strcmp(err.step, "clear case card") == 0);
    bt_db_close(&schemaless);
    const char *checked =
        "CREATE TABLE card(id INTEGER PRIMARY KEY, event TEXT, method TEXT, stage TEXT, "
        "case_key TEXT, created_at INTEGER, updated_at INTEGER, CHECK (created_at < 100));"
        "CREATE TABLE srs_state(card_id INTEGER PRIMARY KEY, due_day INTEGER, interval_days "
        "INTEGER, reps INTEGER, lapses INTEGER, last_review_day INTEGER, stability REAL, "
        "difficulty REAL);"
        "CREATE TABLE review_log(id INTEGER PRIMARY KEY, card_id INTEGER, rating INTEGER, "
        "reviewed_day INTEGER, interval_before INTEGER, interval_after INTEGER, elapsed_days "
        "INTEGER)";
    bt_db constrained;
    open_latest_schema("cards-step-fails", checked, &constrained, path, sizeof path);
    BT_CHECK(bt_repo_cards_save_card(&constrained, &ref, &card, &err) != 0);
    BT_CHECK(strcmp(err.step, "insert card") == 0);
    bt_db_close(&constrained);
    bt_card_free(&card);
    bt_db_close(&blocked);
}

BT_TEST(repo_errors_stores, seeds_empty_pack_is_a_no_op) {
    bt_db db;
    open_memory(&db);
    bt_seed_report report;
    bt_seed_error err;
    BT_CHECK(bt_seed_pack_tx(&db, "pack.tsv", NULL, 0, &report, &err) == 0);
    BT_CHECK(report.inserted == 0);
    BT_CHECK(report.verified == 0);
    BT_CHECK(strcmp(report.file, "pack.tsv") == 0);
    bt_db_close(&db);
}

static bt_seed_row seed_row_of(const char *case_key, const char *hash) {
    bt_seed_row row;
    memset(&row, 0, sizeof row);
    snprintf(row.event, sizeof row.event, "%s", "3x3");
    snprintf(row.method, sizeof row.method, "%s", "cfop");
    snprintf(row.stage, sizeof row.stage, "%s", "pll");
    snprintf(row.case_key, sizeof row.case_key, "%s", case_key);
    row.alg = NULL;
    row.front_json = NULL;
    row.source_url = NULL;
    row.source_at = NULL;
    snprintf(row.content_hash, sizeof row.content_hash, "%s", hash);
    return row;
}

BT_TEST(repo_errors_stores, seeds_pack_tx_prepare_failures_without_and_with_narrow_tables) {
    bt_db db;
    bt_seed_report report;
    bt_seed_error err;
    char path[BT_TU_PATH_MAX];
    open_latest_schema("seeds-no-table", "", &db, path, sizeof path);
    bt_seed_row row = seed_row_of("T", "hash-1");
    row.alg = NULL;
    BT_CHECK(bt_seed_pack_tx(&db, "pack.tsv", &row, 1, &report, &err) != 0);
    BT_CHECK(err.kind == BT_SEED_ERR_DB);
    BT_CHECK(strcmp(err.db.step, "select seed_alg") == 0);
    bt_db_close(&db);
    bt_db narrow;
    open_latest_schema("seeds-narrow-table",
                       "CREATE TABLE seed_alg(event TEXT, method TEXT, stage TEXT, case_key TEXT, "
                       "content_hash TEXT)",
                       &narrow, path, sizeof path);
    BT_CHECK(bt_seed_pack_tx(&narrow, "pack.tsv", &row, 1, &report, &err) != 0);
    BT_CHECK(strcmp(err.db.step, "insert seed_alg") == 0);
    bt_db_close(&narrow);
}

static bt_seed_row full_seed_row(const char *case_key, const char *hash) {
    bt_seed_row row = seed_row_of(case_key, hash);
    row.alg = (char *)"R U R' U'";
    row.front_json = (char *)"{}";
    row.source_url = (char *)"https://example.invalid";
    row.source_at = (char *)"2026-01-01";
    return row;
}

BT_TEST(repo_errors_stores, seeds_pack_tx_verifies_drift_and_stale_rows) {
    bt_db db;
    open_memory(&db);
    raw_exec(&db, "INSERT INTO seed_alg(event, method, stage, case_key, alg, front_json, "
                  "source_url, source_at, content_hash) VALUES ('3x3', 'cfop', 'pll', 'T', "
                  "'R U R', '{}', 'u', 'a', 'hash-1')");
    bt_seed_report report;
    bt_seed_error err;
    bt_seed_row same = full_seed_row("T", "hash-1");
    BT_CHECK(bt_seed_pack_tx(&db, "pack.tsv", &same, 1, &report, &err) == 0);
    BT_CHECK(report.verified == 1);
    BT_CHECK(report.inserted == 0);
    bt_seed_row drifted = full_seed_row("T", "hash-2");
    BT_CHECK(bt_seed_pack_tx(&db, "pack.tsv", &drifted, 1, &report, &err) != 0);
    BT_CHECK(err.kind == BT_SEED_ERR_DRIFT);
    BT_CHECK(strcmp(err.stored, "hash-1") == 0);
    BT_CHECK(strcmp(err.packed, "hash-2") == 0);
    bt_seed_row fresh = full_seed_row("J", "hash-3");
    BT_CHECK(bt_seed_pack_tx(&db, "pack.tsv", &fresh, 1, &report, &err) != 0);
    BT_CHECK(err.kind == BT_SEED_ERR_STALE);
    BT_CHECK(strcmp(err.case_key, "T") == 0);
    bt_db_close(&db);
}

BT_TEST(repo_errors_stores, seeds_pack_tx_reports_stale_null_keys_and_insert_step_failures) {
    bt_db db;
    char path[BT_TU_PATH_MAX];
    open_latest_schema("seeds-null-key",
                       "CREATE TABLE seed_alg(event TEXT, method TEXT, stage TEXT, case_key TEXT, "
                       "alg TEXT, front_json TEXT, source_url TEXT, source_at TEXT, "
                       "content_hash TEXT)",
                       &db, path, sizeof path);
    raw_exec(&db, "INSERT INTO seed_alg(event, method, stage, case_key, alg, front_json, "
                  "source_url, source_at, content_hash) VALUES ('3x3', 'cfop', 'pll', NULL, "
                  "'R U R', '{}', 'u', 'a', 'hash-1')");
    bt_seed_report report;
    bt_seed_error err;
    bt_seed_row fresh = full_seed_row("J", "hash-3");
    BT_CHECK(bt_seed_pack_tx(&db, "pack.tsv", &fresh, 1, &report, &err) != 0);
    BT_CHECK(err.kind == BT_SEED_ERR_STALE);
    BT_CHECK(err.case_key[0] == '\0');
    bt_db_close(&db);
    bt_db blocked_db;
    open_memory(&blocked_db);
    raw_exec(&blocked_db, "CREATE TRIGGER block_seed BEFORE INSERT ON seed_alg "
                          "BEGIN SELECT RAISE(ABORT, 'no seed'); END");
    bt_seed_row blocked = full_seed_row("J", "hash-3");
    BT_CHECK(bt_seed_pack_tx(&blocked_db, "pack.tsv", &blocked, 1, &report, &err) != 0);
    BT_CHECK(err.kind == BT_SEED_ERR_DB);
    BT_CHECK(strcmp(err.db.step, "insert seed_alg") == 0);
    bt_db_close(&blocked_db);
}

BT_TEST(repo_errors_stores, seeds_counts_and_load_fail_without_the_table) {
    bt_db db;
    bt_seed_error err;
    char path[BT_TU_PATH_MAX];
    open_latest_schema("seeds-count-missing", "", &db, path, sizeof path);
    long long count = 0;
    BT_CHECK(bt_seed_row_count(&db, &count, &err) != 0);
    BT_CHECK(strcmp(err.db.step, "count seed_alg") == 0);
    bt_seed_rows rows;
    bt_seed_rows_init(&rows);
    BT_CHECK(bt_load_seed_rows(&db, &rows, &err) != 0);
    BT_CHECK(strcmp(err.db.step, "select all seed_alg") == 0);
    bt_seed_rows_free(&rows);
    bt_db_close(&db);
}

BT_TEST(repo_errors_stores, seeds_all_fails_on_a_missing_registry) {
    bt_db db;
    open_memory(&db);
    const char *dir = repo_stores_dir("seeds-no-registry");
    bt_seed_report *reports = NULL;
    size_t count = 0;
    bt_seed_error err;
    BT_CHECK(bt_seed_all(&db, dir, &reports, &count, &err) != 0);
    BT_CHECK(err.kind == BT_SEED_ERR_IO);
    BT_CHECK(strstr(err.path, "registry.json") != NULL);
    char registry_path[BT_TU_PATH_MAX];
    snprintf(registry_path, sizeof registry_path, "%s/registry.json", dir);
    tu_write_file(registry_path, "not json at all");
    bt_seed_error json_err;
    BT_CHECK(bt_seed_all(&db, dir, &reports, &count, &json_err) != 0);
    BT_CHECK(json_err.kind == BT_SEED_ERR_REGISTRY_JSON);
    bt_db_close(&db);
}

BT_TEST(repo_errors_stores, solves_save_tx_fails_at_clear_and_prepare) {
    bt_db db;
    bt_db_error err;
    char path[BT_TU_PATH_MAX];
    open_latest_schema("solves-no-table", "", &db, path, sizeof path);
    bt_solve_log log;
    bt_solve_log_init(&log);
    BT_CHECK(bt_solve_log_save_tx(&db, &log, &err) != 0);
    BT_CHECK(strcmp(err.step, "clear solves") == 0);
    bt_solve_log_free(&log);
    bt_db_close(&db);
    bt_db narrow;
    open_latest_schema("solves-narrow-table", "CREATE TABLE solves(x INTEGER)", &narrow, path,
                       sizeof path);
    bt_solve_log one;
    bt_solve_log_init(&one);
    bt_solve_record record = {1000, 200, 300, 1};
    BT_CHECK(bt_solve_log_push(&one, record) == 0);
    BT_CHECK(bt_solve_log_save_tx(&narrow, &one, &err) != 0);
    BT_CHECK(strcmp(err.step, "insert solves") == 0);
    bt_solve_log_free(&one);
    bt_db_close(&narrow);
}

BT_TEST(repo_errors_stores, solves_save_and_load_failures) {
    char path[BT_TU_PATH_MAX];
    bt_db db;
    open_memory(&db);
    raw_exec(&db, "CREATE TRIGGER block_solve BEFORE INSERT ON solves "
                  "BEGIN SELECT RAISE(ABORT, 'no solves'); END");
    bt_db_error err;
    bt_solve_log log;
    bt_solve_log_init(&log);
    bt_solve_record record = {1000, 200, 300, 1};
    bt_solve_log_push(&log, record);
    BT_CHECK(bt_solve_log_save_tx(&db, &log, &err) != 0);
    BT_CHECK(strcmp(err.step, "insert solves") == 0);
    BT_CHECK(strstr(err.detail, "no solves") != NULL);
    bt_db_rollback(&db);
    BT_CHECK(bt_db_begin(&db, &err) == 0);
    BT_CHECK(bt_solve_log_save(&db, &log, &err) != 0);
    BT_CHECK(strcmp(err.step, "begin") == 0);
    bt_db_rollback(&db);
    bt_solve_log_free(&log);
    bt_db_close(&db);
    bt_db schemaless;
    open_latest_schema("solves-load-missing", "", &schemaless, path, sizeof path);
    bt_solve_log loaded;
    BT_CHECK(bt_solve_log_load(&schemaless, &loaded, &err) != 0);
    BT_CHECK(err.kind == BT_DB_ERR_QUERY);
    BT_CHECK(strcmp(err.step, "select solves") == 0);
    bt_db_close(&schemaless);
}

BT_TEST(repo_errors_stores, solves_from_json_rejects_each_malformed_shape) {
    const char *bad[] = {
        "{}",
        "{\"solves\":{}}",
        "{\"solves\":[{\"memo_ms\":1,\"exec_ms\":1,\"success\":true}]}",
        "{\"solves\":[{\"started_ms\":1,\"memo_ms\":1,\"exec_ms\":1,\"success\":\"yes\"}]}",
        "{\"solves\":[{\"started_ms\":1.5,\"memo_ms\":1,\"exec_ms\":1,\"success\":true}]}",
    };
    for (int i = 0; i < 5; i++) {
        bt_json_value *root = NULL;
        bt_json_error parse_err;
        BT_CHECK(bt_json_parse(bad[i], strlen(bad[i]), &root, &parse_err) == 0);
        bt_solve_log log;
        BT_CHECK(bt_solve_log_from_json(root, &log) != 0);
        bt_json_free(root);
    }
    const char *good = "{\"solves\":[{\"started_ms\":5,\"memo_ms\":2,\"exec_ms\":3,"
                       "\"success\":true}]}";
    bt_json_value *root = NULL;
    bt_json_error parse_err;
    BT_CHECK(bt_json_parse(good, strlen(good), &root, &parse_err) == 0);
    bt_solve_log log;
    BT_CHECK(bt_solve_log_from_json(root, &log) == 0);
    BT_CHECK(log.count == 1);
    BT_CHECK(log.items[0].started_ms == 5);
    BT_CHECK(log.items[0].success == 1);
    bt_json_free(root);
    bt_solve_log twin;
    bt_solve_log_init(&twin);
    bt_solve_record other = {9, 2, 3, 1};
    bt_solve_log_push(&twin, other);
    BT_CHECK(bt_solve_log_eq(&log, &twin) == 0);
    other.started_ms = 5;
    bt_solve_log_free(&twin);
    bt_solve_log_init(&twin);
    bt_solve_log_push(&twin, other);
    other.success = 0;
    bt_solve_log_push(&twin, other);
    BT_CHECK(bt_solve_log_eq(&log, &twin) == 0);
    bt_solve_log_free(&twin);
    bt_solve_log_free(&log);
}
