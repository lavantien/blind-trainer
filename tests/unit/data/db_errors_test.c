#include "common/bt_harness.h"
#include "data/db.h"
#include "unit/data/test_util.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "sqlite3.h"

static const char *db_err_dir(const char *tag) {
    return tu_temp_dir("bt-db-errors", tag);
}

static void craft_db(const char *path, const char *sql, int user_version) {
    sqlite3 *conn = NULL;
    if (sqlite3_open(path, &conn) == SQLITE_OK) {
        char pragma[64];
        snprintf(pragma, sizeof pragma, "PRAGMA user_version = %d", user_version);
        sqlite3_exec(conn, sql, NULL, NULL, NULL);
        sqlite3_exec(conn, pragma, NULL, NULL, NULL);
    }
    sqlite3_close(conn);
}

static long long read_user_version(const char *path) {
    sqlite3 *conn = NULL;
    long long version = -1;
    if (sqlite3_open_v2(path, &conn, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK) {
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(conn, "PRAGMA user_version", -1, &stmt, NULL) == SQLITE_OK &&
            sqlite3_step(stmt) == SQLITE_ROW) {
            version = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(conn);
    return version;
}

BT_TEST(db_errors, garbage_file_fails_at_the_user_version_query) {
    char dir[BT_TU_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", db_err_dir("garbage"));
    char path[BT_TU_PATH_MAX];
    snprintf(path, sizeof path, "%s/db.sqlite", dir);
    tu_write_file(path, "this is not a database file, just bytes");

    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(path, &db, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_QUERY);
    BT_CHECK_EQ(err.step, "user_version");
    char text[256];
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "query user_version:") == text);
    BT_CHECK(strstr(text, "file is not a database") != NULL);
    BT_CHECK(db.conn == NULL);
}

BT_TEST(db_errors, open_at_fails_when_a_parent_path_is_a_regular_file) {
    char dir[BT_TU_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", db_err_dir("parent-file"));
    char blocker[BT_TU_PATH_MAX];
    snprintf(blocker, sizeof blocker, "%s/blocker", dir);
    tu_write_file(blocker, "x");

    char path[BT_TU_PATH_MAX];
    snprintf(path, sizeof path, "%s/blocker/db.sqlite", dir);
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(path, &db, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_OPEN);
    BT_CHECK_EQ(err.path, path);
    char text[256];
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "open db ") == text);
    BT_CHECK(db.conn == NULL);
}

BT_TEST(db_errors, open_at_creates_missing_parent_directories) {
    char dir[BT_TU_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", db_err_dir("mkdir-ok"));
    char path[BT_TU_PATH_MAX];
    snprintf(path, sizeof path, "%s/a/b/c/db.sqlite", dir);

    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(path, &db, &err) == 0);
    struct stat info;
    BT_CHECK(stat(path, &info) == 0);
    bt_db_close(&db);
}

BT_TEST(db_errors, pending_migrations_back_up_the_file_first) {
    char dir[BT_TU_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", db_err_dir("backup-ok"));
    char path[BT_TU_PATH_MAX];
    snprintf(path, sizeof path, "%s/db.sqlite", dir);
    craft_db(path, "CREATE TABLE srs_state (key TEXT, ease INTEGER)", 1);

    char bak[BT_TU_PATH_MAX];
    snprintf(bak, sizeof bak, "%s.bak", path);
    bt_db db;
    bt_db_error err;
    if (bt_db_open_at(path, &db, &err) != 0) {
        char reason[256];
        bt_db_error_string(&err, reason, sizeof reason);
        BT_ABORT("open_at failed: %s", reason);
    }
    struct stat info;
    BT_CHECK(stat(bak, &info) == 0);
    BT_CHECK_EQ(read_user_version(path), 3);
    sqlite3 *conn = NULL;
    BT_CHECK(sqlite3_open_v2(path, &conn, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK);
    sqlite3_stmt *stmt = NULL;
    BT_CHECK(sqlite3_prepare_v2(conn,
                                "SELECT COUNT(*) FROM pragma_table_info('srs_state')"
                                " WHERE name = 'ease'",
                                -1, &stmt, NULL) == SQLITE_OK);
    BT_CHECK(sqlite3_step(stmt) == SQLITE_ROW);
    BT_CHECK_EQ(sqlite3_column_int64(stmt, 0), 0);
    sqlite3_finalize(stmt);
    sqlite3_close(conn);
    bt_db_close(&db);
}

BT_TEST(db_errors, a_conflicting_table_rolls_the_migration_back) {
    char dir[BT_TU_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", db_err_dir("conflict"));
    char path[BT_TU_PATH_MAX];
    snprintf(path, sizeof path, "%s/db.sqlite", dir);
    craft_db(path, "CREATE TABLE card (id INTEGER PRIMARY KEY)", 0);

    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(path, &db, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_MIGRATE);
    BT_CHECK_EQ(err.step, "001_init");
    char text[256];
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "migration 001_init:") == text);
    BT_CHECK(db.conn == NULL);
    BT_CHECK_EQ(read_user_version(path), 0);
    char bak[BT_TU_PATH_MAX];
    snprintf(bak, sizeof bak, "%s.bak", path);
    struct stat info;
    BT_CHECK(stat(bak, &info) == 0);
}

BT_TEST(db_errors, close_twice_and_conn_after_close_are_safe) {
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(&db, &err) == 0);
    BT_CHECK(bt_db_conn(&db) != NULL);
    bt_db_close(&db);
    bt_db_close(&db);
    BT_CHECK(bt_db_conn(&db) == NULL);
}

BT_TEST(db_errors, exec_and_last_insert_rowid_happy_path) {
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(&db, &err) == 0);
    BT_CHECK(bt_db_exec(&db, "CREATE TABLE marker (k)", "create-marker", &err) == 0);
    BT_CHECK(bt_db_exec(&db, "INSERT INTO marker VALUES (7)", "insert-marker", &err) == 0);
    BT_CHECK_EQ(bt_db_last_insert_rowid(&db), 1);
    BT_CHECK(bt_db_rollback(&db) == 0);
    bt_db_close(&db);
}

BT_TEST(db_errors, user_version_reports_the_landed_schema) {
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(&db, &err) == 0);
    long long version = 0;
    BT_CHECK(bt_db_user_version(&db, &version, &err) == 0);
    BT_CHECK_EQ(version, bt_db_migration_count());
    bt_db_close(&db);
}

BT_TEST(db_errors, migration_name_edges_reject_out_of_range) {
    BT_CHECK_EQ(bt_db_migration_name(0), "001_init");
    BT_CHECK(bt_db_migration_name(-1) == NULL);
    BT_CHECK(bt_db_migration_name(bt_db_migration_count()) == NULL);
    BT_CHECK(bt_db_migration_sql(-1) == NULL);
    BT_CHECK(bt_db_migration_sql(bt_db_migration_count()) == NULL);
}
