#ifndef BT_DATA_REPO_USER_H
#define BT_DATA_REPO_USER_H

#include "data/db.h"

#include <stddef.h>

#define BT_CASE_FIELD_MAX 128

typedef struct {
    char event[BT_CASE_FIELD_MAX];
    char method[BT_CASE_FIELD_MAX];
    char stage[BT_CASE_FIELD_MAX];
    char case_key[BT_CASE_FIELD_MAX];
} bt_case_ref;

typedef struct {
    bt_case_ref ref;
    char *alg;
} bt_override_entry;

typedef struct {
    bt_override_entry *items;
    size_t count;
    size_t cap;
} bt_overrides;

int bt_case_ref_cmp(const bt_case_ref *a, const bt_case_ref *b);
void bt_case_ref_of(bt_case_ref *out, const char *event, const char *method, const char *stage,
                    const char *case_key);
void bt_overrides_init(bt_overrides *ov);
void bt_overrides_free(bt_overrides *ov);
const char *bt_overrides_get(const bt_overrides *ov, const bt_case_ref *key);
size_t bt_overrides_len(const bt_overrides *ov);
int bt_repo_user_upsert(bt_db *db, const bt_case_ref *key, const char *alg, bt_db_error *err);
int bt_repo_user_remove(bt_db *db, const bt_case_ref *key, bt_db_error *err);
int bt_repo_user_load_all(const bt_db *db, bt_overrides *out, bt_db_error *err);

#endif
