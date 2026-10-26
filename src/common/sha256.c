#include "common/sha256.h"

#include <string.h>

#include "common/bt_trace.h"

static const uint32_t BT_SHA256_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static uint32_t rotr32(uint32_t value, unsigned bits) {
    return (value >> bits) | (value << (32U - bits));
}

static void sha256_block(bt_sha256_ctx *ctx, const unsigned char *block) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[(size_t)i * 4U] << 24U) |
               ((uint32_t)block[(size_t)i * 4U + 1U] << 16U) |
               ((uint32_t)block[(size_t)i * 4U + 2U] << 8U) | (uint32_t)block[(size_t)i * 4U + 3U];
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr32(w[i - 15], 7U) ^ rotr32(w[i - 15], 18U) ^ (w[i - 15] >> 3U);
        uint32_t s1 = rotr32(w[i - 2], 17U) ^ rotr32(w[i - 2], 19U) ^ (w[i - 2] >> 10U);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];
    uint32_t e = ctx->state[4];
    uint32_t f = ctx->state[5];
    uint32_t g = ctx->state[6];
    uint32_t h = ctx->state[7];
    for (int i = 0; i < 64; i++) {
        uint32_t sum1 = rotr32(e, 6U) ^ rotr32(e, 11U) ^ rotr32(e, 25U);
        uint32_t choice = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + sum1 + choice + BT_SHA256_K[i] + w[i];
        uint32_t sum0 = rotr32(a, 2U) ^ rotr32(a, 13U) ^ rotr32(a, 22U);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = sum0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

void bt_sha256_init(bt_sha256_ctx *ctx) {
    BT_TRACE_ENTER("sha256.init");
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->bits = 0;
    ctx->block_len = 0;
    BT_TRACE_EXIT("sha256.init");
}

void bt_sha256_update(bt_sha256_ctx *ctx, const void *data, size_t len) {
    BT_TRACE_ENTER("sha256.update");
    const unsigned char *cursor = data;
    ctx->bits += (uint64_t)len * 8U;
    while (len > 0) {
        size_t space = 64 - ctx->block_len;
        size_t take = len < space ? len : space;
        memcpy(ctx->block + ctx->block_len, cursor, take);
        ctx->block_len += take;
        cursor += take;
        len -= take;
        if (ctx->block_len == 64) {
            sha256_block(ctx, ctx->block);
            ctx->block_len = 0;
        }
    }
    BT_TRACE_EXIT("sha256.update");
}

void bt_sha256_final(bt_sha256_ctx *ctx, unsigned char digest[32]) {
    BT_TRACE_ENTER("sha256.final");
    uint64_t bits = ctx->bits;
    unsigned char pad = 0x80;
    bt_sha256_update(ctx, &pad, 1);
    unsigned char zero = 0;
    while (ctx->block_len != 56) {
        bt_sha256_update(ctx, &zero, 1);
    }
    unsigned char tail[8];
    for (int i = 0; i < 8; i++) {
        tail[i] = (unsigned char)(bits >> (56U - 8U * (unsigned)i));
    }
    ctx->bits = bits;
    const unsigned char *tail_cursor = tail;
    for (int i = 0; i < 8; i++) {
        ctx->block[56 + i] = *tail_cursor++;
    }
    sha256_block(ctx, ctx->block);
    ctx->block_len = 0;
    for (int i = 0; i < 8; i++) {
        digest[(size_t)i * 4U] = (unsigned char)(ctx->state[i] >> 24U);
        digest[(size_t)i * 4U + 1U] = (unsigned char)(ctx->state[i] >> 16U);
        digest[(size_t)i * 4U + 2U] = (unsigned char)(ctx->state[i] >> 8U);
        digest[(size_t)i * 4U + 3U] = (unsigned char)ctx->state[i];
    }
    BT_TRACE_EXIT("sha256.final");
}

void bt_sha256(const void *data, size_t len, unsigned char digest[32]) {
    BT_TRACE_ENTER("sha256.oneshot");
    bt_sha256_ctx ctx;
    bt_sha256_init(&ctx);
    bt_sha256_update(&ctx, data, len);
    bt_sha256_final(&ctx, digest);
    BT_TRACE_EXIT("sha256.oneshot");
}

void bt_sha256_hex(const unsigned char digest[32], char out[65]) {
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < 32; i++) {
        out[i * 2U] = digits[digest[i] >> 4U];
        out[i * 2U + 1U] = digits[digest[i] & 0x0FU];
    }
    out[64] = '\0';
}
