#include "release-verify/png.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "release-verify/inflate.h"
#include "release-verify/zip.h"

static const unsigned char RV_PNG_SIGNATURE[8] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};

enum {
    RV_CHUNK_HEAD = 12,
    RV_IHDR_LEN = 13,
    RV_IHDR_INTERLACE_AT = 12,
};

static unsigned int rv_be32(const unsigned char *at) {
    return ((unsigned int)at[0] << 24U) | ((unsigned int)at[1] << 16U) |
           ((unsigned int)at[2] << 8U) | (unsigned int)at[3];
}

static int rv_channels_of(unsigned char color_type) {
    switch (color_type) {
    case 0:
        return 1;
    case 2:
        return 3;
    case 3:
        return 1;
    case 4:
        return 2;
    case 6:
        return 4;
    default:
        return 0;
    }
}

static unsigned char rv_paeth(unsigned char a, unsigned char b, unsigned char c) {
    int p = a + b - c;
    int pa = p > a ? p - a : a - p;
    int pb = p > b ? p - b : b - p;
    int pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) {
        return a;
    }
    if (pb <= pc) {
        return b;
    }
    return c;
}

static int rv_unfilter(unsigned char *raw, size_t raw_len, unsigned int width, unsigned int height,
                       int channels, char *err, size_t err_cap) {
    size_t stride = (size_t)width * (size_t)channels;
    size_t row_bytes = stride + 1;
    if ((size_t)height * row_bytes != raw_len) {
        snprintf(err, err_cap, "pixel data size mismatch");
        return -1;
    }
    for (unsigned int line = 0; line < height; line++) {
        if (raw[(size_t)line * row_bytes] > 4) {
            snprintf(err, err_cap, "bad filter byte %u", raw[(size_t)line * row_bytes]);
            return -1;
        }
    }
    for (unsigned int line = 1; line < height; line++) {
        unsigned char *row = raw + (size_t)line * row_bytes + 1;
        unsigned char *above = raw + (size_t)(line - 1) * row_bytes + 1;
        unsigned char filter = raw[(size_t)line * row_bytes];
        for (size_t at = 0; at < stride; at++) {
            unsigned char left = at >= (size_t)channels ? row[at - channels] : 0;
            unsigned char up = above[at];
            unsigned char up_left = at >= (size_t)channels ? above[at - channels] : 0;
            switch (filter) {
            case 0:
                break;
            case 1:
                row[at] = (unsigned char)(row[at] + left);
                break;
            case 2:
                row[at] = (unsigned char)(row[at] + up);
                break;
            case 3:
                row[at] = (unsigned char)(row[at] + (((unsigned)left + up) >> 1U));
                break;
            case 4:
                row[at] = (unsigned char)(row[at] + rv_paeth(left, up, up_left));
                break;
            default:
                snprintf(err, err_cap, "bad filter byte %u", filter);
                return -1;
            }
        }
    }
    return 0;
}

static int rv_png_decode(const char *path, unsigned int *width, unsigned int *height,
                         unsigned char **raw_out, size_t *raw_len_out, char *err, size_t err_cap) {
    BT_TRACE_ENTER("rv.png_decode");
    *width = 0;
    *height = 0;
    *raw_out = NULL;
    *raw_len_out = 0;
    err[0] = '\0';
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        snprintf(err, err_cap, "cannot read file");
        BT_TRACE_FAIL("rv.png_decode", "%s", err);
        BT_TRACE_EXIT("rv.png_decode");
        return -1;
    }
    size_t cap = (size_t)1 << 20U;
    unsigned char *bytes = malloc(cap);
    size_t len = 0;
    for (;;) {
        if (len == cap) {
            cap *= 2;
            unsigned char *grown = realloc(bytes, cap);
            if (grown == NULL) {
                free(bytes);
                fclose(file);
                snprintf(err, err_cap, "out of memory");
                BT_TRACE_FAIL("rv.png_decode", "%s", err);
                BT_TRACE_EXIT("rv.png_decode");
                return -1;
            }
            bytes = grown;
        }
        size_t got = fread(bytes + len, 1, cap - len, file);
        if (got == 0) {
            break;
        }
        len += got;
    }
    fclose(file);
    if (len < 8 || memcmp(bytes, RV_PNG_SIGNATURE, 8) != 0) {
        free(bytes);
        snprintf(err, err_cap, "not a png");
        BT_TRACE_FAIL("rv.png_decode", "%s", err);
        BT_TRACE_EXIT("rv.png_decode");
        return -1;
    }
    int have_header = 0;
    unsigned char bit_depth = 0;
    unsigned char color_type = 0;
    unsigned char interlace = 0;
    int channels = 0;
    unsigned char *idat = NULL;
    size_t idat_len = 0;
    size_t at = 8;
    int failed = 0;
    while (at + RV_CHUNK_HEAD <= len && !failed) {
        size_t chunk_len = rv_be32(bytes + at);
        if (at + RV_CHUNK_HEAD + chunk_len > len) {
            snprintf(err, err_cap, "truncated chunk");
            failed = 1;
            break;
        }
        const unsigned char *type = bytes + at + 4;
        const unsigned char *data = bytes + at + 8;
        if (bt_rv_crc32(bytes + at + 4, chunk_len + 4) != rv_be32(bytes + at + 8 + chunk_len)) {
            snprintf(err, err_cap, "chunk crc invalid");
            failed = 1;
            break;
        }
        if (!have_header) {
            if (chunk_len != RV_IHDR_LEN || memcmp(type, "IHDR", 4) != 0) {
                snprintf(err, err_cap, "first chunk is not ihdr");
                failed = 1;
                break;
            }
            *width = rv_be32(data);
            *height = rv_be32(data + 4);
            bit_depth = data[8];
            color_type = data[9];
            interlace = data[RV_IHDR_INTERLACE_AT];
            if (*width == 0 || *height == 0) {
                snprintf(err, err_cap, "zero dimension");
                failed = 1;
                break;
            }
            channels = rv_channels_of(color_type);
            if (bit_depth != 8 || channels == 0 || interlace != 0) {
                snprintf(err, err_cap, "unsupported png variant");
                failed = 1;
                break;
            }
            have_header = 1;
        } else if (memcmp(type, "IDAT", 4) == 0) {
            unsigned char *grown = realloc(idat, idat_len + chunk_len);
            if (grown == NULL) {
                snprintf(err, err_cap, "out of memory");
                failed = 1;
                break;
            }
            idat = grown;
            memcpy(idat + idat_len, data, chunk_len);
            idat_len += chunk_len;
        } else if (memcmp(type, "IEND", 4) == 0) {
            break;
        }
        at += RV_CHUNK_HEAD + chunk_len;
    }
    if (!failed && !have_header) {
        snprintf(err, err_cap, "ihdr chunk missing");
        failed = 1;
    }
    if (!failed && idat == NULL) {
        snprintf(err, err_cap, "idat chunk missing");
        failed = 1;
    }
    if (!failed) {
        if (bt_rv_inflate_zlib(idat, idat_len, raw_out, raw_len_out, err, err_cap) != 0) {
            failed = 1;
        } else if (rv_unfilter(*raw_out, *raw_len_out, *width, *height, channels, err, err_cap) !=
                   0) {
            free(*raw_out);
            *raw_out = NULL;
            *raw_len_out = 0;
            failed = 1;
        }
    }
    free(idat);
    free(bytes);
    if (failed) {
        BT_TRACE_FAIL("rv.png_decode", "%s %s", path, err);
        BT_TRACE_EXIT("rv.png_decode");
        return -1;
    }
    BT_TRACE_EXIT("rv.png_decode");
    return 0;
}

int bt_rv_png_raw(const char *path, unsigned int *width, unsigned int *height,
                  unsigned char **raw, size_t *raw_len, char *err, size_t err_cap) {
    return rv_png_decode(path, width, height, raw, raw_len, err, err_cap);
}

int bt_rv_png_check(const char *path, unsigned int *width, unsigned int *height, char *err,
                    size_t err_cap) {
    unsigned char *raw = NULL;
    size_t raw_len = 0;
    if (rv_png_decode(path, width, height, &raw, &raw_len, err, err_cap) != 0) {
        return -1;
    }
    free(raw);
    return 0;
}
