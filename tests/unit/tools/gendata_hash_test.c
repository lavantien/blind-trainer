#include "data/digest.h"

#include <string.h>

#include "common/bt_harness.h"

BT_TEST(gendata_hash, empty_input_matches_nist_vector) {
    char out[BT_DIGEST_SHA256_HEX_LEN];
    bt_digest_sha256_hex("", 0, out);
    BT_CHECK_EQ(strcmp(out, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"), 0);
}

BT_TEST(gendata_hash, short_input_matches_nist_vector) {
    char out[BT_DIGEST_SHA256_HEX_LEN];
    bt_digest_sha256_hex("abc", 3, out);
    BT_CHECK_EQ(strcmp(out, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"), 0);
}

BT_TEST(gendata_hash, two_block_input_matches_nist_vector) {
    char out[BT_DIGEST_SHA256_HEX_LEN];
    bt_digest_sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, out);
    BT_CHECK_EQ(strcmp(out, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"), 0);
}

BT_TEST(gendata_hash, corpus_hash_is_the_ordered_concatenation) {
    const unsigned char *parts[2] = {(const unsigned char *)"a", (const unsigned char *)"bc"};
    const size_t lens[2] = {1, 2};
    char out[BT_DIGEST_SHA256_HEX_LEN];
    bt_digest_corpus_sha256_hex(parts, lens, 2, out);
    char joined[BT_DIGEST_SHA256_HEX_LEN];
    bt_digest_sha256_hex("abc", 3, joined);
    BT_CHECK_EQ(strcmp(out, joined), 0);
    bt_digest_corpus_sha256_hex(NULL, NULL, 0, out);
    bt_digest_sha256_hex("", 0, joined);
    BT_CHECK_EQ(strcmp(out, joined), 0);
}
