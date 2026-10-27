#include "ga/hash.h"

#include "common/bt_harness.h"

#include <stdlib.h>
#include <string.h>

BT_TEST(hash, sha256_matches_reference_vectors) {
    char hex[BT_GA_SHA256_HEX_LEN];
    bt_ga_sha256_hex("", 0, hex);
    BT_CHECK_EQ(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    bt_ga_sha256_hex("abc", 3, hex);
    BT_CHECK_EQ(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    bt_ga_sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, hex);
    BT_CHECK_EQ(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    bt_ga_sha256_hex(
        "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklm"
        "nopqrlmnopqrsmnopqrstnopqrstu",
        112, hex);
    BT_CHECK_EQ(hex, "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
    char *million = malloc(1000000);
    memset(million, 'a', 1000000);
    bt_ga_sha256_hex(million, 1000000, hex);
    BT_CHECK_EQ(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    free(million);
    char again[BT_GA_SHA256_HEX_LEN];
    bt_ga_sha256_hex("không của tía", strlen("không của tía"), hex);
    bt_ga_sha256_hex("không của tía", strlen("không của tía"), again);
    BT_CHECK_EQ(hex, again);
}

BT_TEST(hash, sha256_handles_multiblock_and_utf8) {
    char first[BT_GA_SHA256_HEX_LEN];
    char *long_text = malloc(201);
    memset(long_text, 'a', 200);
    long_text[200] = 'a';
    bt_ga_sha256_hex(long_text, 200, first);
    BT_CHECK_EQ((long long)strlen(first), 64LL);
    char second[BT_GA_SHA256_HEX_LEN];
    bt_ga_sha256_hex(long_text, 201, second);
    BT_CHECK(strcmp(first, second) != 0);
    bt_ga_sha256_hex("nghiệm ươc uôcanh", strlen("nghiệm ươc uôcanh"), first);
    BT_CHECK_EQ((long long)strlen(first), 64LL);
    free(long_text);
}
