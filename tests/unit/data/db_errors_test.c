#include "common/bt_harness.h"
#include "data/db.h"
#include "unit/data/test_util.h"

#include <direct.h>
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

static void open_latest_schema(const char *tag, const char *schema_sql, bt_db *db, char *path_out,
                               size_t path_len) {
    const char *dir = db_err_dir(tag);
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

BT_TEST(db_errors, error_string_renders_every_kind) {
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

BT_TEST(db_errors, open_at_reports_mkdir_failure_when_a_parent_is_a_file) {
    const char *dir = db_err_dir("mkdir-blocked");
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

BT_TEST(db_errors, open_at_rejects_a_future_user_version) {
    const char *dir = db_err_dir("future");
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

BT_TEST(db_errors, migration_sql_second_call_returns_the_cached_text) {
    const char *first = bt_db_migration_sql(0);
    BT_CHECK(first != NULL);
    const char *second = bt_db_migration_sql(0);
    BT_CHECK(second == first);
    BT_CHECK(second[0] != '\0');
}

BT_TEST(db_errors, migration_helpers_reject_out_of_range_indices) {
    int count = bt_db_migration_count();
    BT_CHECK(count > 0);
    BT_CHECK(bt_db_migration_name(-1) == NULL);
    BT_CHECK(bt_db_migration_name(count) == NULL);
    BT_CHECK(bt_db_migration_sql(-1) == NULL);
    BT_CHECK(bt_db_migration_sql(count) == NULL);
}

BT_TEST(db_errors, begin_inside_a_transaction_and_bare_commit_both_fail) {
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

BT_TEST(db_errors, exec_failure_carries_the_step_label) {
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

BT_TEST(db_errors, is_fresh_reports_a_query_error_without_tables) {
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

BT_TEST(db_errors, backup_fails_when_the_bak_path_is_a_directory) {
    const char *dir = db_err_dir("backup-blocked");
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

BT_TEST(db_errors, open_at_accepts_a_bare_filename_in_the_working_directory) {
    const char *dir = db_err_dir("bare-name");
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
