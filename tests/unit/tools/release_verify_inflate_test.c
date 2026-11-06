#include "release-verify/inflate.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_harness.h"
#include "unit/tools/release_verify_test_util.h"

enum {
    INF_TYPE_FINAL = 1,
    INF_TYPE_FIXED = 1,
    INF_TYPE_DYNAMIC = 2,
    INF_TYPE_STORED_PAD = 5,
    INF_HEADER_BITS = 3,
    INF_HLIT_BITS = 5,
    INF_HDIST_BITS = 5,
    INF_HCLEN_BITS = 4,
    INF_CL_BITS = 3,
    INF_DIST_BITS = 5,
    INF_BAD_BLOCK_TYPE = 3,
    INF_BAD_LENGTH_SYMBOL = 286,
    INF_BAD_DIST_SYMBOL = 30,
    INF_LEN_SYMBOL_NEAR = 257,
    INF_LEN_SYMBOL_WIDE = 265,
    INF_DIST_SYMBOL_WIDE = 4,
    INF_LIT_WIDE = 200,
    INF_WIDE_LIT_COUNT = 6,
    INF_TAIL_LIT_COUNT = 5,
    INF_CL_MIN_CODES = 4,
    INF_CL_WIDE_COUNT = 10,
    INF_HCLEN_WIDE = 6,
    INF_COPY_EXTRA_BITS = 2,
    INF_ZERO_EXTRA_BITS = 3,
    INF_RUN_EXTRA_BITS = 7,
    INF_RUN_BASE = 11,
    INF_ZERO_BASE = 3,
    INF_COPY_BASE = 3,
    INF_LIT_A = 65,
    INF_END = 256,
    INF_GROWTH_HALF = 40000,
    INF_GROWTH_TOTAL = 80000,
    INF_GROWTH_CAP = 1 << 16,
    INF_BUF = 131072,
};

static int inflate_raw_case(const unsigned char *data, size_t len, char *err) {
    unsigned char *out = NULL;
    size_t out_len = 0;
    int rc = bt_rv_inflate_raw(data, len, &out, &out_len, err, BT_RV_ERR_MAX);
    free(out);
    return rc;
}

static void block_start(rv_bitwriter *writer, unsigned final, unsigned type) {
    rv_bits_put_lsb(writer, final, 1);
    rv_bits_put_lsb(writer, type, 2);
}

static void stored_block(rv_bitwriter *writer, unsigned final, const unsigned char *data,
                         size_t len) {
    block_start(writer, final, 0);
    rv_bits_put_lsb(writer, 0, INF_TYPE_STORED_PAD);
    rv_bits_put_lsb(writer, (unsigned)(len & 0xFFU), 8);
    rv_bits_put_lsb(writer, (unsigned)((len >> 8U) & 0xFFU), 8);
    rv_bits_put_lsb(writer, (unsigned)((~len & 0xFFU)), 8);
    rv_bits_put_lsb(writer, (unsigned)(((~len >> 8U) & 0xFFU)), 8);
    for (size_t at = 0; at < len; at++) {
        rv_bits_put_lsb(writer, data[at], 8);
    }
}

static void flush_tail(rv_bitwriter *writer) {
    if (writer->cnt > 0) {
        writer->data[writer->len++] = (unsigned char)(writer->bits & 0xFFU);
    }
}

static void dynamic_header(rv_bitwriter *writer, unsigned hlit, unsigned hdist, unsigned hclen,
                           const unsigned char *cl_lengths) {
    block_start(writer, INF_TYPE_FINAL, INF_TYPE_DYNAMIC);
    rv_bits_put_lsb(writer, hlit, INF_HLIT_BITS);
    rv_bits_put_lsb(writer, hdist, INF_HDIST_BITS);
    rv_bits_put_lsb(writer, hclen, INF_HCLEN_BITS);
    for (unsigned at = 0; at < hclen + INF_CL_MIN_CODES; at++) {
        rv_bits_put_lsb(writer, cl_lengths[at], INF_CL_BITS);
    }
}

BT_TEST(release_verify, inflate_raw_rejects_a_type_three_block) {
    unsigned char buf[4] = {0x07, 0, 0, 0};
    char err[BT_RV_ERR_MAX];
    BT_CHECK(inflate_raw_case(buf, sizeof buf, err) != 0);
    BT_CHECK(strstr(err, "bad block type") != NULL);
}

BT_TEST(release_verify, inflate_raw_rejects_a_truncated_block_header) {
    unsigned char buf[2] = {0x01, 0};
    char err[BT_RV_ERR_MAX];
    BT_CHECK(inflate_raw_case(buf, 0, err) != 0);
    BT_CHECK(strstr(err, "input exhausted") != NULL);
}

BT_TEST(release_verify, inflate_raw_rejects_a_stored_length_mismatch) {
    unsigned char data[9] = "payload!";
    unsigned char buf[16];
    rv_bitwriter writer = {.data = buf, .len = 0, .cap = sizeof buf, .bits = 0, .cnt = 0};
    block_start(&writer, INF_TYPE_FINAL, 0);
    rv_bits_put_lsb(&writer, 0, INF_TYPE_STORED_PAD);
    rv_bits_put_lsb(&writer, 8, 8);
    rv_bits_put_lsb(&writer, 0, 8);
    rv_bits_put_lsb(&writer, (unsigned)~8U & 0xFFU, 8);
    rv_bits_put_lsb(&writer, 0, 8);
    for (size_t at = 0; at < sizeof data; at++) {
        rv_bits_put_lsb(&writer, data[at], 8);
    }
    flush_tail(&writer);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(inflate_raw_case(buf, writer.len, err) != 0);
    BT_CHECK(strstr(err, "stored block length mismatch") != NULL);
}

BT_TEST(release_verify, inflate_raw_rejects_a_truncated_stored_header) {
    unsigned char buf[8];
    rv_bitwriter writer = {.data = buf, .len = 0, .cap = sizeof buf, .bits = 0, .cnt = 0};
    block_start(&writer, INF_TYPE_FINAL, 0);
    rv_bits_put_lsb(&writer, 0, INF_TYPE_STORED_PAD);
    flush_tail(&writer);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(inflate_raw_case(buf, writer.len, err) != 0);
    BT_CHECK(strstr(err, "stored block truncated") != NULL);
}

BT_TEST(release_verify, inflate_raw_rejects_truncated_stored_data) {
    unsigned char buf[16];
    rv_bitwriter writer = {.data = buf, .len = 0, .cap = sizeof buf, .bits = 0, .cnt = 0};
    stored_block(&writer, INF_TYPE_FINAL, (const unsigned char *)"0123456789", 10);
    writer.len -= 7;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(inflate_raw_case(buf, writer.len, err) != 0);
    BT_CHECK(strstr(err, "stored block truncated") != NULL);
}

BT_TEST(release_verify, inflate_raw_rejects_a_bad_length_symbol) {
    unsigned char buf[8];
    rv_bitwriter writer = {.data = buf, .len = 0, .cap = sizeof buf, .bits = 0, .cnt = 0};
    block_start(&writer, INF_TYPE_FINAL, INF_TYPE_FIXED);
    rv_fixed_lit(&writer, INF_BAD_LENGTH_SYMBOL);
    flush_tail(&writer);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(inflate_raw_case(buf, writer.len, err) != 0);
    BT_CHECK(strstr(err, "bad length symbol") != NULL);
}

BT_TEST(release_verify, inflate_raw_rejects_a_bad_distance_symbol) {
    unsigned char buf[8];
    rv_bitwriter writer = {.data = buf, .len = 0, .cap = sizeof buf, .bits = 0, .cnt = 0};
    block_start(&writer, INF_TYPE_FINAL, INF_TYPE_FIXED);
    rv_fixed_lit(&writer, INF_LEN_SYMBOL_NEAR);
    rv_bits_put(&writer, INF_BAD_DIST_SYMBOL, INF_DIST_BITS);
    flush_tail(&writer);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(inflate_raw_case(buf, writer.len, err) != 0);
    BT_CHECK(strstr(err, "bad distance symbol") != NULL);
}

BT_TEST(release_verify, inflate_raw_rejects_a_distance_before_start) {
    unsigned char buf[8];
    rv_bitwriter writer = {.data = buf, .len = 0, .cap = sizeof buf, .bits = 0, .cnt = 0};
    block_start(&writer, INF_TYPE_FINAL, INF_TYPE_FIXED);
    rv_fixed_lit(&writer, INF_LEN_SYMBOL_NEAR);
    rv_bits_put(&writer, 0, INF_DIST_BITS);
    rv_bits_put(&writer, 0, 1);
    flush_tail(&writer);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(inflate_raw_case(buf, writer.len, err) != 0);
    BT_CHECK(strstr(err, "distance before start") != NULL);
}

BT_TEST(release_verify, inflate_raw_rejects_input_exhausted_mid_symbol) {
    unsigned char buf[8];
    rv_bitwriter writer = {.data = buf, .len = 0, .cap = sizeof buf, .bits = 0, .cnt = 0};
    block_start(&writer, INF_TYPE_FINAL, INF_TYPE_FIXED);
    flush_tail(&writer);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(inflate_raw_case(buf, writer.len, err) != 0);
    BT_CHECK(strstr(err, "input exhausted") != NULL);
}

BT_TEST(release_verify, inflate_raw_grows_the_sink_past_the_first_reserve) {
    unsigned char *half = malloc(INF_GROWTH_HALF);
    BT_CHECK(half != NULL);
    if (half == NULL) {
        return;
    }
    for (size_t at = 0; at < INF_GROWTH_HALF; at++) {
        half[at] = (unsigned char)(at & 0xFFU);
    }
    unsigned char *buf = malloc(INF_BUF);
    BT_CHECK(buf != NULL);
    if (buf == NULL) {
        free(half);
        return;
    }
    rv_bitwriter writer = {.data = buf, .len = 0, .cap = INF_BUF, .bits = 0, .cnt = 0};
    stored_block(&writer, 0, half, INF_GROWTH_HALF);
    stored_block(&writer, INF_TYPE_FINAL, half, INF_GROWTH_HALF);
    flush_tail(&writer);
    unsigned char *out = NULL;
    size_t out_len = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK_EQ(bt_rv_inflate_raw(buf, writer.len, &out, &out_len, err, sizeof err), 0);
    BT_CHECK_EQ(out_len, (size_t)INF_GROWTH_TOTAL);
    BT_CHECK(out_len == (size_t)INF_GROWTH_TOTAL && memcmp(out, half, INF_GROWTH_HALF) == 0 &&
             memcmp(out + INF_GROWTH_HALF, half, INF_GROWTH_HALF) == 0);
    free(out);
    free(buf);
    free(half);
}

BT_TEST(release_verify, inflate_raw_rejects_a_cut_at_each_take_site) {
    unsigned char buf[16];
    unsigned char cl[19];
    char err[BT_RV_ERR_MAX];
    memset(cl, 0, sizeof cl);
    cl[0] = 1;
    for (size_t cut = 1; cut <= 3; cut++) {
        rv_bitwriter writer = {.data = buf, .len = 0, .cap = sizeof buf, .bits = 0, .cnt = 0};
        dynamic_header(&writer, 0, 0, 0, cl);
        flush_tail(&writer);
        BT_CHECK(inflate_raw_case(buf, cut, err) != 0);
        BT_CHECK(strstr(err, "input exhausted") != NULL);
    }
    memset(cl, 0, sizeof cl);
    cl[0] = 1;
    cl[4] = 1;
    rv_bitwriter copy = {.data = buf, .len = 0, .cap = sizeof buf, .bits = 0, .cnt = 0};
    dynamic_header(&copy, 0, 0, 11, cl);
    rv_bits_put(&copy, 0, 1);
    rv_bits_put(&copy, 1, 1);
    flush_tail(&copy);
    BT_CHECK(inflate_raw_case(buf, copy.len, err) != 0);
    BT_CHECK(strstr(err, "input exhausted") != NULL);
    for (size_t site = 1; site <= 2; site++) {
        memset(cl, 0, sizeof cl);
        cl[site] = 1;
        rv_bitwriter writer = {.data = buf, .len = 0, .cap = sizeof buf, .bits = 0, .cnt = 0};
        dynamic_header(&writer, 0, 0, INF_HCLEN_WIDE, cl);
        rv_bits_put(&writer, 0, 1);
        flush_tail(&writer);
        BT_CHECK(inflate_raw_case(buf, writer.len, err) != 0);
        BT_CHECK(strstr(err, "input exhausted") != NULL);
    }
    rv_bitwriter wide = {.data = buf, .len = 0, .cap = sizeof buf, .bits = 0, .cnt = 0};
    block_start(&wide, INF_TYPE_FINAL, INF_TYPE_FIXED);
    for (int at = 0; at < INF_WIDE_LIT_COUNT; at++) {
        rv_fixed_lit(&wide, INF_LIT_WIDE);
    }
    rv_fixed_lit(&wide, INF_LEN_SYMBOL_WIDE);
    flush_tail(&wide);
    BT_CHECK(inflate_raw_case(buf, wide.len, err) != 0);
    BT_CHECK(strstr(err, "input exhausted") != NULL);
    rv_bitwriter dist = {.data = buf, .len = 0, .cap = sizeof buf, .bits = 0, .cnt = 0};
    block_start(&dist, INF_TYPE_FINAL, INF_TYPE_FIXED);
    rv_fixed_lit(&dist, INF_LIT_WIDE);
    rv_fixed_lit(&dist, INF_LEN_SYMBOL_NEAR);
    rv_bits_put(&dist, INF_DIST_SYMBOL_WIDE, INF_DIST_BITS);
    flush_tail(&dist);
    BT_CHECK(inflate_raw_case(buf, dist.len, err) != 0);
    BT_CHECK(strstr(err, "input exhausted") != NULL);
    rv_bitwriter tail = {.data = buf, .len = 0, .cap = sizeof buf, .bits = 0, .cnt = 0};
    block_start(&tail, 0, INF_TYPE_FIXED);
    for (int at = 0; at < INF_TAIL_LIT_COUNT; at++) {
        rv_fixed_lit(&tail, INF_LIT_WIDE);
    }
    rv_fixed_lit(&tail, INF_END);
    flush_tail(&tail);
    BT_CHECK(inflate_raw_case(buf, tail.len, err) != 0);
    BT_CHECK(strstr(err, "input exhausted") != NULL);
}

BT_TEST(release_verify, inflate_dynamic_rejects_an_oversubscribed_code_length_alphabet) {
    unsigned char buf[32];
    unsigned char cl[19];
    for (size_t at = 0; at < sizeof cl; at++) {
        cl[at] = 1;
    }
    rv_bitwriter writer = {.data = buf, .len = 0, .cap = sizeof buf, .bits = 0, .cnt = 0};
    dynamic_header(&writer, 0, 0, 15, cl);
    flush_tail(&writer);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(inflate_raw_case(buf, writer.len, err) != 0);
    BT_CHECK(strstr(err, "oversubscribed code lengths") != NULL);
}

BT_TEST(release_verify, inflate_dynamic_rejects_a_repeat_before_any_length) {
    unsigned char buf[16];
    unsigned char cl[4] = {1, 0, 0, 0};
    rv_bitwriter writer = {.data = buf, .len = 0, .cap = sizeof buf, .bits = 0, .cnt = 0};
    dynamic_header(&writer, 0, 0, 0, cl);
    rv_bits_put(&writer, 0, 1);
    flush_tail(&writer);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(inflate_raw_case(buf, writer.len, err) != 0);
    BT_CHECK(strstr(err, "repeat with no prior") != NULL);
}

BT_TEST(release_verify, inflate_dynamic_rejects_too_many_lengths) {
    unsigned char buf[32];
    unsigned char cl[4] = {0, 0, 1, 0};
    rv_bitwriter writer = {.data = buf, .len = 0, .cap = sizeof buf, .bits = 0, .cnt = 0};
    dynamic_header(&writer, 0, 0, 0, cl);
    for (int rep = 0; rep < 2; rep++) {
        rv_bits_put(&writer, 0, 1);
        rv_bits_put_lsb(&writer, 127, INF_RUN_EXTRA_BITS);
    }
    flush_tail(&writer);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(inflate_raw_case(buf, writer.len, err) != 0);
    BT_CHECK(strstr(err, "too many lengths") != NULL);
}

BT_TEST(release_verify, inflate_dynamic_rejects_oversubscribed_literal_codes) {
    unsigned char buf[64];
    unsigned char cl[19];
    memset(cl, 0, sizeof cl);
    cl[17] = 1;
    rv_bitwriter writer = {.data = buf, .len = 0, .cap = sizeof buf, .bits = 0, .cnt = 0};
    dynamic_header(&writer, 0, 0, 14, cl);
    for (int at = 0; at < 258; at++) {
        rv_bits_put(&writer, 0, 1);
    }
    flush_tail(&writer);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(inflate_raw_case(buf, writer.len, err) != 0);
    BT_CHECK(strstr(err, "oversubscribed literal codes") != NULL);
}

BT_TEST(release_verify, inflate_dynamic_rejects_oversubscribed_distance_codes) {
    unsigned char buf[64];
    unsigned char cl[19];
    memset(cl, 0, sizeof cl);
    cl[3] = 1;
    cl[17] = 1;
    rv_bitwriter writer = {.data = buf, .len = 0, .cap = sizeof buf, .bits = 0, .cnt = 0};
    dynamic_header(&writer, 0, 2, 14, cl);
    for (int at = 0; at < 257; at++) {
        rv_bits_put(&writer, 0, 1);
    }
    for (int at = 0; at < 3; at++) {
        rv_bits_put(&writer, 1, 1);
    }
    flush_tail(&writer);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(inflate_raw_case(buf, writer.len, err) != 0);
    BT_CHECK(strstr(err, "oversubscribed distance codes") != NULL);
}

BT_TEST(release_verify, inflate_dynamic_rejects_an_invalid_code_length_code) {
    unsigned char buf[16];
    unsigned char cl[4] = {0, 0, 1, 0};
    rv_bitwriter writer = {.data = buf, .len = 0, .cap = sizeof buf, .bits = 0, .cnt = 0};
    dynamic_header(&writer, 0, 0, 0, cl);
    rv_bits_put(&writer, 1, 1);
    rv_bits_put(&writer, 1, 2);
    rv_bits_put(&writer, 1, 4);
    rv_bits_put(&writer, 1, 8);
    flush_tail(&writer);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(inflate_raw_case(buf, writer.len, err) != 0);
    BT_CHECK(strstr(err, "bad code length") != NULL);
}

BT_TEST(release_verify, inflate_dynamic_round_trips_literals_and_repeats) {
    unsigned char buf[64];
    unsigned char cl[19];
    memset(cl, 0, sizeof cl);
    cl[0] = 2;
    cl[2] = 2;
    cl[3] = 2;
    cl[13] = 2;
    rv_bitwriter writer = {.data = buf, .len = 0, .cap = sizeof buf, .bits = 0, .cnt = 0};
    dynamic_header(&writer, 0, 0, 10, cl);
    rv_bits_put(&writer, 3, 2);
    rv_bits_put_lsb(&writer, 54, INF_RUN_EXTRA_BITS);
    rv_bits_put(&writer, 1, 2);
    rv_bits_put(&writer, 2, 2);
    rv_bits_put_lsb(&writer, 0, INF_COPY_EXTRA_BITS);
    rv_bits_put(&writer, 3, 2);
    rv_bits_put_lsb(&writer, 127, INF_RUN_EXTRA_BITS);
    rv_bits_put(&writer, 3, 2);
    rv_bits_put_lsb(&writer, 38, INF_RUN_EXTRA_BITS);
    rv_bits_put(&writer, 1, 2);
    rv_bits_put(&writer, 0, 2);
    rv_bits_put(&writer, 0, 3);
    rv_bits_put(&writer, 4, 3);
    flush_tail(&writer);
    unsigned char *out = NULL;
    size_t out_len = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK_EQ(bt_rv_inflate_raw(buf, writer.len, &out, &out_len, err, sizeof err), 0);
    BT_CHECK_EQ(out_len, 1U);
    BT_CHECK(out_len == 1U && out[0] == INF_LIT_A);
    free(out);
}

BT_TEST(release_verify, inflate_zlib_rejects_each_bad_header_shape) {
    unsigned char ok_head[2] = {0x78, 0x01};
    unsigned char short_in[5] = {0x78, 0x01, 0, 0, 0};
    unsigned char bad_cm[6] = {0x79, 0x01, 0, 0, 0, 0};
    unsigned char bad_dict[6] = {0x78, 0x21, 0, 0, 0, 0};
    unsigned char bad_sum[6] = {0x78, 0x02, 0, 0, 0, 0};
    unsigned char *out = NULL;
    size_t out_len = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(bt_rv_inflate_zlib(short_in, sizeof short_in, &out, &out_len, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "zlib: bad header") != NULL);
    BT_CHECK(bt_rv_inflate_zlib(bad_cm, sizeof bad_cm, &out, &out_len, err, sizeof err) != 0);
    BT_CHECK(bt_rv_inflate_zlib(bad_dict, sizeof bad_dict, &out, &out_len, err, sizeof err) != 0);
    BT_CHECK(bt_rv_inflate_zlib(bad_sum, sizeof bad_sum, &out, &out_len, err, sizeof err) != 0);
    BT_CHECK(bt_rv_inflate_zlib(ok_head, 1, &out, &out_len, err, sizeof err) != 0);
}

BT_TEST(release_verify, inflate_zlib_propagates_a_body_failure) {
    unsigned char body[7] = {0x78, 0x01, 0x07, 0, 0, 0, 0};
    unsigned char *out = NULL;
    size_t out_len = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(bt_rv_inflate_zlib(body, sizeof body, &out, &out_len, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "bad block type") != NULL);
}

BT_TEST(release_verify, inflate_zlib_rejects_an_adler_mismatch) {
    unsigned char body[16];
    unsigned char raw[4] = "abc";
    size_t deflated = rv_util_deflate_fixed(raw, sizeof raw, body + 2, sizeof body - 6);
    body[0] = 0x78;
    body[1] = 0x01;
    uint32_t adler = rv_util_adler32(raw, sizeof raw);
    adler ^= 1U;
    body[2 + deflated + 0] = (unsigned char)(adler >> 24U);
    body[2 + deflated + 1] = (unsigned char)(adler >> 16U);
    body[2 + deflated + 2] = (unsigned char)(adler >> 8U);
    body[2 + deflated + 3] = (unsigned char)adler;
    unsigned char *out = NULL;
    size_t out_len = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(bt_rv_inflate_zlib(body, 2 + deflated + 4, &out, &out_len, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "adler32 mismatch") != NULL);
}

BT_TEST(release_verify, inflate_zlib_round_trips_a_fixed_stream) {
    unsigned char body[64];
    unsigned char raw[9] = "zlib ok!";
    size_t deflated = rv_util_deflate_fixed(raw, sizeof raw, body + 2, sizeof body - 6);
    body[0] = 0x78;
    body[1] = 0x01;
    uint32_t adler = rv_util_adler32(raw, sizeof raw);
    body[2 + deflated + 0] = (unsigned char)(adler >> 24U);
    body[2 + deflated + 1] = (unsigned char)(adler >> 16U);
    body[2 + deflated + 2] = (unsigned char)(adler >> 8U);
    body[2 + deflated + 3] = (unsigned char)adler;
    unsigned char *out = NULL;
    size_t out_len = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK_EQ(bt_rv_inflate_zlib(body, 2 + deflated + 4, &out, &out_len, err, sizeof err), 0);
    BT_CHECK_EQ(out_len, sizeof raw);
    BT_CHECK(out_len == sizeof raw && memcmp(out, raw, sizeof raw) == 0);
    free(out);
}
