#ifndef VERIFY_DIGEST_H
#define VERIFY_DIGEST_H

#include <stddef.h>
#include <stdint.h>

#include "data/corpus.h"

#define VERIFY_SHA256_HEX_LEN 65
#define VERIFY_DIGEST_NAME_MAX 16
#define VERIFY_DIGEST_FILES ((size_t)BT_SOLVER_VENDOR_COUNT * BT_SHEET_CLASS_COUNT)
#define VERIFY_DIGEST_ERR_MAX 512

typedef struct {
    char solver[VERIFY_DIGEST_NAME_MAX];
    char sheet[VERIFY_DIGEST_NAME_MAX];
    size_t rows;
    char sha256[VERIFY_SHA256_HEX_LEN];
} verify_corpus_file_entry;

typedef struct {
    char sha256[VERIFY_SHA256_HEX_LEN];
    size_t rows;
    verify_corpus_file_entry files[VERIFY_DIGEST_FILES];
    size_t file_count;
} verify_corpus_digest;

void verify_sha256_hex(const void *bytes, size_t len, char out[VERIFY_SHA256_HEX_LEN]);
void verify_corpus_sha256_hex(const unsigned char *const *parts, const size_t *lens, size_t count,
                              char out[VERIFY_SHA256_HEX_LEN]);
int verify_digest_corpus(const char *root, verify_corpus_digest *out, char *err, size_t err_len);

#endif
