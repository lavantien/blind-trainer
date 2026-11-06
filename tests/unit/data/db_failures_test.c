#include "common/alloc.h"
#include "common/bt_harness.h"
#include "data/db.h"
#include "data/migrations_embedded.h"
#include "unit/data/test_util.h"

#include <direct.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "sqlite3.h"

static unsigned db_fail_counter;

static const char *db_fail_dir(const char *tag) {
    static char dir[BT_TU_PATH_MAX];
    const char *base = getenv("TEMP");
    snprintf(dir, sizeof dir, "%s/bt-db-failures/%s-%d-%u", base ? base : ".", tag, _getpid(),
             db_fail_counter++);
    tu_remove_tree(dir);
    tu_mkdir_p(dir);
    return dir;
}

static void craft_version_db(const char *path, const char *sql, int user_version) {
    sqlite3 *conn = NULL;
    if (sqlite3_open(path, &conn) == SQLITE_OK) {
        char pragma[64];
        snprintf(pragma, sizeof pragma, "PRAGMA user_version = %d", user_version);
        sqlite3_exec(conn, sql, NULL, NULL, NULL);
        sqlite3_exec(conn, pragma, NULL, NULL, NULL);
    }
    sqlite3_close(conn);
}

BT_TEST(db_failures, a0_migration_sql_fails_closed_when_the_workspace_read_starves) {
    const char *child_root = getenv("BT_DB_EMBEDDED_CHILD_ROOT");
    if (child_root != NULL && child_root[0] != '\0') {
        BT_CHECK(bt_db_migration_sql(2) == bt_db_embedded_migration_sql(2));
        return;
    }
    bt_alloc_fail_arm(0);
    const char *sql = bt_db_migration_sql(2);
    bt_alloc_fail_disarm();
    BT_CHECK(sql == NULL);
    BT_CHECK((int)bt_alloc_fail_count() > 0);
}

BT_TEST(db_failures, error_string_renders_each_db_error_kind) {
    bt_db_error err;
    char text[256];
    memset(&err, 0, sizeof err);
    err.kind = BT_DB_ERR_OPEN;
    snprintf(err.path, sizeof err.path, "%s", "root/store.db");
    snprintf(err.detail, sizeof err.detail, "%s", "unable to open database file");
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK_EQ(text, "open db root/store.db: unable to open database file");
    err.kind = BT_DB_ERR_MKDIR;
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "create db dir root/store.db") == text);
    err.kind = BT_DB_ERR_PRAGMA;
    snprintf(err.step, sizeof err.step, "%s", "foreign_keys");
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "pragma foreign_keys") == text);
    err.kind = BT_DB_ERR_MIGRATE;
    snprintf(err.step, sizeof err.step, "%s", "001_init");
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "migration 001_init") == text);
    err.kind = BT_DB_ERR_FUTURE;
    err.version = 9;
    err.latest = 3;
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "user_version 9 is newer than supported 3") != NULL);
    err.kind = BT_DB_ERR_BACKUP;
    snprintf(err.to_path, sizeof err.to_path, "%s", "store.db.bak");
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "backup root/store.db to store.db.bak") == text);
    err.kind = BT_DB_ERR_QUERY;
    snprintf(err.step, sizeof err.step, "%s", "user_version");
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "query user_version") == text);
    err.kind = BT_DB_ERR_EXEC;
    snprintf(err.step, sizeof err.step, "%s", "commit import");
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "exec commit import") == text);
}

BT_TEST(db_failures, now_ms_reads_a_utc_clock) {
    long long first = bt_db_now_ms();
    long long second = bt_db_now_ms();
    BT_CHECK(first > 1'600'000'000'000LL);
    BT_CHECK(second >= first);
}

BT_TEST(db_failures, a_db_path_nested_under_a_regular_file_fails_at_mkdir) {
    const char *dir = db_fail_dir("mkdir");
    char blocker[BT_TU_PATH_MAX];
    snprintf(blocker, sizeof blocker, "%s/blocker", dir);
    tu_write_file(blocker, "x");
    char db_path[BT_TU_PATH_MAX];
    snprintf(db_path, sizeof db_path, "%s/nested/store.db", blocker);
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(db_path, &db, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_MKDIR);
    char text[256];
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "create db dir") == text);
    tu_remove_tree(dir);
}

BT_TEST(db_failures, a_directory_at_the_backup_path_fails_the_backup_copy) {
    const char *dir = db_fail_dir("backup");
    char db_path[BT_TU_PATH_MAX];
    snprintf(db_path, sizeof db_path, "%s/store.db", dir);
    craft_version_db(db_path, "CREATE TABLE old_marker(x)", 1);
    char backup[BT_TU_PATH_MAX];
    snprintf(backup, sizeof backup, "%s.bak", db_path);
    tu_mkdir_p(backup);
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(db_path, &db, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_BACKUP);
    BT_CHECK_EQ(err.to_path, backup);
    char text[256];
    bt_db_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "backup") == text);
    tu_remove_tree(dir);
}

BT_TEST(db_failures, migration_sql_falls_back_to_the_embedded_text_in_a_repo_free_tree) {
    const char *root = getenv("BT_DB_EMBEDDED_CHILD_ROOT");
    if (root != NULL && root[0] != '\0') {
        for (int i = 0; i < bt_db_migration_count(); i++) {
            const char *sql = bt_db_migration_sql(i);
            BT_CHECK(sql != NULL);
            BT_CHECK(sql == bt_db_embedded_migration_sql(i));
        }
        char db_path[BT_TU_PATH_MAX];
        snprintf(db_path, sizeof db_path, "%s/child.db", root);
        bt_db db;
        bt_db_error err;
        BT_CHECK(bt_db_open_at(db_path, &db, &err) == 0);
        long long version = 0;
        BT_CHECK(bt_db_user_version(&db, &version, &err) == 0);
        BT_CHECK_EQ(version, bt_db_migration_count());
        bt_db_close(&db);
        return;
    }
    const char *dir = db_fail_dir("embedded");
    char bin_dir[BT_TU_PATH_MAX];
    snprintf(bin_dir, sizeof bin_dir, "%s/bin", dir);
    tu_mkdir_p(bin_dir);
    char config_dir[BT_TU_PATH_MAX];
    snprintf(config_dir, sizeof config_dir, "%s/config", dir);
    tu_mkdir_p(config_dir);
    char marker[BT_TU_PATH_MAX];
    snprintf(marker, sizeof marker, "%s/config/config.json", dir);
    tu_write_file(marker, "{}");
    char self[BT_TU_PATH_MAX];
    DWORD len = GetModuleFileNameA(NULL, self, sizeof self);
    BT_CHECK(len > 0 && len < sizeof self);
    char copied[BT_TU_PATH_MAX];
    snprintf(copied, sizeof copied, "%s/bin/suite.exe", dir);
    BT_CHECK(CopyFileA(self, copied, FALSE) != 0);
    char guard_value[BT_TU_PATH_MAX];
    snprintf(guard_value, sizeof guard_value, "%s", dir);
    _putenv_s("BT_DB_EMBEDDED_CHILD_ROOT", guard_value);
    intptr_t rc = _spawnl(_P_WAIT, copied, copied, NULL);
    _putenv_s("BT_DB_EMBEDDED_CHILD_ROOT", "");
    BT_CHECK(rc == 0);
    tu_remove_tree(dir);
}
