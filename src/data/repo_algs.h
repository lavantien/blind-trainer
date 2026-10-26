#ifndef BT_DATA_REPO_ALGS_H
#define BT_DATA_REPO_ALGS_H

#include "data/db.h"
#include "data/paths.h"

#define BT_REPO_ALGS_KEY_MAX (BT_PATH_MAX * 2)

void bt_repo_algs_split_key(const char *key, char *stage, size_t stage_len,
                            char *case_key, size_t case_key_len);
void bt_repo_algs_join_key(const char *stage, const char *case_key, char *out,
                           size_t out_len);
int bt_repo_algs_save_tx(bt_db *db, const bt_user_algs *algs, bt_db_error *err);
int bt_repo_algs_save(bt_db *db, const bt_user_algs *algs, bt_db_error *err);
int bt_repo_algs_load(const bt_db *db, bt_user_algs *out, bt_db_error *err);

#endif
