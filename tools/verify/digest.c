#include "verify/digest.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/sha256.h"
#include "data/tsv.h"

static const bt_solver VENDOR_ORDER[BT_SOLVER_VENDOR_COUNT] = {
    BT_SOLVER_CHERRY,
    BT_SOLVER_EGGINS,
};
static const bt_sheet_class SHEET_ORDER[BT_SHEET_CLASS_COUNT] = {
    BT_SHEET_EDGE, BT_SHEET_CORNER, BT_SHEET_FLIPS, BT_SHEET_TWISTS, BT_SHEET_PARITY, BT_SHEET_LTCT,
};

void verify_sha256_hex(const void *bytes, size_t len, char out[VERIFY_SHA256_HEX_LEN]) {
    BT_TRACE_ENTER("verify.sha256_hex");
    unsigned char digest[32];
    bt_sha256(bytes, len, digest);
    bt_sha256_hex(digest, out);
    BT_TRACE_EXIT("verify.sha256_hex");
}

void verify_corpus_sha256_hex(const unsigned char *const *parts, const size_t *lens, size_t count,
                              char out[VERIFY_SHA256_HEX_LEN]) {
    BT_TRACE_ENTER("verify.corpus_sha256_hex");
    bt_sha256_ctx ctx;
    bt_sha256_init(&ctx);
    for (size_t at = 0; at < count; at++) {
        bt_sha256_update(&ctx, parts[at], lens[at]);
    }
    unsigned char digest[32];
    bt_sha256_final(&ctx, digest);
    bt_sha256_hex(digest, out);
    BT_TRACE_EXIT("verify.corpus_sha256_hex");
}

static char *read_sheet(const char *path, char *err, size_t err_len) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        snprintf(err, err_len, "read %s: %s", path, strerror(errno));
        BT_TRACE_FAIL("verify.read_sheet", "open failed %s", path);
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0) {
        snprintf(err, err_len, "read %s: %s", path, strerror(errno));
        BT_TRACE_FAIL("verify.read_sheet", "size failed %s", path);
        fclose(file);
        return NULL;
    }
    char *text = malloc((size_t)size + 1);
    if (text == NULL) {
        snprintf(err, err_len, "read %s: out of memory", path);
        BT_TRACE_FAIL("verify.read_sheet", "oom %s", path);
        fclose(file);
        return NULL;
    }
    size_t got = fread(text, 1, (size_t)size, file);
    fclose(file);
    text[got] = '\0';
    return text;
}

int verify_digest_corpus(const char *root, verify_corpus_digest *out, char *err, size_t err_len) {
    BT_TRACE_ENTER("verify.digest_corpus");
    memset(out, 0, sizeof *out);
    bt_sha256_ctx ctx;
    bt_sha256_init(&ctx);
    size_t slot = 0;
    for (size_t solver_at = 0; solver_at < BT_SOLVER_VENDOR_COUNT; solver_at++) {
        for (size_t sheet_at = 0; sheet_at < BT_SHEET_CLASS_COUNT; sheet_at++) {
            bt_solver solver = VENDOR_ORDER[solver_at];
            bt_sheet_class sheet = SHEET_ORDER[sheet_at];
            char path[VERIFY_DIGEST_ERR_MAX];
            snprintf(path, sizeof path, "%s/%s/%s.tsv", root, bt_solver_dir(solver),
                     bt_sheet_class_file(sheet));
            char *text = read_sheet(path, err, err_len);
            if (text == NULL) {
                BT_TRACE_EXIT("verify.digest_corpus");
                return 1;
            }
            bt_tsv_rows rows;
            bt_tsv_error tsv_err;
            if (bt_tsv_parse_sheet(text, &rows, &tsv_err) != 0) {
                char detail[160];
                bt_tsv_error_string(&tsv_err, detail, sizeof detail);
                snprintf(err, err_len, "parse %s: %s", path, detail);
                bt_tsv_rows_free(&rows);
                free(text);
                BT_TRACE_FAIL("verify.digest_corpus", "parse failed %s", path);
                BT_TRACE_EXIT("verify.digest_corpus");
                return 1;
            }
            size_t count = rows.count;
            bt_tsv_rows_free(&rows);
            verify_corpus_file_entry *entry = &out->files[slot];
            snprintf(entry->solver, sizeof entry->solver, "%s", bt_solver_dir(solver));
            snprintf(entry->sheet, sizeof entry->sheet, "%s", bt_sheet_class_file(sheet));
            entry->rows = count;
            unsigned char digest[32];
            bt_sha256(text, strlen(text), digest);
            bt_sha256_hex(digest, entry->sha256);
            bt_sha256_update(&ctx, text, strlen(text));
            out->rows += count;
            free(text);
            slot += 1;
        }
    }
    unsigned char digest[32];
    bt_sha256_final(&ctx, digest);
    bt_sha256_hex(digest, out->sha256);
    out->file_count = slot;
    BT_TRACE_EXIT("verify.digest_corpus");
    return 0;
}
