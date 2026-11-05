#include "common/bt_harness.h"
#include "common/json.h"
#include "data/db.h"
#include "data/fsrs.h"
#include "data/repo_algs.h"
#include "data/repo_cards.h"
#include "data/repo_seeds.h"
#include "data/repo_settings.h"
#include "data/repo_solves.h"
#include "data/repo_srs.h"
#include "data/repo_user.h"
#include "data/seedpack/mod.h"
#include "unit/data/test_util.h"

#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

BT_TEST(repo_errors, db_error_string_renders_every_kind) {
    struct {
        bt_db_error_kind kind;
        const char *needle;
    } cases[] = {
        {BT_DB_ERR_OPEN, "open db"},
        {BT_DB_ERR_MKDIR, "create db dir"},
        {BT_DB_ERR_PRAGMA, "pragma"},
        {BT_DB_ERR_MIGRATE, "migration"},
        {BT_DB_ERR_FUTURE, "newer than supported"},
        {BT_DB_ERR_BACKUP, "backup"},
        {BT_DB_ERR_QUERY, "query"},
        {BT_DB_ERR_EXEC, "exec"},
    };
    for (int i = 0; i < 8; i++) {
        bt_db_error err;
        memset(&err, 0, sizeof err);
        err.kind = cases[i].kind;
        snprintf(err.path, sizeof err.path, "%s", "p.db");
        snprintf(err.step, sizeof err.step, "%s", "the step");
        snprintf(err.detail, sizeof err.detail, "%s", "the detail");
        snprintf(err.to_path, sizeof err.to_path, "%s", "p.db.bak");
        err.version = 9;
        err.latest = 3;
        char text[256];
        bt_db_error_string(&err, text, sizeof text);
        BT_CHECK(strstr(text, cases[i].needle) != NULL);
    }
}

BT_TEST(repo_errors, db_open_at_reports_mkdir_failure_when_a_parent_is_a_file) {
    const char *dir = repo_err_dir("mkdir-blocked");
    char blocker[BT_TU_PATH_MAX];
    snprintf(blocker, sizeof blocker, "%s/blocker", dir);
    FILE *file = fopen(blocker, "wb");
    BT_CHECK(file != NULL);
    fputs("occupied", file);
    fclose(file);
    char db_path[BT_TU_PATH_MAX];
    snprintf(db_path, sizeof db_path, "%s/deeper/custom.db", blocker);
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(db_path, &db, &err) != 0);
    BT_CHECK(err.kind == BT_DB_ERR_MKDIR);
    char text[512];
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "create db dir") != NULL);
}

BT_TEST(repo_errors, db_open_at_rejects_a_future_user_version) {
    const char *dir = repo_err_dir("future");
    char db_path[BT_TU_PATH_MAX];
    snprintf(db_path, sizeof db_path, "%s/custom.db", dir);
    sqlite3 *conn = NULL;
    BT_CHECK(sqlite3_open(db_path, &conn) == SQLITE_OK);
    char *err_text = NULL;
    BT_CHECK(sqlite3_exec(conn, "PRAGMA user_version = 99", NULL, NULL, &err_text) == SQLITE_OK);
    sqlite3_close(conn);
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(db_path, &db, &err) != 0);
    BT_CHECK(err.kind == BT_DB_ERR_FUTURE);
    BT_CHECK(err.version == 99);
    BT_CHECK(err.latest == (long long)bt_db_migration_count());
    char text[512];
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "newer than supported") != NULL);
}

BT_TEST(repo_errors, db_migration_helpers_reject_out_of_range_indices) {
    int count = bt_db_migration_count();
    BT_CHECK(count > 0);
    BT_CHECK(bt_db_migration_name(-1) == NULL);
    BT_CHECK(bt_db_migration_name(count) == NULL);
    BT_CHECK(bt_db_migration_sql(-1) == NULL);
    BT_CHECK(bt_db_migration_sql(count) == NULL);
}

BT_TEST(repo_errors, db_migration_sql_second_call_returns_the_cached_text) {
    const char *first = bt_db_migration_sql(0);
    BT_CHECK(first != NULL);
    const char *second = bt_db_migration_sql(0);
    BT_CHECK(second == first);
    BT_CHECK(second[0] != '\0');
}

BT_TEST(repo_errors, db_begin_inside_a_transaction_and_bare_commit_both_fail) {
    bt_db db;
    open_memory(&db);
    bt_db_error err;
    BT_CHECK(bt_db_begin(&db, &err) == 0);
    BT_CHECK(bt_db_begin(&db, &err) != 0);
    BT_CHECK(err.kind == BT_DB_ERR_EXEC);
    BT_CHECK(strcmp(err.step, "begin") == 0);
    BT_CHECK(bt_db_commit(&db, &err) == 0);
    BT_CHECK(bt_db_commit(&db, &err) != 0);
    BT_CHECK(err.kind == BT_DB_ERR_EXEC);
    BT_CHECK(strcmp(err.step, "commit") == 0);
    bt_db_close(&db);
}

BT_TEST(repo_errors, db_exec_failure_carries_the_step_label) {
    bt_db db;
    open_memory(&db);
    bt_db_error err;
    BT_CHECK(bt_db_exec(&db, "DROP TABLE there_is_no_such_table", "probe step", &err) != 0);
    BT_CHECK(err.kind == BT_DB_ERR_EXEC);
    BT_CHECK(strcmp(err.step, "probe step") == 0);
    char text[512];
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "exec probe step") != NULL);
    bt_db_close(&db);
}

BT_TEST(repo_errors, db_is_fresh_reports_a_query_error_without_tables) {
    bt_db db;
    bt_db_error err;
    char path[BT_TU_PATH_MAX];
    open_latest_schema("fresh-no-tables", "", &db, path, sizeof path);
    int fresh = 1;
    BT_CHECK(bt_db_is_fresh(&db, &fresh, &err) != 0);
    BT_CHECK(err.kind == BT_DB_ERR_QUERY);
    BT_CHECK(strcmp(err.step, "user_alg") == 0);
    BT_CHECK(strstr(err.detail, "no such table") != NULL);
    bt_db_close(&db);
}

BT_TEST(repo_errors, db_backup_fails_when_the_bak_path_is_a_directory) {
    const char *dir = repo_err_dir("backup-blocked");
    char db_path[BT_TU_PATH_MAX];
    snprintf(db_path, sizeof db_path, "%s/custom.db", dir);
    sqlite3 *conn = NULL;
    BT_CHECK(sqlite3_open(db_path, &conn) == SQLITE_OK);
    char *err_text = NULL;
    BT_CHECK(sqlite3_exec(conn, "CREATE TABLE legacy(id INTEGER)", NULL, NULL, &err_text) ==
             SQLITE_OK);
    sqlite3_close(conn);
    char bak_dir[BT_TU_PATH_MAX];
    snprintf(bak_dir, sizeof bak_dir, "%s.bak", db_path);
    tu_mkdir_p(bak_dir);
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(db_path, &db, &err) != 0);
    BT_CHECK(err.kind == BT_DB_ERR_BACKUP);
    BT_CHECK(strcmp(err.to_path, bak_dir) == 0);
    char text[512];
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "backup") != NULL);
}

BT_TEST(repo_errors, db_open_at_accepts_a_bare_filename_in_the_working_directory) {
    const char *dir = repo_err_dir("bare-name");
    char previous[BT_TU_PATH_MAX];
    BT_CHECK(_getcwd(previous, sizeof previous) != NULL);
    BT_CHECK(_chdir(dir) == 0);
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at("bare.db", &db, &err) == 0);
    struct stat info;
    BT_CHECK(stat("bare.db", &info) == 0);
    bt_db_close(&db);
    BT_CHECK(_chdir(previous) == 0);
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

BT_TEST(repo_errors, algs_split_and_join_key_edges) {
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

BT_TEST(repo_errors, algs_save_fails_without_the_table_and_on_an_oversized_key) {
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

BT_TEST(repo_errors, algs_load_rejects_null_columns) {
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

BT_TEST(repo_errors, case_ref_cmp_orders_by_every_field) {
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

BT_TEST(repo_errors, overrides_replace_remove_and_lookup) {
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

BT_TEST(repo_errors, user_upsert_covers_update_and_insert_paths) {
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

BT_TEST(repo_errors, user_upsert_remove_and_load_failures) {
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

BT_TEST(repo_errors, user_load_all_rejects_null_columns) {
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

BT_TEST(repo_errors, cards_entry_falls_back_and_stores) {
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

BT_TEST(repo_errors, cards_load_and_save_failures) {
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

BT_TEST(repo_errors, seeds_empty_pack_is_a_no_op) {
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

BT_TEST(repo_errors, seeds_pack_tx_prepare_failures_without_and_with_narrow_tables) {
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

BT_TEST(repo_errors, seeds_pack_tx_verifies_drift_and_stale_rows) {
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

BT_TEST(repo_errors, seeds_pack_tx_reports_stale_null_keys_and_insert_step_failures) {
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

BT_TEST(repo_errors, seeds_counts_and_load_fail_without_the_table) {
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

BT_TEST(repo_errors, seeds_all_fails_on_a_missing_registry) {
    bt_db db;
    open_memory(&db);
    const char *dir = repo_err_dir("seeds-no-registry");
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

BT_TEST(repo_errors, solves_save_tx_fails_at_clear_and_prepare) {
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

BT_TEST(repo_errors, solves_save_and_load_failures) {
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

BT_TEST(repo_errors, solves_from_json_rejects_each_malformed_shape) {
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
