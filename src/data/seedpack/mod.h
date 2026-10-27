#ifndef BT_DATA_SEEDPACK_MOD_H
#define BT_DATA_SEEDPACK_MOD_H

#include <stdbool.h>
#include <stddef.h>

#include "data/db.h"

#define BT_SEED_FILE_MAX 96
#define BT_SEED_FIELD_MAX 48
#define BT_SEED_CASE_KEY_MAX 64
#define BT_SEED_HASH_LEN 65
#define BT_SEED_NSPEC_SIZES_MAX 8

typedef enum {
    BT_NSPEC_UNSET,
    BT_NSPEC_FIXED,
    BT_NSPEC_SIZES,
} bt_nspec_kind;

typedef struct {
    bt_nspec_kind kind;
    long long fixed;
    size_t size_count;
    long long sizes[BT_SEED_NSPEC_SIZES_MAX];
} bt_nspec;

typedef struct {
    char file[BT_SEED_FILE_MAX];
    char event[BT_SEED_FIELD_MAX];
    char method[BT_SEED_FIELD_MAX];
    char stage[BT_SEED_FIELD_MAX];
    bt_nspec n;
    size_t expected_rows;
} bt_pack_entry;

typedef struct {
    char event[BT_SEED_FIELD_MAX];
    char method[BT_SEED_FIELD_MAX];
    char stage[BT_SEED_FIELD_MAX];
    char case_key[BT_SEED_CASE_KEY_MAX];
    char *alg;
    char *front_json;
    char *source_url;
    char *source_at;
    char content_hash[BT_SEED_HASH_LEN];
} bt_seed_row;

typedef struct {
    bt_seed_row *items;
    size_t count;
} bt_seed_rows;

typedef enum {
    BT_SEED_ERR_REGISTRY_JSON,
    BT_SEED_ERR_REGISTRY_RULE,
    BT_SEED_ERR_IO,
    BT_SEED_ERR_BAD_HEADER,
    BT_SEED_ERR_RAGGED_ROW,
    BT_SEED_ERR_EMPTY_FIELD,
    BT_SEED_ERR_BAD_FIELD,
    BT_SEED_ERR_DUPLICATE_KEY,
    BT_SEED_ERR_BAD_FRONT_JSON,
    BT_SEED_ERR_FRONT_JSON_SHAPE,
    BT_SEED_ERR_NOTE_COLLISION,
    BT_SEED_ERR_COUNT_DRIFT,
    BT_SEED_ERR_NORMALIZE,
    BT_SEED_ERR_DRIFT,
    BT_SEED_ERR_STALE,
    BT_SEED_ERR_DB,
} bt_seed_error_kind;

typedef struct {
    bt_seed_error_kind kind;
    char path[BT_PATH_MAX];
    char file[BT_SEED_FILE_MAX];
    size_t line;
    size_t found;
    char case_key[BT_SEED_CASE_KEY_MAX];
    char column[32];
    char field[32];
    char json_error[160];
    char normalize[768];
    size_t expected;
    char stored[BT_SEED_HASH_LEN];
    char packed[BT_SEED_HASH_LEN];
    bt_db_error db;
} bt_seed_error;

void bt_seed_error_string(const bt_seed_error *err, char *out, size_t out_len);
bool bt_nspec_eq(const bt_nspec *a, const bt_nspec *b);
void bt_seed_rows_init(bt_seed_rows *rows);
void bt_seed_rows_free(bt_seed_rows *rows);

#endif
