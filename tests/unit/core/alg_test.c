#include <string.h>

#include "common/bt_harness.h"
#include "common/strbuf.h"
#include "core/alg.h"
#include "core/moves.h"

static bool parse_ok(const char *text, bt_alg *alg) {
    char err[64];
    return bt_alg_parse(text, alg, err, sizeof err);
}

static void fmt_of(const char *text, bt_strbuf *out) {
    bt_alg alg;
    bt_alg_init(&alg);
    char err[64];
    if (!bt_alg_parse(text, &alg, err, sizeof err)) {
        BT_ABORT("parse failed for %s: %s", text, err);
    }
    bt_alg_format(&alg, out);
    bt_alg_free(&alg);
}

BT_TEST(alg, parses_every_token_kind) {
    bt_alg alg;
    bt_alg_init(&alg);
    BT_CHECK(parse_ok("U D L R F B u d l r f b 2U 2D 2L 2R 2F 2B M E S x y z", &alg));
    BT_CHECK_EQ(bt_alg_len(&alg), (size_t)24);
    BT_CHECK_EQ(bt_alg_htm(&alg), 21U);
    bt_alg_free(&alg);
}

BT_TEST(alg, parses_and_canonicalizes_suffixes) {
    bt_strbuf sb;
    bt_strbuf_init(&sb, 64);
    fmt_of("U U' U2 U2' r r' r2 r2' 2F 2F' 2F2 2F2' M M' M2 M2' x x' x2 x2'", &sb);
    BT_CHECK_EQ(sb.data, "U U' U2 U2 r r' r2 r2 2F 2F' 2F2 2F2 M M' M2 M2 x x' x2 x2");
    bt_strbuf_free(&sb);
}

BT_TEST(alg, empty_and_whitespace_parse_to_empty) {
    const char *texts[] = {"", " ", "   "};
    for (size_t i = 0; i < 3; i++) {
        bt_alg alg;
        bt_alg_init(&alg);
        BT_CHECK(parse_ok(texts[i], &alg));
        BT_CHECK(bt_alg_is_empty(&alg));
        BT_CHECK_EQ(bt_alg_len(&alg), (size_t)0);
        bt_strbuf sb;
        bt_strbuf_init(&sb, 16);
        bt_alg_format(&alg, &sb);
        BT_CHECK_EQ(sb.len, (size_t)0);
        BT_CHECK_EQ(bt_alg_qtm(&alg), 0U);
        BT_CHECK_EQ(bt_alg_htm(&alg), 0U);
        bt_strbuf_free(&sb);
        bt_alg_free(&alg);
    }
}

BT_TEST(alg, metrics) {
    struct {
        const char *text;
        uint32_t qtm;
        uint32_t htm;
    } pins[] = {
        {"U", 1, 1},
        {"U2", 2, 1},
        {"U'", 1, 1},
        {"x", 0, 0},
        {"x2", 0, 0},
        {"R U R' U'", 4, 4},
        {"U M2 U M' U2 M U M2 U'", 12, 9},
        {"x y2 z' U", 1, 1},
    };
    for (size_t i = 0; i < sizeof pins / sizeof pins[0]; i++) {
        bt_alg alg;
        bt_alg_init(&alg);
        BT_CHECK(parse_ok(pins[i].text, &alg));
        BT_CHECK_EQ(bt_alg_qtm(&alg), pins[i].qtm);
        BT_CHECK_EQ(bt_alg_htm(&alg), pins[i].htm);
        bt_alg_free(&alg);
    }
}

BT_TEST(alg, inverse_format) {
    bt_strbuf sb;
    bt_strbuf_init(&sb, 64);
    bt_alg alg;
    bt_alg_init(&alg);
    BT_CHECK(parse_ok("R U r' 2F M2 x", &alg));
    bt_alg inverse = bt_alg_inverse(&alg);
    bt_alg_format(&inverse, &sb);
    BT_CHECK_EQ(sb.data, "x' M2 2F' r U' R'");
    bt_alg_free(&alg);
    bt_alg_free(&inverse);
    bt_strbuf_free(&sb);
}

BT_TEST(alg, rejects_garbage_tokens) {
    const char *bad[] = {
        "V",   "U3",  "Uu",  "2u",   "2",  "2W", "2M", "2x",   "m",  "e",   "s",    "X",
        "U''", "U2x", "2UU", "2U''", "U5", "r3", "Mx", "x2''", "U-", "R T", "U ok", "2U 2q",
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        bt_alg alg;
        bt_alg_init(&alg);
        char err[64];
        BT_CHECK(!bt_alg_parse(bad[i], &alg, err, sizeof err));
        bt_alg_free(&alg);
    }
}

BT_TEST(alg, parse_error_carries_the_token) {
    bt_alg alg;
    bt_alg_init(&alg);
    char err[64];
    BT_CHECK(!bt_alg_parse("U wut", &alg, err, sizeof err));
    BT_CHECK_EQ(err, "wut");
    bt_alg_free(&alg);
}

BT_TEST(alg, splits_unicode_whitespace) {
    const char *texts[] = {
        "U\xc2\xa0"
        "M",
        "U\x0bM",
        "U\xe3\x80\x80"
        "M",
        "U\x0cM",
        "U\xc2\x85"
        "M",
        "U\xe2\x80\xa8"
        "M",
    };
    for (size_t i = 0; i < sizeof texts / sizeof texts[0]; i++) {
        bt_alg alg;
        bt_alg_init(&alg);
        char err[64];
        if (!bt_alg_parse(texts[i], &alg, err, sizeof err)) {
            BT_ABORT("whitespace split failed for %s: %s", texts[i], err);
        }
        BT_CHECK_EQ(bt_alg_len(&alg), (size_t)2);
        bt_strbuf sb;
        bt_strbuf_init(&sb, 16);
        bt_alg_format(&alg, &sb);
        BT_CHECK_EQ(sb.data, "U M");
        bt_strbuf_free(&sb);
        bt_alg_free(&alg);
    }
}
