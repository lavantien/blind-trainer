#include "common/bt_harness.h"
#include "common/paths.h"
#include "data/db.h"
#include "data/migrations_embedded.h"
#include "unit/data/test_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "sqlite3.h"

static unsigned dir_counter;

static const char *temp_dir(const char *tag) {
    static char dir[BT_TU_PATH_MAX];
    const char *base = getenv("TEMP");
    snprintf(dir, sizeof dir, "%s/bt-data-db-tests/%s-%d-%u", base ? base : ".", tag, _getpid(),
             dir_counter++);
    tu_remove_tree(dir);
    tu_mkdir_p(dir);
    return dir;
}

static void db_file_at(char *out, size_t n, const char *dir) {
    snprintf(out, n, "%s/blind-trainer.db", dir);
}

static void join_path(char *out, size_t n, const char *a, const char *b) {
    snprintf(out, n, "%s/%s", a, b);
}

static size_t table_columns(bt_db *db, const char *table, char names[16][32]) {
    char sql[160];
    snprintf(sql, sizeof sql, "PRAGMA table_info(%s)", table);
    sqlite3_stmt *stmt = NULL;
    BT_CHECK(sqlite3_prepare_v2(bt_db_conn(db), sql, -1, &stmt, NULL) == SQLITE_OK);
    size_t count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < 16) {
        snprintf(names[count], 32, "%s", sqlite3_column_text(stmt, 1));
        count++;
    }
    sqlite3_finalize(stmt);
    return count;
}

static void integrity_ok(bt_db *db) {
    sqlite3_stmt *stmt = NULL;
    BT_CHECK(sqlite3_prepare_v2(bt_db_conn(db), "PRAGMA integrity_check", -1, &stmt, NULL) ==
             SQLITE_OK);
    BT_CHECK(sqlite3_step(stmt) == SQLITE_ROW);
    BT_CHECK_EQ((const char *)sqlite3_column_text(stmt, 0), "ok");
    sqlite3_finalize(stmt);
}

static void raw_exec_path(const char *path, const char *sql) {
    sqlite3 *conn = NULL;
    BT_CHECK(sqlite3_open(path, &conn) == SQLITE_OK);
    char *err = NULL;
    BT_CHECK(sqlite3_exec(conn, sql, NULL, NULL, &err) == SQLITE_OK);
    sqlite3_close(conn);
}

static void seed_table(bt_db *db, const char *sql) {
    char *err = NULL;
    BT_CHECK(sqlite3_exec(bt_db_conn(db), sql, NULL, NULL, &err) == SQLITE_OK);
}

BT_TEST(db, fresh_open_migrates_to_latest_without_a_backup) {
    const char *dir = temp_dir("fresh");
    char db_path[BT_PATH_MAX];
    db_file_at(db_path, sizeof db_path, dir);
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(db_path, &db, &err) == 0);
    long long version = 0;
    BT_CHECK(bt_db_user_version(&db, &version, &err) == 0);
    BT_CHECK(version == bt_db_migration_count());
    integrity_ok(&db);
    struct stat info;
    BT_CHECK(stat(db_path, &info) == 0);
    char bak[BT_PATH_MAX];
    bt_db_backup_path(db_path, bak, sizeof bak);
    BT_CHECK(stat(bak, &info) != 0);
    bt_db_close(&db);
}

BT_TEST(db, schema_holds_every_mandated_table_and_column) {
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(&db, &err) == 0);
    char names[16][32];
    size_t count = table_columns(&db, "seed_alg", names);
    const char *seed_alg[] = {
        "event",      "method",     "stage",     "case_key",     "alg",
        "front_json", "source_url", "source_at", "content_hash",
    };
    BT_CHECK(count == 9);
    for (int i = 0; i < 9; i++) {
        BT_CHECK_EQ(names[i], seed_alg[i]);
    }
    count = table_columns(&db, "user_alg", names);
    const char *user_alg[] = {
        "event", "method", "stage", "case_key", "alg", "created_at", "updated_at",
    };
    BT_CHECK(count == 7);
    for (int i = 0; i < 7; i++) {
        BT_CHECK_EQ(names[i], user_alg[i]);
    }
    count = table_columns(&db, "card", names);
    const char *card[] = {"id", "event", "method", "stage", "case_key", "created_at", "updated_at"};
    BT_CHECK(count == 7);
    for (int i = 0; i < 7; i++) {
        BT_CHECK_EQ(names[i], card[i]);
    }
    count = table_columns(&db, "srs_state", names);
    const char *srs_state[] = {
        "card_id", "due_day",         "interval_days", "reps",
        "lapses",  "last_review_day", "stability",     "difficulty",
    };
    BT_CHECK(count == 8);
    for (int i = 0; i < 8; i++) {
        BT_CHECK_EQ(names[i], srs_state[i]);
    }
    count = table_columns(&db, "review_log", names);
    const char *review_log[] = {
        "id",           "card_id", "rating", "reviewed_day", "interval_before", "interval_after",
        "elapsed_days",
    };
    BT_CHECK(count == 7);
    for (int i = 0; i < 7; i++) {
        BT_CHECK_EQ(names[i], review_log[i]);
    }
    count = table_columns(&db, "solves", names);
    const char *solves[] = {"id", "started_ms", "memo_ms", "exec_ms", "success"};
    BT_CHECK(count == 5);
    for (int i = 0; i < 5; i++) {
        BT_CHECK_EQ(names[i], solves[i]);
    }
    count = table_columns(&db, "settings", names);
    BT_CHECK(count == 2);
    BT_CHECK_EQ(names[0], "key");
    BT_CHECK_EQ(names[1], "value");
    bt_db_close(&db);
}

BT_TEST(db, migrations_embed_the_committed_sql_files) {
    BT_CHECK(bt_db_migration_count() == 3);
    BT_CHECK_EQ(bt_db_migration_name(0), "001_init");
    BT_CHECK_EQ(bt_db_migration_name(1), "002_drop_srs_ease");
    BT_CHECK_EQ(bt_db_migration_name(2), "003_seed_alg");
    BT_CHECK(bt_db_embedded_migration_count() == 3);
    for (int i = 0; i < 3; i++) {
        const char *sql = bt_db_migration_sql(i);
        BT_CHECK(sql != NULL);
        BT_CHECK(sql[0] != '\0');
        const char *embedded = bt_db_embedded_migration_sql((size_t)i);
        BT_CHECK(embedded != NULL);
        BT_CHECK(strcmp(sql, embedded) == 0);
        BT_CHECK_EQ(bt_db_embedded_migration_name((size_t)i), bt_db_migration_name(i));
    }
}

BT_TEST(db, ease_column_drops_under_002_and_keeps_the_remaining_rows) {
    const char *dir = temp_dir("ease-drop");
    char db_path[BT_PATH_MAX];
    db_file_at(db_path, sizeof db_path, dir);
    sqlite3 *conn = NULL;
    BT_CHECK(sqlite3_open(db_path, &conn) == SQLITE_OK);
    char *err = NULL;
    BT_CHECK(sqlite3_exec(conn, bt_db_migration_sql(0), NULL, NULL, &err) == SQLITE_OK);
    char pragma[48];
    snprintf(pragma, sizeof pragma, "PRAGMA user_version = 1");
    BT_CHECK(sqlite3_exec(conn, pragma, NULL, NULL, &err) == SQLITE_OK);
    const char *seed = "INSERT INTO card(event, method, stage, case_key, created_at, updated_at)"
                       " VALUES ('3bld', '3style', 'edge', 'UF', 1, 1);"
                       "INSERT INTO srs_state(card_id, due_day, interval_days, ease, reps, lapses, "
                       "last_review_day)"
                       " VALUES ((SELECT MAX(id) FROM card), 10, 6, 2.5, 2, 0, 4);"
                       "INSERT INTO review_log(card_id, rating, reviewed_day, interval_before, "
                       "interval_after, elapsed_days)"
                       " VALUES ((SELECT MAX(id) FROM card), 4, 4, 0, 6, 3);";
    BT_CHECK(sqlite3_exec(conn, seed, NULL, NULL, &err) == SQLITE_OK);
    sqlite3_close(conn);
    bt_db db;
    bt_db_error db_err;
    BT_CHECK(bt_db_open_at(db_path, &db, &db_err) == 0);
    long long version = 0;
    BT_CHECK(bt_db_user_version(&db, &version, &db_err) == 0);
    BT_CHECK(version == bt_db_migration_count());
    char names[16][32];
    size_t count = table_columns(&db, "srs_state", names);
    int has_ease = 0;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(names[i], "ease") == 0) {
            has_ease = 1;
        }
    }
    BT_CHECK(has_ease == 0);
    sqlite3_stmt *stmt = NULL;
    BT_CHECK(sqlite3_prepare_v2(bt_db_conn(&db),
                                "SELECT due_day, interval_days, reps FROM srs_state", -1, &stmt,
                                NULL) == SQLITE_OK);
    BT_CHECK(sqlite3_step(stmt) == SQLITE_ROW);
    BT_CHECK(sqlite3_column_int64(stmt, 0) == 10);
    BT_CHECK(sqlite3_column_int64(stmt, 1) == 6);
    BT_CHECK(sqlite3_column_int64(stmt, 2) == 2);
    sqlite3_finalize(stmt);
    BT_CHECK(sqlite3_prepare_v2(bt_db_conn(&db), "SELECT COUNT(*) FROM review_log", -1, &stmt,
                                NULL) == SQLITE_OK);
    BT_CHECK(sqlite3_step(stmt) == SQLITE_ROW);
    BT_CHECK(sqlite3_column_int64(stmt, 0) == 1);
    sqlite3_finalize(stmt);
    char bak[BT_PATH_MAX];
    bt_db_backup_path(db_path, bak, sizeof bak);
    struct stat info;
    BT_CHECK(stat(bak, &info) == 0);
    bt_db_close(&db);
}

static char *read_all_bytes(const char *path, size_t *out_len) {
    FILE *file = fopen(path, "rb");
    BT_CHECK(file != NULL);
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *bytes = malloc((size_t)size + 1);
    BT_CHECK(fread(bytes, 1, (size_t)size, file) == (size_t)size);
    fclose(file);
    *out_len = (size_t)size;
    return bytes;
}

BT_TEST(db, backup_before_migrate_keeps_pre_migration_bytes) {
    const char *dir = temp_dir("backup");
    char db_path[BT_PATH_MAX];
    db_file_at(db_path, sizeof db_path, dir);
    raw_exec_path(db_path, "CREATE TABLE legacy_data(id INTEGER PRIMARY KEY, note TEXT);"
                           "INSERT INTO legacy_data(note) VALUES ('keep me');");
    size_t before_len = 0;
    char *before = read_all_bytes(db_path, &before_len);
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(db_path, &db, &err) == 0);
    long long version = 0;
    BT_CHECK(bt_db_user_version(&db, &version, &err) == 0);
    BT_CHECK(version == bt_db_migration_count());
    char bak[BT_PATH_MAX];
    bt_db_backup_path(db_path, bak, sizeof bak);
    size_t bak_len = 0;
    char *bak_bytes = read_all_bytes(bak, &bak_len);
    BT_CHECK(bak_len == before_len);
    BT_CHECK(memcmp(before, bak_bytes, before_len) == 0);
    free(before);
    free(bak_bytes);
    sqlite3_stmt *stmt = NULL;
    BT_CHECK(sqlite3_prepare_v2(bt_db_conn(&db), "SELECT COUNT(*) FROM legacy_data", -1, &stmt,
                                NULL) == SQLITE_OK);
    BT_CHECK(sqlite3_step(stmt) == SQLITE_ROW);
    BT_CHECK(sqlite3_column_int64(stmt, 0) == 1);
    sqlite3_finalize(stmt);
    bt_db_close(&db);
}

BT_TEST(db, failed_migration_rolls_back_version_and_schema) {
    const char *dir = temp_dir("rollback");
    char db_path[BT_PATH_MAX];
    db_file_at(db_path, sizeof db_path, dir);
    raw_exec_path(db_path, "CREATE TABLE user_alg(event TEXT, method TEXT, stage TEXT, "
                           "case_key TEXT, alg TEXT);");
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(db_path, &db, &err) != 0);
    char text[512];
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "migration 001_init") != NULL);
    sqlite3 *conn = NULL;
    BT_CHECK(sqlite3_open(db_path, &conn) == SQLITE_OK);
    sqlite3_stmt *stmt = NULL;
    BT_CHECK(sqlite3_prepare_v2(conn, "PRAGMA user_version", -1, &stmt, NULL) == SQLITE_OK);
    BT_CHECK(sqlite3_step(stmt) == SQLITE_ROW);
    BT_CHECK(sqlite3_column_int64(stmt, 0) == 0);
    sqlite3_finalize(stmt);
    sqlite3_close(conn);
}

BT_TEST(db, open_fails_closed_when_the_db_path_is_unopenable) {
    const char *dir = temp_dir("unopenable");
    char blocker[BT_PATH_MAX];
    join_path(blocker, sizeof blocker, dir, "blocker");
    FILE *file = fopen(blocker, "wb");
    BT_CHECK(file != NULL);
    fputs("occupied", file);
    fclose(file);
    char db_path[BT_PATH_MAX];
    join_path(db_path, sizeof db_path, blocker, "blind-trainer.db");
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(db_path, &db, &err) != 0);
    char text[512];
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "open db") != NULL);
    BT_CHECK(strstr(text, "blind-trainer.db") != NULL);
}

BT_TEST(db, open_fails_closed_on_a_corrupt_db_file) {
    const char *dir = temp_dir("corrupt");
    char db_path[BT_PATH_MAX];
    db_file_at(db_path, sizeof db_path, dir);
    FILE *file = fopen(db_path, "wb");
    BT_CHECK(file != NULL);
    fputs("definitely not a sqlite database", file);
    fclose(file);
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(db_path, &db, &err) != 0);
    char text[512];
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "user_version") != NULL);
}

static void seed_settings_row(bt_db *db) {
    seed_table(db, "INSERT INTO settings(key, value) VALUES ('k', 'v')");
}

BT_TEST(db, fresh_db_reports_empty_and_fills_after_a_row) {
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(&db, &err) == 0);
    int fresh = 0;
    BT_CHECK(bt_db_is_fresh(&db, &fresh, &err) == 0);
    BT_CHECK(fresh == 1);
    seed_settings_row(&db);
    BT_CHECK(bt_db_is_fresh(&db, &fresh, &err) == 0);
    BT_CHECK(fresh == 0);
    bt_db_close(&db);
}

BT_TEST(db, is_fresh_is_false_for_each_single_nonempty_table) {
    const char *seeds[] = {
        "INSERT INTO user_alg(stage, case_key, alg, created_at, updated_at) VALUES ('s', 'c', 'a', 0, 0)",
        "INSERT INTO card(stage, case_key, created_at, updated_at) VALUES ('s', 'c', 0, 0)",
        "INSERT INTO solves(started_ms, memo_ms, exec_ms, success) VALUES (0, 0, 0, 1)",
        "INSERT INTO settings(key, value) VALUES ('k', 'v')",
    };
    for (int i = 0; i < 4; i++) {
        bt_db db;
        bt_db_error err;
        BT_CHECK(bt_db_open_memory(&db, &err) == 0);
        seed_table(&db, seeds[i]);
        int fresh = 1;
        BT_CHECK(bt_db_is_fresh(&db, &fresh, &err) == 0);
        BT_CHECK(fresh == 0);
        bt_db_close(&db);
    }
}

BT_TEST(db, is_fresh_is_false_when_two_equal_counts_could_cancel) {
    const char *partners[] = {
        "INSERT INTO card(stage, case_key, created_at, updated_at) VALUES ('s', 'c', 0, 0)",
        "INSERT INTO solves(started_ms, memo_ms, exec_ms, success) VALUES (0, 0, 0, 1)",
        "INSERT INTO settings(key, value) VALUES ('k', 'v')",
    };
    for (int i = 0; i < 3; i++) {
        bt_db db;
        bt_db_error err;
        BT_CHECK(bt_db_open_memory(&db, &err) == 0);
        seed_table(&db, "INSERT INTO user_alg(stage, case_key, alg, created_at, updated_at) VALUES "
                        "('s', 'c', 'a', 0, 0)");
        seed_table(&db, partners[i]);
        int fresh = 1;
        BT_CHECK(bt_db_is_fresh(&db, &fresh, &err) == 0);
        BT_CHECK(fresh == 0);
        bt_db_close(&db);
    }
}

BT_TEST(db, reopening_a_fully_migrated_db_succeeds) {
    const char *dir = temp_dir("reopen");
    char db_path[BT_PATH_MAX];
    db_file_at(db_path, sizeof db_path, dir);
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(db_path, &db, &err) == 0);
    long long version = 0;
    BT_CHECK(bt_db_user_version(&db, &version, &err) == 0);
    BT_CHECK(version == bt_db_migration_count());
    bt_db_close(&db);
    bt_db reopened;
    BT_CHECK(bt_db_open_at(db_path, &reopened, &err) == 0);
    BT_CHECK(bt_db_user_version(&reopened, &version, &err) == 0);
    BT_CHECK(version == bt_db_migration_count());
    bt_db_close(&reopened);
}

BT_TEST(db, open_at_creates_missing_parent_directories) {
    const char *dir = temp_dir("nested");
    char db_path[BT_PATH_MAX];
    snprintf(db_path, sizeof db_path, "%s/deep/nested/blind-trainer.db", dir);
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(db_path, &db, &err) == 0);
    bt_db_close(&db);
    struct stat info;
    BT_CHECK(stat(db_path, &info) == 0);
}

BT_TEST(db, path_reports_the_file_for_disk_and_none_for_memory) {
    const char *dir = temp_dir("path");
    char db_path[BT_PATH_MAX];
    db_file_at(db_path, sizeof db_path, dir);
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(db_path, &db, &err) == 0);
    BT_CHECK(db.has_path == 1);
    BT_CHECK(strcmp(db.path, db_path) == 0);
    bt_db_close(&db);
    bt_db memory;
    BT_CHECK(bt_db_open_memory(&memory, &err) == 0);
    BT_CHECK(memory.has_path == 0);
    bt_db_close(&memory);
}

BT_TEST(db, now_ms_is_epoch_millis_and_monotonic) {
    long long first = bt_db_now_ms();
    long long second = bt_db_now_ms();
    BT_CHECK(first > 1700000000000LL);
    BT_CHECK(second >= first);
}
