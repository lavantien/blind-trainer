#ifndef BT_DATA_REPO_SRS_H
#define BT_DATA_REPO_SRS_H

#include "common/json.h"
#include "data/db.h"
#include "data/fsrs.h"
#include "data/repo_algs.h"
#include "data/repo_user.h"
#include "data/srs.h"

#include <stddef.h>

typedef struct {
    char *key;
    bt_card card;
} bt_srs_entry;

typedef struct {
    bt_srs_entry *items;
    size_t count;
    size_t cap;
} bt_srs_store;

typedef enum {
    BT_SRS_LOAD_DB,
    BT_SRS_LOAD_UNKNOWN_QUALITY,
    BT_SRS_LOAD_INVALID_MEMORY,
} bt_srs_load_error_kind;

typedef struct {
    bt_srs_load_error_kind kind;
    bt_db_error db;
    char key[BT_REPO_ALGS_KEY_MAX];
    unsigned int quality;
    int has_stability;
    double stability;
    int has_difficulty;
    double difficulty;
} bt_srs_load_error;

void bt_srs_load_error_string(const bt_srs_load_error *err, char *out, size_t out_len);
void bt_srs_store_init(bt_srs_store *store);
void bt_srs_store_free(bt_srs_store *store);
int bt_srs_store_clone(const bt_srs_store *src, bt_srs_store *dst);
int bt_srs_store_eq(const bt_srs_store *a, const bt_srs_store *b);
int bt_srs_store_set(bt_srs_store *store, const char *key, const bt_card *card);
const bt_card *bt_srs_store_get(const bt_srs_store *store, const char *key);
size_t bt_srs_store_len(const bt_srs_store *store);
int bt_repo_srs_insert_card(bt_db *db, const char *event, const char *method, const char *stage,
                            const char *case_key, const bt_card *card, long long now,
                            bt_db_error *err);
int bt_repo_srs_save_tx(bt_db *db, const bt_srs_store *store, bt_db_error *err);
int bt_repo_srs_save(bt_db *db, const bt_srs_store *store, bt_db_error *err);
typedef enum {
    BT_SRS_FILTER_ONLY_3BLD,
    BT_SRS_FILTER_EXCLUDE_3BLD,
} bt_srs_filter;

typedef struct {
    char event[BT_CASE_FIELD_MAX];
    char method[BT_CASE_FIELD_MAX];
    char stage[BT_CASE_FIELD_MAX];
    char case_key[BT_CASE_FIELD_MAX];
    bt_card card;
} bt_srs_card_row;

int bt_repo_srs_load_rows(const bt_db *db, const bt_fsrs_config *cfg, bt_srs_filter filter,
                          bt_srs_card_row **out, size_t *count, bt_srs_load_error *err);
void bt_repo_srs_free_rows(bt_srs_card_row *rows, size_t count);
int bt_repo_srs_load_filtered(const bt_db *db, const bt_fsrs_config *cfg, bt_srs_filter filter,
                              bt_srs_store *out, bt_srs_load_error *err);
int bt_repo_srs_load(const bt_db *db, const bt_fsrs_config *cfg, bt_srs_store *out,
                     bt_srs_load_error *err);
int bt_srs_store_from_json(const bt_json_value *root, bt_srs_store *out);

#endif
