#ifndef BT_SHA256_H
#define BT_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t state[8];
    uint64_t bits;
    unsigned char block[64];
    size_t block_len;
} bt_sha256_ctx;

void bt_sha256_init(bt_sha256_ctx *ctx);
void bt_sha256_update(bt_sha256_ctx *ctx, const void *data, size_t len);
void bt_sha256_final(bt_sha256_ctx *ctx, unsigned char digest[32]);
void bt_sha256(const void *data, size_t len, unsigned char digest[32]);
void bt_sha256_hex(const unsigned char digest[32], char out[65]);

#endif
