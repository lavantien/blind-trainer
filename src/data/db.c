#include "data/db.h"

#include "common/bt_trace.h"

#include <direct.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "sqlite3.h"

static const char *const MIGRATION_NAMES[] = {
    "001_init",
    "002_drop_srs_ease",
    "003_seed_alg",
};

#define MIGRATION_COUNT ((int)(sizeof MIGRATION_NAMES / sizeof MIGRATION_NAMES[0]))

static char *migration_sql_cache[MIGRATION_COUNT];

static void db_fail(bt_db_error *err, bt_db_error_kind kind, const char *step, const char *path,
                    const char *fmt_detail) {
    err->kind = kind;
    snprintf(err->step, sizeof err->step, "%s", step ? step : "");
    if (path != NULL) {
        snprintf(err->path, sizeof err->path, "%s", path);
    } else {
        err->path[0] = '\0';
    }
    snprintf(err->detail, sizeof err->detail, "%s", fmt_detail ? fmt_detail : "");
}

void bt_db_error_string(const bt_db_error *err, char *out, size_t out_len) {
    switch (err->kind) {
    case BT_DB_ERR_OPEN:
        snprintf(out, out_len, "open db %s: %s", err->path, err->detail);
        break;
    case BT_DB_ERR_MKDIR:
        snprintf(out, out_len, "create db dir %s: %s", err->path, err->detail);
        break;
    case BT_DB_ERR_PRAGMA:
        snprintf(out, out_len, "pragma %s: %s", err->step, err->detail);
        break;
    case BT_DB_ERR_MIGRATE:
        snprintf(out, out_len, "migration %s: %s", err->step, err->detail);
        break;
    case BT_DB_ERR_FUTURE:
        snprintf(out, out_len, "db user_version %lld is newer than supported %lld", err->version,
                 err->latest);
        break;
    case BT_DB_ERR_BACKUP:
        snprintf(out, out_len, "backup %s to %s: %s", err->path, err->to_path, err->detail);
        break;
    case BT_DB_ERR_QUERY:
        snprintf(out, out_len, "query %s: %s", err->step, err->detail);
        break;
    case BT_DB_ERR_EXEC:
        snprintf(out, out_len, "exec %s: %s", err->step, err->detail);
        break;
    }
}

int bt_db_migration_count(void) {
    return MIGRATION_COUNT;
}

const char *bt_db_migration_name(int index) {
    if (index < 0 || index >= MIGRATION_COUNT) {
        return NULL;
    }
    return MIGRATION_NAMES[index];
}

const char *bt_db_migration_sql(int index) {
    if (index < 0 || index >= MIGRATION_COUNT) {
        return NULL;
    }
    if (migration_sql_cache[index] != NULL) {
        return migration_sql_cache[index];
    }
    char root[BT_PATH_MAX];
    bt_paths_workspace_root(root);
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/db/migrations/%s.sql", root, MIGRATION_NAMES[index]);
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        BT_TRACE_FAIL("db.migration_sql", "unreadable %s", path);
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = malloc((size_t)size + 1);
    if (text == NULL) {
        fclose(file);
        return NULL;
    }
    size_t read = fread(text, 1, (size_t)size, file);
    fclose(file);
    text[read] = '\0';
    migration_sql_cache[index] = text;
    return text;
}

void bt_db_backup_path(const char *path, char *out, size_t out_len) {
    snprintf(out, out_len, "%s.bak", path);
}

long long bt_db_now_ms(void) {
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    return (long long)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static int mkdir_parents(const char *path) {
    char buf[BT_PATH_MAX];
    snprintf(buf, sizeof buf, "%s", path);
    size_t len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '/' || buf[len - 1] == '\\')) {
        buf[--len] = '\0';
    }
    for (char *p = buf + (buf[0] && buf[1] == ':' ? 3 : 1); *p; p++) {
        if (*p == '/' || *p == '\\') {
            char save = *p;
            *p = '\0';
            _mkdir(buf);
            *p = save;
        }
    }
    if (_mkdir(buf) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static int has_parent(const char *path, char *parent, size_t parent_len) {
    snprintf(parent, parent_len, "%s", path);
    char *slash = strrchr(parent, '/');
    char *back = strrchr(parent, '\\');
    char *sep = slash > back ? slash : back;
    if (sep == NULL || sep == parent) {
        return 0;
    }
    *sep = '\0';
    if (parent[0] == '\0') {
        return 0;
    }
    return 1;
}

static int path_exists(const char *path) {
    struct stat info;
    return stat(path, &info) == 0;
}

static int copy_file_bytes(const char *from, const char *to, char *detail, size_t detail_len) {
    FILE *src = fopen(from, "rb");
    if (src == NULL) {
        snprintf(detail, detail_len, "%s", strerror(errno));
        return -1;
    }
    FILE *dst = fopen(to, "wb");
    if (dst == NULL) {
        snprintf(detail, detail_len, "%s", strerror(errno));
        fclose(src);
        return -1;
    }
    char chunk[8192];
    size_t read;
    while ((read = fread(chunk, 1, sizeof chunk, src)) > 0) {
        if (fwrite(chunk, 1, read, dst) != read) {
            snprintf(detail, detail_len, "short write");
            fclose(src);
            fclose(dst);
            return -1;
        }
    }
    int failed = ferror(src);
    fclose(src);
    fclose(dst);
    if (failed) {
        snprintf(detail, detail_len, "read error");
        return -1;
    }
    return 0;
}

static int query_user_version(sqlite3 *conn, long long *out, bt_db_error *err) {
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(conn, "PRAGMA user_version", -1, &stmt, NULL) != SQLITE_OK) {
        db_fail(err, BT_DB_ERR_QUERY, "user_version", NULL, sqlite3_errmsg(conn));
        return -1;
    }
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        db_fail(err, BT_DB_ERR_QUERY, "user_version", NULL, sqlite3_errmsg(conn));
        sqlite3_finalize(stmt);
        return -1;
    }
    *out = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return 0;
}

static int has_any_table(sqlite3 *conn, int *out, bt_db_error *err) {
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name NOT LIKE "
                      "'sqlite\\_%' ESCAPE '\\'";
    if (sqlite3_prepare_v2(conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_fail(err, BT_DB_ERR_QUERY, "sqlite_master", NULL, sqlite3_errmsg(conn));
        return -1;
    }
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        db_fail(err, BT_DB_ERR_QUERY, "sqlite_master", NULL, sqlite3_errmsg(conn));
        sqlite3_finalize(stmt);
        return -1;
    }
    *out = sqlite3_column_int64(stmt, 0) > 0;
    sqlite3_finalize(stmt);
    return 0;
}

static int backup_file(const bt_db *db, bt_db_error *err) {
    if (!db->has_path) {
        return 0;
    }
    char to[BT_PATH_MAX];
    bt_db_backup_path(db->path, to, sizeof to);
    char detail[192];
    if (copy_file_bytes(db->path, to, detail, sizeof detail) != 0) {
        db_fail(err, BT_DB_ERR_BACKUP, NULL, db->path, detail);
        snprintf(err->to_path, sizeof err->to_path, "%s", to);
        return -1;
    }
    return 0;
}

static int migrate(bt_db *db, bt_db_error *err) {
    long long current = 0;
    if (query_user_version(db->conn, &current, err) != 0) {
        return -1;
    }
    long long latest = MIGRATION_COUNT;
    if (current > latest) {
        err->kind = BT_DB_ERR_FUTURE;
        err->version = current;
        err->latest = latest;
        return -1;
    }
    long long first = current > 0 ? current : 0;
    for (int index = (int)first; index < MIGRATION_COUNT; index++) {
        if (index == (int)first) {
            int any = 0;
            if (has_any_table(db->conn, &any, err) != 0) {
                return -1;
            }
            if (any && backup_file(db, err) != 0) {
                return -1;
            }
        }
        const char *sql = bt_db_migration_sql(index);
        if (sql == NULL) {
            db_fail(err, BT_DB_ERR_MIGRATE, MIGRATION_NAMES[index], NULL,
                    "migration file unreadable");
            return -1;
        }
        char *exec_err = NULL;
        sqlite3_exec(db->conn, "BEGIN", NULL, NULL, &exec_err);
        if (exec_err != NULL) {
            db_fail(err, BT_DB_ERR_MIGRATE, MIGRATION_NAMES[index], NULL, exec_err);
            sqlite3_free(exec_err);
            return -1;
        }
        if (sqlite3_exec(db->conn, sql, NULL, NULL, &exec_err) != SQLITE_OK) {
            db_fail(err, BT_DB_ERR_MIGRATE, MIGRATION_NAMES[index], NULL, exec_err);
            sqlite3_free(exec_err);
            sqlite3_exec(db->conn, "ROLLBACK", NULL, NULL, NULL);
            return -1;
        }
        char pragma[64];
        snprintf(pragma, sizeof pragma, "PRAGMA user_version = %d", index + 1);
        if (sqlite3_exec(db->conn, pragma, NULL, NULL, &exec_err) != SQLITE_OK) {
            db_fail(err, BT_DB_ERR_MIGRATE, MIGRATION_NAMES[index], NULL, exec_err);
            sqlite3_free(exec_err);
            sqlite3_exec(db->conn, "ROLLBACK", NULL, NULL, NULL);
            return -1;
        }
        if (sqlite3_exec(db->conn, "COMMIT", NULL, NULL, &exec_err) != SQLITE_OK) {
            db_fail(err, BT_DB_ERR_MIGRATE, MIGRATION_NAMES[index], NULL, exec_err);
            sqlite3_free(exec_err);
            sqlite3_exec(db->conn, "ROLLBACK", NULL, NULL, NULL);
            return -1;
        }
        sqlite3_free(exec_err);
    }
    return 0;
}

static int finish_open(bt_db *db, bt_db_error *err) {
    BT_TRACE_ENTER("db.finish_open");
    char *exec_err = NULL;
    if (sqlite3_exec(db->conn, "PRAGMA foreign_keys = ON", NULL, NULL, &exec_err) != SQLITE_OK) {
        db_fail(err, BT_DB_ERR_PRAGMA, "foreign_keys", NULL,
                exec_err ? exec_err : sqlite3_errmsg(db->conn));
        sqlite3_free(exec_err);
        bt_db_close(db);
        BT_TRACE_FAIL("db.finish_open", "foreign_keys pragma failed");
        return -1;
    }
    sqlite3_free(exec_err);
    if (migrate(db, err) != 0) {
        bt_db_close(db);
        BT_TRACE_FAIL("db.finish_open", "migration failed");
        return -1;
    }
    BT_TRACE_EXIT("db.finish_open");
    return 0;
}

int bt_db_open_at(const char *path, bt_db *db, bt_db_error *err) {
    BT_TRACE_ENTER("db.open_at");
    memset(db, 0, sizeof *db);
    char parent[BT_PATH_MAX];
    if (has_parent(path, parent, sizeof parent) && !path_exists(parent) &&
        mkdir_parents(parent) != 0) {
        db_fail(err, BT_DB_ERR_MKDIR, NULL, parent, strerror(errno));
        BT_TRACE_FAIL("db.open_at", "cannot create %s", parent);
        return -1;
    }
    sqlite3 *conn = NULL;
    if (sqlite3_open_v2(path, &conn,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                        NULL) != SQLITE_OK) {
        const char *msg = conn ? sqlite3_errmsg(conn) : "cannot open";
        db_fail(err, BT_DB_ERR_OPEN, NULL, path, msg);
        sqlite3_close(conn);
        BT_TRACE_FAIL("db.open_at", "cannot open %s", path);
        return -1;
    }
    db->conn = conn;
    snprintf(db->path, sizeof db->path, "%s", path);
    db->has_path = 1;
    return finish_open(db, err);
}

int bt_db_open_memory(bt_db *db, bt_db_error *err) {
    BT_TRACE_ENTER("db.open_memory");
    memset(db, 0, sizeof *db);
    sqlite3 *conn = NULL;
    if (sqlite3_open_v2(":memory:", &conn,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                        NULL) != SQLITE_OK) {
        db_fail(err, BT_DB_ERR_OPEN, NULL, ":memory:", sqlite3_errmsg(conn));
        sqlite3_close(conn);
        BT_TRACE_FAIL("db.open_memory", "cannot open");
        return -1;
    }
    db->conn = conn;
    db->has_path = 0;
    return finish_open(db, err);
}

void bt_db_close(bt_db *db) {
    BT_TRACE_ENTER("db.close");
    if (db->conn != NULL) {
        sqlite3_close(db->conn);
        db->conn = NULL;
    }
    BT_TRACE_EXIT("db.close");
}

sqlite3 *bt_db_conn(const bt_db *db) {
    return db->conn;
}

int bt_db_begin(bt_db *db, bt_db_error *err) {
    char *exec_err = NULL;
    if (sqlite3_exec(db->conn, "BEGIN", NULL, NULL, &exec_err) != SQLITE_OK) {
        db_fail(err, BT_DB_ERR_EXEC, "begin", NULL, exec_err ? exec_err : sqlite3_errmsg(db->conn));
        sqlite3_free(exec_err);
        return -1;
    }
    sqlite3_free(exec_err);
    return 0;
}

int bt_db_commit(bt_db *db, bt_db_error *err) {
    char *exec_err = NULL;
    if (sqlite3_exec(db->conn, "COMMIT", NULL, NULL, &exec_err) != SQLITE_OK) {
        db_fail(err, BT_DB_ERR_EXEC, "commit", NULL,
                exec_err ? exec_err : sqlite3_errmsg(db->conn));
        sqlite3_free(exec_err);
        return -1;
    }
    sqlite3_free(exec_err);
    return 0;
}

int bt_db_rollback(bt_db *db) {
    sqlite3_exec(db->conn, "ROLLBACK", NULL, NULL, NULL);
    return 0;
}

long long bt_db_last_insert_rowid(bt_db *db) {
    return sqlite3_last_insert_rowid(db->conn);
}

int bt_db_user_version(bt_db *db, long long *out, bt_db_error *err) {
    return query_user_version(db->conn, out, err);
}

int bt_db_is_fresh(bt_db *db, int *fresh, bt_db_error *err) {
    static const char *const TABLES[] = {"user_alg", "card", "solves", "settings"};
    long long total = 0;
    for (int i = 0; i < 4; i++) {
        sqlite3_stmt *stmt = NULL;
        char sql[96];
        snprintf(sql, sizeof sql, "SELECT COUNT(*) FROM %s", TABLES[i]);
        if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
            db_fail(err, BT_DB_ERR_QUERY, TABLES[i], NULL, sqlite3_errmsg(db->conn));
            return -1;
        }
        if (sqlite3_step(stmt) != SQLITE_ROW) {
            db_fail(err, BT_DB_ERR_QUERY, TABLES[i], NULL, sqlite3_errmsg(db->conn));
            sqlite3_finalize(stmt);
            return -1;
        }
        total += sqlite3_column_int64(stmt, 0);
        sqlite3_finalize(stmt);
    }
    *fresh = total == 0;
    return 0;
}

int bt_db_exec(bt_db *db, const char *sql, const char *step, bt_db_error *err) {
    char *exec_err = NULL;
    if (sqlite3_exec(db->conn, sql, NULL, NULL, &exec_err) != SQLITE_OK) {
        db_fail(err, BT_DB_ERR_EXEC, step, NULL, exec_err ? exec_err : sqlite3_errmsg(db->conn));
        sqlite3_free(exec_err);
        return -1;
    }
    sqlite3_free(exec_err);
    return 0;
}
