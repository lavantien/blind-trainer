#include "data/repo_srs.h"

#include "common/bt_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"

static void format_option(char *out, size_t out_len, int has, double value) {
    if (has) {
        snprintf(out, out_len, "Some(%.17g)", value);
    } else {
        snprintf(out, out_len, "None");
    }
}

void bt_srs_load_error_string(const bt_srs_load_error *err, char *out, size_t out_len) {
    switch (err->kind) {
    case BT_SRS_LOAD_DB:
        bt_db_error_string(&err->db, out, out_len);
        break;
    case BT_SRS_LOAD_UNKNOWN_QUALITY:
        snprintf(out, out_len,
                 "card %s holds review quality %u with no sm-2 grade mapping", err->key,
                 err->quality);
        break;
    case BT_SRS_LOAD_INVALID_MEMORY: {
        char stability[40];
        char difficulty[40];
        format_option(stability, sizeof stability, err->has_stability, err->stability);
        format_option(difficulty, sizeof difficulty, err->has_difficulty, err->difficulty);
        snprintf(out, out_len,
                 "card %s holds an invalid fsrs memory state (stability %s, difficulty %s)",
                 err->key, stability, difficulty);
        break;
    }
    }
}

void bt_srs_store_init(bt_srs_store *store) {
    BT_TRACE_ENTER("repo_srs.store_init");
    memset(store, 0, sizeof *store);
    BT_TRACE_EXIT("repo_srs.store_init");
}

void bt_srs_store_free(bt_srs_store *store) {
    BT_TRACE_ENTER("repo_srs.store_free");
    for (size_t i = 0; i < store->count; i++) {
        free(store->items[i].key);
        bt_card_free(&store->items[i].card);
    }
    free(store->items);
    memset(store, 0, sizeof *store);
    BT_TRACE_EXIT("repo_srs.store_free");
}

int bt_srs_store_set(bt_srs_store *store, const char *key, const bt_card *card) {
    if (store->count == store->cap) {
        size_t cap = store->cap == 0 ? 8 : store->cap * 2;
        bt_srs_entry *grown = realloc(store->items, cap * sizeof *grown);
        if (grown == NULL) {
            return -1;
        }
        store->items = grown;
        store->cap = cap;
    }
    size_t at = store->count;
    for (size_t i = 0; i < store->count; i++) {
        int cmp = strcmp(store->items[i].key, key);
        if (cmp == 0) {
            bt_card clone;
            if (bt_card_clone(card, &clone) != 0) {
                return -1;
            }
            bt_card_free(&store->items[i].card);
            store->items[i].card = clone;
            return 0;
        }
        if (cmp > 0) {
            at = i;
            break;
        }
    }
    char *key_copy = malloc(strlen(key) + 1);
    if (key_copy == NULL) {
        return -1;
    }
    strcpy(key_copy, key);
    bt_card clone;
    if (bt_card_clone(card, &clone) != 0) {
        free(key_copy);
        return -1;
    }
    memmove(&store->items[at + 1], &store->items[at],
            (store->count - at) * sizeof *store->items);
    store->items[at].key = key_copy;
    store->items[at].card = clone;
    store->count++;
    return 0;
}

const bt_card *bt_srs_store_get(const bt_srs_store *store, const char *key) {
    for (size_t i = 0; i < store->count; i++) {
        if (strcmp(store->items[i].key, key) == 0) {
            return &store->items[i].card;
        }
    }
    return NULL;
}

size_t bt_srs_store_len(const bt_srs_store *store) {
    return store->count;
}

int bt_srs_store_clone(const bt_srs_store *src, bt_srs_store *dst) {
    bt_srs_store_init(dst);
    for (size_t i = 0; i < src->count; i++) {
        if (bt_srs_store_set(dst, src->items[i].key, &src->items[i].card) != 0) {
            bt_srs_store_free(dst);
            return -1;
        }
    }
    return 0;
}

int bt_srs_store_eq(const bt_srs_store *a, const bt_srs_store *b) {
    if (a->count != b->count) {
        return 0;
    }
    for (size_t i = 0; i < a->count; i++) {
        if (strcmp(a->items[i].key, b->items[i].key) != 0) {
            return 0;
        }
        if (bt_card_eq(&a->items[i].card, &b->items[i].card) != 1) {
            return 0;
        }
    }
    return 1;
}

static void db_step_error(bt_db *db, bt_db_error *err, bt_db_error_kind kind,
                          const char *step) {
    err->kind = kind;
    snprintf(err->step, sizeof err->step, "%s", step);
    snprintf(err->detail, sizeof err->detail, "%s", sqlite3_errmsg(bt_db_conn(db)));
}

int bt_repo_srs_insert_card(bt_db *db, const char *event, const char *method,
                            const char *stage, const char *case_key, const bt_card *card,
                            long long now, bt_db_error *err) {
    sqlite3_stmt *card_stmt = NULL;
    const char *card_sql =
        "INSERT INTO card(event, method, stage, case_key, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(bt_db_conn(db), card_sql, -1, &card_stmt, NULL) != SQLITE_OK) {
        db_step_error(db, err, BT_DB_ERR_EXEC, "insert card");
        return -1;
    }
    sqlite3_bind_text(card_stmt, 1, event, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(card_stmt, 2, method, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(card_stmt, 3, stage, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(card_stmt, 4, case_key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(card_stmt, 5, now);
    sqlite3_bind_int64(card_stmt, 6, now);
    if (sqlite3_step(card_stmt) != SQLITE_DONE) {
        db_step_error(db, err, BT_DB_ERR_EXEC, "insert card");
        sqlite3_finalize(card_stmt);
        return -1;
    }
    sqlite3_finalize(card_stmt);
    long long card_id = bt_db_last_insert_rowid(db);
    sqlite3_stmt *state_stmt = NULL;
    const char *state_sql =
        "INSERT INTO srs_state(card_id, due_day, interval_days, reps, lapses, last_review_day, stability, difficulty) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(bt_db_conn(db), state_sql, -1, &state_stmt, NULL) != SQLITE_OK) {
        db_step_error(db, err, BT_DB_ERR_EXEC, "insert srs_state");
        return -1;
    }
    long long last_review = 0;
    int has_last = bt_card_last_review_day(card, &last_review);
    sqlite3_bind_int64(state_stmt, 1, card_id);
    sqlite3_bind_int64(state_stmt, 2, card->due_day);
    sqlite3_bind_int64(state_stmt, 3, card->interval_days);
    sqlite3_bind_int64(state_stmt, 4, (long long)card->reps);
    sqlite3_bind_int64(state_stmt, 5, (long long)card->lapses);
    if (has_last) {
        sqlite3_bind_int64(state_stmt, 6, last_review);
    } else {
        sqlite3_bind_null(state_stmt, 6);
    }
    if (card->has_memory) {
        sqlite3_bind_double(state_stmt, 7, card->memory.stability);
        sqlite3_bind_double(state_stmt, 8, card->memory.difficulty);
    } else {
        sqlite3_bind_null(state_stmt, 7);
        sqlite3_bind_null(state_stmt, 8);
    }
    if (sqlite3_step(state_stmt) != SQLITE_DONE) {
        db_step_error(db, err, BT_DB_ERR_EXEC, "insert srs_state");
        sqlite3_finalize(state_stmt);
        return -1;
    }
    sqlite3_finalize(state_stmt);
    sqlite3_stmt *log_stmt = NULL;
    const char *log_sql =
        "INSERT INTO review_log(card_id, rating, reviewed_day, interval_before, interval_after, elapsed_days) "
        "VALUES (?, ?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(bt_db_conn(db), log_sql, -1, &log_stmt, NULL) != SQLITE_OK) {
        db_step_error(db, err, BT_DB_ERR_EXEC, "insert review_log");
        return -1;
    }
    long long interval_before = 0;
    for (size_t i = 0; i < card->review_count; i++) {
        const bt_review_entry *entry = &card->reviews[i];
        sqlite3_reset(log_stmt);
        sqlite3_bind_int64(log_stmt, 1, card_id);
        sqlite3_bind_int64(log_stmt, 2, (long long)entry->quality);
        sqlite3_bind_int64(log_stmt, 3, entry->ts_days);
        sqlite3_bind_int64(log_stmt, 4, interval_before);
        sqlite3_bind_int64(log_stmt, 5, entry->scheduled_days);
        sqlite3_bind_int64(log_stmt, 6, entry->elapsed_days);
        if (sqlite3_step(log_stmt) != SQLITE_DONE) {
            db_step_error(db, err, BT_DB_ERR_EXEC, "insert review_log");
            sqlite3_finalize(log_stmt);
            return -1;
        }
        interval_before = entry->scheduled_days;
    }
    sqlite3_finalize(log_stmt);
    return 0;
}

int bt_repo_srs_save_tx(bt_db *db, const bt_srs_store *store, bt_db_error *err) {
    BT_TRACE_ENTER("repo_srs.save_tx");
    sqlite3_stmt *clear = NULL;
    const char *clear_sql = "DELETE FROM card WHERE event = ? AND method = ?";
    if (sqlite3_prepare_v2(bt_db_conn(db), clear_sql, -1, &clear, NULL) != SQLITE_OK) {
        db_step_error(db, err, BT_DB_ERR_EXEC, "clear card");
        BT_TRACE_FAIL("repo_srs.save_tx", "prepare clear failed");
        return -1;
    }
    sqlite3_bind_text(clear, 1, BT_DB_EVENT_3BLD, -1, SQLITE_STATIC);
    sqlite3_bind_text(clear, 2, BT_DB_METHOD_3STYLE, -1, SQLITE_STATIC);
    if (sqlite3_step(clear) != SQLITE_DONE) {
        db_step_error(db, err, BT_DB_ERR_EXEC, "clear card");
        sqlite3_finalize(clear);
        BT_TRACE_FAIL("repo_srs.save_tx", "clear failed");
        return -1;
    }
    sqlite3_finalize(clear);
    long long now = bt_db_now_ms();
    for (size_t i = 0; i < store->count; i++) {
        char stage[BT_PATH_MAX];
        char case_key[BT_PATH_MAX];
        bt_repo_algs_split_key(store->items[i].key, stage, sizeof stage, case_key,
                               sizeof case_key);
        if (bt_repo_srs_insert_card(db, BT_DB_EVENT_3BLD, BT_DB_METHOD_3STYLE, stage,
                                    case_key, &store->items[i].card, now, err) != 0) {
            BT_TRACE_FAIL("repo_srs.save_tx", "insert %s failed", store->items[i].key);
            return -1;
        }
    }
    BT_TRACE_EXIT("repo_srs.save_tx");
    return 0;
}

int bt_repo_srs_save(bt_db *db, const bt_srs_store *store, bt_db_error *err) {
    BT_TRACE_ENTER("repo_srs.save");
    if (bt_db_begin(db, err) != 0) {
        return -1;
    }
    if (bt_repo_srs_save_tx(db, store, err) != 0) {
        bt_db_rollback(db);
        return -1;
    }
    if (bt_db_commit(db, err) != 0) {
        snprintf(err->step, sizeof err->step, "%s", "commit card");
        bt_db_rollback(db);
        return -1;
    }
    BT_TRACE_EXIT("repo_srs.save");
    return 0;
}

static int memory_from_row(const char *key, int has_stability, double stability,
                           int has_difficulty, double difficulty,
                           const bt_review_entry *entries, size_t entry_count,
                           const bt_fsrs_config *cfg, int *has_memory,
                           bt_fsrs_memory *memory, bt_srs_load_error *err) {
    *has_memory = 0;
    if (!has_stability && !has_difficulty) {
        for (size_t i = 0; i < entry_count; i++) {
            bt_grade grade;
            if (bt_grade_from_quality(entries[i].quality, &grade) != 0) {
                memset(err, 0, sizeof *err);
                err->kind = BT_SRS_LOAD_UNKNOWN_QUALITY;
                snprintf(err->key, sizeof err->key, "%s", key);
                err->quality = entries[i].quality;
                return -1;
            }
        }
        bt_fsrs_memory replayed;
        int has = 0;
        if (bt_replay_sm2(entries, entry_count, cfg, &has, &replayed) != 0) {
            memset(err, 0, sizeof *err);
            err->kind = BT_SRS_LOAD_UNKNOWN_QUALITY;
            snprintf(err->key, sizeof err->key, "%s", key);
            err->quality = entries[0].quality;
            return -1;
        }
        if (has) {
            if (!bt_fsrs_memory_is_valid(replayed)) {
                memset(err, 0, sizeof *err);
                err->kind = BT_SRS_LOAD_INVALID_MEMORY;
                snprintf(err->key, sizeof err->key, "%s", key);
                err->has_stability = 1;
                err->stability = replayed.stability;
                err->has_difficulty = 1;
                err->difficulty = replayed.difficulty;
                return -1;
            }
            *has_memory = 1;
            *memory = replayed;
        }
        return 0;
    }
    if (has_stability && has_difficulty) {
        bt_fsrs_memory candidate = {stability, difficulty};
        if (!bt_fsrs_memory_is_valid(candidate)) {
            memset(err, 0, sizeof *err);
            err->kind = BT_SRS_LOAD_INVALID_MEMORY;
            snprintf(err->key, sizeof err->key, "%s", key);
            err->has_stability = 1;
            err->stability = stability;
            err->has_difficulty = 1;
            err->difficulty = difficulty;
            return -1;
        }
        *has_memory = 1;
        *memory = candidate;
        return 0;
    }
    memset(err, 0, sizeof *err);
    err->kind = BT_SRS_LOAD_INVALID_MEMORY;
    snprintf(err->key, sizeof err->key, "%s", key);
    err->has_stability = has_stability;
    err->stability = stability;
    err->has_difficulty = has_difficulty;
    err->difficulty = difficulty;
    return -1;
}

static int rows_push(bt_srs_card_row **rows, size_t *count, size_t *cap,
                     const bt_srs_card_row *row) {
    if (*count == *cap) {
        size_t grown_cap = *cap == 0 ? 8 : *cap * 2;
        bt_srs_card_row *grown = realloc(*rows, grown_cap * sizeof *grown);
        if (grown == NULL) {
            return -1;
        }
        *rows = grown;
        *cap = grown_cap;
    }
    (*rows)[(*count)++] = *row;
    return 0;
}

void bt_repo_srs_free_rows(bt_srs_card_row *rows, size_t count) {
    for (size_t i = 0; i < count; i++) {
        bt_card_free(&rows[i].card);
    }
    free(rows);
}

int bt_repo_srs_load_rows(const bt_db *db, const bt_fsrs_config *cfg, bt_srs_filter filter,
                          bt_srs_card_row **out, size_t *count, bt_srs_load_error *err) {
    BT_TRACE_ENTER("repo_srs.load_rows");
    *out = NULL;
    *count = 0;
    const char *condition =
        filter == BT_SRS_FILTER_ONLY_3BLD ? "c.event = ? AND c.method = ?"
                                          : "c.event <> ? AND c.method <> ?";
    char sql[512];
    snprintf(sql, sizeof sql,
             "SELECT c.id, c.event, c.method, c.stage, c.case_key, s.due_day, "
             "s.interval_days, s.reps, s.lapses, s.stability, s.difficulty "
             "FROM card c JOIN srs_state s ON s.card_id = c.id WHERE %s",
             condition);
    sqlite3_stmt *cards = NULL;
    if (sqlite3_prepare_v2(bt_db_conn((bt_db *)db), sql, -1, &cards, NULL) != SQLITE_OK) {
        err->kind = BT_SRS_LOAD_DB;
        db_step_error((bt_db *)db, &err->db, BT_DB_ERR_QUERY, "select card");
        BT_TRACE_FAIL("repo_srs.load_rows", "prepare failed");
        return -1;
    }
    sqlite3_bind_text(cards, 1, BT_DB_EVENT_3BLD, -1, SQLITE_STATIC);
    sqlite3_bind_text(cards, 2, BT_DB_METHOD_3STYLE, -1, SQLITE_STATIC);
    sqlite3_stmt *reviews = NULL;
    const char *review_sql =
        "SELECT rating, reviewed_day, elapsed_days, interval_after "
        "FROM review_log WHERE card_id = ? ORDER BY id";
    if (sqlite3_prepare_v2(bt_db_conn((bt_db *)db), review_sql, -1, &reviews, NULL) !=
        SQLITE_OK) {
        sqlite3_finalize(cards);
        err->kind = BT_SRS_LOAD_DB;
        db_step_error((bt_db *)db, &err->db, BT_DB_ERR_QUERY, "select card");
        BT_TRACE_FAIL("repo_srs.load_rows", "prepare reviews failed");
        return -1;
    }
    bt_srs_card_row *rows = NULL;
    size_t rows_count = 0;
    size_t rows_cap = 0;
    int step = sqlite3_step(cards);
    while (step == SQLITE_ROW) {
        long long id = sqlite3_column_int64(cards, 0);
        bt_srs_card_row row;
        memset(&row, 0, sizeof row);
        snprintf(row.event, BT_CASE_FIELD_MAX, "%s", sqlite3_column_text(cards, 1));
        snprintf(row.method, BT_CASE_FIELD_MAX, "%s", sqlite3_column_text(cards, 2));
        snprintf(row.stage, BT_CASE_FIELD_MAX, "%s", sqlite3_column_text(cards, 3));
        snprintf(row.case_key, BT_CASE_FIELD_MAX, "%s", sqlite3_column_text(cards, 4));
        row.card.due_day = sqlite3_column_int64(cards, 5);
        row.card.interval_days = sqlite3_column_int64(cards, 6);
        row.card.reps = (unsigned int)sqlite3_column_int64(cards, 7);
        row.card.lapses = (unsigned int)sqlite3_column_int64(cards, 8);
        int has_stability = sqlite3_column_type(cards, 9) != SQLITE_NULL;
        double stability = sqlite3_column_double(cards, 9);
        int has_difficulty = sqlite3_column_type(cards, 10) != SQLITE_NULL;
        double difficulty = sqlite3_column_double(cards, 10);
        sqlite3_reset(reviews);
        sqlite3_bind_int64(reviews, 1, id);
        int review_step = sqlite3_step(reviews);
        while (review_step == SQLITE_ROW) {
            bt_review_entry entry = {
                sqlite3_column_int64(reviews, 1),
                (unsigned int)sqlite3_column_int64(reviews, 0),
                sqlite3_column_int64(reviews, 2),
                sqlite3_column_int64(reviews, 3),
            };
            if (row.card.review_count == row.card.review_cap) {
                size_t cap = row.card.review_cap == 0 ? 4 : row.card.review_cap * 2;
                bt_review_entry *grown =
                    realloc(row.card.reviews, cap * sizeof *grown);
                if (grown == NULL) {
                    sqlite3_finalize(reviews);
                    sqlite3_finalize(cards);
                    bt_repo_srs_free_rows(rows, rows_count);
                    err->kind = BT_SRS_LOAD_DB;
                    db_step_error((bt_db *)db, &err->db, BT_DB_ERR_QUERY, "select card");
                    BT_TRACE_FAIL("repo_srs.load_rows", "oom");
                    return -1;
                }
                row.card.reviews = grown;
                row.card.review_cap = cap;
            }
            row.card.reviews[row.card.review_count++] = entry;
            review_step = sqlite3_step(reviews);
        }
        if (review_step != SQLITE_DONE) {
            sqlite3_finalize(reviews);
            sqlite3_finalize(cards);
            bt_repo_srs_free_rows(rows, rows_count);
            err->kind = BT_SRS_LOAD_DB;
            db_step_error((bt_db *)db, &err->db, BT_DB_ERR_QUERY, "select card");
            BT_TRACE_FAIL("repo_srs.load_rows", "review step failed");
            return -1;
        }
        char key[BT_REPO_ALGS_KEY_MAX];
        bt_repo_algs_join_key(row.stage, row.case_key, key, sizeof key);
        int has_memory = 0;
        bt_fsrs_memory memory;
        if (memory_from_row(key, has_stability, stability, has_difficulty, difficulty,
                            row.card.reviews, row.card.review_count, cfg, &has_memory,
                            &memory, err) != 0) {
            sqlite3_finalize(reviews);
            sqlite3_finalize(cards);
            bt_repo_srs_free_rows(rows, rows_count);
            BT_TRACE_FAIL("repo_srs.load_rows", "memory from row failed");
            return -1;
        }
        row.card.has_memory = has_memory;
        row.card.memory = memory;
        if (rows_push(&rows, &rows_count, &rows_cap, &row) != 0) {
            sqlite3_finalize(reviews);
            sqlite3_finalize(cards);
            bt_repo_srs_free_rows(rows, rows_count);
            err->kind = BT_SRS_LOAD_DB;
            db_step_error((bt_db *)db, &err->db, BT_DB_ERR_QUERY, "select card");
            BT_TRACE_FAIL("repo_srs.load_rows", "oom");
            return -1;
        }
        step = sqlite3_step(cards);
    }
    if (step != SQLITE_DONE) {
        sqlite3_finalize(reviews);
        sqlite3_finalize(cards);
        bt_repo_srs_free_rows(rows, rows_count);
        err->kind = BT_SRS_LOAD_DB;
        db_step_error((bt_db *)db, &err->db, BT_DB_ERR_QUERY, "select card");
        BT_TRACE_FAIL("repo_srs.load_rows", "card step failed");
        return -1;
    }
    sqlite3_finalize(reviews);
    sqlite3_finalize(cards);
    *out = rows;
    *count = rows_count;
    BT_TRACE_EXIT("repo_srs.load_rows");
    return 0;
}

int bt_repo_srs_load_filtered(const bt_db *db, const bt_fsrs_config *cfg,
                              bt_srs_filter filter, bt_srs_store *out,
                              bt_srs_load_error *err) {
    BT_TRACE_ENTER("repo_srs.load_filtered");
    bt_srs_store_init(out);
    bt_srs_card_row *rows = NULL;
    size_t count = 0;
    if (bt_repo_srs_load_rows(db, cfg, filter, &rows, &count, err) != 0) {
        BT_TRACE_FAIL("repo_srs.load_filtered", "rows load failed");
        return -1;
    }
    for (size_t i = 0; i < count; i++) {
        char key[BT_REPO_ALGS_KEY_MAX];
        bt_repo_algs_join_key(rows[i].stage, rows[i].case_key, key, sizeof key);
        if (bt_srs_store_set(out, key, &rows[i].card) != 0) {
            bt_repo_srs_free_rows(rows, count);
            bt_srs_store_free(out);
            err->kind = BT_SRS_LOAD_DB;
            err->db.kind = BT_DB_ERR_QUERY;
            snprintf(err->db.step, sizeof err->db.step, "%s", "select card");
            snprintf(err->db.detail, sizeof err->db.detail, "%s", "out of memory");
            BT_TRACE_FAIL("repo_srs.load_filtered", "store set failed");
            return -1;
        }
    }
    bt_repo_srs_free_rows(rows, count);
    BT_TRACE_EXIT("repo_srs.load_filtered");
    return 0;
}

int bt_repo_srs_load(const bt_db *db, const bt_fsrs_config *cfg, bt_srs_store *out,
                     bt_srs_load_error *err) {
    return bt_repo_srs_load_filtered(db, cfg, BT_SRS_FILTER_ONLY_3BLD, out, err);
}

static int card_from_json(const bt_json_value *node, bt_card *card) {
    bt_card_init(card, 0);
    bt_json_value *memory = bt_json_get(node, "memory");
    if (memory != NULL && memory->type == BT_JSON_OBJECT) {
        bt_json_value *stability = bt_json_get(memory, "stability");
        bt_json_value *difficulty = bt_json_get(memory, "difficulty");
        if (stability == NULL || stability->type != BT_JSON_NUMBER ||
            difficulty == NULL || difficulty->type != BT_JSON_NUMBER) {
            bt_card_free(card);
            return -1;
        }
        card->has_memory = 1;
        card->memory.stability = stability->as.number;
        card->memory.difficulty = difficulty->as.number;
    }
    bt_json_value *fields[] = {
        bt_json_get(node, "interval_days"), bt_json_get(node, "due_day"),
        bt_json_get(node, "reps"), bt_json_get(node, "lapses"),
    };
    for (int i = 0; i < 4; i++) {
        if (fields[i] == NULL || fields[i]->type != BT_JSON_NUMBER) {
            bt_card_free(card);
            return -1;
        }
    }
    card->interval_days = (long long)fields[0]->as.number;
    card->due_day = (long long)fields[1]->as.number;
    card->reps = (unsigned int)fields[2]->as.number;
    card->lapses = (unsigned int)fields[3]->as.number;
    bt_json_value *reviews = bt_json_get(node, "reviews");
    if (reviews == NULL || reviews->type != BT_JSON_ARRAY) {
        bt_card_free(card);
        return -1;
    }
    for (size_t i = 0; i < reviews->as.array.count; i++) {
        bt_json_value *item = bt_json_at(reviews, i);
        bt_json_value *parts[] = {
            bt_json_get(item, "ts_days"), bt_json_get(item, "quality"),
            bt_json_get(item, "elapsed_days"), bt_json_get(item, "scheduled_days"),
        };
        for (int p = 0; p < 4; p++) {
            if (parts[p] == NULL || parts[p]->type != BT_JSON_NUMBER) {
                bt_card_free(card);
                return -1;
            }
        }
        bt_review_entry entry = {
            (long long)parts[0]->as.number,
            (unsigned int)parts[1]->as.number,
            (long long)parts[2]->as.number,
            (long long)parts[3]->as.number,
        };
        if (card->review_count == card->review_cap) {
            size_t cap = card->review_cap == 0 ? 4 : card->review_cap * 2;
            bt_review_entry *grown = realloc(card->reviews, cap * sizeof *grown);
            if (grown == NULL) {
                bt_card_free(card);
                return -1;
            }
            card->reviews = grown;
            card->review_cap = cap;
        }
        card->reviews[card->review_count++] = entry;
    }
    return 0;
}

int bt_srs_store_from_json(const bt_json_value *root, bt_srs_store *out) {
    BT_TRACE_ENTER("repo_srs.store_from_json");
    bt_srs_store_init(out);
    bt_json_value *cards = bt_json_get(root, "cards");
    if (cards == NULL || cards->type != BT_JSON_OBJECT) {
        BT_TRACE_FAIL("repo_srs.store_from_json", "missing cards object");
        return -1;
    }
    for (size_t i = 0; i < cards->as.object.count; i++) {
        const char *key = cards->as.object.keys[i];
        bt_json_value *node = cards->as.object.values[i];
        bt_card card;
        if (node->type != BT_JSON_OBJECT || card_from_json(node, &card) != 0) {
            bt_srs_store_free(out);
            BT_TRACE_FAIL("repo_srs.store_from_json", "bad card %s", key);
            return -1;
        }
        if (bt_srs_store_set(out, key, &card) != 0) {
            bt_card_free(&card);
            bt_srs_store_free(out);
            return -1;
        }
        bt_card_free(&card);
    }
    BT_TRACE_EXIT("repo_srs.store_from_json");
    return 0;
}
