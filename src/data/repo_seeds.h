#ifndef BT_DATA_REPO_SEEDS_H
#define BT_DATA_REPO_SEEDS_H

#include <stddef.h>

#include "data/db.h"
#include "data/seedpack/mod.h"
#include "data/seedpack/parse.h"
#include "data/seedpack/registry.h"

typedef struct {
    char file[BT_SEED_FILE_MAX];
    size_t inserted;
    size_t verified;
} bt_seed_report;

int bt_seed_pack_tx(bt_db *db, const char *file, const bt_seed_row *rows, size_t count,
                    bt_seed_report *out, bt_seed_error *err);
int bt_seed_pack(bt_db *db, const char *packs_root, const bt_pack_entry *entry, bt_seed_report *out,
                 bt_seed_error *err);
int bt_seed_all(bt_db *db, const char *packs_root, bt_seed_report **out_reports, size_t *out_count,
                bt_seed_error *err);
void bt_seed_reports_free(bt_seed_report *reports);
int bt_seed_row_count(bt_db *db, long long *out, bt_seed_error *err);
int bt_load_seed_rows(bt_db *db, bt_seed_rows *out, bt_seed_error *err);

#endif
