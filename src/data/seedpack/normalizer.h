#ifndef BT_DATA_SEEDPACK_NORMALIZER_H
#define BT_DATA_SEEDPACK_NORMALIZER_H

#include <stdbool.h>
#include <stddef.h>

#define BT_SEEDPACK_SQ1_EVENT "sq1"
#define BT_SEEDPACK_REPEAT_LIMIT 99
#define BT_SEEDPACK_EXPAND_LIMIT 100000
#define BT_SEEDPACK_ALG_MAX 1024

typedef struct {
    char *alg;
    char **notes;
    size_t note_count;
} bt_normalized;

typedef enum {
    BT_NORMALIZE_ERR_UNKNOWN_TOKEN,
    BT_NORMALIZE_ERR_UNBALANCED,
    BT_NORMALIZE_ERR_BAD_REPEAT,
    BT_NORMALIZE_ERR_BAD_CHAIN,
    BT_NORMALIZE_ERR_EMPTY_ALG,
} bt_normalize_error_kind;

typedef struct {
    bt_normalize_error_kind kind;
    char alg[BT_SEEDPACK_ALG_MAX];
    char token[256];
} bt_normalize_error;

void bt_normalize_error_string(const bt_normalize_error *err, char *out, size_t out_len);
bool bt_seedpack_normalize_alg(const char *event, const char *raw, bt_normalized *out,
                               bt_normalize_error *err);
void bt_normalized_free(bt_normalized *normalized);

#endif
