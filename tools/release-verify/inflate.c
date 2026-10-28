#include "release-verify/inflate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"

#define RV_HUFF_BITS 15
#define RV_LIT_CODES 288
#define RV_DIST_CODES 32
#define RV_CODE_ORDER 19

typedef struct {
    const unsigned char *data;
    size_t len;
    size_t pos;
    unsigned long bitbuf;
    int bitcnt;
    char *err;
    size_t err_cap;
    int failed;
} rv_bits;

typedef struct {
    short count[RV_HUFF_BITS + 1];
    short symbol[RV_LIT_CODES];
} rv_huff;

typedef struct {
    unsigned char *data;
    size_t len;
    size_t cap;
    char *err;
    size_t err_cap;
    int failed;
} rv_sink;

static void rv_fail(char *err, size_t err_cap, int *flag, const char *message) {
    if (*flag) {
        return;
    }
    *flag = 1;
    snprintf(err, err_cap, "%s", message);
}

static int rv_need(rv_bits *bits, int need) {
    long val = (long)bits->bitbuf;
    while (bits->bitcnt < need) {
        if (bits->pos >= bits->len) {
            rv_fail(bits->err, bits->err_cap, &bits->failed, "inflate: input exhausted");
            return -1;
        }
        val |= (long)bits->data[bits->pos++] << bits->bitcnt;
        bits->bitcnt += 8;
    }
    bits->bitbuf = (unsigned long)val;
    return 0;
}

static int rv_bits_take(rv_bits *bits, int need) {
    if (rv_need(bits, need) != 0) {
        return -1;
    }
    int val = (int)(bits->bitbuf & ((1UL << need) - 1));
    bits->bitbuf >>= need;
    bits->bitcnt -= need;
    return val;
}

static int rv_huff_build(rv_huff *huff, const short *lengths, int count) {
    int symbol;
    int len;
    int left;
    short offs[RV_HUFF_BITS + 1];
    for (len = 0; len <= RV_HUFF_BITS; len++) {
        huff->count[len] = 0;
    }
    for (symbol = 0; symbol < count; symbol++) {
        huff->count[lengths[symbol]]++;
    }
    if (huff->count[0] == count) {
        return 0;
    }
    left = 1;
    for (len = 1; len <= RV_HUFF_BITS; len++) {
        left <<= 1;
        left -= huff->count[len];
        if (left < 0) {
            return left;
        }
    }
    offs[1] = 0;
    for (len = 1; len < RV_HUFF_BITS; len++) {
        offs[len + 1] = (short)(offs[len] + huff->count[len]);
    }
    for (symbol = 0; symbol < count; symbol++) {
        if (lengths[symbol] != 0) {
            huff->symbol[offs[lengths[symbol]]++] = (short)symbol;
        }
    }
    return left;
}

static int rv_huff_decode(rv_bits *bits, const rv_huff *huff) {
    int code = 0;
    int first = 0;
    int index = 0;
    for (int len = 1; len <= RV_HUFF_BITS; len++) {
        int bit = rv_bits_take(bits, 1);
        if (bit < 0) {
            return -1;
        }
        code |= bit;
        int count = huff->count[len];
        if (code - count < first) {
            return huff->symbol[index + (code - first)];
        }
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    rv_fail(bits->err, bits->err_cap, &bits->failed, "inflate: bad code length");
    return -1;
}

static int rv_sink_reserve(rv_sink *sink, size_t extra) {
    if (sink->len + extra <= sink->cap) {
        return 0;
    }
    size_t cap = sink->cap > 0 ? sink->cap * 2 : 4096;
    while (sink->len + extra > cap) {
        cap *= 2;
    }
    unsigned char *grown = realloc(sink->data, cap);
    if (grown == NULL) {
        rv_fail(sink->err, sink->err_cap, &sink->failed, "inflate: out of memory");
        return -1;
    }
    sink->data = grown;
    sink->cap = cap;
    return 0;
}

static int rv_sink_byte(rv_sink *sink, int byte) {
    if (rv_sink_reserve(sink, 1) != 0) {
        return -1;
    }
    sink->data[sink->len++] = (unsigned char)byte;
    return 0;
}

static const short RV_LEN_BASE[29] = {3,  4,  5,  6,  7,  8,  9,  10,  11,  13,  15,  17,  19,  23, 27,
                                      31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const short RV_LEN_EXTRA[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                       2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const short RV_DIST_BASE[30] = {1,    2,    3,    4,    5,    7,     9,     13,    17,  25,
                                       33,   49,   65,   97,   129,  193,   257,   385,   513, 769,
                                       1025, 1537, 2049, 3073, 4097, 6145,  8193,  12289, 16385, 24577};
static const short RV_DIST_EXTRA[30] = {0, 0, 0, 0, 1, 1, 2,  2,  3,  3,  4,  4,  5,  5,  6,
                                        6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
static const unsigned char RV_ORDER[RV_CODE_ORDER] = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                                      11, 4,  12, 3, 13, 2, 14, 1, 15};

static int rv_fixed_tables(rv_huff *lit, rv_huff *dist) {
    short lengths[RV_LIT_CODES];
    int symbol = 0;
    for (; symbol < 144; symbol++) {
        lengths[symbol] = 8;
    }
    for (; symbol < 256; symbol++) {
        lengths[symbol] = 9;
    }
    for (; symbol < 280; symbol++) {
        lengths[symbol] = 7;
    }
    for (; symbol < RV_LIT_CODES; symbol++) {
        lengths[symbol] = 8;
    }
    int rc = rv_huff_build(lit, lengths, RV_LIT_CODES);
    if (rc < 0) {
        return rc;
    }
    short dist_lengths[RV_DIST_CODES];
    for (symbol = 0; symbol < RV_DIST_CODES; symbol++) {
        dist_lengths[symbol] = 5;
    }
    return rv_huff_build(dist, dist_lengths, RV_DIST_CODES);
}

static int rv_inflate_codes(rv_bits *bits, rv_sink *sink, const rv_huff *lit, const rv_huff *dist) {
    for (;;) {
        int symbol = rv_huff_decode(bits, lit);
        if (symbol < 0) {
            return -1;
        }
        if (symbol < 256) {
            if (rv_sink_byte(sink, symbol) != 0) {
                return -1;
            }
            continue;
        }
        if (symbol == 256) {
            return 0;
        }
        symbol -= 257;
        if (symbol >= 29) {
            rv_fail(bits->err, bits->err_cap, &bits->failed, "inflate: bad length symbol");
            return -1;
        }
        int extra = rv_bits_take(bits, RV_LEN_EXTRA[symbol]);
        if (extra < 0) {
            return -1;
        }
        int length = RV_LEN_BASE[symbol] + extra;
        int dist_symbol = rv_huff_decode(bits, dist);
        if (dist_symbol < 0 || dist_symbol >= 30) {
            rv_fail(bits->err, bits->err_cap, &bits->failed, "inflate: bad distance symbol");
            return -1;
        }
        extra = rv_bits_take(bits, RV_DIST_EXTRA[dist_symbol]);
        if (extra < 0) {
            return -1;
        }
        size_t distance = (size_t)(RV_DIST_BASE[dist_symbol] + extra);
        if (distance > sink->len) {
            rv_fail(bits->err, bits->err_cap, &bits->failed, "inflate: distance before start");
            return -1;
        }
        if (rv_sink_reserve(sink, (size_t)length) != 0) {
            return -1;
        }
        size_t from = sink->len - distance;
        for (int at = 0; at < length; at++) {
            sink->data[sink->len++] = sink->data[from++];
        }
    }
}

static int rv_dynamic_tables(rv_bits *bits, rv_huff *lit, rv_huff *dist) {
    int hlit = rv_bits_take(bits, 5);
    if (hlit < 0) {
        return -1;
    }
    int hdist = rv_bits_take(bits, 5);
    if (hdist < 0) {
        return -1;
    }
    int hclen = rv_bits_take(bits, 4);
    if (hclen < 0) {
        return -1;
    }
    int lit_count = hlit + 257;
    int dist_count = hdist + 1;
    int code_count = hclen + 4;
    if (lit_count > RV_LIT_CODES || dist_count > RV_DIST_CODES) {
        rv_fail(bits->err, bits->err_cap, &bits->failed, "inflate: too many codes");
        return -1;
    }
    short lengths[RV_LIT_CODES + RV_DIST_CODES];
    for (int at = 0; at < code_count; at++) {
        int value = rv_bits_take(bits, 3);
        if (value < 0) {
            return -1;
        }
        lengths[RV_ORDER[at]] = (short)value;
    }
    for (int at = code_count; at < RV_CODE_ORDER; at++) {
        lengths[RV_ORDER[at]] = 0;
    }
    int rc = rv_huff_build(lit, lengths, RV_CODE_ORDER);
    if (rc < 0) {
        rv_fail(bits->err, bits->err_cap, &bits->failed, "inflate: oversubscribed code lengths");
        return -1;
    }
    int at = 0;
    while (at < lit_count + dist_count) {
        int symbol = rv_huff_decode(bits, lit);
        if (symbol < 0) {
            return -1;
        }
        if (symbol < 16) {
            lengths[at++] = (short)symbol;
            continue;
        }
        int len = 0;
        if (symbol == 16) {
            if (at == 0) {
                rv_fail(bits->err, bits->err_cap, &bits->failed, "inflate: repeat with no prior");
                return -1;
            }
            len = lengths[at - 1];
            symbol = rv_bits_take(bits, 2);
            if (symbol < 0) {
                return -1;
            }
            symbol += 3;
        } else if (symbol == 17) {
            symbol = rv_bits_take(bits, 3);
            if (symbol < 0) {
                return -1;
            }
            symbol += 3;
        } else {
            symbol = rv_bits_take(bits, 7);
            if (symbol < 0) {
                return -1;
            }
            symbol += 11;
        }
        if (at + symbol > lit_count + dist_count) {
            rv_fail(bits->err, bits->err_cap, &bits->failed, "inflate: too many lengths");
            return -1;
        }
        while (symbol-- > 0) {
            lengths[at++] = (short)len;
        }
    }
    rc = rv_huff_build(lit, lengths, lit_count);
    if (rc < 0) {
        rv_fail(bits->err, bits->err_cap, &bits->failed, "inflate: oversubscribed literal codes");
        return -1;
    }
    rc = rv_huff_build(dist, lengths + lit_count, dist_count);
    if (rc < 0) {
        rv_fail(bits->err, bits->err_cap, &bits->failed, "inflate: oversubscribed distance codes");
        return -1;
    }
    return 0;
}

static int rv_inflate_stream(const unsigned char *in, size_t in_len, rv_sink *sink, char *err,
                             size_t err_cap) {
    rv_bits bits = {.data = in, .len = in_len, .pos = 0, .bitbuf = 0, .bitcnt = 0,
                    .err = err, .err_cap = err_cap, .failed = 0};
    int final = 0;
    while (!final) {
        int type;
        final = rv_bits_take(&bits, 1);
        if (final < 0) {
            return -1;
        }
        type = rv_bits_take(&bits, 2);
        if (type < 0) {
            return -1;
        }
        if (type == 0) {
            bits.bitbuf = 0;
            bits.bitcnt = 0;
            if (bits.pos + 4 > bits.len) {
                rv_fail(err, err_cap, &bits.failed, "inflate: stored block truncated");
                return -1;
            }
            unsigned len = bits.data[bits.pos] | ((unsigned)bits.data[bits.pos + 1] << 8);
            unsigned nlen = bits.data[bits.pos + 2] | ((unsigned)bits.data[bits.pos + 3] << 8);
            bits.pos += 4;
            if (len != (~nlen & 0xFFFF)) {
                rv_fail(err, err_cap, &bits.failed, "inflate: stored block length mismatch");
                return -1;
            }
            if (bits.pos + len > bits.len) {
                rv_fail(err, err_cap, &bits.failed, "inflate: stored block truncated");
                return -1;
            }
            if (rv_sink_reserve(sink, len) != 0) {
                return -1;
            }
            memcpy(sink->data + sink->len, bits.data + bits.pos, len);
            sink->len += len;
            bits.pos += len;
            continue;
        }
        if (type == 3) {
            rv_fail(err, err_cap, &bits.failed, "inflate: bad block type");
            return -1;
        }
        rv_huff lit;
        rv_huff dist;
        int rc = type == 1 ? rv_fixed_tables(&lit, &dist) : rv_dynamic_tables(&bits, &lit, &dist);
        if (rc != 0) {
            return -1;
        }
        if (rv_inflate_codes(&bits, sink, &lit, &dist) != 0) {
            return -1;
        }
    }
    return 0;
}

int bt_rv_inflate_raw(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len,
                      char *err, size_t err_cap) {
    BT_TRACE_ENTER("rv.inflate_raw");
    *out = NULL;
    *out_len = 0;
    err[0] = '\0';
    rv_sink sink = {.data = NULL, .len = 0, .cap = 0, .err = err, .err_cap = err_cap, .failed = 0};
    if (rv_sink_reserve(&sink, 65536) != 0 || rv_inflate_stream(in, in_len, &sink, err, err_cap) != 0) {
        free(sink.data);
        BT_TRACE_FAIL("rv.inflate_raw", "%s", err);
        BT_TRACE_EXIT("rv.inflate_raw");
        return -1;
    }
    *out = sink.data;
    *out_len = sink.len;
    BT_TRACE_EXIT("rv.inflate_raw");
    return 0;
}

static unsigned long rv_adler32(const unsigned char *data, size_t len) {
    unsigned long a = 1;
    unsigned long b = 0;
    for (size_t at = 0; at < len; at++) {
        a = (a + data[at]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

int bt_rv_inflate_zlib(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len,
                       char *err, size_t err_cap) {
    BT_TRACE_ENTER("rv.inflate_zlib");
    *out = NULL;
    *out_len = 0;
    err[0] = '\0';
    if (in_len < 6 || (in[0] & 0x0F) != 8 || (in[1] & 0x20) != 0 ||
        (((unsigned long)in[0] << 8) | in[1]) % 31 != 0) {
        snprintf(err, err_cap, "zlib: bad header");
        BT_TRACE_FAIL("rv.inflate_zlib", "%s", err);
        BT_TRACE_EXIT("rv.inflate_zlib");
        return -1;
    }
    unsigned char *raw = NULL;
    size_t raw_len = 0;
    char inner[96];
    if (bt_rv_inflate_raw(in + 2, in_len - 6, &raw, &raw_len, inner, sizeof inner) != 0) {
        snprintf(err, err_cap, "%s", inner);
        BT_TRACE_FAIL("rv.inflate_zlib", "%s", err);
        BT_TRACE_EXIT("rv.inflate_zlib");
        return -1;
    }
    unsigned long want = ((unsigned long)in[in_len - 4] << 24) | ((unsigned long)in[in_len - 3] << 16) |
                         ((unsigned long)in[in_len - 2] << 8) | in[in_len - 1];
    if (rv_adler32(raw, raw_len) != want) {
        free(raw);
        snprintf(err, err_cap, "zlib: adler32 mismatch");
        BT_TRACE_FAIL("rv.inflate_zlib", "%s", err);
        BT_TRACE_EXIT("rv.inflate_zlib");
        return -1;
    }
    *out = raw;
    *out_len = raw_len;
    BT_TRACE_EXIT("rv.inflate_zlib");
    return 0;
}
