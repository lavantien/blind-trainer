#include "common/bt_harness.h"
#include "common/json.h"
#include "data/db.h"
#include "data/fsrs.h"
#include "data/repo_settings.h"
#include "data/repo_srs.h"
#include "unit/data/test_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"

static unsigned repo_err_counter;

static const char *repo_err_dir(const char *tag) {
    static char dir[BT_TU_PATH_MAX];
    const char *base = getenv("TEMP");
    snprintf(dir, sizeof dir, "%s/bt-repo-errors/%s-%d-%u", base ? base : ".", tag, _getpid(),
             repo_err_counter++);
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
    const char *dir = repo_err_dir(tag);
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

BT_TEST(repo_errors, settings_save_and_load_fail_without_the_table) {
    bt_db db;
    bt_db_error err;
    char path[BT_TU_PATH_MAX];
    open_latest_schema("settings-missing", "", &db, path, sizeof path);
    BT_CHECK(bt_repo_settings_save_tx(&db, "k", "v", &err) != 0);
    BT_CHECK(err.kind == BT_DB_ERR_EXEC);
    BT_CHECK(strcmp(err.step, "upsert settings") == 0);
    BT_CHECK(strstr(err.detail, "no such table") != NULL);
    char *loaded = NULL;
    BT_CHECK(bt_repo_settings_load_text(&db, "k", &loaded, &err) != 0);
    BT_CHECK(err.kind == BT_DB_ERR_QUERY);
    BT_CHECK(strcmp(err.step, "select settings") == 0);
    bt_db_close(&db);
}

BT_TEST(repo_errors, settings_save_step_fails_under_a_blocking_trigger) {
    bt_db db;
    open_memory(&db);
    raw_exec(&db, "CREATE TRIGGER block_settings BEFORE INSERT ON settings "
                  "BEGIN SELECT RAISE(ABORT, 'blocked'); END");
    bt_db_error err;
    BT_CHECK(bt_repo_settings_save_tx(&db, "k", "v", &err) != 0);
    BT_CHECK(err.kind == BT_DB_ERR_EXEC);
    BT_CHECK(strcmp(err.step, "upsert settings") == 0);
    BT_CHECK(strstr(err.detail, "blocked") != NULL);
    bt_db_close(&db);
}

BT_TEST(repo_errors, settings_save_fails_inside_an_open_transaction) {
    bt_db db;
    open_memory(&db);
    bt_db_error err;
    BT_CHECK(bt_db_begin(&db, &err) == 0);
    BT_CHECK(bt_repo_settings_save(&db, "k", "v", &err) != 0);
    BT_CHECK(err.kind == BT_DB_ERR_EXEC);
    BT_CHECK(strcmp(err.step, "begin") == 0);
    bt_db_rollback(&db);
    bt_db_close(&db);
}

BT_TEST(repo_errors, srs_load_rows_fails_without_the_card_table) {
    bt_db db;
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_srs_load_error err;
    char path[BT_TU_PATH_MAX];
    open_latest_schema("srs-no-tables", "", &db, path, sizeof path);
    bt_srs_card_row *rows = NULL;
    size_t count = 0;
    BT_CHECK(bt_repo_srs_load_rows(&db, &cfg, BT_SRS_FILTER_ONLY_3BLD, &rows, &count, &err) != 0);
    BT_CHECK(err.kind == BT_SRS_LOAD_DB);
    BT_CHECK(err.db.kind == BT_DB_ERR_QUERY);
    BT_CHECK(strcmp(err.db.step, "select card") == 0);
    bt_db_close(&db);
}

BT_TEST(repo_errors, srs_insert_card_prepare_fails_stage_by_stage) {
    struct {
        const char *tag;
        const char *schema;
        const char *step;
    } cases[] = {
        {"card-table", "", "insert card"},
        {
            "state-table",
            "CREATE TABLE card(id INTEGER PRIMARY KEY, event TEXT, method TEXT, stage TEXT, "
            "case_key TEXT, created_at INTEGER, updated_at INTEGER)",
            "insert srs_state",
        },
        {
            "log-table",
            "CREATE TABLE card(id INTEGER PRIMARY KEY, event TEXT, method TEXT, stage TEXT, "
            "case_key TEXT, created_at INTEGER, updated_at INTEGER);"
            "CREATE TABLE srs_state(card_id INTEGER PRIMARY KEY, due_day INTEGER, interval_days "
            "INTEGER, reps INTEGER, lapses INTEGER, last_review_day INTEGER, stability REAL, "
            "difficulty REAL)",
            "insert review_log",
        },
    };
    for (int i = 0; i < 3; i++) {
        bt_db db;
        bt_db_error err;
        char path[BT_TU_PATH_MAX];
        open_latest_schema(cases[i].tag, cases[i].schema, &db, path, sizeof path);
        bt_card card = plain_card();
        BT_CHECK(bt_repo_srs_insert_card(&db, BT_DB_EVENT_3BLD, BT_DB_METHOD_3STYLE, "edge", "UF",
                                         &card, 1, &err) != 0);
        BT_CHECK(err.kind == BT_DB_ERR_EXEC);
        BT_CHECK(strcmp(err.step, cases[i].step) == 0);
        bt_card_free(&card);
        bt_db_close(&db);
    }
}

BT_TEST(repo_errors, srs_insert_card_step_failures_report_each_statement) {
    const char *schema =
        "CREATE TABLE card(id INTEGER PRIMARY KEY, event TEXT NOT NULL, method TEXT, stage TEXT, "
        "case_key TEXT, created_at INTEGER, updated_at INTEGER);"
        "CREATE TABLE srs_state(card_id INTEGER PRIMARY KEY, due_day INTEGER NOT NULL CHECK "
        "(due_day >= 0), interval_days INTEGER, reps INTEGER, lapses INTEGER, last_review_day "
        "INTEGER, stability REAL, difficulty REAL);"
        "CREATE TABLE review_log(id INTEGER PRIMARY KEY, card_id INTEGER, rating INTEGER NOT "
        "NULL CHECK (rating <= 4), reviewed_day INTEGER, interval_before INTEGER, "
        "interval_after INTEGER, elapsed_days INTEGER)";
    bt_db db;
    bt_db_error err;
    char path[BT_TU_PATH_MAX];
    open_latest_schema("srs-step-fails", schema, &db, path, sizeof path);
    bt_card null_bound = plain_card();
    BT_CHECK(bt_repo_srs_insert_card(&db, BT_DB_EVENT_3BLD, BT_DB_METHOD_3STYLE, "edge", "UF",
                                     &null_bound, 1, &err) == 0);
    bt_card_free(&null_bound);
    bt_card negative_due = plain_card();
    negative_due.due_day = -5;
    BT_CHECK(bt_repo_srs_insert_card(&db, BT_DB_EVENT_3BLD, BT_DB_METHOD_3STYLE, "edge", "UR",
                                     &negative_due, 1, &err) != 0);
    BT_CHECK(strcmp(err.step, "insert srs_state") == 0);
    BT_CHECK(strstr(err.detail, "due_day") != NULL);
    bt_card_free(&negative_due);
    bt_card bad_rating = plain_card();
    bad_rating.review_count = 1;
    bad_rating.reviews = malloc(sizeof(bt_review_entry));
    bad_rating.reviews[0].ts_days = 1;
    bad_rating.reviews[0].quality = 9;
    bad_rating.reviews[0].elapsed_days = 0;
    bad_rating.reviews[0].scheduled_days = 1;
    BT_CHECK(bt_repo_srs_insert_card(&db, BT_DB_EVENT_3BLD, BT_DB_METHOD_3STYLE, "edge", "UL",
                                     &bad_rating, 1, &err) != 0);
    BT_CHECK(strcmp(err.step, "insert review_log") == 0);
    bt_card_free(&bad_rating);
    bt_db_close(&db);
}

BT_TEST(repo_errors, srs_save_tx_fails_on_an_oversized_key) {
    bt_db db;
    open_memory(&db);
    bt_srs_store store;
    bt_srs_store_init(&store);
    size_t huge_len = BT_PATH_MAX + 16;
    char *huge = malloc(huge_len + 1);
    memset(huge, 'k', huge_len);
    huge[huge_len] = '\0';
    bt_card card = plain_card();
    BT_CHECK(bt_srs_store_set(&store, huge, &card) == 0);
    bt_db_error err;
    BT_CHECK(bt_repo_srs_save_tx(&db, &store, &err) != 0);
    BT_CHECK(err.kind == BT_DB_ERR_EXEC);
    BT_CHECK(strcmp(err.step, "insert card") == 0);
    bt_card_free(&card);
    bt_srs_store_free(&store);
    free(huge);
    bt_db_close(&db);
}

BT_TEST(repo_errors, srs_save_fails_inside_an_open_transaction_and_without_tables) {
    bt_db db;
    open_memory(&db);
    bt_db_error err;
    BT_CHECK(bt_db_begin(&db, &err) == 0);
    bt_srs_store store;
    bt_srs_store_init(&store);
    BT_CHECK(bt_repo_srs_save(&db, &store, &err) != 0);
    BT_CHECK(strcmp(err.step, "begin") == 0);
    bt_db_rollback(&db);
    bt_srs_store_free(&store);
    bt_db schemaless;
    char path[BT_TU_PATH_MAX];
    open_latest_schema("srs-save-no-tables", "", &schemaless, path, sizeof path);
    BT_CHECK(bt_repo_srs_save_tx(&schemaless, &store, &err) != 0);
    BT_CHECK(strcmp(err.step, "clear card") == 0);
    bt_srs_store_free(&store);
    bt_db_close(&schemaless);
    bt_db_close(&db);
}

BT_TEST(repo_errors, srs_load_rows_rejects_null_text_columns) {
    bt_db db;
    char path[BT_TU_PATH_MAX];
    open_latest_schema("srs-null-stage",
                       "CREATE TABLE card(id INTEGER PRIMARY KEY, event TEXT, method TEXT, "
                       "stage TEXT, case_key TEXT, created_at INTEGER, updated_at INTEGER);"
                       "CREATE TABLE srs_state(card_id INTEGER PRIMARY KEY, due_day INTEGER, "
                       "interval_days INTEGER, reps INTEGER, lapses INTEGER, last_review_day "
                       "INTEGER, stability REAL, difficulty REAL);"
                       "CREATE TABLE review_log(id INTEGER PRIMARY KEY, card_id INTEGER, rating "
                       "INTEGER, reviewed_day INTEGER, interval_before INTEGER, interval_after "
                       "INTEGER, elapsed_days INTEGER)",
                       &db, path, sizeof path);
    raw_exec(&db, "INSERT INTO card(id, event, method, stage, case_key, created_at, updated_at) "
                  "VALUES (1, '3bld', '3style', NULL, 'UF', 1, 1);"
                  "INSERT INTO srs_state(card_id, due_day, interval_days, reps, lapses, "
                  "last_review_day, stability, difficulty) VALUES (1, 2, 1, 1, 0, 1, NULL, NULL)");
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_srs_load_error err;
    bt_srs_card_row *rows = NULL;
    size_t count = 0;
    BT_CHECK(bt_repo_srs_load_rows(&db, &cfg, BT_SRS_FILTER_ONLY_3BLD, &rows, &count, &err) != 0);
    BT_CHECK(err.kind == BT_SRS_LOAD_DB);
    bt_db_close(&db);
}

static void seed_one_card(bt_db *db, const char *case_key, const char *stability,
                          const char *difficulty, const char *rating) {
    char sql[512];
    snprintf(sql, sizeof sql,
             "INSERT INTO card(id, event, method, stage, case_key, created_at, updated_at) "
             "VALUES (1, '3bld', '3style', 'edge', '%s', 1, 1);"
             "INSERT INTO srs_state(card_id, due_day, interval_days, reps, lapses, "
             "last_review_day, stability, difficulty) VALUES (1, 2, 1, 1, 0, 1, %s, %s)",
             case_key, stability, difficulty);
    raw_exec(db, sql);
    if (rating != NULL) {
        char log_sql[320];
        snprintf(log_sql, sizeof log_sql,
                 "INSERT INTO review_log(card_id, rating, reviewed_day, interval_before, "
                 "interval_after, elapsed_days) VALUES (1, %s, 1, 0, 1, 0)",
                 rating);
        raw_exec(db, log_sql);
    }
}

BT_TEST(repo_errors, srs_load_reports_an_unknown_review_quality) {
    bt_db db;
    open_memory(&db);
    seed_one_card(&db, "UF", "NULL", "NULL", "9");
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_srs_load_error err;
    bt_srs_store store;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &store, &err) != 0);
    BT_CHECK(err.kind == BT_SRS_LOAD_UNKNOWN_QUALITY);
    BT_CHECK(err.quality == 9);
    BT_CHECK(strcmp(err.key, "edge/UF") == 0);
    char text[512];
    bt_srs_load_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "review quality 9") != NULL);
    bt_db_close(&db);
}

BT_TEST(repo_errors, srs_load_reports_each_invalid_memory_shape) {
    struct {
        const char *tag;
        const char *stability;
        const char *difficulty;
        const char *rating;
        int want_stability;
        int want_difficulty;
    } cases[] = {
        {"stability-only", "0.5", "NULL", NULL, 1, 0},
        {"difficulty-only", "NULL", "5.0", NULL, 0, 1},
        {"stability-at-zero", "0.0", "5.0", NULL, 1, 1},
        {"difficulty-above-ceiling", "5.0", "11.0", NULL, 1, 1},
    };
    for (int i = 0; i < 4; i++) {
        bt_db db;
        open_memory(&db);
        seed_one_card(&db, "UF", cases[i].stability, cases[i].difficulty, cases[i].rating);
        bt_fsrs_config cfg;
        bt_fsrs_config_defaults(&cfg);
        bt_srs_load_error err;
        bt_srs_store store;
        BT_CHECK(bt_repo_srs_load(&db, &cfg, &store, &err) != 0);
        BT_CHECK(err.kind == BT_SRS_LOAD_INVALID_MEMORY);
        BT_CHECK(err.has_stability == cases[i].want_stability);
        BT_CHECK(err.has_difficulty == cases[i].want_difficulty);
        bt_db_close(&db);
    }
    bt_db db;
    open_memory(&db);
    seed_one_card(&db, "UF", "0.5", "5.0", NULL);
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_srs_load_error err;
    bt_srs_store store;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &store, &err) == 0);
    BT_CHECK(bt_srs_store_len(&store) == 1);
    const bt_card *loaded = bt_srs_store_get(&store, "edge/UF");
    BT_CHECK(loaded != NULL);
    BT_CHECK(loaded->has_memory == 1);
    BT_CHECK_F64_NEAR(loaded->memory.stability, 0.5, 1e-12);
    BT_CHECK_F64_NEAR(loaded->memory.difficulty, 5.0, 1e-12);
    BT_CHECK(bt_srs_store_get(&store, "edge/missing") == NULL);
    bt_srs_store_free(&store);
    bt_db_close(&db);
}

BT_TEST(repo_errors, srs_store_from_json_rejects_each_malformed_shape) {
    const char *bad[] = {
        "{}",
        "{\"cards\":[]}",
        "{\"cards\":{\"k\":1}}",
        "{\"cards\":{\"k\":{\"memory\":{\"stability\":1.0},\"interval_days\":1,\"due_day\":1,"
        "\"reps\":0,\"lapses\":0,\"reviews\":[]}}}",
        "{\"cards\":{\"k\":{\"interval_days\":1.5,\"due_day\":1,\"reps\":0,\"lapses\":0,"
        "\"reviews\":[]}}}",
        "{\"cards\":{\"k\":{\"interval_days\":1,\"due_day\":1,\"reps\":0,\"lapses\":0}}}",
        "{\"cards\":{\"k\":{\"interval_days\":1,\"due_day\":1,\"reps\":0,\"lapses\":0,"
        "\"reviews\":{}}}}",
        "{\"cards\":{\"k\":{\"interval_days\":1,\"due_day\":1,\"reps\":0,\"lapses\":0,"
        "\"reviews\":[{\"ts_days\":1,\"elapsed_days\":0,\"scheduled_days\":1}]}}}",
    };
    for (int i = 0; i < 8; i++) {
        bt_json_value *root = NULL;
        bt_json_error parse_err;
        BT_CHECK(bt_json_parse(bad[i], strlen(bad[i]), &root, &parse_err) == 0);
        bt_srs_store store;
        BT_CHECK(bt_srs_store_from_json(root, &store) != 0);
        bt_json_free(root);
    }
    const char *good =
        "{\"cards\":{\"edge/UF\":{\"interval_days\":3,\"due_day\":5,\"reps\":1,\"lapses\":0,"
        "\"reviews\":[]}}}";
    bt_json_value *root = NULL;
    bt_json_error parse_err;
    BT_CHECK(bt_json_parse(good, strlen(good), &root, &parse_err) == 0);
    bt_srs_store store;
    BT_CHECK(bt_srs_store_from_json(root, &store) == 0);
    BT_CHECK(bt_srs_store_len(&store) == 1);
    const bt_card *card = bt_srs_store_get(&store, "edge/UF");
    BT_CHECK(card != NULL);
    BT_CHECK(card->interval_days == 3);
    bt_srs_store_free(&store);
    bt_json_free(root);
}

BT_TEST(repo_errors, srs_store_set_replaces_orders_and_clones) {
    bt_srs_store store;
    bt_srs_store_init(&store);
    bt_card first = plain_card();
    first.reps = 1;
    bt_card second = plain_card();
    second.reps = 2;
    BT_CHECK(bt_srs_store_set(&store, "edge/UF", &first) == 0);
    BT_CHECK(bt_srs_store_set(&store, "corner/UFR", &second) == 0);
    BT_CHECK(strcmp(store.items[0].key, "corner/UFR") == 0);
    BT_CHECK(strcmp(store.items[1].key, "edge/UF") == 0);
    bt_card mid = plain_card();
    mid.reps = 3;
    BT_CHECK(bt_srs_store_set(&store, "edge/UM", &mid) == 0);
    BT_CHECK(strcmp(store.items[1].key, "edge/UF") == 0);
    BT_CHECK(strcmp(store.items[2].key, "edge/UM") == 0);
    bt_card replacement = plain_card();
    replacement.reps = 9;
    BT_CHECK(bt_srs_store_set(&store, "edge/UF", &replacement) == 0);
    BT_CHECK(bt_srs_store_len(&store) == 3);
    const bt_card *got = bt_srs_store_get(&store, "edge/UF");
    BT_CHECK(got != NULL && got->reps == 9);
    bt_srs_store clone;
    BT_CHECK(bt_srs_store_clone(&store, &clone) == 0);
    BT_CHECK(bt_srs_store_eq(&store, &clone) == 1);
    bt_card drift = plain_card();
    drift.reps = 42;
    bt_srs_store_set(&clone, "edge/UF", &drift);
    BT_CHECK(bt_srs_store_eq(&store, &clone) == 0);
    bt_srs_store_set(&clone, "edge/ZZ", &drift);
    BT_CHECK(bt_srs_store_eq(&store, &clone) == 0);
    bt_srs_store_free(&clone);
    bt_srs_store_free(&store);
    bt_card_free(&first);
    bt_card_free(&second);
    bt_card_free(&mid);
    bt_card_free(&replacement);
    bt_card_free(&drift);
}

BT_TEST(repo_errors, srs_load_rows_grows_review_and_row_capacity) {
    bt_db db;
    open_memory(&db);
    for (int card_at = 0; card_at < 9; card_at++) {
        char sql[720];
        snprintf(sql, sizeof sql,
                 "INSERT INTO card(id, event, method, stage, case_key, created_at, updated_at) "
                 "VALUES (%d, '3bld', '3style', 'edge', 'K%d', 1, 1);"
                 "INSERT INTO srs_state(card_id, due_day, interval_days, reps, lapses, "
                 "last_review_day, stability, difficulty) VALUES (%d, 2, 1, 1, 0, 1, NULL, NULL)",
                 card_at + 1, card_at, card_at + 1);
        raw_exec(&db, sql);
        for (int review_at = 0; review_at < 10; review_at++) {
            char log_sql[320];
            snprintf(log_sql, sizeof log_sql,
                     "INSERT INTO review_log(card_id, rating, reviewed_day, interval_before, "
                     "interval_after, elapsed_days) VALUES (%d, 3, %d, 0, 1, 0)",
                     card_at + 1, review_at);
            raw_exec(&db, log_sql);
        }
    }
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_srs_load_error err;
    bt_srs_card_row *rows = NULL;
    size_t count = 0;
    BT_CHECK(bt_repo_srs_load_rows(&db, &cfg, BT_SRS_FILTER_ONLY_3BLD, &rows, &count, &err) == 0);
    BT_CHECK(count == 9);
    BT_CHECK(rows != NULL && rows[0].card.review_count == 10);
    bt_repo_srs_free_rows(rows, count);
    bt_db_close(&db);
}
