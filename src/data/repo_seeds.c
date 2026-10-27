#include "data/repo_seeds.h"

#include "common/bt_trace.h"

#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"

static void db_error(bt_db *db, bt_seed_error *err, const char *step) {
    err->kind = BT_SEED_ERR_DB;
    err->db.kind = BT_DB_ERR_QUERY;
    snprintf(err->db.step, sizeof err->db.step, "%s", step);
    snprintf(err->db.detail, sizeof err->db.detail, "%s", sqlite3_errmsg(bt_db_conn(db)));
}

static char *text_dup(const char *text) {
    size_t len = strlen(text);
    char *out = malloc(len + 1);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, text, len + 1);
    return out;
}

int bt_seed_pack_tx(bt_db *db, const char *file, const bt_seed_row *rows, size_t count,
                    bt_seed_report *out, bt_seed_error *err) {
    BT_TRACE_ENTER("repo_seeds.seed_pack_tx");
    snprintf(out->file, sizeof out->file, "%s", file);
    out->inserted = 0;
    out->verified = 0;
    if (count == 0) {
        BT_TRACE_EXIT("repo_seeds.seed_pack_tx");
        return 0;
    }
    sqlite3_stmt *check = NULL;
    const char *check_sql = "SELECT content_hash FROM seed_alg "
                            "WHERE event = ? AND method = ? AND stage = ? AND case_key = ?";
    if (sqlite3_prepare_v2(bt_db_conn(db), check_sql, -1, &check, NULL) != SQLITE_OK) {
        db_error(db, err, "select seed_alg");
        BT_TRACE_FAIL("repo_seeds.seed_pack_tx", "prepare check failed");
        return -1;
    }
    sqlite3_stmt *insert = NULL;
    const char *insert_sql =
        "INSERT INTO seed_alg(event, method, stage, case_key, alg, front_json, source_url, "
        "source_at, content_hash) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(bt_db_conn(db), insert_sql, -1, &insert, NULL) != SQLITE_OK) {
        sqlite3_finalize(check);
        db_error(db, err, "insert seed_alg");
        BT_TRACE_FAIL("repo_seeds.seed_pack_tx", "prepare insert failed");
        return -1;
    }
    for (size_t i = 0; i < count; i++) {
        const bt_seed_row *row = &rows[i];
        sqlite3_reset(check);
        sqlite3_bind_text(check, 1, row->event, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(check, 2, row->method, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(check, 3, row->stage, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(check, 4, row->case_key, -1, SQLITE_TRANSIENT);
        int step = sqlite3_step(check);
        if (step == SQLITE_ROW) {
            const unsigned char *stored = sqlite3_column_text(check, 0);
            if (stored != NULL && strcmp((const char *)stored, row->content_hash) == 0) {
                out->verified += 1;
                continue;
            }
            err->kind = BT_SEED_ERR_DRIFT;
            snprintf(err->file, sizeof err->file, "%s", file);
            snprintf(err->case_key, sizeof err->case_key, "%s", row->case_key);
            snprintf(err->stored, sizeof err->stored, "%s",
                     stored == NULL ? "" : (const char *)stored);
            snprintf(err->packed, sizeof err->packed, "%s", row->content_hash);
            sqlite3_finalize(check);
            sqlite3_finalize(insert);
            BT_TRACE_FAIL("repo_seeds.seed_pack_tx", "drift on %s in %s", row->case_key, file);
            return -1;
        }
        if (step != SQLITE_DONE) {
            sqlite3_finalize(check);
            sqlite3_finalize(insert);
            db_error(db, err, "select seed_alg");
            BT_TRACE_FAIL("repo_seeds.seed_pack_tx", "check step failed on %s", row->case_key);
            return -1;
        }
        sqlite3_reset(insert);
        sqlite3_bind_text(insert, 1, row->event, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insert, 2, row->method, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insert, 3, row->stage, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insert, 4, row->case_key, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insert, 5, row->alg, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insert, 6, row->front_json, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insert, 7, row->source_url, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insert, 8, row->source_at, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insert, 9, row->content_hash, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(insert) != SQLITE_DONE) {
            sqlite3_finalize(check);
            sqlite3_finalize(insert);
            db_error(db, err, "insert seed_alg");
            BT_TRACE_FAIL("repo_seeds.seed_pack_tx", "insert failed on %s", row->case_key);
            return -1;
        }
        out->inserted += 1;
    }
    sqlite3_finalize(check);
    sqlite3_finalize(insert);
    sqlite3_stmt *stale = NULL;
    const char *stale_sql =
        "SELECT case_key FROM seed_alg WHERE event = ? AND method = ? AND stage = ?";
    if (sqlite3_prepare_v2(bt_db_conn(db), stale_sql, -1, &stale, NULL) != SQLITE_OK) {
        db_error(db, err, "select seed_alg");
        BT_TRACE_FAIL("repo_seeds.seed_pack_tx", "prepare stale failed");
        return -1;
    }
    sqlite3_bind_text(stale, 1, rows[0].event, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stale, 2, rows[0].method, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stale, 3, rows[0].stage, -1, SQLITE_TRANSIENT);
    int step = sqlite3_step(stale);
    int rc = 0;
    while (step == SQLITE_ROW) {
        const unsigned char *key = sqlite3_column_text(stale, 0);
        bool present = false;
        if (key != NULL) {
            for (size_t i = 0; i < count; i++) {
                if (strcmp(rows[i].case_key, (const char *)key) == 0) {
                    present = true;
                    break;
                }
            }
        }
        if (!present) {
            err->kind = BT_SEED_ERR_STALE;
            snprintf(err->case_key, sizeof err->case_key, "%s",
                     key == NULL ? "" : (const char *)key);
            snprintf(err->file, sizeof err->file, "%s", file);
            rc = -1;
            BT_TRACE_FAIL("repo_seeds.seed_pack_tx", "stale row %s in %s", err->case_key, file);
            break;
        }
        step = sqlite3_step(stale);
    }
    if (rc == 0 && step != SQLITE_DONE) {
        db_error(db, err, "select seed_alg");
        rc = -1;
        BT_TRACE_FAIL("repo_seeds.seed_pack_tx", "stale step failed");
    }
    sqlite3_finalize(stale);
    if (rc != 0) {
        BT_TRACE_FAIL("repo_seeds.seed_pack_tx", "failed for %s", file);
        return -1;
    }
    BT_TRACE_EXIT("repo_seeds.seed_pack_tx");
    return 0;
}

int bt_seed_pack(bt_db *db, const char *packs_root, const bt_pack_entry *entry, bt_seed_report *out,
                 bt_seed_error *err) {
    BT_TRACE_ENTER("repo_seeds.seed_pack");
    bt_seed_rows rows;
    if (bt_seedpack_load_pack(packs_root, entry, &rows, err) != 0) {
        BT_TRACE_FAIL("repo_seeds.seed_pack", "load pack %s failed", entry->file);
        return -1;
    }
    bt_db_error db_err;
    if (bt_db_begin(db, &db_err) != 0) {
        bt_seed_rows_free(&rows);
        err->kind = BT_SEED_ERR_DB;
        err->db = db_err;
        BT_TRACE_FAIL("repo_seeds.seed_pack", "begin failed");
        return -1;
    }
    int rc = bt_seed_pack_tx(db, entry->file, rows.items, rows.count, out, err);
    bt_seed_rows_free(&rows);
    if (rc != 0) {
        bt_db_rollback(db);
        BT_TRACE_FAIL("repo_seeds.seed_pack", "seed tx failed for %s", entry->file);
        return -1;
    }
    if (bt_db_commit(db, &db_err) != 0) {
        bt_db_rollback(db);
        err->kind = BT_SEED_ERR_DB;
        err->db = db_err;
        err->db.kind = BT_DB_ERR_EXEC;
        snprintf(err->db.step, sizeof err->db.step, "%s", "commit seed_alg");
        BT_TRACE_FAIL("repo_seeds.seed_pack", "commit failed for %s", entry->file);
        return -1;
    }
    BT_TRACE_EXIT("repo_seeds.seed_pack");
    return 0;
}

int bt_seed_all(bt_db *db, const char *packs_root, bt_seed_report **out_reports, size_t *out_count,
                bt_seed_error *err) {
    BT_TRACE_ENTER("repo_seeds.seed_all");
    *out_reports = NULL;
    *out_count = 0;
    bt_pack_entry *entries = NULL;
    size_t count = 0;
    if (bt_seedpack_load_registry(packs_root, &entries, &count, err) != 0) {
        BT_TRACE_FAIL("repo_seeds.seed_all", "registry failed");
        return -1;
    }
    bt_seed_report *reports = calloc(count, sizeof *reports);
    if (reports == NULL) {
        bt_seedpack_free_entries(entries);
        BT_TRACE_FAIL("repo_seeds.seed_all", "alloc failed");
        return -1;
    }
    for (size_t i = 0; i < count; i++) {
        if (bt_seed_pack(db, packs_root, &entries[i], &reports[i], err) != 0) {
            free(reports);
            bt_seedpack_free_entries(entries);
            BT_TRACE_FAIL("repo_seeds.seed_all", "seed %s failed", entries[i].file);
            return -1;
        }
    }
    bt_seedpack_free_entries(entries);
    *out_reports = reports;
    *out_count = count;
    BT_TRACE_EXIT("repo_seeds.seed_all");
    return 0;
}

void bt_seed_reports_free(bt_seed_report *reports) {
    free(reports);
}

int bt_seed_row_count(bt_db *db, long long *out, bt_seed_error *err) {
    BT_TRACE_ENTER("repo_seeds.seed_row_count");
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(bt_db_conn(db), "SELECT COUNT(*) FROM seed_alg", -1, &stmt, NULL) !=
        SQLITE_OK) {
        db_error(db, err, "count seed_alg");
        BT_TRACE_FAIL("repo_seeds.seed_row_count", "prepare failed");
        return -1;
    }
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        db_error(db, err, "count seed_alg");
        BT_TRACE_FAIL("repo_seeds.seed_row_count", "step failed");
        return -1;
    }
    *out = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    BT_TRACE_EXIT("repo_seeds.seed_row_count");
    return 0;
}

int bt_load_seed_rows(bt_db *db, bt_seed_rows *out, bt_seed_error *err) {
    BT_TRACE_ENTER("repo_seeds.load_seed_rows");
    bt_seed_rows_init(out);
    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT event, method, stage, case_key, alg, front_json, source_url, source_at, "
        "content_hash FROM seed_alg ORDER BY rowid";
    if (sqlite3_prepare_v2(bt_db_conn(db), sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_error(db, err, "select all seed_alg");
        BT_TRACE_FAIL("repo_seeds.load_seed_rows", "prepare failed");
        return -1;
    }
    int step = sqlite3_step(stmt);
    while (step == SQLITE_ROW) {
        bt_seed_row *grown = realloc(out->items, (out->count + 1) * sizeof *grown);
        if (grown == NULL) {
            sqlite3_finalize(stmt);
            bt_seed_rows_free(out);
            BT_TRACE_FAIL("repo_seeds.load_seed_rows", "alloc failed");
            return -1;
        }
        out->items = grown;
        bt_seed_row *row = &out->items[out->count];
        memset(row, 0, sizeof *row);
        snprintf(row->event, sizeof row->event, "%s", sqlite3_column_text(stmt, 0));
        snprintf(row->method, sizeof row->method, "%s", sqlite3_column_text(stmt, 1));
        snprintf(row->stage, sizeof row->stage, "%s", sqlite3_column_text(stmt, 2));
        snprintf(row->case_key, sizeof row->case_key, "%s", sqlite3_column_text(stmt, 3));
        row->alg = text_dup((const char *)sqlite3_column_text(stmt, 4));
        row->front_json = text_dup((const char *)sqlite3_column_text(stmt, 5));
        row->source_url = text_dup((const char *)sqlite3_column_text(stmt, 6));
        row->source_at = text_dup((const char *)sqlite3_column_text(stmt, 7));
        snprintf(row->content_hash, sizeof row->content_hash, "%s", sqlite3_column_text(stmt, 8));
        if (row->alg == NULL || row->front_json == NULL || row->source_url == NULL ||
            row->source_at == NULL) {
            sqlite3_finalize(stmt);
            bt_seed_rows_free(out);
            BT_TRACE_FAIL("repo_seeds.load_seed_rows", "string alloc failed");
            return -1;
        }
        out->count += 1;
        step = sqlite3_step(stmt);
    }
    if (step != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        bt_seed_rows_free(out);
        db_error(db, err, "select all seed_alg");
        BT_TRACE_FAIL("repo_seeds.load_seed_rows", "step failed");
        return -1;
    }
    sqlite3_finalize(stmt);
    BT_TRACE_EXIT("repo_seeds.load_seed_rows");
    return 0;
}
