#include "data/repo_cards.h"

#include "common/bt_trace.h"

#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"

void bt_case_cards_init(bt_case_cards *cards) {
    BT_TRACE_ENTER("repo_cards.init");
    memset(cards, 0, sizeof *cards);
    BT_TRACE_EXIT("repo_cards.init");
}

void bt_case_cards_free(bt_case_cards *cards) {
    BT_TRACE_ENTER("repo_cards.free");
    for (size_t i = 0; i < cards->count; i++) {
        bt_card_free(&cards->items[i].card);
    }
    free(cards->items);
    memset(cards, 0, sizeof *cards);
    BT_TRACE_EXIT("repo_cards.free");
}

static int cards_insert_sorted(bt_case_cards *cards, const bt_case_ref *key,
                               const bt_card *card) {
    if (cards->count == cards->cap) {
        size_t cap = cards->cap == 0 ? 8 : cards->cap * 2;
        bt_case_card_entry *grown = realloc(cards->items, cap * sizeof *grown);
        if (grown == NULL) {
            return -1;
        }
        cards->items = grown;
        cards->cap = cap;
    }
    size_t at = cards->count;
    for (size_t i = 0; i < cards->count; i++) {
        int cmp = bt_case_ref_cmp(&cards->items[i].ref, key);
        if (cmp == 0) {
            bt_card clone;
            if (bt_card_clone(card, &clone) != 0) {
                return -1;
            }
            bt_card_free(&cards->items[i].card);
            cards->items[i].card = clone;
            return 0;
        }
        if (cmp > 0) {
            at = i;
            break;
        }
    }
    bt_card clone;
    if (bt_card_clone(card, &clone) != 0) {
        return -1;
    }
    memmove(&cards->items[at + 1], &cards->items[at],
            (cards->count - at) * sizeof *cards->items);
    cards->items[at].ref = *key;
    cards->items[at].card = clone;
    cards->count++;
    return 0;
}

const bt_card *bt_case_cards_get(const bt_case_cards *cards, const bt_case_ref *key) {
    for (size_t i = 0; i < cards->count; i++) {
        if (bt_case_ref_cmp(&cards->items[i].ref, key) == 0) {
            return &cards->items[i].card;
        }
    }
    return NULL;
}

size_t bt_case_cards_len(const bt_case_cards *cards) {
    return cards->count;
}

int bt_case_cards_contains(const bt_case_cards *cards, const bt_case_ref *key) {
    return bt_case_cards_get(cards, key) != NULL;
}

int bt_repo_cards_load(const bt_db *db, const bt_fsrs_config *cfg, bt_case_cards *out,
                       bt_srs_load_error *err) {
    BT_TRACE_ENTER("repo_cards.load");
    bt_case_cards_init(out);
    bt_srs_card_row *rows = NULL;
    size_t count = 0;
    if (bt_repo_srs_load_rows(db, cfg, BT_SRS_FILTER_EXCLUDE_3BLD, &rows, &count, err) !=
        0) {
        BT_TRACE_FAIL("repo_cards.load", "rows load failed");
        return -1;
    }
    for (size_t i = 0; i < count; i++) {
        bt_case_ref ref;
        bt_case_ref_of(&ref, rows[i].event, rows[i].method, rows[i].stage,
                       rows[i].case_key);
        if (cards_insert_sorted(out, &ref, &rows[i].card) != 0) {
            bt_repo_srs_free_rows(rows, count);
            bt_case_cards_free(out);
            err->kind = BT_SRS_LOAD_DB;
            err->db.kind = BT_DB_ERR_QUERY;
            snprintf(err->db.step, sizeof err->db.step, "%s", "select card");
            snprintf(err->db.detail, sizeof err->db.detail, "%s", "out of memory");
            BT_TRACE_FAIL("repo_cards.load", "insert failed");
            return -1;
        }
    }
    bt_repo_srs_free_rows(rows, count);
    BT_TRACE_EXIT("repo_cards.load");
    return 0;
}

int bt_repo_cards_save_card(bt_db *db, const bt_case_ref *key, const bt_card *card,
                            bt_db_error *err) {
    BT_TRACE_ENTER("repo_cards.save_card");
    long long now = bt_db_now_ms();
    if (bt_db_begin(db, err) != 0) {
        return -1;
    }
    sqlite3_stmt *clear = NULL;
    const char *clear_sql =
        "DELETE FROM card WHERE event = ? AND method = ? AND stage = ? AND case_key = ?";
    if (sqlite3_prepare_v2(bt_db_conn(db), clear_sql, -1, &clear, NULL) != SQLITE_OK) {
        err->kind = BT_DB_ERR_EXEC;
        snprintf(err->step, sizeof err->step, "%s", "clear case card");
        snprintf(err->detail, sizeof err->detail, "%s", sqlite3_errmsg(bt_db_conn(db)));
        bt_db_rollback(db);
        BT_TRACE_FAIL("repo_cards.save_card", "prepare clear failed");
        return -1;
    }
    sqlite3_bind_text(clear, 1, key->event, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(clear, 2, key->method, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(clear, 3, key->stage, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(clear, 4, key->case_key, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(clear) != SQLITE_DONE) {
        err->kind = BT_DB_ERR_EXEC;
        snprintf(err->step, sizeof err->step, "%s", "clear case card");
        snprintf(err->detail, sizeof err->detail, "%s", sqlite3_errmsg(bt_db_conn(db)));
        sqlite3_finalize(clear);
        bt_db_rollback(db);
        BT_TRACE_FAIL("repo_cards.save_card", "clear failed");
        return -1;
    }
    sqlite3_finalize(clear);
    if (bt_repo_srs_insert_card(db, key->event, key->method, key->stage, key->case_key,
                                card, now, err) != 0) {
        bt_db_rollback(db);
        BT_TRACE_FAIL("repo_cards.save_card", "insert failed");
        return -1;
    }
    if (bt_db_commit(db, err) != 0) {
        snprintf(err->step, sizeof err->step, "%s", "commit case card");
        bt_db_rollback(db);
        return -1;
    }
    BT_TRACE_EXIT("repo_cards.save_card");
    return 0;
}
