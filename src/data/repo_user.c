#include "data/repo_user.h"

#include "common/bt_trace.h"

#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"

int bt_case_ref_cmp(const bt_case_ref *a, const bt_case_ref *b) {
    int cmp = strcmp(a->event, b->event);
    if (cmp != 0) {
        return cmp;
    }
    cmp = strcmp(a->method, b->method);
    if (cmp != 0) {
        return cmp;
    }
    cmp = strcmp(a->stage, b->stage);
    if (cmp != 0) {
        return cmp;
    }
    return strcmp(a->case_key, b->case_key);
}

void bt_case_ref_of(bt_case_ref *out, const char *event, const char *method,
                    const char *stage, const char *case_key) {
    BT_TRACE_ENTER("repo_user.case_ref_of");
    snprintf(out->event, BT_CASE_FIELD_MAX, "%s", event);
    snprintf(out->method, BT_CASE_FIELD_MAX, "%s", method);
    snprintf(out->stage, BT_CASE_FIELD_MAX, "%s", stage);
    snprintf(out->case_key, BT_CASE_FIELD_MAX, "%s", case_key);
    BT_TRACE_EXIT("repo_user.case_ref_of");
}

void bt_overrides_init(bt_overrides *ov) {
    BT_TRACE_ENTER("repo_user.overrides_init");
    memset(ov, 0, sizeof *ov);
    BT_TRACE_EXIT("repo_user.overrides_init");
}

void bt_overrides_free(bt_overrides *ov) {
    BT_TRACE_ENTER("repo_user.overrides_free");
    for (size_t i = 0; i < ov->count; i++) {
        free(ov->items[i].alg);
    }
    free(ov->items);
    memset(ov, 0, sizeof *ov);
    BT_TRACE_EXIT("repo_user.overrides_free");
}

const char *bt_overrides_get(const bt_overrides *ov, const bt_case_ref *key) {
    for (size_t i = 0; i < ov->count; i++) {
        if (bt_case_ref_cmp(&ov->items[i].ref, key) == 0) {
            return ov->items[i].alg;
        }
    }
    return NULL;
}

size_t bt_overrides_len(const bt_overrides *ov) {
    return ov->count;
}

static void db_step_error(bt_db *db, bt_db_error *err, bt_db_error_kind kind,
                          const char *step) {
    err->kind = kind;
    snprintf(err->step, sizeof err->step, "%s", step);
    snprintf(err->detail, sizeof err->detail, "%s", sqlite3_errmsg(bt_db_conn(db)));
}

static int overrides_insert_sorted(bt_overrides *ov, const bt_case_ref *key,
                                   const char *alg) {
    if (ov->count == ov->cap) {
        size_t cap = ov->cap == 0 ? 8 : ov->cap * 2;
        bt_override_entry *grown = realloc(ov->items, cap * sizeof *grown);
        if (grown == NULL) {
            return -1;
        }
        ov->items = grown;
        ov->cap = cap;
    }
    size_t at = ov->count;
    for (size_t i = 0; i < ov->count; i++) {
        int cmp = bt_case_ref_cmp(&ov->items[i].ref, key);
        if (cmp == 0) {
            char *copy = malloc(strlen(alg) + 1);
            if (copy == NULL) {
                return -1;
            }
            strcpy(copy, alg);
            free(ov->items[i].alg);
            ov->items[i].alg = copy;
            return 0;
        }
        if (cmp > 0) {
            at = i;
            break;
        }
    }
    char *alg_copy = malloc(strlen(alg) + 1);
    if (alg_copy == NULL) {
        return -1;
    }
    strcpy(alg_copy, alg);
    memmove(&ov->items[at + 1], &ov->items[at],
            (ov->count - at) * sizeof *ov->items);
    ov->items[at].ref = *key;
    ov->items[at].alg = alg_copy;
    ov->count++;
    return 0;
}

int bt_repo_user_upsert(bt_db *db, const bt_case_ref *key, const char *alg,
                        bt_db_error *err) {
    BT_TRACE_ENTER("repo_user.upsert");
    long long now = bt_db_now_ms();
    if (bt_db_begin(db, err) != 0) {
        return -1;
    }
    sqlite3_stmt *update = NULL;
    const char *update_sql =
        "UPDATE user_alg SET alg = ?, updated_at = ? "
        "WHERE event = ? AND method = ? AND stage = ? AND case_key = ?";
    if (sqlite3_prepare_v2(bt_db_conn(db), update_sql, -1, &update, NULL) != SQLITE_OK) {
        db_step_error(db, err, BT_DB_ERR_EXEC, "update user_alg");
        bt_db_rollback(db);
        BT_TRACE_FAIL("repo_user.upsert", "prepare update failed");
        return -1;
    }
    sqlite3_bind_text(update, 1, alg, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(update, 2, now);
    sqlite3_bind_text(update, 3, key->event, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(update, 4, key->method, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(update, 5, key->stage, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(update, 6, key->case_key, -1, SQLITE_TRANSIENT);
    int updated = 0;
    if (sqlite3_step(update) == SQLITE_DONE) {
        updated = sqlite3_changes(bt_db_conn(db));
    }
    sqlite3_finalize(update);
    if (updated == 0) {
        sqlite3_stmt *insert = NULL;
        const char *insert_sql =
            "INSERT INTO user_alg(event, method, stage, case_key, alg, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)";
        if (sqlite3_prepare_v2(bt_db_conn(db), insert_sql, -1, &insert, NULL) != SQLITE_OK) {
            db_step_error(db, err, BT_DB_ERR_EXEC, "insert user_alg");
            bt_db_rollback(db);
            BT_TRACE_FAIL("repo_user.upsert", "prepare insert failed");
            return -1;
        }
        sqlite3_bind_text(insert, 1, key->event, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insert, 2, key->method, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insert, 3, key->stage, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insert, 4, key->case_key, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insert, 5, alg, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(insert, 6, now);
        sqlite3_bind_int64(insert, 7, now);
        int step = sqlite3_step(insert);
        sqlite3_finalize(insert);
        if (step != SQLITE_DONE) {
            db_step_error(db, err, BT_DB_ERR_EXEC, "insert user_alg");
            bt_db_rollback(db);
            BT_TRACE_FAIL("repo_user.upsert", "insert failed");
            return -1;
        }
    }
    if (bt_db_commit(db, err) != 0) {
        snprintf(err->step, sizeof err->step, "%s", "commit user_alg");
        bt_db_rollback(db);
        return -1;
    }
    BT_TRACE_EXIT("repo_user.upsert");
    return 0;
}

int bt_repo_user_remove(bt_db *db, const bt_case_ref *key, bt_db_error *err) {
    BT_TRACE_ENTER("repo_user.remove");
    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "DELETE FROM user_alg "
        "WHERE event = ? AND method = ? AND stage = ? AND case_key = ?";
    if (sqlite3_prepare_v2(bt_db_conn(db), sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_step_error(db, err, BT_DB_ERR_EXEC, "delete user_alg");
        BT_TRACE_FAIL("repo_user.remove", "prepare failed");
        return -1;
    }
    sqlite3_bind_text(stmt, 1, key->event, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, key->method, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, key->stage, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, key->case_key, -1, SQLITE_TRANSIENT);
    int step = sqlite3_step(stmt);
    int deleted = 0;
    if (step == SQLITE_DONE) {
        deleted = sqlite3_changes(bt_db_conn(db));
    }
    sqlite3_finalize(stmt);
    if (step != SQLITE_DONE) {
        db_step_error(db, err, BT_DB_ERR_EXEC, "delete user_alg");
        BT_TRACE_FAIL("repo_user.remove", "delete failed");
        return -1;
    }
    BT_TRACE_EXIT("repo_user.remove");
    return deleted > 0;
}

int bt_repo_user_load_all(const bt_db *db, bt_overrides *out, bt_db_error *err) {
    BT_TRACE_ENTER("repo_user.load_all");
    bt_overrides_init(out);
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT event, method, stage, case_key, alg FROM user_alg";
    if (sqlite3_prepare_v2(bt_db_conn((bt_db *)db), sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_step_error((bt_db *)db, err, BT_DB_ERR_QUERY, "select user_alg overrides");
        BT_TRACE_FAIL("repo_user.load_all", "prepare failed");
        return -1;
    }
    int step = sqlite3_step(stmt);
    while (step == SQLITE_ROW) {
        bt_case_ref ref;
        bt_case_ref_of(&ref, (const char *)sqlite3_column_text(stmt, 0),
                       (const char *)sqlite3_column_text(stmt, 1),
                       (const char *)sqlite3_column_text(stmt, 2),
                       (const char *)sqlite3_column_text(stmt, 3));
        const char *alg = (const char *)sqlite3_column_text(stmt, 4);
        if (overrides_insert_sorted(out, &ref, alg) != 0) {
            sqlite3_finalize(stmt);
            bt_overrides_free(out);
            db_step_error((bt_db *)db, err, BT_DB_ERR_QUERY, "select user_alg overrides");
            BT_TRACE_FAIL("repo_user.load_all", "insert failed");
            return -1;
        }
        step = sqlite3_step(stmt);
    }
    if (step != SQLITE_DONE) {
        db_step_error((bt_db *)db, err, BT_DB_ERR_QUERY, "select user_alg overrides");
        sqlite3_finalize(stmt);
        bt_overrides_free(out);
        BT_TRACE_FAIL("repo_user.load_all", "step failed");
        return -1;
    }
    sqlite3_finalize(stmt);
    BT_TRACE_EXIT("repo_user.load_all");
    return 0;
}
