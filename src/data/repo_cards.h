#ifndef BT_DATA_REPO_CARDS_H
#define BT_DATA_REPO_CARDS_H

#include "data/db.h"
#include "data/fsrs.h"
#include "data/repo_srs.h"
#include "data/repo_user.h"
#include "data/srs.h"

#include <stddef.h>

typedef struct {
    bt_case_ref ref;
    bt_card card;
} bt_case_card_entry;

typedef struct {
    bt_case_card_entry *items;
    size_t count;
    size_t cap;
} bt_case_cards;

void bt_case_cards_init(bt_case_cards *cards);
void bt_case_cards_free(bt_case_cards *cards);
const bt_card *bt_case_cards_get(const bt_case_cards *cards, const bt_case_ref *key);
size_t bt_case_cards_len(const bt_case_cards *cards);
int bt_case_cards_contains(const bt_case_cards *cards, const bt_case_ref *key);
int bt_repo_cards_load(const bt_db *db, const bt_fsrs_config *cfg, bt_case_cards *out,
                       bt_srs_load_error *err);
int bt_repo_cards_save_card(bt_db *db, const bt_case_ref *key, const bt_card *card,
                            bt_db_error *err);

#endif
