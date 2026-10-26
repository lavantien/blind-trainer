#include "data/repo_settings.h"

#include "common/bt_trace.h"

#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"

int bt_repo_settings_save_tx(bt_db *db, const char *key, const char *value,
                             bt_db_error *err) {
    BT_TRACE_ENTER("repo_settings.save_tx");
    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "INSERT INTO settings(key, value) VALUES (?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value";
    if (sqlite3_prepare_v2(bt_db_conn((bt_db *)db), sql, -1, &stmt, NULL) != SQLITE_OK) {
        err->kind = BT_DB_ERR_EXEC;
        snprintf(err->step, sizeof err->step, "%s", "upsert settings");
        snprintf(err->detail, sizeof err->detail, "%s",
                 sqlite3_errmsg(bt_db_conn((bt_db *)db)));
        BT_TRACE_FAIL("repo_settings.save_tx", "prepare failed");
        return -1;
    }
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        err->kind = BT_DB_ERR_EXEC;
        snprintf(err->step, sizeof err->step, "%s", "upsert settings");
        snprintf(err->detail, sizeof err->detail, "%s",
                 sqlite3_errmsg(bt_db_conn((bt_db *)db)));
        sqlite3_finalize(stmt);
        BT_TRACE_FAIL("repo_settings.save_tx", "step failed");
        return -1;
    }
    sqlite3_finalize(stmt);
    BT_TRACE_EXIT("repo_settings.save_tx");
    return 0;
}

int bt_repo_settings_save(bt_db *db, const char *key, const char *value,
                          bt_db_error *err) {
    BT_TRACE_ENTER("repo_settings.save");
    if (bt_db_begin(db, err) != 0) {
        return -1;
    }
    if (bt_repo_settings_save_tx(db, key, value, err) != 0) {
        bt_db_rollback(db);
        return -1;
    }
    if (bt_db_commit(db, err) != 0) {
        snprintf(err->step, sizeof err->step, "%s", "commit settings");
        bt_db_rollback(db);
        return -1;
    }
    BT_TRACE_EXIT("repo_settings.save");
    return 0;
}

int bt_repo_settings_load_text(const bt_db *db, const char *key, char **out,
                               bt_db_error *err) {
    BT_TRACE_ENTER("repo_settings.load_text");
    *out = NULL;
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT value FROM settings WHERE key = ?";
    if (sqlite3_prepare_v2(bt_db_conn((bt_db *)db), sql, -1, &stmt, NULL) != SQLITE_OK) {
        err->kind = BT_DB_ERR_QUERY;
        snprintf(err->step, sizeof err->step, "%s", "select settings");
        snprintf(err->detail, sizeof err->detail, "%s",
                 sqlite3_errmsg(bt_db_conn((bt_db *)db)));
        BT_TRACE_FAIL("repo_settings.load_text", "prepare failed");
        return -1;
    }
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
    int step = sqlite3_step(stmt);
    if (step == SQLITE_ROW) {
        const unsigned char *text = sqlite3_column_text(stmt, 0);
        size_t len = (size_t)sqlite3_column_bytes(stmt, 0);
        char *copy = malloc(len + 1);
        if (copy == NULL) {
            sqlite3_finalize(stmt);
            err->kind = BT_DB_ERR_QUERY;
            snprintf(err->step, sizeof err->step, "%s", "select settings");
            snprintf(err->detail, sizeof err->detail, "%s", "out of memory");
            BT_TRACE_FAIL("repo_settings.load_text", "oom");
            return -1;
        }
        memcpy(copy, text, len);
        copy[len] = '\0';
        *out = copy;
        sqlite3_finalize(stmt);
        BT_TRACE_EXIT("repo_settings.load_text");
        return 0;
    }
    if (step == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        BT_TRACE_EXIT("repo_settings.load_text");
        return 0;
    }
    err->kind = BT_DB_ERR_QUERY;
    snprintf(err->step, sizeof err->step, "%s", "select settings");
    snprintf(err->detail, sizeof err->detail, "%s",
             sqlite3_errmsg(bt_db_conn((bt_db *)db)));
    sqlite3_finalize(stmt);
    BT_TRACE_FAIL("repo_settings.load_text", "step failed");
    return -1;
}
