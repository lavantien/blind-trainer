#include "common/bt_harness.h"
#include "common/json.h"
#include "data/db.h"
#include "data/repo_srs.h"
#include "data/srs.h"
#include "unit/data/test_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"

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

static int raw_exec(sqlite3 *conn, const char *sql) {
    return sqlite3_exec(conn, sql, NULL, NULL, NULL);
}

static void craft_file_db(const char *path, const char *sql, int user_version) {
    sqlite3 *conn = NULL;
    if (sqlite3_open(path, &conn) == SQLITE_OK) {
        char pragma[64];
        snprintf(pragma, sizeof pragma, "PRAGMA user_version = %d", user_version);
        raw_exec(conn, sql);
        raw_exec(conn, pragma);
    }
    sqlite3_close(conn);
}

static void open_at(const char *tag, bt_db *db, char *path, size_t path_len) {
    char dir[BT_TU_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", tu_temp_dir("bt-repo-fail", tag));
    snprintf(path, path_len, "%s/db.sqlite", dir);
    bt_db_error err;
    if (bt_db_open_at(path, db, &err) != 0) {
        char reason[256];
        bt_db_error_string(&err, reason, sizeof reason);
        BT_ABORT("open_at failed: %s", reason);
    }
}

static void seed_card_row(sqlite3 *conn, long long id, const char *event, const char *method,
                          const char *stage, const char *case_key, int has_stability,
                          int has_difficulty) {
    char sql[512];
    snprintf(sql, sizeof sql,
             "INSERT INTO card(id, event, method, stage, case_key, created_at, updated_at) "
             "VALUES (%lld, '%s', '%s', '%s', '%s', 0, 0)",
             id, event, method, stage, case_key);
    raw_exec(conn, sql);
    snprintf(sql, sizeof sql,
             "INSERT INTO srs_state(card_id, due_day, interval_days, reps, lapses, stability, "
             "difficulty) VALUES (%lld, 0, 0, 0, 0, %s, %s)",
             id, has_stability ? "-1" : "NULL", has_difficulty ? "5" : "NULL");
    raw_exec(conn, sql);
}

static void seed_review_row(sqlite3 *conn, long long card_id, long long rating) {
    char sql[512];
    snprintf(sql, sizeof sql,
             "INSERT INTO review_log(card_id, rating, reviewed_day, interval_before, "
             "interval_after, elapsed_days) VALUES (%lld, %lld, 0, 0, 0, 0)",
             card_id, rating);
    raw_exec(conn, sql);
}

BT_TEST(repo_failures, authorizer_denial_fails_the_user_version_query) {
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(&db, &err) == 0);
    sqlite3 *conn = bt_db_conn(&db);
    sqlite3_set_authorizer(conn, deny_all, NULL);
    long long version = 0;
    BT_CHECK(bt_db_user_version(&db, &version, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_QUERY);
    BT_CHECK_EQ(err.step, "user_version");
    BT_CHECK(strstr(err.detail, "not authorized") != NULL);
    sqlite3_set_authorizer(conn, NULL, NULL);
    bt_db_close(&db);
}

BT_TEST(repo_failures, authorizer_denial_fails_is_fresh) {
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(&db, &err) == 0);
    sqlite3 *conn = bt_db_conn(&db);
    sqlite3_set_authorizer(conn, deny_all, NULL);
    int fresh = 0;
    BT_CHECK(bt_db_is_fresh(&db, &fresh, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_QUERY);
    BT_CHECK_EQ(err.step, "user_alg");
    BT_CHECK(strstr(err.detail, "not authorized") != NULL);
    sqlite3_set_authorizer(conn, NULL, NULL);
    bt_db_close(&db);
}

BT_TEST(repo_failures, authorizer_denial_fails_exec) {
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(&db, &err) == 0);
    sqlite3 *conn = bt_db_conn(&db);
    sqlite3_set_authorizer(conn, deny_all, NULL);
    BT_CHECK(bt_db_exec(&db, "SELECT 1", "probe-select", &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_EXEC);
    BT_CHECK_EQ(err.step, "probe-select");
    BT_CHECK(strstr(err.detail, "not authorized") != NULL);
    char text[256];
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "exec probe-select:") == text);
    sqlite3_set_authorizer(conn, NULL, NULL);
    bt_db_close(&db);
}

BT_TEST(repo_failures, authorizer_denial_fails_the_card_select_prepare) {
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(&db, &err) == 0);
    sqlite3 *conn = bt_db_conn(&db);
    sqlite3_set_authorizer(conn, deny_all, NULL);
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_srs_store store;
    bt_srs_load_error load_err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &store, &load_err) != 0);
    BT_CHECK_EQ(load_err.kind, BT_SRS_LOAD_DB);
    BT_CHECK_EQ(load_err.db.kind, BT_DB_ERR_QUERY);
    BT_CHECK_EQ(load_err.db.step, "select card");
    BT_CHECK(strstr(load_err.db.detail, "not authorized") != NULL);
    sqlite3_set_authorizer(conn, NULL, NULL);
    bt_db_close(&db);
}

BT_TEST(repo_failures, begin_inside_an_open_transaction_fails) {
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(&db, &err) == 0);
    BT_CHECK(raw_exec(bt_db_conn(&db), "BEGIN") == SQLITE_OK);
    BT_CHECK(bt_db_begin(&db, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_EXEC);
    BT_CHECK_EQ(err.step, "begin");
    BT_CHECK(strstr(err.detail, "within a transaction") != NULL);
    BT_CHECK(raw_exec(bt_db_conn(&db), "ROLLBACK") == SQLITE_OK);
    bt_db_close(&db);
}

BT_TEST(repo_failures, commit_without_a_transaction_fails) {
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(&db, &err) == 0);
    BT_CHECK(bt_db_commit(&db, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_EXEC);
    BT_CHECK_EQ(err.step, "commit");
    BT_CHECK(strstr(err.detail, "no transaction") != NULL);
    bt_db_close(&db);
}

BT_TEST(repo_failures, opening_a_locked_database_fails_at_the_user_version_read) {
    char dir[BT_TU_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", tu_temp_dir("bt-repo-fail", "locked-open"));
    char path[BT_TU_PATH_MAX];
    snprintf(path, sizeof path, "%s/db.sqlite", dir);
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(path, &db, &err) == 0);
    bt_db_close(&db);

    sqlite3 *locker = NULL;
    BT_CHECK(sqlite3_open_v2(path, &locker, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK);
    BT_CHECK(raw_exec(locker, "BEGIN EXCLUSIVE") == SQLITE_OK);

    bt_db blocked;
    BT_CHECK(bt_db_open_at(path, &blocked, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_QUERY);
    BT_CHECK_EQ(err.step, "user_version");
    BT_CHECK(strstr(err.detail, "locked") != NULL || strstr(err.detail, "busy") != NULL);
    BT_CHECK(blocked.conn == NULL);

    raw_exec(locker, "ROLLBACK");
    sqlite3_close(locker);
}

BT_TEST(repo_failures, a_locked_database_fails_is_fresh_at_the_step) {
    char path[BT_TU_PATH_MAX];
    bt_db db;
    bt_db_error err;
    open_at("locked-fresh", &db, path, sizeof path);

    sqlite3 *locker = NULL;
    BT_CHECK(sqlite3_open_v2(path, &locker, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK);
    BT_CHECK(raw_exec(locker, "BEGIN EXCLUSIVE") == SQLITE_OK);

    int fresh = 0;
    BT_CHECK(bt_db_is_fresh(&db, &fresh, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_QUERY);
    BT_CHECK_EQ(err.step, "user_alg");
    BT_CHECK(strstr(err.detail, "locked") != NULL || strstr(err.detail, "busy") != NULL);

    raw_exec(locker, "ROLLBACK");
    sqlite3_close(locker);
    bt_db_close(&db);
}

BT_TEST(repo_failures, a_locked_database_fails_the_card_load_at_the_step) {
    char path[BT_TU_PATH_MAX];
    bt_db db;
    open_at("locked-load", &db, path, sizeof path);

    sqlite3 *locker = NULL;
    BT_CHECK(sqlite3_open_v2(path, &locker, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK);
    BT_CHECK(raw_exec(locker, "BEGIN EXCLUSIVE") == SQLITE_OK);

    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_srs_store store;
    bt_srs_load_error load_err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &store, &load_err) != 0);
    BT_CHECK_EQ(load_err.kind, BT_SRS_LOAD_DB);
    BT_CHECK_EQ(load_err.db.step, "select card");
    BT_CHECK(strstr(load_err.db.detail, "locked") != NULL ||
             strstr(load_err.db.detail, "busy") != NULL);

    raw_exec(locker, "ROLLBACK");
    sqlite3_close(locker);
    bt_db_close(&db);
}

BT_TEST(repo_failures, a_duplicate_card_violates_the_unique_constraint_at_the_step) {
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(&db, &err) == 0);
    bt_card card;
    bt_card_init(&card, 0);
    BT_CHECK(bt_repo_srs_insert_card(&db, "3bld", "3style", "s", "c", &card, 0, &err) == 0);
    BT_CHECK(bt_repo_srs_insert_card(&db, "3bld", "3style", "s", "c", &card, 0, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_EXEC);
    BT_CHECK_EQ(err.step, "insert card");
    BT_CHECK(strstr(err.detail, "UNIQUE constraint failed") != NULL);
    bt_card_free(&card);
    bt_db_close(&db);
}

BT_TEST(repo_failures, an_orphaned_srs_state_row_conflicts_at_the_state_step) {
    char path[BT_TU_PATH_MAX];
    bt_db db;
    bt_db_error err;
    open_at("orphan-state", &db, path, sizeof path);

    sqlite3 *raw = NULL;
    BT_CHECK(sqlite3_open_v2(path, &raw, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK);
    seed_card_row(raw, 1, "3bld", "3style", "s", "c", 0, 0);
    BT_CHECK(raw_exec(raw, "DELETE FROM card WHERE id = 1") == SQLITE_OK);
    sqlite3_close(raw);

    bt_card card;
    bt_card_init(&card, 0);
    BT_CHECK(bt_repo_srs_insert_card(&db, "3bld", "3style", "s", "c", &card, 0, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_EXEC);
    BT_CHECK_EQ(err.step, "insert srs_state");
    BT_CHECK(strstr(err.detail, "UNIQUE constraint failed") != NULL);
    bt_card_free(&card);
    bt_db_close(&db);
}

BT_TEST(repo_failures, a_foreign_schema_with_null_card_text_fails_the_load) {
    char dir[BT_TU_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", tu_temp_dir("bt-repo-fail", "null-text"));
    char path[BT_TU_PATH_MAX];
    snprintf(path, sizeof path, "%s/db.sqlite", dir);
    sqlite3 *raw = NULL;
    BT_CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    raw_exec(raw, "CREATE TABLE card (id INTEGER PRIMARY KEY, event TEXT, method TEXT, "
                  "stage TEXT, case_key TEXT)");
    raw_exec(raw, "CREATE TABLE srs_state (card_id INTEGER PRIMARY KEY, due_day INTEGER, "
                  "interval_days INTEGER, reps INTEGER, lapses INTEGER, stability REAL, "
                  "difficulty REAL)");
    raw_exec(raw, "CREATE TABLE review_log (id INTEGER PRIMARY KEY, card_id INTEGER, "
                  "rating INTEGER, reviewed_day INTEGER, interval_before INTEGER, "
                  "interval_after INTEGER, elapsed_days INTEGER)");
    raw_exec(raw, "PRAGMA user_version = 3");
    raw_exec(raw, "INSERT INTO card(id, event, method, stage, case_key) "
                  "VALUES (1, '3bld', '3style', NULL, 'c')");
    raw_exec(raw, "INSERT INTO srs_state(card_id, due_day, interval_days, reps, lapses) "
                  "VALUES (1, 0, 0, 0, 0)");
    sqlite3_close(raw);

    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(path, &db, &err) == 0);
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_srs_store store;
    bt_srs_load_error load_err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &store, &load_err) != 0);
    BT_CHECK_EQ(load_err.kind, BT_SRS_LOAD_DB);
    BT_CHECK_EQ(load_err.db.step, "select card");
    bt_db_close(&db);
}

BT_TEST(repo_failures, an_unknown_review_quality_fails_the_sm2_replay_load) {
    char path[BT_TU_PATH_MAX];
    bt_db db;
    open_at("unknown-quality", &db, path, sizeof path);

    sqlite3 *raw = NULL;
    BT_CHECK(sqlite3_open_v2(path, &raw, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK);
    seed_card_row(raw, 1, "3bld", "3style", "s", "c", 0, 0);
    seed_review_row(raw, 1, 99);
    sqlite3_close(raw);

    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_srs_store store;
    bt_srs_load_error load_err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &store, &load_err) != 0);
    BT_CHECK_EQ(load_err.kind, BT_SRS_LOAD_UNKNOWN_QUALITY);
    BT_CHECK_EQ(load_err.quality, 99);
    char text[512];
    bt_srs_load_error_string(&load_err, text, sizeof text);
    BT_CHECK(strstr(text, "card s/c holds review quality 99") != NULL);
    bt_db_close(&db);
}

BT_TEST(repo_failures, mixed_null_memory_columns_fail_the_load) {
    char path[BT_TU_PATH_MAX];
    bt_db db;
    open_at("mixed-null", &db, path, sizeof path);

    sqlite3 *raw = NULL;
    BT_CHECK(sqlite3_open_v2(path, &raw, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK);
    seed_card_row(raw, 1, "3bld", "3style", "s", "c", 0, 1);
    sqlite3_close(raw);

    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_srs_store store;
    bt_srs_load_error load_err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &store, &load_err) != 0);
    BT_CHECK_EQ(load_err.kind, BT_SRS_LOAD_INVALID_MEMORY);
    BT_CHECK_EQ(load_err.has_stability, 0);
    BT_CHECK_EQ(load_err.has_difficulty, 1);
    char text[512];
    bt_srs_load_error_string(&load_err, text, sizeof text);
    BT_CHECK(strstr(text, "stability None, difficulty Some(5)") != NULL);
    bt_db_close(&db);
}

BT_TEST(repo_failures, an_invalid_memory_pair_fails_the_load) {
    char path[BT_TU_PATH_MAX];
    bt_db db;
    open_at("invalid-pair", &db, path, sizeof path);

    sqlite3 *raw = NULL;
    BT_CHECK(sqlite3_open_v2(path, &raw, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK);
    raw_exec(raw, "INSERT INTO card(id, event, method, stage, case_key, created_at, updated_at) "
                  "VALUES (1, '3bld', '3style', 's', 'c', 0, 0)");
    raw_exec(raw, "INSERT INTO srs_state(card_id, due_day, interval_days, reps, lapses, "
                  "stability, difficulty) VALUES (1, 0, 0, 0, 0, -1, 5)");
    sqlite3_close(raw);

    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_srs_store store;
    bt_srs_load_error load_err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &store, &load_err) != 0);
    BT_CHECK_EQ(load_err.kind, BT_SRS_LOAD_INVALID_MEMORY);
    BT_CHECK_EQ(load_err.has_stability, 1);
    BT_CHECK_EQ(load_err.has_difficulty, 1);
    char text[512];
    bt_srs_load_error_string(&load_err, text, sizeof text);
    BT_CHECK(strstr(text, "stability Some(-1), difficulty Some(5)") != NULL);
    bt_db_close(&db);
}

BT_TEST(repo_failures, reviews_without_memory_columns_replay_through_sm2) {
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(&db, &err) == 0);
    sqlite3 *conn = bt_db_conn(&db);
    seed_card_row(conn, 1, "3bld", "3style", "s", "c", 0, 0);
    seed_review_row(conn, 1, 3);
    seed_review_row(conn, 1, 4);

    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_srs_store store;
    bt_srs_load_error load_err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &store, &load_err) == 0);
    const bt_card *card = bt_srs_store_get(&store, "s/c");
    BT_CHECK(card != NULL);
    BT_CHECK_EQ(card->has_memory, 1);
    BT_CHECK(bt_fsrs_memory_is_valid(card->memory));
    BT_CHECK(bt_srs_store_get(&store, "missing") == NULL);
    BT_CHECK_EQ(bt_srs_store_len(&store), 1);
    bt_srs_store_free(&store);
    bt_db_close(&db);
}

static int store_from_text(const char *text) {
    bt_json_value *root = NULL;
    bt_json_error json_err;
    if (bt_json_parse(text, strlen(text), &root, &json_err) != 0) {
        return -1;
    }
    bt_srs_store store;
    int rc = bt_srs_store_from_json(root, &store);
    if (rc == 0) {
        bt_srs_store_free(&store);
    }
    bt_json_free(root);
    return rc;
}

BT_TEST(repo_failures, malformed_card_json_shapes_fail_the_store_load) {
    BT_CHECK(store_from_text("{\"cards\": []}") != 0);
    BT_CHECK(store_from_text("{}") != 0);
    BT_CHECK(store_from_text("{\"cards\": {\"k\": 7}}") != 0);
    BT_CHECK(store_from_text("{\"cards\": {\"k\": {\"memory\": {\"stability\": \"x\", "
                             "\"difficulty\": 2}, \"interval_days\": 1, \"due_day\": 2, "
                             "\"reps\": 0, \"lapses\": 0, \"reviews\": []}}}") != 0);
    BT_CHECK(store_from_text("{\"cards\": {\"k\": {\"memory\": {\"stability\": 1}, "
                             "\"interval_days\": 1, \"due_day\": 2, \"reps\": 0, "
                             "\"lapses\": 0, \"reviews\": []}}}") != 0);
    BT_CHECK(store_from_text("{\"cards\": {\"k\": {\"due_day\": 2, \"reps\": 0, "
                             "\"lapses\": 0, \"reviews\": []}}}") != 0);
    BT_CHECK(store_from_text("{\"cards\": {\"k\": {\"interval_days\": 1, \"due_day\": 2.5, "
                             "\"reps\": 0, \"lapses\": 0, \"reviews\": []}}}") != 0);
    BT_CHECK(store_from_text("{\"cards\": {\"k\": {\"interval_days\": 1, \"due_day\": 2, "
                             "\"reps\": 0, \"lapses\": 0}}}") != 0);
    BT_CHECK(store_from_text("{\"cards\": {\"k\": {\"interval_days\": 1, \"due_day\": 2, "
                             "\"reps\": 0, \"lapses\": 0, \"reviews\": {}}}}") != 0);
    BT_CHECK(store_from_text("{\"cards\": {\"k\": {\"interval_days\": 1, \"due_day\": 2, "
                             "\"reps\": 0, \"lapses\": 0, \"reviews\": ["
                             "{\"ts_days\": 1, \"quality\": \"x\", \"elapsed_days\": 0, "
                             "\"scheduled_days\": 1}]}}}") != 0);
    BT_CHECK(store_from_text("{\"cards\": {\"k\": {\"interval_days\": 1, \"due_day\": 2, "
                             "\"reps\": 0, \"lapses\": 0, \"reviews\": ["
                             "{\"ts_days\": 1, \"quality\": 3, \"elapsed_days\": 0}]}}}") != 0);
    BT_CHECK(store_from_text("{\"cards\": {\"k\": {\"interval_days\": 1, \"due_day\": 2, "
                             "\"reps\": 0, \"lapses\": 0, \"reviews\": []}}}") == 0);
}

BT_TEST(repo_failures, an_overlong_stage_in_a_store_key_fails_save_tx) {
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(&db, &err) == 0);
    bt_srs_store store;
    bt_srs_store_init(&store);
    char key[BT_REPO_ALGS_KEY_MAX];
    memset(key, 'S', (size_t)BT_PATH_MAX);
    key[BT_PATH_MAX] = '\0';
    strcat(key, "/c");
    bt_card card;
    bt_card_init(&card, 0);
    BT_CHECK(bt_srs_store_set(&store, key, &card) == 0);
    BT_CHECK(bt_repo_srs_save_tx(&db, &store, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_EXEC);
    BT_CHECK_EQ(err.step, "insert card");
    BT_CHECK(strstr(err.detail, "key SSSS") == err.detail);
    bt_card_free(&card);
    bt_srs_store_free(&store);
    bt_db_close(&db);
}

BT_TEST(repo_failures, error_string_renders_every_kind) {
    bt_db_error err;
    memset(&err, 0, sizeof err);
    char text[256];

    err.kind = BT_DB_ERR_MKDIR;
    snprintf(err.path, sizeof err.path, "%s", "dir");
    snprintf(err.detail, sizeof err.detail, "%s", "denied");
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "create db dir dir: denied") == text);

    err.kind = BT_DB_ERR_PRAGMA;
    snprintf(err.step, sizeof err.step, "%s", "foreign_keys");
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "pragma foreign_keys: denied") == text);

    err.kind = BT_DB_ERR_BACKUP;
    snprintf(err.path, sizeof err.path, "%s", "from");
    snprintf(err.to_path, sizeof err.to_path, "%s", "to");
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "backup from to to: denied") == text);

    err.kind = BT_DB_ERR_FUTURE;
    err.version = 9;
    err.latest = 3;
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strcmp(text, "db user_version 9 is newer than supported 3") == 0);

    err.kind = BT_DB_ERR_QUERY;
    snprintf(err.step, sizeof err.step, "%s", "user_alg");
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "query user_alg: denied") == text);
}

BT_TEST(repo_failures, backup_path_and_now_ms_report_their_contracts) {
    char out[BT_TU_PATH_MAX];
    bt_db_backup_path("a/b.sqlite", out, sizeof out);
    BT_CHECK_EQ(out, "a/b.sqlite.bak");
    long long first = bt_db_now_ms();
    long long second = bt_db_now_ms();
    BT_CHECK(first > 0);
    BT_CHECK(second >= first);
}

BT_TEST(repo_failures, a_write_denied_parent_directory_fails_at_mkdir) {
    char dir[BT_TU_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", tu_temp_dir("bt-repo-fail", "deny-parent"));
    char ro[BT_TU_PATH_MAX];
    snprintf(ro, sizeof ro, "%s/ro", dir);
    _mkdir(ro);
    char deny[BT_TU_PATH_MAX + 64];
    snprintf(deny, sizeof deny, "icacls \"%s\" /deny *S-1-1-0:(NP)(WD,AD)", ro);
    BT_CHECK(system(deny) == 0);

    char path[BT_TU_PATH_MAX];
    snprintf(path, sizeof path, "%s/sub/db.sqlite", ro);
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(path, &db, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_MKDIR);
    char text[256];
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "create db dir ") == text);
    BT_CHECK(db.conn == NULL);

    char allow[BT_TU_PATH_MAX + 64];
    snprintf(allow, sizeof allow, "icacls \"%s\" /remove:d *S-1-1-0", ro);
    system(allow);
}

BT_TEST(repo_failures, a_directory_at_the_backup_target_fails_the_backup) {
    char dir[BT_TU_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", tu_temp_dir("bt-repo-fail", "bak-dir"));
    char path[BT_TU_PATH_MAX];
    snprintf(path, sizeof path, "%s/db.sqlite", dir);
    char bak[BT_TU_PATH_MAX];
    snprintf(bak, sizeof bak, "%s.bak", path);
    craft_file_db(path, "CREATE TABLE srs_state (key TEXT, ease INTEGER, extra INTEGER)", 1);
    _mkdir(bak);

    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(path, &db, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_BACKUP);
    BT_CHECK_EQ(err.path, path);
    BT_CHECK_EQ(err.to_path, bak);
    char text[512];
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "backup ") == text);
    BT_CHECK(strstr(text, bak) != NULL);
    BT_CHECK(db.conn == NULL);
}

BT_TEST(repo_failures, a_reader_holding_shared_locks_fails_the_migration_commit) {
    char dir[BT_TU_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", tu_temp_dir("bt-repo-fail", "shared-reader"));
    char path[BT_TU_PATH_MAX];
    snprintf(path, sizeof path, "%s/db.sqlite", dir);
    craft_file_db(path, "CREATE TABLE blocker (k)", 0);

    sqlite3 *reader = NULL;
    BT_CHECK(sqlite3_open_v2(path, &reader, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK);
    sqlite3_stmt *held = NULL;
    BT_CHECK(sqlite3_prepare_v2(reader, "SELECT COUNT(*) FROM blocker", -1, &held, NULL) ==
             SQLITE_OK);
    BT_CHECK(sqlite3_step(held) == SQLITE_ROW);

    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(path, &db, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_MIGRATE);
    BT_CHECK_EQ(err.step, "001_init");
    BT_CHECK(strstr(err.detail, "locked") != NULL || strstr(err.detail, "busy") != NULL);
    BT_CHECK(db.conn == NULL);

    sqlite3_finalize(held);
    sqlite3_close(reader);
    bt_db reopened;
    BT_CHECK(bt_db_open_at(path, &reopened, &err) == 0);
    bt_db_close(&reopened);
}

BT_TEST(repo_failures, a_happy_transaction_round_trips_the_store) {
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(&db, &err) == 0);
    bt_srs_store store;
    bt_srs_store_init(&store);
    bt_card card;
    bt_card_init(&card, 0);
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_card_review(&card, BT_GRADE_GOOD, 0, &cfg);
    BT_CHECK(bt_srs_store_set(&store, "s/c", &card) == 0);
    BT_CHECK(bt_repo_srs_save(&db, &store, &err) == 0);
    bt_card_free(&card);
    bt_srs_store_free(&store);

    bt_srs_store loaded;
    bt_srs_load_error load_err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &loaded, &load_err) == 0);
    const bt_card *round = bt_srs_store_get(&loaded, "s/c");
    BT_CHECK(round != NULL);
    BT_CHECK_EQ(round->review_count, 1);
    bt_srs_store_clone(&loaded, &store);
    BT_CHECK_EQ(bt_srs_store_eq(&loaded, &store), 1);
    bt_srs_store_free(&store);
    bt_srs_store_free(&loaded);
    bt_db_close(&db);
}

BT_TEST(repo_failures, key_join_and_split_edges) {
    char out[8];
    BT_CHECK(bt_repo_algs_join_key("stage", "case", out, sizeof out) != 0);
    BT_CHECK(bt_repo_algs_join_key("stage", "", out, sizeof out) == 0);
    BT_CHECK_EQ(out, "stage");
    char room[16];
    BT_CHECK(bt_repo_algs_join_key("stag", "case", room, sizeof room) == 0);
    BT_CHECK_EQ(room, "stag/case");
    char stage[8];
    char case_key[8];
    BT_CHECK(bt_repo_algs_split_key("overlong", stage, sizeof stage, case_key, sizeof case_key) !=
             0);
    BT_CHECK(bt_repo_algs_split_key("s/toolongcase", stage, sizeof stage, case_key,
                                    sizeof case_key) != 0);
    BT_CHECK(bt_repo_algs_split_key("s/c", stage, sizeof stage, case_key, sizeof case_key) == 0);
    BT_CHECK_EQ(stage, "s");
    BT_CHECK_EQ(case_key, "c");
    BT_CHECK(bt_repo_algs_split_key("bare", stage, sizeof stage, case_key, sizeof case_key) == 0);
    BT_CHECK_EQ(stage, "bare");
    BT_CHECK_EQ(case_key, "");
}

BT_TEST(repo_failures, srs_edges_over_grades_replay_and_review) {
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_grade grade;
    BT_CHECK(bt_grade_from_quality(1, &grade) != 0);
    BT_CHECK(bt_grade_from_quality(6, &grade) != 0);
    BT_CHECK(bt_grade_from_quality(2, &grade) == 0);
    BT_CHECK_EQ(grade, BT_GRADE_AGAIN);

    bt_review_entry bad_first = {0, 9, 0, 0};
    int has = 0;
    bt_fsrs_memory memory;
    BT_CHECK(bt_replay_sm2(&bad_first, 1, &cfg, &has, &memory) != 0);
    bt_review_entry mixed[] = {
        {0, 3, 0, 0},
        {1, 8, 1, 1},
    };
    BT_CHECK(bt_replay_sm2(mixed, 2, &cfg, &has, &memory) != 0);
    BT_CHECK(bt_replay_sm2(NULL, 0, &cfg, &has, &memory) == 0);
    BT_CHECK_EQ(has, 0);

    bt_card card;
    bt_card_init(&card, 5);
    BT_CHECK_EQ(bt_card_is_due(&card, 4), 0);
    BT_CHECK_EQ(bt_card_is_due(&card, 5), 1);
    long long day = 0;
    BT_CHECK_EQ(bt_card_last_review_day(&card, &day), 0);
    bt_card_review(&card, BT_GRADE_GOOD, 9, &cfg);
    bt_card_review(&card, BT_GRADE_GOOD, 3, &cfg);
    BT_CHECK_EQ(card.review_count, 2);
    BT_CHECK_EQ(card.reviews[1].elapsed_days, 0);
    bt_card_free(&card);

    bt_card_init(&card, 0);
    bt_review_entry *broken = malloc(sizeof *broken);
    BT_CHECK(broken != NULL);
    broken[0] = (bt_review_entry){0, 7, 0, 0};
    card.reviews = broken;
    card.review_count = 1;
    card.review_cap = 1;
    bt_card_review(&card, BT_GRADE_GOOD, 1, &cfg);
    BT_CHECK_EQ(card.has_memory, 1);
    bt_card_free(&card);
}
