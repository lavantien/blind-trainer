#include "data/repo_algs.h"

#include "common/bt_trace.h"

#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"

void bt_repo_algs_split_key(const char *key, char *stage, size_t stage_len, char *case_key,
                            size_t case_key_len) {
    BT_TRACE_ENTER("repo_algs.split_key");
    const char *slash = strchr(key, '/');
    if (slash == NULL) {
        snprintf(stage, stage_len, "%s", key);
        snprintf(case_key, case_key_len, "%s", "");
        BT_TRACE_EXIT("repo_algs.split_key");
        return;
    }
    size_t head = (size_t)(slash - key);
    if (head >= stage_len) {
        head = stage_len - 1;
    }
    memcpy(stage, key, head);
    stage[head] = '\0';
    snprintf(case_key, case_key_len, "%s", slash + 1);
    BT_TRACE_EXIT("repo_algs.split_key");
}

void bt_repo_algs_join_key(const char *stage, const char *case_key, char *out, size_t out_len) {
    BT_TRACE_ENTER("repo_algs.join_key");
    if (case_key[0] == '\0') {
        snprintf(out, out_len, "%s", stage);
    } else {
        snprintf(out, out_len, "%s/%s", stage, case_key);
    }
    BT_TRACE_EXIT("repo_algs.join_key");
}

static void db_step_error(const bt_db *db, bt_db_error *err, bt_db_error_kind kind,
                          const char *step) {
    err->kind = kind;
    snprintf(err->step, sizeof err->step, "%s", step);
    snprintf(err->detail, sizeof err->detail, "%s", sqlite3_errmsg(bt_db_conn(db)));
}

int bt_repo_algs_save_tx(bt_db *db, const bt_user_algs *algs, bt_db_error *err) {
    BT_TRACE_ENTER("repo_algs.save_tx");
    sqlite3_stmt *clear = NULL;
    const char *clear_sql = "DELETE FROM user_alg WHERE event = ? AND method = ?";
    if (sqlite3_prepare_v2(bt_db_conn(db), clear_sql, -1, &clear, NULL) != SQLITE_OK) {
        db_step_error(db, err, BT_DB_ERR_EXEC, "clear user_alg");
        BT_TRACE_FAIL("repo_algs.save_tx", "prepare clear failed");
        return -1;
    }
    sqlite3_bind_text(clear, 1, BT_DB_EVENT_3BLD, -1, SQLITE_STATIC);
    sqlite3_bind_text(clear, 2, BT_DB_METHOD_3STYLE, -1, SQLITE_STATIC);
    if (sqlite3_step(clear) != SQLITE_DONE) {
        db_step_error(db, err, BT_DB_ERR_EXEC, "clear user_alg");
        sqlite3_finalize(clear);
        BT_TRACE_FAIL("repo_algs.save_tx", "clear failed");
        return -1;
    }
    sqlite3_finalize(clear);
    sqlite3_stmt *insert = NULL;
    const char *insert_sql =
        "INSERT INTO user_alg(event, method, stage, case_key, alg, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(bt_db_conn(db), insert_sql, -1, &insert, NULL) != SQLITE_OK) {
        db_step_error(db, err, BT_DB_ERR_EXEC, "insert user_alg");
        BT_TRACE_FAIL("repo_algs.save_tx", "prepare insert failed");
        return -1;
    }
    long long now = bt_db_now_ms();
    for (size_t i = 0; i < algs->count; i++) {
        char stage[BT_PATH_MAX];
        char case_key[BT_PATH_MAX];
        bt_repo_algs_split_key(algs->items[i].key, stage, sizeof stage, case_key, sizeof case_key);
        sqlite3_reset(insert);
        sqlite3_bind_text(insert, 1, BT_DB_EVENT_3BLD, -1, SQLITE_STATIC);
        sqlite3_bind_text(insert, 2, BT_DB_METHOD_3STYLE, -1, SQLITE_STATIC);
        sqlite3_bind_text(insert, 3, stage, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insert, 4, case_key, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insert, 5, algs->items[i].alg, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(insert, 6, now);
        sqlite3_bind_int64(insert, 7, now);
        if (sqlite3_step(insert) != SQLITE_DONE) {
            db_step_error(db, err, BT_DB_ERR_EXEC, "insert user_alg");
            sqlite3_finalize(insert);
            BT_TRACE_FAIL("repo_algs.save_tx", "insert %zu failed", i);
            return -1;
        }
    }
    sqlite3_finalize(insert);
    BT_TRACE_EXIT("repo_algs.save_tx");
    return 0;
}

int bt_repo_algs_save(bt_db *db, const bt_user_algs *algs, bt_db_error *err) {
    BT_TRACE_ENTER("repo_algs.save");
    if (bt_db_begin(db, err) != 0) {
        return -1;
    }
    if (bt_repo_algs_save_tx(db, algs, err) != 0) {
        bt_db_rollback(db);
        return -1;
    }
    if (bt_db_commit(db, err) != 0) {
        snprintf(err->step, sizeof err->step, "%s", "commit user_alg");
        bt_db_rollback(db);
        return -1;
    }
    BT_TRACE_EXIT("repo_algs.save");
    return 0;
}

int bt_repo_algs_load(const bt_db *db, bt_user_algs *out, bt_db_error *err) {
    BT_TRACE_ENTER("repo_algs.load");
    bt_user_algs_init(out);
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT stage, case_key, alg FROM user_alg WHERE event = ? AND method = ?";
    if (sqlite3_prepare_v2(bt_db_conn(db), sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_step_error(db, err, BT_DB_ERR_QUERY, "select user_alg");
        BT_TRACE_FAIL("repo_algs.load", "prepare failed");
        return -1;
    }
    sqlite3_bind_text(stmt, 1, BT_DB_EVENT_3BLD, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, BT_DB_METHOD_3STYLE, -1, SQLITE_STATIC);
    int step = sqlite3_step(stmt);
    while (step == SQLITE_ROW) {
        char stage[BT_PATH_MAX];
        char case_key[BT_PATH_MAX];
        char key[BT_REPO_ALGS_KEY_MAX];
        snprintf(stage, sizeof stage, "%s", sqlite3_column_text(stmt, 0));
        snprintf(case_key, sizeof case_key, "%s", sqlite3_column_text(stmt, 1));
        const unsigned char *alg = sqlite3_column_text(stmt, 2);
        bt_repo_algs_join_key(stage, case_key, key, sizeof key);
        if (bt_user_algs_set_entry(out, key, (const char *)alg) != 0) {
            sqlite3_finalize(stmt);
            bt_user_algs_free(out);
            db_step_error(db, err, BT_DB_ERR_QUERY, "select user_alg");
            BT_TRACE_FAIL("repo_algs.load", "insert failed");
            return -1;
        }
        step = sqlite3_step(stmt);
    }
    if (step != SQLITE_DONE) {
        db_step_error(db, err, BT_DB_ERR_QUERY, "select user_alg");
        sqlite3_finalize(stmt);
        bt_user_algs_free(out);
        BT_TRACE_FAIL("repo_algs.load", "step failed");
        return -1;
    }
    sqlite3_finalize(stmt);
    BT_TRACE_EXIT("repo_algs.load");
    return 0;
}
