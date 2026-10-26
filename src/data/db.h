#ifndef BT_DATA_DB_H
#define BT_DATA_DB_H

#include "common/paths.h"

#include <stddef.h>

#define BT_DB_EVENT_3BLD "3bld"
#define BT_DB_METHOD_3STYLE "3style"

typedef struct sqlite3 sqlite3;

typedef enum {
    BT_DB_ERR_OPEN,
    BT_DB_ERR_MKDIR,
    BT_DB_ERR_PRAGMA,
    BT_DB_ERR_MIGRATE,
    BT_DB_ERR_FUTURE,
    BT_DB_ERR_BACKUP,
    BT_DB_ERR_QUERY,
    BT_DB_ERR_EXEC,
} bt_db_error_kind;

typedef struct {
    bt_db_error_kind kind;
    char path[BT_PATH_MAX];
    char step[48];
    char detail[192];
    long long version;
    long long latest;
    char to_path[BT_PATH_MAX];
} bt_db_error;

typedef struct {
    sqlite3 *conn;
    char path[BT_PATH_MAX];
    int has_path;
} bt_db;

void bt_db_error_string(const bt_db_error *err, char *out, size_t out_len);
int bt_db_migration_count(void);
const char *bt_db_migration_name(int index);
const char *bt_db_migration_sql(int index);
void bt_db_backup_path(const char *path, char *out, size_t out_len);
long long bt_db_now_ms(void);
int bt_db_open_at(const char *path, bt_db *db, bt_db_error *err);
int bt_db_open_memory(bt_db *db, bt_db_error *err);
void bt_db_close(bt_db *db);
sqlite3 *bt_db_conn(bt_db *db);
int bt_db_begin(bt_db *db, bt_db_error *err);
int bt_db_commit(bt_db *db, bt_db_error *err);
int bt_db_rollback(bt_db *db);
long long bt_db_last_insert_rowid(bt_db *db);
int bt_db_user_version(bt_db *db, long long *out, bt_db_error *err);
int bt_db_is_fresh(bt_db *db, int *fresh, bt_db_error *err);
int bt_db_exec(bt_db *db, const char *sql, const char *step, bt_db_error *err);

#endif
