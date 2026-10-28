#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/space.h"
#include "core/alg.h"
#include "core/cube.h"
#include "core/moves.h"

static size_t whitespace_len(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == 0x0BU || *p == 0x0CU) {
        return 1;
    }
    if (p[0] == 0xC2U && (p[1] == 0x85U || p[1] == 0xA0U)) {
        return 2;
    }
    if (p[0] == 0xE1U && p[1] == 0x9AU && p[2] == 0x80U) {
        return 3;
    }
    if (p[0] == 0xE2U && p[1] == 0x80U) {
        if (p[2] >= 0x80U && p[2] <= 0x8AU) {
            return 3;
        }
        if (p[2] == 0xA8U || p[2] == 0xA9U || p[2] == 0xAFU) {
            return 3;
        }
    }
    if (p[0] == 0xE2U && p[1] == 0x81U && p[2] == 0x9FU) {
        return 3;
    }
    if (p[0] == 0xE3U && p[1] == 0x80U && p[2] == 0x80U) {
        return 3;
    }
    return 0;
}

static int fail_count = 0;

static void dump_buf(const char *tag, const unsigned char *b, size_t n) {
    printf("%s len=%zu bytes=", tag, n);
    for (size_t i = 0; i < n; i++) {
        printf("%02X ", b[i]);
    }
    printf("\n");
}

static int utf8_next(const unsigned char *s, size_t len, size_t at, uint32_t *cp, size_t *span) {
    if (at >= len) {
        return 0;
    }
    unsigned char b0 = s[at];
    if (b0 < 0x80U) {
        *cp = b0;
        *span = 1;
        return 1;
    }
    if (b0 >= 0xC2U && b0 <= 0xDFU) {
        if (at + 1 >= len || (s[at + 1] & 0xC0U) != 0x80U) {
            return 0;
        }
        *cp = ((uint32_t)(b0 & 0x1FU) << 6) | (uint32_t)(s[at + 1] & 0x3FU);
        *span = 2;
        return 1;
    }
    if (b0 >= 0xE0U && b0 <= 0xEFU) {
        if (at + 2 >= len || (s[at + 1] & 0xC0U) != 0x80U || (s[at + 2] & 0xC0U) != 0x80U) {
            return 0;
        }
        if (b0 == 0xE0U && s[at + 1] < 0xA0U) {
            return 0;
        }
        if (b0 == 0xEDU && s[at + 1] >= 0xA0U) {
            return 0;
        }
        *cp = ((uint32_t)(b0 & 0x0FU) << 12) | ((uint32_t)(s[at + 1] & 0x3FU) << 6) |
              (uint32_t)(s[at + 2] & 0x3FU);
        *span = 3;
        return 1;
    }
    if (b0 >= 0xF0U && b0 <= 0xF4U) {
        if (at + 3 >= len || (s[at + 1] & 0xC0U) != 0x80U || (s[at + 2] & 0xC0U) != 0x80U ||
            (s[at + 3] & 0xC0U) != 0x80U) {
            return 0;
        }
        if (b0 == 0xF0U && s[at + 1] < 0x90U) {
            return 0;
        }
        if (b0 == 0xF4U && s[at + 1] > 0x8FU) {
            return 0;
        }
        *cp = ((uint32_t)(b0 & 0x07U) << 18) | ((uint32_t)(s[at + 1] & 0x3FU) << 12) |
              ((uint32_t)(s[at + 2] & 0x3FU) << 6) | (uint32_t)(s[at + 3] & 0x3FU);
        *span = 4;
        return 1;
    }
    return 0;
}

static int cp_is_ws(uint32_t cp) {
    return (cp >= 0x09U && cp <= 0x0DU) || cp == 0x20U || cp == 0x85U || cp == 0xA0U ||
           cp == 0x1680U || (cp >= 0x2000U && cp <= 0x200AU) || cp == 0x2028U || cp == 0x2029U ||
           cp == 0x202FU || cp == 0x205FU || cp == 0x3000U;
}

static size_t utf8_enc(uint32_t cp, unsigned char *out) {
    if (cp < 0x80U) {
        out[0] = (unsigned char)cp;
        return 1;
    }
    if (cp < 0x800U) {
        out[0] = (unsigned char)(0xC0U | (cp >> 6));
        out[1] = (unsigned char)(0x80U | (cp & 0x3FU));
        return 2;
    }
    if (cp < 0x10000U) {
        out[0] = (unsigned char)(0xE0U | (cp >> 12));
        out[1] = (unsigned char)(0x80U | ((cp >> 6) & 0x3FU));
        out[2] = (unsigned char)(0x80U | (cp & 0x3FU));
        return 3;
    }
    out[0] = (unsigned char)(0xF0U | (cp >> 18));
    out[1] = (unsigned char)(0x80U | ((cp >> 12) & 0x3FU));
    out[2] = (unsigned char)(0x80U | ((cp >> 6) & 0x3FU));
    out[3] = (unsigned char)(0x80U | (cp & 0x3FU));
    return 4;
}

#define MAX_TOK 512

static int oracle_split(const char *text, size_t toff[MAX_TOK], size_t tlen[MAX_TOK]) {
    const unsigned char *s = (const unsigned char *)text;
    size_t len = strlen(text);
    size_t at = 0;
    size_t n = 0;
    while (at < len) {
        uint32_t cp;
        size_t span;
        if (!utf8_next(s, len, at, &cp, &span)) {
            return -1;
        }
        if (!cp_is_ws(cp)) {
            size_t start = at;
            while (at < len) {
                if (!utf8_next(s, len, at, &cp, &span)) {
                    return -1;
                }
                if (cp_is_ws(cp)) {
                    break;
                }
                at += span;
            }
            if (n < MAX_TOK) {
                toff[n] = start;
                tlen[n] = at - start;
            }
            n++;
        } else {
            at += span;
        }
    }
    return (int)n;
}

static int face_of_cp(uint32_t cp) {
    switch (cp) {
    case 'U':
        return BT_FACE_U;
    case 'L':
        return BT_FACE_L;
    case 'F':
        return BT_FACE_F;
    case 'R':
        return BT_FACE_R;
    case 'B':
        return BT_FACE_B;
    case 'D':
        return BT_FACE_D;
    default:
        return -1;
    }
}

static int oracle_token(const unsigned char *tok, size_t len, bt_move *out) {
    uint32_t cps[64];
    size_t n = 0;
    size_t at = 0;
    while (at < len) {
        uint32_t cp;
        size_t span;
        if (!utf8_next(tok, len, at, &cp, &span)) {
            return -1;
        }
        if (n >= 64) {
            return 0;
        }
        cps[n++] = cp;
        at += span;
    }
    size_t idx = 0;
    int inner = 0;
    if (n > 0 && cps[0] == '2') {
        idx = 1;
        inner = 1;
    }
    if (idx >= n) {
        return 0;
    }
    uint32_t base = cps[idx];
    size_t r0 = idx + 1;
    uint8_t quarter;
    if (r0 == n) {
        quarter = 1;
    } else if (r0 + 1 == n && cps[r0] == '2') {
        quarter = 2;
    } else if (r0 + 1 == n && cps[r0] == '\'') {
        quarter = 3;
    } else if (r0 + 2 == n && cps[r0] == '2' && cps[r0 + 1] == '\'') {
        quarter = 2;
    } else {
        return 0;
    }
    int face = face_of_cp(base);
    if (face >= 0) {
        if (inner) {
            *out = bt_move_of_inner((uint8_t)face, quarter);
        } else {
            *out = bt_move_of_face((uint8_t)face, quarter);
        }
        return 1;
    }
    if (base >= 'a' && base <= 'z') {
        int upper = face_of_cp(base - 'a' + 'A');
        if (upper >= 0) {
            if (inner) {
                return 0;
            }
            *out = bt_move_of_wide((uint8_t)upper, quarter);
            return 1;
        }
    }
    if (base == 'M' || base == 'E' || base == 'S') {
        if (inner) {
            return 0;
        }
        uint8_t slice = base == 'M' ? BT_SLICE_M : (base == 'E' ? BT_SLICE_E : BT_SLICE_S);
        *out = bt_move_of_slice(slice, quarter);
        return 1;
    }
    if (base == 'x' || base == 'y' || base == 'z') {
        if (inner) {
            return 0;
        }
        uint8_t axis = base == 'x' ? BT_AXIS_X : (base == 'y' ? BT_AXIS_Y : BT_AXIS_Z);
        *out = bt_move_of_rotation(axis, quarter);
        return 1;
    }
    return 0;
}

static int oracle_parse(const char *text, bt_move out[MAX_TOK]) {
    size_t toff[MAX_TOK];
    size_t tlen[MAX_TOK];
    int n = oracle_split(text, toff, tlen);
    if (n < 0) {
        return -1;
    }
    if (n > MAX_TOK) {
        return -2;
    }
    for (int i = 0; i < n; i++) {
        int rc = oracle_token((const unsigned char *)text + toff[i], tlen[i], &out[i]);
        if (rc != 1) {
            return -2;
        }
    }
    return n;
}

static int mirror_tokens(const char *text, size_t toff[MAX_TOK], size_t tlen[MAX_TOK]) {
    size_t text_len = strlen(text);
    const char *cursor = text;
    size_t n = 0;
    while (*cursor != '\0') {
        size_t ws = bt_space_len(text, (size_t)(cursor - text), text_len);
        while (ws > 0) {
            cursor += ws;
            ws = bt_space_len(text, (size_t)(cursor - text), text_len);
        }
        if (*cursor == '\0') {
            break;
        }
        const char *start = cursor;
        while (*cursor != '\0' && bt_space_len(text, (size_t)(cursor - text), text_len) == 0) {
            cursor++;
        }
        if (n < MAX_TOK) {
            toff[n] = (size_t)(start - text);
            tlen[n] = (size_t)(cursor - start);
        }
        n++;
    }
    return (int)n;
}

static int check_pred_eq(const char *buf) {
    size_t slen = strlen(buf);
    for (size_t at = 0; at <= slen; at++) {
        size_t old_v = whitespace_len(buf + at);
        size_t new_v = bt_space_len(buf, at, slen);
        if (old_v != new_v) {
            printf("PRED MISMATCH at=%zu old=%zu new=%zu slen=%zu\n", at, old_v, new_v, slen);
            dump_buf("buf", (const unsigned char *)buf, slen + 1);
            return 0;
        }
    }
    return 1;
}

static uint64_t smix(uint64_t *s) {
    *s += 0x9E3779B97F4A7C15ULL;
    uint64_t z = *s;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static const uint32_t ws_cps[] = {
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x20, 0x85, 0xA0, 0x1680, 0x2000, 0x2001, 0x2002, 0x2003,
    0x2004, 0x2005, 0x2006, 0x2007, 0x2008, 0x2009, 0x200A, 0x2028, 0x2029, 0x202F, 0x205F,
    0x3000,
};

static const uint32_t near_cps[] = {
    0x08, 0x0E, 0x1F, 0x7F, 0x84, 0x86, 0x9F, 0xA1, 0x180E, 0x167F, 0x1681, 0x1FFF, 0x200B,
    0x2027, 0x202A, 0x2030, 0x205E, 0x2060, 0x2FFF, 0x3001, 0xFEFF, 0x00AD, 0x2065, 0x2190,
};

static int check_one(const char *text) {
    if (!check_pred_eq(text)) {
        return 0;
    }
    size_t moff[MAX_TOK];
    size_t mlen[MAX_TOK];
    size_t ooff[MAX_TOK];
    size_t olen[MAX_TOK];
    int mn = mirror_tokens(text, moff, mlen);
    int on = oracle_split(text, ooff, olen);
    if (on >= 0) {
        if (mn != on) {
            printf("TOKEN COUNT mirror=%d oracle=%d\n", mn, on);
            dump_buf("buf", (const unsigned char *)text, strlen(text) + 1);
            return 0;
        }
        if (on > MAX_TOK) {
            on = MAX_TOK;
        }
        for (int i = 0; i < on; i++) {
            if (moff[i] != ooff[i] || mlen[i] != olen[i]) {
                printf("TOKEN %d mirror=%zu+%zu oracle=%zu+%zu\n", i, moff[i], mlen[i], ooff[i],
                       olen[i]);
                dump_buf("buf", (const unsigned char *)text, strlen(text) + 1);
                return 0;
            }
        }
        bt_move want[MAX_TOK];
        int wn = oracle_parse(text, want);
        bt_alg alg;
        bt_alg_init(&alg);
        char err[64];
        int got_ok = bt_alg_parse(text, &alg, err, sizeof err) ? 1 : 0;
        if (wn == -1) {
            printf("ORACLE DECODE FLIP\n");
            dump_buf("buf", (const unsigned char *)text, strlen(text) + 1);
            bt_alg_free(&alg);
            return 0;
        }
        int want_ok = wn >= 0 ? 1 : 0;
        if (got_ok != want_ok) {
            printf("PARSE VERDICT c=%d rust=%d err=%s\n", got_ok, want_ok, err);
            dump_buf("buf", (const unsigned char *)text, strlen(text) + 1);
            bt_alg_free(&alg);
            return 0;
        }
        if (got_ok) {
            if (bt_alg_len(&alg) != (size_t)wn) {
                printf("MOVE COUNT c=%zu rust=%d\n", bt_alg_len(&alg), wn);
                dump_buf("buf", (const unsigned char *)text, strlen(text) + 1);
                bt_alg_free(&alg);
                return 0;
            }
            for (int i = 0; i < wn; i++) {
                bt_move g = alg.items[i];
                bt_move w = want[i];
                if (g.kind != w.kind || g.face != w.face || g.slice != w.slice || g.axis != w.axis ||
                    g.quarter != w.quarter) {
                    printf("MOVE %d c=%u/%u/%u/%u/%u rust=%u/%u/%u/%u/%u\n", i, g.kind, g.face,
                           g.slice, g.axis, g.quarter, w.kind, w.face, w.slice, w.axis, w.quarter);
                    dump_buf("buf", (const unsigned char *)text, strlen(text) + 1);
                    bt_alg_free(&alg);
                    return 0;
                }
            }
        }
        bt_alg_free(&alg);
    } else {
        bt_alg alg;
        bt_alg_init(&alg);
        char err[64];
        (void)bt_alg_parse(text, &alg, err, sizeof err);
        bt_alg_free(&alg);
    }
    return 1;
}

static void directed(void) {
    char buf[16];
    for (size_t i = 0; i < sizeof ws_cps / sizeof ws_cps[0]; i++) {
        unsigned char enc[4];
        size_t el = utf8_enc(ws_cps[i], enc);
        buf[0] = 'U';
        memcpy(buf + 1, enc, el);
        buf[1 + el] = 'M';
        buf[2 + el] = '\0';
        size_t slen = strlen(buf);
        size_t got = bt_space_len(buf, 1, slen);
        if (got != el) {
            printf("WS LEN cp=%04X want=%zu got=%zu\n", ws_cps[i], el, got);
            fail_count++;
        }
        if (!check_one(buf)) {
            fail_count++;
        }
    }
    for (size_t i = 0; i < sizeof near_cps / sizeof near_cps[0]; i++) {
        unsigned char enc[4];
        size_t el = utf8_enc(near_cps[i], enc);
        buf[0] = 'U';
        memcpy(buf + 1, enc, el);
        buf[1 + el] = 'M';
        buf[2 + el] = '\0';
        size_t slen = strlen(buf);
        size_t got = bt_space_len(buf, 1, slen);
        if (got != 0) {
            printf("NEAR MATCH cp=%04X got=%zu\n", near_cps[i], got);
            fail_count++;
        }
        if (!check_one(buf)) {
            fail_count++;
        }
        bt_alg alg;
        bt_alg_init(&alg);
        char err[64];
        if (bt_alg_parse(buf, &alg, err, sizeof err)) {
            printf("NEAR PARSED cp=%04X\n", near_cps[i]);
            fail_count++;
        }
        bt_alg_free(&alg);
    }
    const char *trunc[] = {
        "\xC2", "\xE1", "\xE1\x9A", "\xE2", "\xE2\x80", "\xE2\x81", "\xE3", "\xE3\x80",
        "U\xC2", "U\xE1\x9A", "U\xE2\x80", "U\xE3\x80", "\xC2\xC2", "\xE2\x80\xE2", "\xE2\x80\xC2\x85",
        "U\xE2\x80\xAF", "U\xC2\x85\xC2\xA0M", "\xE1\x9A\x80", "U\xE1\x9A\x80M", "\xE2\x81\x9F",
        "U\xE2\x81\x9FM", "\xE3\x80\x80", "U\xE3\x80\x80M", "\xE2\x80\xA8\xE2\x80\xA9",
    };
    for (size_t i = 0; i < sizeof trunc / sizeof trunc[0]; i++) {
        if (!check_one(trunc[i])) {
            fail_count++;
        }
    }
}

static const unsigned char alpha[] = {
    0x00, 0x09, 0x0D, 0x20, 0x41, 0x80, 0x81, 0x85, 0x8A, 0x8B, 0x9A,
    0x9F, 0xA0, 0xA8, 0xA9, 0xAF, 0xC2, 0xE1, 0xE2, 0xE3,
};

static void exhaustive(void) {
    unsigned char buf[6];
    unsigned long long cases = 0;
    size_t base = sizeof alpha / sizeof alpha[0];
    for (size_t l = 1; l <= 4; l++) {
        unsigned long long total = 1;
        for (size_t i = 0; i < l; i++) {
            total *= base;
        }
        for (unsigned long long idx = 0; idx < total; idx++) {
            unsigned long long x = idx;
            for (size_t i = 0; i < l; i++) {
                buf[i] = alpha[x % base];
                x /= base;
            }
            buf[l] = '\0';
            cases++;
            if (!check_pred_eq((const char *)buf)) {
                fail_count++;
                if (fail_count > 20) {
                    return;
                }
            }
        }
    }
    printf("exhaustive cases=%llu\n", cases);
}

static void fuzz(unsigned long long iters, uint64_t seed) {
    char buf[64];
    unsigned long long done = 0;
    for (unsigned long long it = 0; it < iters; it++) {
        size_t len = (size_t)(smix(&seed) % 49);
        for (size_t i = 0; i < len; i++) {
            uint64_t r = smix(&seed);
            if (r % 100 < 55) {
                buf[i] = (char)alpha[smix(&seed) % (sizeof alpha / sizeof alpha[0])];
            } else {
                buf[i] = (char)(smix(&seed) % 256);
            }
        }
        buf[len] = '\0';
        done++;
        if (!check_one(buf)) {
            fail_count++;
            if (fail_count > 20) {
                break;
            }
        }
    }
    printf("fuzz iters=%llu\n", done);
}

int main(int argc, char **argv) {
    unsigned long long iters = argc > 1 ? strtoull(argv[1], NULL, 10) : 3000000ULL;
    uint64_t seed = argc > 2 ? strtoull(argv[2], NULL, 10) : 20261012ULL;
    directed();
    exhaustive();
    fuzz(iters, seed);
    if (fail_count == 0) {
        printf("PROBE PASS\n");
        return 0;
    }
    printf("PROBE FAIL count=%d\n", fail_count);
    return 1;
}
