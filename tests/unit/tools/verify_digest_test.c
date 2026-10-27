#include "verify_test_util.h"

#include <string.h>

#include "data/corpus.h"
#include "verify/digest.h"

static const bt_solver WALK_SOLVERS[BT_SOLVER_VENDOR_COUNT] = {
    BT_SOLVER_CHERRY,
    BT_SOLVER_EGGINS,
};
static const bt_sheet_class WALK_SHEETS[BT_SHEET_CLASS_COUNT] = {
    BT_SHEET_EDGE, BT_SHEET_CORNER, BT_SHEET_FLIPS, BT_SHEET_TWISTS, BT_SHEET_PARITY, BT_SHEET_LTCT,
};

BT_TEST(verify_digest, empty_input_matches_nist_vector) {
    char out[VERIFY_SHA256_HEX_LEN];
    verify_sha256_hex("", 0, out);
    BT_CHECK_EQ(out, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

BT_TEST(verify_digest, short_input_matches_nist_vector) {
    char out[VERIFY_SHA256_HEX_LEN];
    verify_sha256_hex("abc", 3, out);
    BT_CHECK_EQ(out, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

BT_TEST(verify_digest, two_block_input_matches_nist_vector) {
    char out[VERIFY_SHA256_HEX_LEN];
    verify_sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, out);
    BT_CHECK_EQ(out, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

BT_TEST(verify_digest, corpus_hash_is_the_ordered_concatenation) {
    char out[VERIFY_SHA256_HEX_LEN];
    char want[VERIFY_SHA256_HEX_LEN];
    const unsigned char *parts[2] = {(const unsigned char *)"a", (const unsigned char *)"bc"};
    const size_t lens[2] = {1, 2};
    verify_corpus_sha256_hex(parts, lens, 2, out);
    verify_sha256_hex("abc", 3, want);
    BT_CHECK_EQ(out, want);
    verify_corpus_sha256_hex(NULL, NULL, 0, out);
    verify_sha256_hex("", 0, want);
    BT_CHECK_EQ(out, want);
}

BT_TEST(verify_digest, digest_corpus_walks_vendor_sheets_in_canonical_order) {
    char root[TV_PATH_MAX];
    tv_scratch("digest-walk", root);
    unsigned char *texts[VERIFY_DIGEST_FILES];
    size_t text_lens[VERIFY_DIGEST_FILES];
    size_t slot = 0;
    for (size_t index = 0; index < BT_SOLVER_VENDOR_COUNT; index++) {
        for (size_t sheet_at = 0; sheet_at < BT_SHEET_CLASS_COUNT; sheet_at++) {
            size_t data_rows = index * (size_t)BT_SHEET_CLASS_COUNT + sheet_at + 1;
            char text[8192];
            size_t used =
                (size_t)snprintf(text, sizeof text,
                                 "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link\n");
            for (size_t row = 0; row < data_rows; row++) {
                used += (size_t)snprintf(text + used, sizeof text - used,
                                         "p%zu-%zu-%zu\tR U R' U'\t\t\t\n", index, sheet_at, row);
            }
            char path[TV_PATH_MAX];
            snprintf(path, sizeof path, "%s/%s/%s.tsv", root, bt_solver_dir(WALK_SOLVERS[index]),
                     bt_sheet_class_file(WALK_SHEETS[sheet_at]));
            tv_write_file(path, text);
            texts[slot] = malloc(used + 1);
            BT_CHECK(texts[slot] != NULL);
            memcpy(texts[slot], text, used + 1);
            text_lens[slot] = used;
            slot += 1;
        }
    }
    verify_corpus_digest digest;
    char err[VERIFY_DIGEST_ERR_MAX];
    int rc = verify_digest_corpus(root, &digest, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    char want[VERIFY_SHA256_HEX_LEN];
    verify_corpus_sha256_hex((const unsigned char *const *)texts, text_lens, VERIFY_DIGEST_FILES,
                             want);
    BT_CHECK_EQ(digest.sha256, want);
    BT_CHECK_EQ(digest.rows, 78);
    BT_CHECK_EQ(digest.file_count, 12);
    for (size_t at = 0; at < digest.file_count; at++) {
        BT_CHECK_EQ(digest.files[at].solver,
                    bt_solver_dir(WALK_SOLVERS[at / BT_SHEET_CLASS_COUNT]));
        BT_CHECK_EQ(digest.files[at].sheet,
                    bt_sheet_class_file(WALK_SHEETS[at % BT_SHEET_CLASS_COUNT]));
        BT_CHECK_EQ(digest.files[at].rows, at + 1);
        verify_sha256_hex(texts[at], text_lens[at], want);
        BT_CHECK_EQ(digest.files[at].sha256, want);
        free(texts[at]);
    }
    tv_rm_rf(root);
}

BT_TEST(verify_digest, digest_corpus_reports_a_missing_sheet) {
    char root[TV_PATH_MAX];
    tv_scratch("digest-miss", root);
    verify_corpus_digest digest;
    char err[VERIFY_DIGEST_ERR_MAX];
    int rc = verify_digest_corpus(root, &digest, err, sizeof err);
    BT_CHECK_EQ(rc, 1);
    BT_CHECK(strncmp(err, "read ", 5) == 0);
    tv_rm_rf(root);
}

BT_TEST(verify_digest, digest_corpus_rejects_a_malformed_sheet) {
    char root[TV_PATH_MAX];
    char path[TV_PATH_MAX];
    tv_scratch("digest-bad", root);
    snprintf(path, sizeof path, "%s/%s/%s.tsv", root, bt_solver_dir(BT_SOLVER_CHERRY),
             bt_sheet_class_file(BT_SHEET_EDGE));
    tv_write_file(path,
                  "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link\nonly\ttwo\n");
    verify_corpus_digest digest;
    char err[VERIFY_DIGEST_ERR_MAX];
    int rc = verify_digest_corpus(root, &digest, err, sizeof err);
    BT_CHECK_EQ(rc, 1);
    BT_CHECK(strncmp(err, "parse ", 6) == 0);
    tv_rm_rf(root);
}
