#ifndef BT_DATA_DIGEST_H
#define BT_DATA_DIGEST_H

#include <stddef.h>
#include <stdint.h>

#include "data/corpus.h"

#define BT_DATA_TOTAL_ROWS 1077
#define BT_DIGEST_SHA256_HEX_LEN 65
#define BT_DIGEST_NAME_MAX 16
#define BT_DIGEST_FILES ((size_t)BT_SOLVER_VENDOR_COUNT * BT_SHEET_CLASS_COUNT)
#define BT_DIGEST_ERR_MAX 512

typedef struct {
    char solver[BT_DIGEST_NAME_MAX];
    char sheet[BT_DIGEST_NAME_MAX];
    size_t rows;
    char sha256[BT_DIGEST_SHA256_HEX_LEN];
} bt_digest_file_entry;

typedef struct {
    char sha256[BT_DIGEST_SHA256_HEX_LEN];
    size_t rows;
    bt_digest_file_entry files[BT_DIGEST_FILES];
    size_t file_count;
} bt_corpus_digest;

void bt_digest_sha256_hex(const void *bytes, size_t len, char out[BT_DIGEST_SHA256_HEX_LEN]);
void bt_digest_corpus_sha256_hex(const unsigned char *const *parts, const size_t *lens,
                                 size_t count, char out[BT_DIGEST_SHA256_HEX_LEN]);
int bt_digest_corpus(const char *root, bt_corpus_digest *out, char *err, size_t err_len);

#endif
