#ifndef BT_APP_PERSIST_H
#define BT_APP_PERSIST_H

#include "common/json.h"
#include "common/paths.h"
#include "data/db.h"
#include "data/fsrs.h"
#include "data/import.h"
#include "data/paths.h"
#include "data/repo_algs.h"
#include "data/repo_seeds.h"
#include "data/repo_settings.h"
#include "data/repo_solves.h"
#include "data/repo_srs.h"

#include <stddef.h>

#define BT_APP_ERROR_MAX 192
#define BT_MEMO_MODE_COUNT 3

typedef enum {
    BT_MEMO_SYLLABLES = 0,
    BT_MEMO_IMAGES,
    BT_MEMO_PER_ORBIT,
} bt_memo_mode;

typedef struct {
    const char *label;
    bt_memo_mode mode;
} bt_memo_label;

extern const bt_memo_label BT_MEMO_MODE_LABELS[BT_MEMO_MODE_COUNT];

typedef struct {
    unsigned int memo_cadence_ms;
    bt_memo_mode memo_mode;
    unsigned int drill_reps;
} bt_settings;

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} bt_str_list;

typedef struct {
    bt_seed_rows rows;
    bt_pack_entry *packs;
    size_t pack_count;
    size_t pack_cap;
    bt_str_list hidden;
    bt_str_list warnings;
} bt_seed_repo;

typedef struct {
    bt_settings settings;
    bt_srs_store srs;
    bt_solve_log solves;
    bt_user_algs user_algs;
    bt_seed_repo seeds;
} bt_stores;

int bt_memo_mode_parse(const char *text, bt_memo_mode *out);
const char *bt_memo_mode_label(bt_memo_mode mode);
int bt_memo_mode_index(bt_memo_mode mode);
int bt_memo_mode_from_index(int index, bt_memo_mode *out);
void bt_str_list_init(bt_str_list *list);
void bt_str_list_free(bt_str_list *list);
int bt_str_list_push(bt_str_list *list, const char *text);
int bt_str_list_sorted_insert_unique(bt_str_list *list, const char *text);
int bt_settings_from_json(const bt_json_value *value, bt_settings *out, char *error,
                          size_t error_len);
bt_json_value *bt_settings_to_json(const bt_settings *settings);
int bt_stores_load(const char *root, bt_settings defaults, const bt_fsrs_config *fsrs,
                   bt_stores *out, char *error, size_t error_len);
int bt_stores_load_with_packs(const char *root, bt_settings defaults, const bt_fsrs_config *fsrs,
                              const char *packs_root, bt_stores *out, char *error,
                              size_t error_len);
void bt_stores_free(bt_stores *stores);
int bt_app_save_settings(const char *root, const bt_settings *settings, char *error,
                         size_t error_len);
int bt_app_save_srs(const char *root, const bt_srs_store *srs, char *error, size_t error_len);
int bt_app_save_solves(const char *root, const bt_solve_log *solves, char *error, size_t error_len);
int bt_app_save_user_algs(const char *root, const bt_user_algs *algs, char *error,
                          size_t error_len);

#endif
