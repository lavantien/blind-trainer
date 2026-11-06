#include "release-verify/png.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_harness.h"
#include "common/paths.h"
#include "unit/tools/release_verify_test_util.h"

enum {
    PNG_TEST_DEPTH_8 = 8,
    PNG_TEST_DEPTH_16 = 16,
    PNG_TEST_COLOR_GRAY = 0,
    PNG_TEST_COLOR_RGB = 2,
    PNG_TEST_COLOR_PALETTE = 3,
    PNG_TEST_COLOR_GRAY_ALPHA = 4,
    PNG_TEST_COLOR_RGBA = 6,
    PNG_TEST_COLOR_WEIRD = 7,
    PNG_TEST_INTERLACE = 1,
    PNG_TEST_IHDR_LEN = 13,
    PNG_TEST_CHUNK_HEAD = 12,
    PNG_TEST_SIG_LEN = 8,
    PNG_TEST_WIDE_W = 1320,
    PNG_TEST_WIDE_H = 795,
    PNG_TEST_READ_CAP = 1U << 20U,
    PNG_TEST_BUF = 1U << 21U,
    PNG_TEST_BAD_FILTER = 5,
};

static const unsigned PNG_TEST_SKIP_IDAT = 1U;
static const unsigned PNG_TEST_BAD_IHDR_CRC = 2U;
static const unsigned PNG_TEST_JUNK_FIRST = 4U;
static const unsigned PNG_TEST_SIG_ONLY = 8U;
static const unsigned PNG_TEST_JUNK_IDAT = 16U;
static const unsigned PNG_TEST_NO_IEND = 32U;

typedef struct {
    unsigned char *buf;
    size_t len;
    size_t cap;
} png_out;

static void png_put(png_out *out, const void *data, size_t len) {
    if (out->len + len > out->cap) {
        bt_harness_fail(__FILE__, __LINE__, "png fixture overflow");
        return;
    }
    memcpy(out->buf + out->len, data, len);
    out->len += len;
}

static void png_be32(png_out *out, uint32_t value) {
    unsigned char be[4];
    be[0] = (unsigned char)(value >> 24U);
    be[1] = (unsigned char)(value >> 16U);
    be[2] = (unsigned char)(value >> 8U);
    be[3] = (unsigned char)value;
    png_put(out, be, 4);
}

static void png_chunk(png_out *out, const char *type, const unsigned char *data, size_t len,
                      int bad_crc) {
    png_be32(out, (uint32_t)len);
    unsigned char head[4];
    memcpy(head, type, 4);
    png_put(out, head, 4);
    if (len > 0) {
        png_put(out, data, len);
    }
    unsigned char *with_type = malloc(len + 4);
    if (with_type == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "png fixture out of memory");
        return;
    }
    memcpy(with_type, head, 4);
    if (len > 0) {
        memcpy(with_type + 4, data, len);
    }
    uint32_t crc = bt_rv_crc32(with_type, len + 4) ^ (bad_crc ? 1U : 0U);
    free(with_type);
    png_be32(out, crc);
}

static void build_png(const char *path, uint32_t width, uint32_t height, unsigned char depth,
                      unsigned char color, unsigned char interlace, const unsigned char *raw,
                      size_t raw_len, unsigned flags) {
    unsigned char *store = malloc(PNG_TEST_BUF);
    if (store == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "png fixture out of memory");
        return;
    }
    png_out out = {
        .buf = store,
        .len = 0,
        .cap = PNG_TEST_BUF,
    };
    static const unsigned char signature[PNG_TEST_SIG_LEN] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
    };
    png_put(&out, signature, PNG_TEST_SIG_LEN);
    if ((flags & PNG_TEST_SIG_ONLY) == 0) {
        if ((flags & PNG_TEST_JUNK_FIRST) != 0) {
            unsigned char junk[4] = {1, 2, 3, 4};
            png_chunk(&out, "jUNK", junk, sizeof junk, 0);
        }
        unsigned char ihdr[PNG_TEST_IHDR_LEN];
        memset(ihdr, 0, sizeof ihdr);
        ihdr[0] = (unsigned char)(width >> 24U);
        ihdr[1] = (unsigned char)(width >> 16U);
        ihdr[2] = (unsigned char)(width >> 8U);
        ihdr[3] = (unsigned char)width;
        ihdr[4] = (unsigned char)(height >> 24U);
        ihdr[5] = (unsigned char)(height >> 16U);
        ihdr[6] = (unsigned char)(height >> 8U);
        ihdr[7] = (unsigned char)height;
        ihdr[8] = depth;
        ihdr[9] = color;
        ihdr[12] = interlace;
        png_chunk(&out, "IHDR", ihdr, PNG_TEST_IHDR_LEN, (flags & PNG_TEST_BAD_IHDR_CRC) != 0);
        if ((flags & PNG_TEST_SKIP_IDAT) == 0) {
            if ((flags & PNG_TEST_JUNK_IDAT) != 0) {
                unsigned char junk[4] = {0xFF, 0xFE, 0x00, 0x01};
                png_chunk(&out, "IDAT", junk, sizeof junk, 0);
            } else {
                unsigned char *zlib = malloc(raw_len + raw_len / 4 + 64);
                if (zlib == NULL) {
                    free(store);
                    bt_harness_fail(__FILE__, __LINE__, "png fixture out of memory");
                    return;
                }
                size_t deflated =
                    rv_util_deflate_fixed(raw, raw_len, zlib + 2, raw_len + raw_len / 4);
                uint32_t adler = rv_util_adler32(raw, raw_len);
                zlib[0] = 0x78;
                zlib[1] = 0x01;
                zlib[2 + deflated + 0] = (unsigned char)(adler >> 24U);
                zlib[2 + deflated + 1] = (unsigned char)(adler >> 16U);
                zlib[2 + deflated + 2] = (unsigned char)(adler >> 8U);
                zlib[2 + deflated + 3] = (unsigned char)adler;
                png_chunk(&out, "IDAT", zlib, 2 + deflated + 4, 0);
                free(zlib);
            }
        }
        if ((flags & PNG_TEST_NO_IEND) == 0) {
            png_chunk(&out, "IEND", NULL, 0, 0);
        }
    }
    rv_util_write_bytes(path, out.buf, out.len);
    free(store);
}

static int check_png(const char *path, char *err) {
    unsigned int width = 0;
    unsigned int height = 0;
    return bt_rv_png_check(path, &width, &height, err, BT_RV_ERR_MAX);
}

BT_TEST(release_verify, png_check_rejects_a_missing_file) {
    char err[BT_RV_ERR_MAX];
    BT_CHECK(check_png("Z:/no-such-dir/shot.png", err) != 0);
    BT_CHECK(strstr(err, "cannot read file") != NULL);
}

BT_TEST(release_verify, png_check_rejects_short_and_signature_less_files) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("png-sig", dir);
    char path[BT_PATH_MAX];
    rv_util_join(path, dir, "short.png");
    rv_util_write_bytes(path, (const unsigned char *)"abcd", 4);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(check_png(path, err) != 0);
    BT_CHECK(strstr(err, "not a png") != NULL);
    rv_util_join(path, dir, "magic.png");
    rv_util_write_bytes(path, (const unsigned char *)"not a png at all", 16);
    BT_CHECK(check_png(path, err) != 0);
    BT_CHECK(strstr(err, "not a png") != NULL);
}

BT_TEST(release_verify, png_check_reports_a_missing_ihdr_or_idat) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("png-parts", dir);
    char path[BT_PATH_MAX];
    rv_util_join(path, dir, "sig-only.png");
    build_png(path, 1, 1, PNG_TEST_DEPTH_8, PNG_TEST_COLOR_GRAY, 0, NULL, 0, PNG_TEST_SIG_ONLY);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(check_png(path, err) != 0);
    BT_CHECK(strstr(err, "ihdr chunk missing") != NULL);
    unsigned char raw[2] = {0, 0};
    rv_util_join(path, dir, "no-idat.png");
    build_png(path, 1, 1, PNG_TEST_DEPTH_8, PNG_TEST_COLOR_GRAY, 0, raw, sizeof raw,
              PNG_TEST_SKIP_IDAT);
    BT_CHECK(check_png(path, err) != 0);
    BT_CHECK(strstr(err, "idat chunk missing") != NULL);
}

BT_TEST(release_verify, png_check_rejects_a_truncated_chunk) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("png-trunc", dir);
    char path[BT_PATH_MAX];
    rv_util_join(path, dir, "cut.png");
    unsigned char raw[2] = {0, 0};
    build_png(path, 1, 1, PNG_TEST_DEPTH_8, PNG_TEST_COLOR_GRAY, 0, raw, sizeof raw, 0);
    unsigned char *bytes = NULL;
    size_t len = 0;
    BT_CHECK_EQ(rv_util_file_bytes(path, &bytes, &len), 0);
    if (bytes == NULL) {
        return;
    }
    rv_util_write_bytes(path, bytes, len - 20);
    free(bytes);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(check_png(path, err) != 0);
    BT_CHECK(strstr(err, "truncated chunk") != NULL);
}

BT_TEST(release_verify, png_check_rejects_a_bad_chunk_crc) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("png-crc", dir);
    char path[BT_PATH_MAX];
    rv_util_join(path, dir, "crc.png");
    unsigned char raw[2] = {0, 0};
    build_png(path, 1, 1, PNG_TEST_DEPTH_8, PNG_TEST_COLOR_GRAY, 0, raw, sizeof raw,
              PNG_TEST_BAD_IHDR_CRC);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(check_png(path, err) != 0);
    BT_CHECK(strstr(err, "chunk crc invalid") != NULL);
}

BT_TEST(release_verify, png_check_rejects_a_first_chunk_that_is_not_ihdr) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("png-junk", dir);
    char path[BT_PATH_MAX];
    rv_util_join(path, dir, "junk.png");
    unsigned char raw[2] = {0, 0};
    build_png(path, 1, 1, PNG_TEST_DEPTH_8, PNG_TEST_COLOR_GRAY, 0, raw, sizeof raw,
              PNG_TEST_JUNK_FIRST);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(check_png(path, err) != 0);
    BT_CHECK(strstr(err, "first chunk is not ihdr") != NULL);
}

BT_TEST(release_verify, png_check_rejects_zero_dimensions) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("png-zero", dir);
    char path[BT_PATH_MAX];
    rv_util_join(path, dir, "zero.png");
    unsigned char raw[2] = {0, 0};
    build_png(path, 0, 1, PNG_TEST_DEPTH_8, PNG_TEST_COLOR_GRAY, 0, raw, sizeof raw, 0);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(check_png(path, err) != 0);
    BT_CHECK(strstr(err, "zero dimension") != NULL);
}

BT_TEST(release_verify, png_check_rejects_unsupported_variants) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("png-variant", dir);
    char path[BT_PATH_MAX];
    unsigned char raw[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    char err[BT_RV_ERR_MAX];
    rv_util_join(path, dir, "variant.png");
    build_png(path, 1, 1, PNG_TEST_DEPTH_16, PNG_TEST_COLOR_GRAY, 0, raw, 2, 0);
    BT_CHECK(check_png(path, err) != 0);
    BT_CHECK(strstr(err, "unsupported png variant") != NULL);
    build_png(path, 1, 1, PNG_TEST_DEPTH_8, PNG_TEST_COLOR_WEIRD, 0, raw, 2, 0);
    BT_CHECK(check_png(path, err) != 0);
    BT_CHECK(strstr(err, "unsupported png variant") != NULL);
    build_png(path, 1, 1, PNG_TEST_DEPTH_8, PNG_TEST_COLOR_GRAY, PNG_TEST_INTERLACE, raw, 2, 0);
    BT_CHECK(check_png(path, err) != 0);
    BT_CHECK(strstr(err, "unsupported png variant") != NULL);
}

BT_TEST(release_verify, png_check_accepts_every_supported_color_type) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("png-colors", dir);
    char path[BT_PATH_MAX];
    unsigned char raw[8] = {0, 3, 6, 9, 1, 4, 7, 2};
    static const unsigned char colors[] = {
        PNG_TEST_COLOR_GRAY,       PNG_TEST_COLOR_RGB,  PNG_TEST_COLOR_PALETTE,
        PNG_TEST_COLOR_GRAY_ALPHA, PNG_TEST_COLOR_RGBA,
    };
    static const unsigned char channels[] = {1, 3, 1, 2, 4};
    char err[BT_RV_ERR_MAX];
    for (size_t at = 0; at < sizeof colors; at++) {
        rv_util_join(path, dir, "color.png");
        build_png(path, 1, 1, PNG_TEST_DEPTH_8, colors[at], 0, raw, 1 + channels[at], 0);
        unsigned int width = 0;
        unsigned int height = 0;
        BT_CHECK_EQ(bt_rv_png_check(path, &width, &height, err, sizeof err), 0);
        BT_CHECK_EQ(width, 1U);
        BT_CHECK_EQ(height, 1U);
    }
}

BT_TEST(release_verify, png_check_applies_each_row_filter_and_each_paeth_pick) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("png-filters", dir);
    char path[BT_PATH_MAX];
    unsigned char paeth_raw[8] = {0, 10, 0, 0, 4, 10, 0, 0};
    rv_util_join(path, dir, "paeth.png");
    build_png(path, 3, 2, PNG_TEST_DEPTH_8, PNG_TEST_COLOR_GRAY, 0, paeth_raw, sizeof paeth_raw, 0);
    unsigned int width = 0;
    unsigned int height = 0;
    unsigned char *raw = NULL;
    size_t raw_len = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK_EQ(bt_rv_png_raw(path, &width, &height, &raw, &raw_len, err, sizeof err), 0);
    static const unsigned char paeth_want[8] = {0, 10, 0, 0, 4, 20, 10, 10};
    BT_CHECK_EQ(raw_len, sizeof paeth_want);
    BT_CHECK(raw_len == sizeof paeth_want && memcmp(raw, paeth_want, raw_len) == 0);
    free(raw);
    unsigned char filter_raw[9] = {1, 5, 7, 2, 3, 4, 3, 1, 1};
    rv_util_join(path, dir, "filters.png");
    build_png(path, 2, 3, PNG_TEST_DEPTH_8, PNG_TEST_COLOR_GRAY, 0, filter_raw, sizeof filter_raw,
              0);
    BT_CHECK_EQ(bt_rv_png_raw(path, &width, &height, &raw, &raw_len, err, sizeof err), 0);
    static const unsigned char filter_want[9] = {1, 5, 12, 2, 8, 16, 3, 5, 11};
    BT_CHECK_EQ(raw_len, sizeof filter_want);
    BT_CHECK(raw_len == sizeof filter_want && memcmp(raw, filter_want, raw_len) == 0);
    free(raw);
    unsigned char bad_raw[2] = {PNG_TEST_BAD_FILTER, 0};
    rv_util_join(path, dir, "bad-filter.png");
    build_png(path, 1, 1, PNG_TEST_DEPTH_8, PNG_TEST_COLOR_GRAY, 0, bad_raw, sizeof bad_raw, 0);
    BT_CHECK(check_png(path, err) != 0);
    BT_CHECK(strstr(err, "bad filter byte 5") != NULL);
}

BT_TEST(release_verify, png_check_rejects_raw_that_does_not_match_the_header_dims) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("png-size", dir);
    char path[BT_PATH_MAX];
    rv_util_join(path, dir, "size.png");
    unsigned char raw[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    build_png(path, 2, 2, PNG_TEST_DEPTH_8, PNG_TEST_COLOR_GRAY, 0, raw, sizeof raw, 0);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(check_png(path, err) != 0);
    BT_CHECK(strstr(err, "pixel data size mismatch") != NULL);
}

BT_TEST(release_verify, png_check_rejects_an_idat_that_does_not_inflate) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("png-idat", dir);
    char path[BT_PATH_MAX];
    rv_util_join(path, dir, "junk.png");
    build_png(path, 1, 1, PNG_TEST_DEPTH_8, PNG_TEST_COLOR_GRAY, 0, NULL, 0, PNG_TEST_JUNK_IDAT);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(check_png(path, err) != 0);
    BT_CHECK(strstr(err, "zlib: bad header") != NULL || strstr(err, "inflate") != NULL);
}

BT_TEST(release_verify, png_check_grows_the_reader_past_one_mebibyte) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("png-wide", dir);
    size_t raw_len = (size_t)(PNG_TEST_WIDE_W + 1) * PNG_TEST_WIDE_H;
    unsigned char *raw = malloc(raw_len);
    BT_CHECK(raw != NULL);
    if (raw == NULL) {
        return;
    }
    for (size_t at = 0; at < raw_len; at++) {
        raw[at] = at % (PNG_TEST_WIDE_W + 1) == 0 ? 0 : (unsigned char)((at * 31U + 7U) & 0xFFU);
    }
    char path[BT_PATH_MAX];
    rv_util_join(path, dir, "wide.png");
    build_png(path, PNG_TEST_WIDE_W, PNG_TEST_WIDE_H, PNG_TEST_DEPTH_8, PNG_TEST_COLOR_GRAY, 0, raw,
              raw_len, 0);
    unsigned char *bytes = NULL;
    size_t len = 0;
    BT_CHECK_EQ(rv_util_file_bytes(path, &bytes, &len), 0);
    BT_CHECK(bytes != NULL && len > (size_t)PNG_TEST_READ_CAP);
    free(bytes);
    unsigned int width = 0;
    unsigned int height = 0;
    char err[BT_RV_ERR_MAX];
    if (bt_rv_png_check(path, &width, &height, err, sizeof err) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "wide png rejected: %s", err);
        free(raw);
        return;
    }
    BT_CHECK_EQ(width, PNG_TEST_WIDE_W);
    BT_CHECK_EQ(height, PNG_TEST_WIDE_H);
    free(raw);
}
