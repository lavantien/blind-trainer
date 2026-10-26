#include "common.h"

BT_TEST(parse_roundtrip, m2_prime_parses_as_m2) {
    bt_alg folded;
    bt_alg plain;
    bt_alg_init(&folded);
    bt_alg_init(&plain);
    char err[64];
    BT_CHECK(bt_alg_parse("M2'", &folded, err, sizeof err));
    BT_CHECK(bt_alg_parse("M2", &plain, err, sizeof err));
    BT_CHECK(bt_alg_eq(&folded, &plain));
    bt_cube a = bt_cube_solved();
    bt_cube b = bt_cube_solved();
    bt_cube_apply_alg(&a, &folded);
    bt_cube_apply_alg(&b, &plain);
    BT_CHECK(bt_cube_eq(&a, &b));
    bt_alg_free(&folded);
    bt_alg_free(&plain);
}

BT_TEST(parse_roundtrip, canonical_formatting) {
    struct {
        const char *text;
        const char *want;
    } pins[] = {
        {"U2'", "U2"}, {"2L2'", "2L2"}, {"x2'", "x2"},
        {"r2'", "r2"}, {"", ""},        {"  R   U  ", "R U"},
    };
    for (size_t i = 0; i < 6; i++) {
        bt_alg alg;
        bt_alg_init(&alg);
        char err[64];
        BT_CHECK(bt_alg_parse(pins[i].text, &alg, err, sizeof err));
        bt_strbuf sb;
        bt_strbuf_init(&sb, 32);
        bt_alg_format(&alg, &sb);
        BT_CHECK_EQ(sb.data, pins[i].want);
        bt_strbuf_free(&sb);
        bt_alg_free(&alg);
    }
}

BT_TEST(parse_roundtrip, rejects_unknown_tokens) {
    const char *bad[] = {
        "V",  "U3",   "Uu",       "2u",  "2",   "2W",   "2M",    "2x", "m",
        "e",  "s",    "X",        "U''", "U2x", "2UU",  "2U''",  "U5", "r3",
        "Mx", "x2''", "\xc3\x9c", "U-",  "R T", "U ok", "2U 2q",
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        bt_alg alg;
        bt_alg_init(&alg);
        char err[64];
        BT_CHECK(!bt_alg_parse(bad[i], &alg, err, sizeof err));
        bt_alg_free(&alg);
    }
}

BT_TEST(parse_roundtrip, format_parse_round_trip) {
    bt_rng rng;
    bt_rng_seed(&rng, 20261012);
    char text[256];
    for (size_t case_index = 0; case_index < 2000; case_index++) {
        tc_alg_text(&rng, text, sizeof text);
        bt_alg alg;
        bt_alg_init(&alg);
        char err[64];
        if (!bt_alg_parse(text, &alg, err, sizeof err)) {
            BT_ABORT("generated text failed to parse: %s: %s", text, err);
        }
        bt_strbuf canonical;
        bt_strbuf_init(&canonical, 256);
        bt_alg_format(&alg, &canonical);
        if (strstr(canonical.data, "2'") != NULL) {
            BT_ABORT("canonical form %s", canonical.data);
        }
        bt_alg reparsed;
        bt_alg_init(&reparsed);
        BT_CHECK(bt_alg_parse(canonical.data, &reparsed, err, sizeof err));
        if (!bt_alg_eq(&reparsed, &alg)) {
            BT_ABORT("round trip changed alg: %s -> %s", text, canonical.data);
        }
        bt_strbuf_free(&canonical);
        bt_alg_free(&alg);
        bt_alg_free(&reparsed);
    }
}
