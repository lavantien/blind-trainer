#include "common/bt_harness.h"
#include "common/sha256.h"

BT_TEST(sha256, nist_empty_message) {
    unsigned char digest[32];
    bt_sha256("", 0, digest);
    char hex[65];
    bt_sha256_hex(digest, hex);
    BT_CHECK(strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0);
}

BT_TEST(sha256, nist_abc) {
    unsigned char digest[32];
    bt_sha256("abc", 3, digest);
    char hex[65];
    bt_sha256_hex(digest, hex);
    BT_CHECK(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);
}

BT_TEST(sha256, nist_two_block_message) {
    const char *message = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    unsigned char digest[32];
    bt_sha256(message, strlen(message), digest);
    char hex[65];
    bt_sha256_hex(digest, hex);
    BT_CHECK(strcmp(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1") == 0);
}

BT_TEST(sha256, nist_million_a) {
    bt_sha256_ctx ctx;
    bt_sha256_init(&ctx);
    char block[1000];
    memset(block, 'a', sizeof block);
    for (int i = 0; i < 1000; i++) {
        bt_sha256_update(&ctx, block, sizeof block);
    }
    unsigned char digest[32];
    bt_sha256_final(&ctx, digest);
    char hex[65];
    bt_sha256_hex(digest, hex);
    BT_CHECK(strcmp(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") == 0);
}

BT_TEST(sha256, incremental_matches_oneshot) {
    const char *message = "the quick brown fox jumps over the lazy dog, twice for luck";
    bt_sha256_ctx ctx;
    bt_sha256_init(&ctx);
    for (size_t i = 0; i < strlen(message); i += 7) {
        size_t chunk = strlen(message) - i;
        if (chunk > 7) {
            chunk = 7;
        }
        bt_sha256_update(&ctx, message + i, chunk);
    }
    unsigned char incremental[32];
    bt_sha256_final(&ctx, incremental);
    unsigned char oneshot[32];
    bt_sha256(message, strlen(message), oneshot);
    BT_CHECK(memcmp(incremental, oneshot, 32) == 0);
}
