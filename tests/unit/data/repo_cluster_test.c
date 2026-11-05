#include "common/bt_harness.h"
#include "data/db.h"
#include "data/repo_algs.h"
#include "data/repo_cards.h"
#include "data/repo_seeds.h"
#include "data/repo_user.h"
#include "unit/data/test_util.h"

#include <stdio.h>
#include <string.h>

#include "sqlite3.h"

static int deny_select_on_seed_alg(void *ctx, int code, const char *arg1, const char *arg2,
                                   const char *db, const char *trigger) {
    (void)ctx;
    (void)arg2;
    (void)db;
    (void)trigger;
    if (code == SQLITE_READ && arg1 != NULL && strcmp(arg1, "seed_alg") == 0) {
        return SQLITE_DENY;
    }
    return SQLITE_OK;
}

static int deny_statements(void *ctx, int code, const char *arg1, const char *arg2, const char *db,
                           const char *trigger) {
    (void)ctx;
    (void)arg1;
    (void)arg2;
    (void)db;
    (void)trigger;
    if (code == SQLITE_SELECT || code == SQLITE_READ || code == SQLITE_INSERT ||
        code == SQLITE_UPDATE || code == SQLITE_DELETE) {
        return SQLITE_DENY;
    }
    return SQLITE_OK;
}

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

static void open_memory(bt_db *db) {
    bt_db_error err;
    if (bt_db_open_memory(db, &err) != 0) {
        char reason[256];
        bt_db_error_string(&err, reason, sizeof reason);
        BT_ABORT("open_memory failed: %s", reason);
    }
}

static void fill_row(bt_seed_row *row, const char *case_key, const char *content_hash) {
    memset(row, 0, sizeof *row);
    snprintf(row->event, sizeof row->event, "%s", "3x3");
    snprintf(row->method, sizeof row->method, "%s", "cfop");
    snprintf(row->stage, sizeof row->stage, "%s", "pll");
    snprintf(row->case_key, sizeof row->case_key, "%s", case_key);
    row->alg = "R U R'";
    row->front_json = "{}";
    row->source_url = "https://example.test";
    row->source_at = "2026-01-01";
    snprintf(row->content_hash, sizeof row->content_hash, "%s", content_hash);
}

static void open_locked_pair(const char *tag, bt_db *db, sqlite3 **locker, char *path,
                             size_t path_len) {
    char dir[BT_TU_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", tu_temp_dir("bt-repo-cluster", tag));
    snprintf(path, path_len, "%s/db.sqlite", dir);
    bt_db_error err;
    if (bt_db_open_at(path, db, &err) != 0) {
        char reason[256];
        bt_db_error_string(&err, reason, sizeof reason);
        BT_ABORT("open_at failed: %s", reason);
    }
    *locker = NULL;
    BT_CHECK(sqlite3_open_v2(path, locker, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK);
    BT_CHECK(sqlite3_exec(*locker, "BEGIN EXCLUSIVE", NULL, NULL, NULL) == SQLITE_OK);
}

static void close_locked_pair(bt_db *db, sqlite3 *locker) {
    sqlite3_exec(locker, "ROLLBACK", NULL, NULL, NULL);
    sqlite3_close(locker);
    bt_db_close(db);
}

static void open_shared_reader(const char *path, sqlite3 **reader, sqlite3_stmt **held) {
    *reader = NULL;
    *held = NULL;
    BT_CHECK(sqlite3_open_v2(path, reader, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK);
    BT_CHECK(sqlite3_prepare_v2(*reader, "SELECT COUNT(*) FROM card", -1, held, NULL) == SQLITE_OK);
    BT_CHECK(sqlite3_step(*held) == SQLITE_ROW);
}

static void close_shared_reader(sqlite3 *reader, sqlite3_stmt *held) {
    sqlite3_finalize(held);
    sqlite3_close(reader);
}

BT_TEST(repo_cluster, seed_tx_inserts_then_verifies_then_fails_on_drift) {
    bt_db db;
    open_memory(&db);
    bt_seed_row row;
    fill_row(&row, "case-a", "hash-1");
    bt_seed_report report;
    bt_seed_error err;
    BT_CHECK(bt_seed_pack_tx(&db, "pack.tsv", &row, 1, &report, &err) == 0);
    BT_CHECK_EQ(report.inserted, 1u);
    BT_CHECK_EQ(report.verified, 0u);
    BT_CHECK_EQ(report.file, "pack.tsv");

    BT_CHECK(bt_seed_pack_tx(&db, "pack.tsv", &row, 1, &report, &err) == 0);
    BT_CHECK_EQ(report.inserted, 0u);
    BT_CHECK_EQ(report.verified, 1u);

    sqlite3 *conn = bt_db_conn(&db);
    BT_CHECK(sqlite3_exec(conn, "UPDATE seed_alg SET content_hash = 'tampered'", NULL, NULL,
                          NULL) == SQLITE_OK);
    BT_CHECK(bt_seed_pack_tx(&db, "pack.tsv", &row, 1, &report, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_SEED_ERR_DRIFT);
    BT_CHECK_EQ(err.file, "pack.tsv");
    BT_CHECK_EQ(err.case_key, "case-a");
    BT_CHECK_EQ(err.stored, "tampered");
    BT_CHECK_EQ(err.packed, "hash-1");
    bt_db_close(&db);
}

BT_TEST(repo_cluster, seed_tx_fails_on_a_stale_row_for_the_same_stage) {
    bt_db db;
    open_memory(&db);
    bt_seed_row row;
    fill_row(&row, "case-a", "hash-1");
    bt_seed_report report;
    bt_seed_error err;
    BT_CHECK(bt_seed_pack_tx(&db, "pack.tsv", &row, 1, &report, &err) == 0);
    sqlite3 *conn = bt_db_conn(&db);
    BT_CHECK(sqlite3_exec(conn,
                          "INSERT INTO seed_alg(event, method, stage, case_key, alg, front_json, "
                          "source_url, source_at, content_hash) VALUES ('3x3', 'cfop', 'pll', "
                          "'case-old', 'x', '{}', 'u', 'd', 'h2')",
                          NULL, NULL, NULL) == SQLITE_OK);
    BT_CHECK(bt_seed_pack_tx(&db, "pack.tsv", &row, 1, &report, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_SEED_ERR_STALE);
    BT_CHECK_EQ(err.case_key, "case-old");
    bt_db_close(&db);
}

BT_TEST(repo_cluster, seed_tx_count_zero_short_circuits) {
    bt_db db;
    open_memory(&db);
    bt_seed_report report;
    bt_seed_error err;
    BT_CHECK(bt_seed_pack_tx(&db, "empty.tsv", NULL, 0, &report, &err) == 0);
    BT_CHECK_EQ(report.inserted, 0u);
    BT_CHECK_EQ(report.verified, 0u);
    BT_CHECK_EQ(report.file, "empty.tsv");
    bt_db_close(&db);
}

BT_TEST(repo_cluster, seed_preparation_fails_under_authorizer_denials) {
    bt_db db;
    open_memory(&db);
    bt_seed_row row;
    fill_row(&row, "case-a", "hash-1");
    bt_seed_report report;
    bt_seed_error err;
    sqlite3 *conn = bt_db_conn(&db);
    sqlite3_set_authorizer(conn, deny_select_on_seed_alg, NULL);
    BT_CHECK(bt_seed_pack_tx(&db, "pack.tsv", &row, 1, &report, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_SEED_ERR_DB);
    BT_CHECK_EQ(err.db.step, "select seed_alg");
    sqlite3_set_authorizer(conn, deny_all, NULL);
    long long count = 0;
    BT_CHECK(bt_seed_row_count(&db, &count, &err) != 0);
    BT_CHECK_EQ(err.db.step, "count seed_alg");
    sqlite3_set_authorizer(conn, NULL, NULL);
    bt_db_close(&db);
}

BT_TEST(repo_cluster, seed_steps_fail_on_a_locked_database) {
    bt_db db;
    sqlite3 *locker = NULL;
    char path[BT_TU_PATH_MAX];
    open_locked_pair("seed-lock", &db, &locker, path, sizeof path);
    bt_seed_row row;
    fill_row(&row, "case-a", "hash-1");
    bt_seed_report report;
    bt_seed_error err;
    BT_CHECK(bt_seed_pack_tx(&db, "pack.tsv", &row, 1, &report, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_SEED_ERR_DB);
    BT_CHECK_EQ(err.db.step, "select seed_alg");
    long long count = 0;
    BT_CHECK(bt_seed_row_count(&db, &count, &err) != 0);
    BT_CHECK_EQ(err.db.step, "count seed_alg");
    close_locked_pair(&db, locker);
}

BT_TEST(repo_cluster, seed_load_rows_round_trip_and_fail_under_authorizer) {
    bt_db db;
    open_memory(&db);
    bt_seed_row row;
    fill_row(&row, "case-a", "hash-1");
    bt_seed_report report;
    bt_seed_error err;
    BT_CHECK(bt_seed_pack_tx(&db, "pack.tsv", &row, 1, &report, &err) == 0);
    bt_seed_rows rows;
    BT_CHECK(bt_load_seed_rows(&db, &rows, &err) == 0);
    BT_CHECK_EQ(rows.count, 1u);
    BT_CHECK_EQ(rows.items[0].case_key, "case-a");
    BT_CHECK_EQ(rows.items[0].alg, "R U R'");
    bt_seed_rows_free(&rows);
    sqlite3_set_authorizer(bt_db_conn(&db), deny_all, NULL);
    BT_CHECK(bt_load_seed_rows(&db, &rows, &err) != 0);
    BT_CHECK_EQ(err.db.step, "select all seed_alg");
    sqlite3_set_authorizer(bt_db_conn(&db), NULL, NULL);
    bt_db_close(&db);
}

BT_TEST(repo_cluster, cards_save_and_load_round_trip_with_a_fallback_entry) {
    bt_db db;
    open_memory(&db);
    bt_case_ref ref;
    bt_case_ref_of(&ref, "2x2", "orto", "stage", "case-a");
    bt_card card;
    bt_card_init(&card, 0);
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_card_review(&card, BT_GRADE_EASY, 0, &cfg);
    BT_CHECK(bt_repo_cards_save_card(&db, &ref, &card, NULL) == 0);

    bt_case_cards cards;
    bt_srs_load_error load_err;
    BT_CHECK(bt_repo_cards_load(&db, &cfg, &cards, &load_err) == 0);
    BT_CHECK_EQ(bt_case_cards_len(&cards), 1u);
    BT_CHECK(bt_case_cards_contains(&cards, &ref) == 1);
    BT_CHECK(bt_case_cards_get(&cards, &ref) != NULL);
    bt_case_ref missing;
    bt_case_ref_of(&missing, "2x2", "orto", "stage", "case-b");
    BT_CHECK(bt_case_cards_get(&cards, &missing) == NULL);
    const bt_card *fallback = bt_case_cards_entry(&cards, &missing, &card);
    BT_CHECK(fallback != NULL);
    BT_CHECK_EQ(bt_case_cards_len(&cards), 2u);
    const bt_card *again = bt_case_cards_entry(&cards, &missing, &card);
    BT_CHECK(again == fallback);
    bt_case_cards_free(&cards);
    bt_card_free(&card);
    bt_db_close(&db);
}

BT_TEST(repo_cluster, cards_save_fails_under_authorizer_and_lock) {
    bt_db db;
    open_memory(&db);
    bt_case_ref ref;
    bt_case_ref_of(&ref, "2x2", "orto", "stage", "case-a");
    bt_card card;
    bt_card_init(&card, 0);
    bt_db_error err;
    sqlite3 *conn = bt_db_conn(&db);
    sqlite3_set_authorizer(conn, deny_statements, NULL);
    BT_CHECK(bt_repo_cards_save_card(&db, &ref, &card, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_EXEC);
    BT_CHECK_EQ(err.step, "clear case card");
    sqlite3_set_authorizer(conn, NULL, NULL);
    BT_CHECK(sqlite3_exec(conn, "BEGIN", NULL, NULL, NULL) == SQLITE_OK);
    BT_CHECK(bt_repo_cards_save_card(&db, &ref, &card, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_EXEC);
    BT_CHECK_EQ(err.step, "begin");
    BT_CHECK(sqlite3_exec(conn, "ROLLBACK", NULL, NULL, NULL) == SQLITE_OK);
    bt_card_free(&card);
    bt_db_close(&db);
}

BT_TEST(repo_cluster, cards_save_commits_fail_under_a_shared_reader) {
    char dir[BT_TU_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", tu_temp_dir("bt-repo-cluster", "cards-reader"));
    char path[BT_TU_PATH_MAX];
    snprintf(path, sizeof path, "%s/db.sqlite", dir);
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(path, &db, &err) == 0);

    sqlite3 *reader = NULL;
    sqlite3_stmt *held = NULL;
    open_shared_reader(path, &reader, &held);

    bt_case_ref ref;
    bt_case_ref_of(&ref, "2x2", "orto", "stage", "case-a");
    bt_card card;
    bt_card_init(&card, 0);
    BT_CHECK(bt_repo_cards_save_card(&db, &ref, &card, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_EXEC);
    BT_CHECK_EQ(err.step, "commit case card");
    BT_CHECK(strstr(err.detail, "locked") != NULL || strstr(err.detail, "busy") != NULL);

    close_shared_reader(reader, held);
    bt_card_free(&card);
    bt_db_close(&db);
}

BT_TEST(repo_cluster, algs_load_fails_on_a_null_column_from_a_foreign_schema) {
    char dir[BT_TU_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", tu_temp_dir("bt-repo-cluster", "algs-null"));
    char path[BT_TU_PATH_MAX];
    snprintf(path, sizeof path, "%s/db.sqlite", dir);
    sqlite3 *raw = NULL;
    BT_CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    sqlite3_exec(raw,
                 "CREATE TABLE user_alg (event TEXT, method TEXT, stage TEXT, case_key TEXT, "
                 "alg TEXT, created_at INTEGER, updated_at INTEGER)",
                 NULL, NULL, NULL);
    sqlite3_exec(raw,
                 "INSERT INTO user_alg(event, method, stage, case_key, alg, created_at, "
                 "updated_at) VALUES ('3bld', '3style', NULL, 'c', 'x', 0, 0)",
                 NULL, NULL, NULL);
    sqlite3_exec(raw, "PRAGMA user_version = 3", NULL, NULL, NULL);
    sqlite3_close(raw);

    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(path, &db, &err) == 0);
    bt_user_algs loaded;
    BT_CHECK(bt_repo_algs_load(&db, &loaded, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_QUERY);
    BT_CHECK_EQ(err.step, "select user_alg");
    bt_db_close(&db);
}

BT_TEST(repo_cluster, algs_save_and_load_round_trip) {
    bt_db db;
    open_memory(&db);
    bt_user_algs algs;
    bt_user_algs_init(&algs);
    BT_CHECK(bt_user_algs_set_entry(&algs, "stage/case-a", "R U R'") == 0);
    bt_db_error err;
    BT_CHECK(bt_repo_algs_save(&db, &algs, &err) == 0);
    bt_user_algs loaded;
    BT_CHECK(bt_repo_algs_load(&db, &loaded, &err) == 0);
    bt_user_algs_free(&loaded);
    bt_user_algs_free(&algs);
    bt_db_close(&db);
}

BT_TEST(repo_cluster, algs_save_and_load_fail_under_authorizer_and_lock) {
    bt_db db;
    open_memory(&db);
    bt_user_algs algs;
    bt_user_algs_init(&algs);
    BT_CHECK(bt_user_algs_set_entry(&algs, "stage/case-a", "R U R'") == 0);
    bt_user_algs loaded;
    bt_db_error err;
    sqlite3 *conn = bt_db_conn(&db);
    sqlite3_set_authorizer(conn, deny_statements, NULL);
    BT_CHECK(bt_repo_algs_save(&db, &algs, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_EXEC);
    BT_CHECK_EQ(err.step, "clear user_alg");
    BT_CHECK(bt_repo_algs_load(&db, &loaded, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_QUERY);
    BT_CHECK_EQ(err.step, "select user_alg");
    sqlite3_set_authorizer(conn, NULL, NULL);
    bt_user_algs_free(&loaded);
    bt_user_algs_free(&algs);
    bt_db_close(&db);
}

BT_TEST(repo_cluster, algs_save_tx_fails_on_an_overlong_key) {
    bt_db db;
    open_memory(&db);
    bt_user_algs algs;
    bt_user_algs_init(&algs);
    char key[BT_REPO_ALGS_KEY_MAX];
    memset(key, 'K', (size_t)BT_PATH_MAX);
    key[BT_PATH_MAX] = '\0';
    strcat(key, "/c");
    BT_CHECK(bt_user_algs_set_entry(&algs, key, "R U R'") == 0);
    bt_db_error err;
    BT_CHECK(bt_repo_algs_save_tx(&db, &algs, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_EXEC);
    BT_CHECK_EQ(err.step, "insert user_alg");
    BT_CHECK(strstr(err.detail, "key KKKK") == err.detail);
    bt_user_algs_free(&algs);
    bt_db_close(&db);
}
