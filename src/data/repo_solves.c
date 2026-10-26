#include "data/repo_solves.h"

#include "common/bt_trace.h"

#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"

void bt_solve_log_init(bt_solve_log *log) {
    BT_TRACE_ENTER("repo_solves.log_init");
    memset(log, 0, sizeof *log);
    BT_TRACE_EXIT("repo_solves.log_init");
}

void bt_solve_log_free(bt_solve_log *log) {
    BT_TRACE_ENTER("repo_solves.log_free");
    free(log->items);
    memset(log, 0, sizeof *log);
    BT_TRACE_EXIT("repo_solves.log_free");
}

int bt_solve_log_push(bt_solve_log *log, bt_solve_record record) {
    if (log->count == log->cap) {
        size_t cap = log->cap == 0 ? 8 : log->cap * 2;
        bt_solve_record *grown = realloc(log->items, cap * sizeof *grown);
        if (grown == NULL) {
            return -1;
        }
        log->items = grown;
        log->cap = cap;
    }
    log->items[log->count++] = record;
    return 0;
}

int bt_solve_log_eq(const bt_solve_log *a, const bt_solve_log *b) {
    if (a->count != b->count) {
        return 0;
    }
    for (size_t i = 0; i < a->count; i++) {
        if (a->items[i].started_ms != b->items[i].started_ms ||
            a->items[i].memo_ms != b->items[i].memo_ms ||
            a->items[i].exec_ms != b->items[i].exec_ms ||
            a->items[i].success != b->items[i].success) {
            return 0;
        }
    }
    return 1;
}

static void db_exec_step(bt_db *db, bt_db_error *err, bt_db_error_kind kind,
                         const char *step) {
    err->kind = kind;
    snprintf(err->step, sizeof err->step, "%s", step);
    snprintf(err->detail, sizeof err->detail, "%s", sqlite3_errmsg(bt_db_conn(db)));
}

int bt_solve_log_save_tx(bt_db *db, const bt_solve_log *log, bt_db_error *err) {
    BT_TRACE_ENTER("repo_solves.save_tx");
    if (bt_db_exec(db, "DELETE FROM solves", "clear solves", err) != 0) {
        BT_TRACE_FAIL("repo_solves.save_tx", "clear failed");
        return -1;
    }
    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "INSERT INTO solves(started_ms, memo_ms, exec_ms, success) VALUES (?, ?, ?, ?)";
    if (sqlite3_prepare_v2(bt_db_conn(db), sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_exec_step(db, err, BT_DB_ERR_EXEC, "insert solves");
        BT_TRACE_FAIL("repo_solves.save_tx", "prepare failed");
        return -1;
    }
    for (size_t i = 0; i < log->count; i++) {
        sqlite3_reset(stmt);
        sqlite3_bind_int64(stmt, 1, log->items[i].started_ms);
        sqlite3_bind_int64(stmt, 2, log->items[i].memo_ms);
        sqlite3_bind_int64(stmt, 3, log->items[i].exec_ms);
        sqlite3_bind_int64(stmt, 4, log->items[i].success ? 1 : 0);
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            db_exec_step(db, err, BT_DB_ERR_EXEC, "insert solves");
            sqlite3_finalize(stmt);
            BT_TRACE_FAIL("repo_solves.save_tx", "insert %zu failed", i);
            return -1;
        }
    }
    sqlite3_finalize(stmt);
    BT_TRACE_EXIT("repo_solves.save_tx");
    return 0;
}

int bt_solve_log_save(bt_db *db, const bt_solve_log *log, bt_db_error *err) {
    BT_TRACE_ENTER("repo_solves.save");
    if (bt_db_begin(db, err) != 0) {
        return -1;
    }
    if (bt_solve_log_save_tx(db, log, err) != 0) {
        bt_db_rollback(db);
        return -1;
    }
    if (bt_db_commit(db, err) != 0) {
        snprintf(err->step, sizeof err->step, "%s", "commit solves");
        bt_db_rollback(db);
        return -1;
    }
    BT_TRACE_EXIT("repo_solves.save");
    return 0;
}

int bt_solve_log_load(const bt_db *db, bt_solve_log *out, bt_db_error *err) {
    BT_TRACE_ENTER("repo_solves.load");
    bt_solve_log_init(out);
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT started_ms, memo_ms, exec_ms, success FROM solves ORDER BY id";
    if (sqlite3_prepare_v2(bt_db_conn((bt_db *)db), sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_exec_step((bt_db *)db, err, BT_DB_ERR_QUERY, "select solves");
        BT_TRACE_FAIL("repo_solves.load", "prepare failed");
        return -1;
    }
    int step = sqlite3_step(stmt);
    while (step == SQLITE_ROW) {
        bt_solve_record record = {
            sqlite3_column_int64(stmt, 0),
            sqlite3_column_int64(stmt, 1),
            sqlite3_column_int64(stmt, 2),
            sqlite3_column_int64(stmt, 3) != 0,
        };
        if (bt_solve_log_push(out, record) != 0) {
            sqlite3_finalize(stmt);
            bt_solve_log_free(out);
            db_exec_step((bt_db *)db, err, BT_DB_ERR_QUERY, "select solves");
            BT_TRACE_FAIL("repo_solves.load", "oom");
            return -1;
        }
        step = sqlite3_step(stmt);
    }
    if (step != SQLITE_DONE) {
        db_exec_step((bt_db *)db, err, BT_DB_ERR_QUERY, "select solves");
        sqlite3_finalize(stmt);
        bt_solve_log_free(out);
        BT_TRACE_FAIL("repo_solves.load", "step failed");
        return -1;
    }
    sqlite3_finalize(stmt);
    BT_TRACE_EXIT("repo_solves.load");
    return 0;
}

int bt_solve_log_from_json(const bt_json_value *root, bt_solve_log *out) {
    BT_TRACE_ENTER("repo_solves.from_json");
    bt_solve_log_init(out);
    bt_json_value *solves = bt_json_get(root, "solves");
    if (solves == NULL || solves->type != BT_JSON_ARRAY) {
        BT_TRACE_FAIL("repo_solves.from_json", "missing solves array");
        return -1;
    }
    for (size_t i = 0; i < solves->as.array.count; i++) {
        bt_json_value *item = bt_json_at(solves, i);
        bt_json_value *started = bt_json_get(item, "started_ms");
        bt_json_value *memo = bt_json_get(item, "memo_ms");
        bt_json_value *exec = bt_json_get(item, "exec_ms");
        bt_json_value *success = bt_json_get(item, "success");
        if (started == NULL || started->type != BT_JSON_NUMBER ||
            memo == NULL || memo->type != BT_JSON_NUMBER ||
            exec == NULL || exec->type != BT_JSON_NUMBER ||
            success == NULL || success->type != BT_JSON_BOOL) {
            bt_solve_log_free(out);
            BT_TRACE_FAIL("repo_solves.from_json", "bad record %zu", i);
            return -1;
        }
        bt_solve_record record = {
            (long long)started->as.number,
            (long long)memo->as.number,
            (long long)exec->as.number,
            success->as.boolean,
        };
        if (bt_solve_log_push(out, record) != 0) {
            bt_solve_log_free(out);
            return -1;
        }
    }
    BT_TRACE_EXIT("repo_solves.from_json");
    return 0;
}
