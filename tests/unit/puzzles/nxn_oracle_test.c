#include "common/bt_harness.h"
#include "common/rng.h"
#include "common/strbuf.h"
#include "core/alg.h"
#include "core/cube.h"
#include "puzzles/geometry.h"
#include "puzzles/nxn.h"
#include "puzzles/tier1.h"
#include "unit/core/common.h"
#include "unit/puzzles/death_test.h"

#include <string.h>

enum {
    NX_MAX_ROWS = 256,
    NX_TEXT_MAX = 512,
};

static bool nxn_parse_ok(const bt_pz_nxn *nxn, const char *text, void *alg) {
    bt_pz_error err;
    if (bt_pz_nxn_parse_alg(nxn, text, strlen(text), alg, &err)) {
        return true;
    }
    BT_ABORT("parse %s failed: %s", text, err.token);
}

static void nxn_applied(const bt_pz_nxn *nxn, const char *text, bt_pz_nxn_state *out) {
    bt_pz_nxn_solved_state(nxn, out);
    void *alg = bt_pz_alg_create();
    nxn_parse_ok(nxn, text, alg);
    bt_pz_error err;
    if (!bt_pz_nxn_apply_alg(nxn, out, alg, &err)) {
        BT_ABORT("apply %s failed: %s", text, err.token);
    }
    bt_pz_alg_destroy(alg);
}

static void nxn_format(const bt_pz_nxn *nxn, const void *alg, char *out, size_t out_size) {
    bt_strbuf sb;
    bt_strbuf_init(&sb, 64);
    bt_pz_nxn_format_alg(nxn, alg, &sb);
    snprintf(out, out_size, "%s", sb.data);
    bt_strbuf_free(&sb);
}

static bool nxn_move_ok(const bt_pz_nxn *nxn, const char *token, bt_pz_nxn_move *out) {
    bt_pz_error err;
    return bt_pz_nxn_parse_move(nxn, token, strlen(token), out, &err);
}

static bt_pz_engine nxn_engine_of(const bt_pz_nxn *nxn) {
    return bt_pz_nxn_engine(nxn);
}

static bool nxn_alg_moves_eq(const bt_pz_nxn *nxn, const void *a, const void *b) {
    bt_pz_engine eng = nxn_engine_of(nxn);
    if (bt_pz_alg_len(a) != bt_pz_alg_len(b)) {
        return false;
    }
    for (size_t i = 0; i < bt_pz_alg_len(a); i++) {
        const bt_pz_nxn_move *ma = bt_pz_alg_at(&eng, a, i);
        const bt_pz_nxn_move *mb = bt_pz_alg_at(&eng, b, i);
        if (!bt_pz_nxn_move_eq(*ma, *mb)) {
            return false;
        }
    }
    return true;
}

BT_TEST(nxn_oracle, constructor_bounds_n_to_two_through_seven) {
    static const size_t rejected[] = {0, 1, 8, 100};
    for (size_t i = 0; i < sizeof rejected / sizeof rejected[0]; i++) {
        bt_pz_nxn nxn;
        BT_CHECK(!bt_pz_nxn_init(&nxn, rejected[i]));
    }
    for (size_t n = 2; n <= 7; n++) {
        bt_pz_nxn nxn;
        BT_CHECK(bt_pz_nxn_init(&nxn, n));
        BT_CHECK_EQ(bt_pz_nxn_n(&nxn), n);
    }
}

BT_TEST(nxn_oracle, solved_state_renders_uniform_faces_at_every_n) {
    for (size_t n = 2; n <= 7; n++) {
        bt_pz_nxn nxn;
        bt_pz_nxn_init(&nxn, n);
        bt_pz_nxn_state state;
        bt_pz_nxn_solved_state(&nxn, &state);
        BT_CHECK(bt_pz_nxn_is_solved(&nxn, &state));
        bt_pz_grid grid;
        bt_pz_nxn_facelets(&nxn, &state, &grid);
        BT_CHECK_EQ(grid.face_count, (size_t)6);
        for (size_t face = 0; face < 6; face++) {
            BT_CHECK_EQ(grid.faces[face].row_count, n);
            for (size_t r = 0; r < n; r++) {
                BT_CHECK_EQ(grid.faces[face].rows[r].cols, n);
                for (size_t c = 0; c < n; c++) {
                    BT_CHECK_EQ(grid.faces[face].rows[r].cells[c], (uint8_t)face);
                }
            }
        }
    }
}

BT_TEST(nxn_oracle, parses_every_token_class_at_n3) {
    static const char *tokens[] = {
        "U",  "U'",  "U2",  "U2'", "U'2",  "D",    "L",   "F",    "R",   "B",  "D'",  "L2",
        "F2", "Rw",  "Rw'", "Rw2", "Rw2'", "Rw'2", "Lw",  "Uw",   "Dw",  "Fw", "Bw",  "Uw2'",
        "r",  "l",   "u",   "d",   "f",    "b",    "r'",  "l2",   "u2'", "b2", "M",   "M'",
        "M2", "M2'", "E",   "E'",  "E2",   "S",    "S'",  "S2",   "S2'", "x",  "x'",  "x2",
        "y",  "y'",  "y2",  "z",   "z'",   "z2",   "x2'", "y'2",  "z2'", "2U", "2U'", "2U2",
        "2D", "2L",  "2F",  "2R",  "2B",   "2Rw",  "3Rw", "3Rw'",
    };
    bt_pz_nxn nxn;
    bt_pz_nxn_init(&nxn, 3);
    for (size_t i = 0; i < sizeof tokens / sizeof tokens[0]; i++) {
        bt_pz_nxn_move mv;
        BT_CHECK(nxn_move_ok(&nxn, tokens[i], &mv));
    }
}

BT_TEST(nxn_oracle, parses_digit_prefixes_up_to_n7) {
    static const char faces[] = "UDLRFB";
    bt_pz_nxn nxn;
    bt_pz_nxn_init(&nxn, 7);
    char token[8];
    for (size_t f = 0; f < 6; f++) {
        for (size_t digit = 2; digit <= 7; digit++) {
            bt_pz_nxn_move mv;
            snprintf(token, sizeof token, "%zu%c", digit, faces[f]);
            BT_CHECK(nxn_move_ok(&nxn, token, &mv));
            snprintf(token, sizeof token, "%zu%cw", digit, faces[f]);
            BT_CHECK(nxn_move_ok(&nxn, token, &mv));
            snprintf(token, sizeof token, "%zu%cw'", digit, faces[f]);
            BT_CHECK(nxn_move_ok(&nxn, token, &mv));
        }
    }
    bt_pz_nxn capped;
    bt_pz_nxn_init(&capped, 4);
    static const char *bad[] = {"5R", "6R", "7R", "5Rw", "6Rw", "7Rw", "8R", "8Rw"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        bt_pz_nxn_move mv;
        BT_CHECK(!nxn_move_ok(&capped, bad[i], &mv));
    }
}

BT_TEST(nxn_oracle, both_prime_orders_agree_and_format_to_the_half_turn) {
    static const struct {
        const char *a;
        const char *b;
        const char *canonical;
    } cases[] = {
        {"U2'", "U'2", "U2"}, {"Rw2'", "Rw'2", "Rw2"}, {"M2'", "M'2", "M2"},
        {"x2'", "x'2", "x2"}, {"u2'", "u'2", "Uw2"},   {"2R2'", "2R'2", "2R2"},
    };
    bt_pz_nxn nxn;
    bt_pz_nxn_init(&nxn, 3);
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        bt_pz_nxn_move ma;
        bt_pz_nxn_move mb;
        BT_CHECK(nxn_move_ok(&nxn, cases[i].a, &ma));
        BT_CHECK(nxn_move_ok(&nxn, cases[i].b, &mb));
        BT_CHECK(bt_pz_nxn_move_eq(ma, mb));
        void *alg = bt_pz_alg_create();
        nxn_parse_ok(&nxn, cases[i].a, alg);
        char text[NX_TEXT_MAX];
        nxn_format(&nxn, alg, text, sizeof text);
        BT_CHECK(strcmp(text, cases[i].canonical) == 0);
        bt_pz_alg_destroy(alg);
    }
}

BT_TEST(nxn_oracle, wide_aliases_parse_to_identical_moves) {
    static const struct {
        const char *a;
        const char *b;
    } cases[] = {
        {"Rw", "2Rw"}, {"u", "Uw"},  {"r", "Rw"},   {"f", "Fw"},
        {"l2", "Lw2"}, {"2r", "Rw"}, {"3r", "3Rw"},
    };
    bt_pz_nxn nxn;
    bt_pz_nxn_init(&nxn, 3);
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        bt_pz_nxn_move ma;
        bt_pz_nxn_move mb;
        BT_CHECK(nxn_move_ok(&nxn, cases[i].a, &ma));
        BT_CHECK(nxn_move_ok(&nxn, cases[i].b, &mb));
        BT_CHECK(bt_pz_nxn_move_eq(ma, mb));
    }
}

BT_TEST(nxn_oracle, rejects_garbage_tokens) {
    static const char *tokens[] = {
        "",    "V",  "N",  "P",  "Q",   "T",    "W",   "A",    "I",   "O",    "8R",   "9Rw",  "1R",
        "0U",  "2",  "7",  "U3", "U4",  "U5",   "U''", "U'''", "U22", "U222", "U2''", "U'2'", "rw",
        "Rww", "wR", "m",  "e",  "s",   "2M",   "2x",  "3y",   "X",   "Y",    "Z",    "'",    "''",
        "2'",  "'U", "U-", "U+", "UwU", "Uw2w", "Rw3", "rw'",  "ü",   "Uw-",  "(R",
    };
    bt_pz_nxn nxn;
    bt_pz_nxn_init(&nxn, 3);
    for (size_t i = 0; i < sizeof tokens / sizeof tokens[0]; i++) {
        bt_pz_nxn_move mv;
        BT_CHECK(!nxn_move_ok(&nxn, tokens[i], &mv));
    }
}

BT_TEST(nxn_oracle, parse_alg_splits_on_whitespace_and_accepts_empty) {
    bt_pz_nxn nxn;
    bt_pz_nxn_init(&nxn, 3);
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    BT_CHECK(bt_pz_nxn_parse_alg(&nxn, "", strlen(""), alg, &err));
    BT_CHECK(bt_pz_alg_is_empty(alg));
    BT_CHECK(bt_pz_nxn_parse_alg(&nxn, "  R   U'  ", strlen("  R   U'  "), alg, &err));
    BT_CHECK_EQ(bt_pz_alg_len(alg), (size_t)2);
    bt_pz_error bad;
    BT_CHECK(!bt_pz_nxn_parse_alg(&nxn, "R wut", strlen("R wut"), alg, &bad));
    BT_CHECK(strcmp(bad.token, "wut") == 0);
    char text[NX_TEXT_MAX];
    nxn_format(&nxn, alg, text, sizeof text);
    BT_CHECK(strcmp(text, "R U'") == 0);
    bt_pz_alg_destroy(alg);
}

BT_TEST(nxn_oracle, formatting_is_canonical_per_token_class) {
    bt_pz_nxn nxn;
    bt_pz_nxn_init(&nxn, 3);
    static const struct {
        const char *token;
        const char *canonical;
    } n3[] = {
        {"U", "U"}, {"U'", "U'"}, {"U2'", "U2"}, {"u", "Uw"},  {"2Uw", "Uw"},   {"2U", "2U"},
        {"M", "M"}, {"S'", "S'"}, {"x", "x"},    {"Rw", "Rw"}, {"Rw2'", "Rw2"}, {"3Rw'", "x'"},
    };
    for (size_t i = 0; i < sizeof n3 / sizeof n3[0]; i++) {
        void *alg = bt_pz_alg_create();
        nxn_parse_ok(&nxn, n3[i].token, alg);
        char text[NX_TEXT_MAX];
        nxn_format(&nxn, alg, text, sizeof text);
        BT_CHECK(strcmp(text, n3[i].canonical) == 0);
        bt_pz_alg_destroy(alg);
    }
    bt_pz_nxn tiny;
    bt_pz_nxn_init(&tiny, 2);
    static const struct {
        const char *token;
        const char *canonical;
    } n2[] = {
        {"Rw", "x"}, {"Uw", "y"}, {"Fw", "z"}, {"Lw", "Lw"}, {"M", "M"},
    };
    for (size_t i = 0; i < sizeof n2 / sizeof n2[0]; i++) {
        void *alg = bt_pz_alg_create();
        nxn_parse_ok(&tiny, n2[i].token, alg);
        char text[NX_TEXT_MAX];
        nxn_format(&tiny, alg, text, sizeof text);
        BT_CHECK(strcmp(text, n2[i].canonical) == 0);
        bt_pz_alg_destroy(alg);
    }
    bt_pz_nxn big;
    bt_pz_nxn_init(&big, 4);
    static const struct {
        const char *token;
        const char *canonical;
    } n4[] = {
        {"3Rw'", "3Rw'"},
        {"4Rw", "x"},
        {"3R", "3R"},
    };
    for (size_t i = 0; i < sizeof n4 / sizeof n4[0]; i++) {
        void *alg = bt_pz_alg_create();
        nxn_parse_ok(&big, n4[i].token, alg);
        char text[NX_TEXT_MAX];
        nxn_format(&big, alg, text, sizeof text);
        BT_CHECK(strcmp(text, n4[i].canonical) == 0);
        bt_pz_alg_destroy(alg);
    }
}

static void nxn_random_token(bt_rng *rng, size_t n, char *out, size_t out_size) {
    static const char faces[] = "ULFRBD";
    static const char *suffixes[5] = {"", "'", "2", "2'", "'2"};
    static const char *extra[3] = {"M", "E", "S"};
    static const char *rotations[3] = {"x", "y", "z"};
    uint8_t cls = (uint8_t)bt_rng_below(rng, 8);
    size_t face = (size_t)bt_rng_below(rng, 6);
    size_t digit = 2 + (size_t)bt_rng_below(rng, n - 1);
    size_t suffix = (size_t)bt_rng_below(rng, 5);
    if (cls <= 1) {
        snprintf(out, out_size, "%c%s", faces[face], suffixes[suffix]);
    } else if (cls == 2) {
        snprintf(out, out_size, "%cw%s", faces[face], suffixes[suffix]);
    } else if (cls == 3) {
        snprintf(out, out_size, "%zu%c%s", digit, faces[face], suffixes[suffix]);
    } else if (cls == 4) {
        snprintf(out, out_size, "%zu%cw%s", digit, faces[face], suffixes[suffix]);
    } else if (cls == 5) {
        char lower = (char)((unsigned char)faces[face] | 0x20U);
        snprintf(out, out_size, "%c%s", lower, suffixes[suffix]);
    } else if (cls == 6) {
        snprintf(out, out_size, "%s%s", extra[face % 3], suffixes[suffix]);
    } else {
        snprintf(out, out_size, "%s%s", rotations[face % 3], suffixes[suffix]);
    }
}

static size_t nxn_random_case(bt_rng *rng, char *text, size_t text_size) {
    size_t n = 2 + (size_t)bt_rng_below(rng, 6);
    size_t count = 1 + (size_t)bt_rng_below(rng, 24);
    size_t used = 0;
    text[0] = '\0';
    for (size_t i = 0; i < count; i++) {
        char token[16];
        nxn_random_token(rng, n, token, sizeof token);
        int written = snprintf(text + used, text_size - used, "%s%s", i == 0 ? "" : " ", token);
        if (written < 0 || (size_t)written >= text_size - used) {
            BT_ABORT("token text overflow at %zu", i);
        }
        used += (size_t)written;
    }
    return n;
}

BT_TEST(nxn_oracle, random_algs_round_trip_through_their_inverse_at_every_n) {
    for (uint64_t seed = 0; seed < 400; seed++) {
        bt_rng rng;
        bt_rng_seed(&rng, seed);
        char text[NX_TEXT_MAX];
        size_t n = nxn_random_case(&rng, text, sizeof text);
        bt_pz_nxn nxn;
        bt_pz_nxn_init(&nxn, n);
        bt_pz_error err;
        void *alg = bt_pz_alg_create();
        if (!bt_pz_nxn_parse_alg(&nxn, text, strlen(text), alg, &err)) {
            BT_ABORT("parse %s failed: %s", text, err.token);
        }
        void *inverse = bt_pz_alg_create();
        bt_pz_nxn_invert_alg(&nxn, alg, inverse);
        bt_pz_nxn_state forward;
        bt_pz_nxn_solved_state(&nxn, &forward);
        BT_CHECK(bt_pz_nxn_apply_alg(&nxn, &forward, alg, &err));
        BT_CHECK(bt_pz_nxn_apply_alg(&nxn, &forward, inverse, &err));
        BT_CHECK(bt_pz_nxn_is_solved(&nxn, &forward));
        bt_pz_nxn_state reverse;
        bt_pz_nxn_solved_state(&nxn, &reverse);
        BT_CHECK(bt_pz_nxn_apply_alg(&nxn, &reverse, inverse, &err));
        BT_CHECK(bt_pz_nxn_apply_alg(&nxn, &reverse, alg, &err));
        BT_CHECK(bt_pz_nxn_is_solved(&nxn, &reverse));
        bt_pz_alg_destroy(alg);
        bt_pz_alg_destroy(inverse);
    }
}

BT_TEST(nxn_oracle, format_and_invert_are_stable_on_random_algs) {
    for (uint64_t seed = 0; seed < 400; seed++) {
        bt_rng rng;
        bt_rng_seed(&rng, seed);
        char text[NX_TEXT_MAX];
        size_t n = nxn_random_case(&rng, text, sizeof text);
        bt_pz_nxn nxn;
        bt_pz_nxn_init(&nxn, n);
        bt_pz_error err;
        void *alg = bt_pz_alg_create();
        if (!bt_pz_nxn_parse_alg(&nxn, text, strlen(text), alg, &err)) {
            BT_ABORT("parse %s failed: %s", text, err.token);
        }
        char formatted[NX_TEXT_MAX];
        nxn_format(&nxn, alg, formatted, sizeof formatted);
        void *reparsed = bt_pz_alg_create();
        if (!bt_pz_nxn_parse_alg(&nxn, formatted, strlen(formatted), reparsed, &err)) {
            BT_ABORT("reparse %s failed: %s", formatted, err.token);
        }
        BT_CHECK_EQ(bt_pz_alg_len(reparsed), bt_pz_alg_len(alg));
        for (size_t i = 0; i < bt_pz_alg_len(alg); i++) {
            bt_pz_engine eng = nxn_engine_of(&nxn);
            const bt_pz_nxn_move *a = bt_pz_alg_at(&eng, alg, i);
            const bt_pz_nxn_move *b = bt_pz_alg_at(&eng, reparsed, i);
            BT_CHECK(bt_pz_nxn_move_eq(*a, *b));
        }
        void *twice = bt_pz_alg_create();
        bt_pz_nxn_invert_alg(&nxn, alg, twice);
        void *restored = bt_pz_alg_create();
        bt_pz_nxn_invert_alg(&nxn, twice, restored);
        BT_CHECK_EQ(bt_pz_alg_len(restored), bt_pz_alg_len(alg));
        for (size_t i = 0; i < bt_pz_alg_len(alg); i++) {
            bt_pz_engine eng = nxn_engine_of(&nxn);
            const bt_pz_nxn_move *a = bt_pz_alg_at(&eng, alg, i);
            const bt_pz_nxn_move *b = bt_pz_alg_at(&eng, restored, i);
            BT_CHECK(bt_pz_nxn_move_eq(*a, *b));
        }
        bt_pz_alg_destroy(alg);
        bt_pz_alg_destroy(reparsed);
        bt_pz_alg_destroy(twice);
        bt_pz_alg_destroy(restored);
    }
}

BT_TEST(nxn_oracle, applying_random_algs_preserves_the_color_multiset) {
    for (uint64_t seed = 0; seed < 400; seed++) {
        bt_rng rng;
        bt_rng_seed(&rng, seed);
        char text[NX_TEXT_MAX];
        size_t n = nxn_random_case(&rng, text, sizeof text);
        bt_pz_nxn nxn;
        bt_pz_nxn_init(&nxn, n);
        bt_pz_nxn_state state;
        nxn_applied(&nxn, text, &state);
        bt_pz_grid grid;
        bt_pz_nxn_facelets(&nxn, &state, &grid);
        for (uint8_t face = 0; face < 6; face++) {
            size_t count = 0;
            for (size_t f = 0; f < grid.face_count; f++) {
                for (size_t r = 0; r < grid.faces[f].row_count; r++) {
                    for (size_t c = 0; c < grid.faces[f].rows[r].cols; c++) {
                        if (grid.faces[f].rows[r].cells[c] == face) {
                            count++;
                        }
                    }
                }
            }
            BT_CHECK_EQ(count, n * n);
        }
    }
}

BT_TEST(nxn_oracle, every_quarter_move_has_order_dividing_four) {
    for (size_t n = 2; n <= 7; n++) {
        bt_pz_nxn nxn;
        bt_pz_nxn_init(&nxn, n);
        static const char *base[] = {
            "R", "R'", "R2", "Rw", "2R", "M", "E", "S", "x", "y", "z", "u",
        };
        char pool[24][8];
        size_t pool_len = 0;
        for (size_t i = 0; i < sizeof base / sizeof base[0]; i++) {
            snprintf(pool[pool_len++], 8, "%s", base[i]);
        }
        for (size_t digit = 2; digit <= n; digit++) {
            snprintf(pool[pool_len++], 8, "%zuR", digit);
            snprintf(pool[pool_len++], 8, "%zuRw", digit);
        }
        for (size_t i = 0; i < pool_len; i++) {
            bt_pz_nxn_state state;
            bt_pz_nxn_solved_state(&nxn, &state);
            void *alg = bt_pz_alg_create();
            nxn_parse_ok(&nxn, pool[i], alg);
            bt_pz_error err;
            for (size_t rep = 0; rep < 4; rep++) {
                if (!bt_pz_nxn_apply_alg(&nxn, &state, alg, &err)) {
                    BT_ABORT("apply %s failed: %s", pool[i], err.token);
                }
            }
            BT_CHECK(bt_pz_nxn_is_solved(&nxn, &state));
            bt_pz_alg_destroy(alg);
        }
    }
}

BT_TEST(nxn_oracle, wide_moves_turn_both_outer_layers) {
    for (size_t n = 2; n <= 7; n++) {
        bt_pz_nxn nxn;
        bt_pz_nxn_init(&nxn, n);
        bt_pz_nxn_state wide;
        bt_pz_nxn_state composed;
        bt_pz_nxn_state single;
        nxn_applied(&nxn, "Rw", &wide);
        nxn_applied(&nxn, "R 2R", &composed);
        BT_CHECK(bt_pz_nxn_state_eq(&wide, &composed));
        nxn_applied(&nxn, "u", &wide);
        nxn_applied(&nxn, "U 2U", &composed);
        BT_CHECK(bt_pz_nxn_state_eq(&wide, &composed));
        nxn_applied(&nxn, "Rw", &wide);
        nxn_applied(&nxn, "R", &single);
        BT_CHECK(!bt_pz_nxn_state_eq(&wide, &single));
    }
}

BT_TEST(nxn_oracle, digit_prefixes_select_the_matching_depth_band) {
    for (size_t n = 3; n <= 7; n++) {
        bt_pz_nxn nxn;
        bt_pz_nxn_init(&nxn, n);
        char singles[8][8];
        snprintf(singles[0], 8, "%s", "R");
        for (size_t d = 2; d <= n; d++) {
            snprintf(singles[d - 1], 8, "%zuR", d);
        }
        for (size_t depth = 3; depth <= n; depth++) {
            bt_pz_nxn_state wide;
            bt_pz_nxn_state composed;
            char wide_text[8];
            snprintf(wide_text, sizeof wide_text, "%zuRw", depth);
            char composed_text[64];
            size_t used = 0;
            for (size_t d = 0; d < depth; d++) {
                int written = snprintf(composed_text + used, sizeof composed_text - used, "%s%s",
                                       d == 0 ? "" : " ", singles[d]);
                if (written < 0 || (size_t)written >= sizeof composed_text - used) {
                    BT_ABORT("composed text overflow");
                }
                used += (size_t)written;
            }
            nxn_applied(&nxn, wide_text, &wide);
            nxn_applied(&nxn, composed_text, &composed);
            BT_CHECK(bt_pz_nxn_state_eq(&wide, &composed));
        }
        if (n >= 4) {
            bt_pz_nxn_state a;
            bt_pz_nxn_state b;
            nxn_applied(&nxn, "3Rw", &a);
            nxn_applied(&nxn, "4Rw", &b);
            BT_CHECK(!bt_pz_nxn_state_eq(&a, &b));
        }
    }
    bt_pz_nxn nxn;
    bt_pz_nxn_init(&nxn, 4);
    static const char *tokens[] = {"2R", "3R"};
    bt_pz_nxn_state solved;
    bt_pz_nxn_solved_state(&nxn, &solved);
    for (size_t i = 0; i < 2; i++) {
        bt_pz_nxn_state state;
        nxn_applied(&nxn, tokens[i], &state);
        BT_CHECK(!bt_pz_nxn_state_eq(&state, &solved));
        bt_pz_grid grid;
        bt_pz_nxn_facelets(&nxn, &state, &grid);
        static const size_t faces[2] = {1, 3};
        for (size_t k = 0; k < 2; k++) {
            size_t face = faces[k];
            for (size_t r = 0; r < grid.faces[face].row_count; r++) {
                for (size_t c = 0; c < grid.faces[face].rows[r].cols; c++) {
                    BT_CHECK_EQ(grid.faces[face].rows[r].cells[c], (uint8_t)face);
                }
            }
        }
    }
    bt_pz_nxn_state a;
    bt_pz_nxn_state b;
    nxn_applied(&nxn, "2R", &a);
    nxn_applied(&nxn, "3R", &b);
    BT_CHECK(!bt_pz_nxn_state_eq(&a, &b));
}

BT_TEST(nxn_oracle, slice_moves_turn_only_the_middle_layer) {
    bt_pz_nxn n3;
    bt_pz_nxn_init(&n3, 3);
    bt_pz_nxn_state a;
    bt_pz_nxn_state b;
    nxn_applied(&n3, "M", &a);
    nxn_applied(&n3, "2L", &b);
    BT_CHECK(bt_pz_nxn_state_eq(&a, &b));
    nxn_applied(&n3, "E", &a);
    nxn_applied(&n3, "2D", &b);
    BT_CHECK(bt_pz_nxn_state_eq(&a, &b));
    nxn_applied(&n3, "S", &a);
    nxn_applied(&n3, "2F", &b);
    BT_CHECK(bt_pz_nxn_state_eq(&a, &b));
    nxn_applied(&n3, "M", &a);
    nxn_applied(&n3, "L", &b);
    BT_CHECK(!bt_pz_nxn_state_eq(&a, &b));
    bt_pz_nxn n4;
    bt_pz_nxn_init(&n4, 4);
    nxn_applied(&n4, "M", &a);
    nxn_applied(&n4, "2L", &b);
    BT_CHECK(bt_pz_nxn_state_eq(&a, &b));
    nxn_applied(&n4, "3L", &b);
    BT_CHECK(!bt_pz_nxn_state_eq(&a, &b));
    bt_pz_nxn n5;
    bt_pz_nxn_init(&n5, 5);
    nxn_applied(&n5, "M", &a);
    nxn_applied(&n5, "3L", &b);
    BT_CHECK(bt_pz_nxn_state_eq(&a, &b));
    nxn_applied(&n5, "E", &a);
    nxn_applied(&n5, "3D", &b);
    BT_CHECK(bt_pz_nxn_state_eq(&a, &b));
    nxn_applied(&n5, "S", &a);
    nxn_applied(&n5, "3F", &b);
    BT_CHECK(bt_pz_nxn_state_eq(&a, &b));
    nxn_applied(&n5, "M", &a);
    nxn_applied(&n5, "2L", &b);
    BT_CHECK(!bt_pz_nxn_state_eq(&a, &b));
    bt_pz_nxn n2;
    bt_pz_nxn_init(&n2, 2);
    nxn_applied(&n2, "M", &a);
    nxn_applied(&n2, "L", &b);
    BT_CHECK(bt_pz_nxn_state_eq(&a, &b));
}

BT_TEST(nxn_oracle, rotations_reorient_every_layer_together) {
    bt_pz_nxn n3;
    bt_pz_nxn_init(&n3, 3);
    bt_pz_nxn_state a;
    bt_pz_nxn_state b;
    nxn_applied(&n3, "x", &a);
    nxn_applied(&n3, "R M' L'", &b);
    BT_CHECK(bt_pz_nxn_state_eq(&a, &b));
    nxn_applied(&n3, "y", &a);
    nxn_applied(&n3, "U E' D'", &b);
    BT_CHECK(bt_pz_nxn_state_eq(&a, &b));
    nxn_applied(&n3, "z", &a);
    nxn_applied(&n3, "F S B'", &b);
    BT_CHECK(bt_pz_nxn_state_eq(&a, &b));
    nxn_applied(&n3, "x", &a);
    bt_pz_nxn_state solved;
    bt_pz_nxn_solved_state(&n3, &solved);
    BT_CHECK(!bt_pz_nxn_state_eq(&a, &solved));
    bt_pz_nxn n2;
    bt_pz_nxn_init(&n2, 2);
    nxn_applied(&n2, "x", &a);
    nxn_applied(&n2, "Rw", &b);
    BT_CHECK(bt_pz_nxn_state_eq(&a, &b));
}

BT_TEST(nxn_oracle, tier1_accepts_representative_algs_at_every_n) {
    for (size_t n = 2; n <= 7; n++) {
        bt_pz_nxn nxn;
        bt_pz_nxn_init(&nxn, n);
        bt_pz_engine eng = nxn_engine_of(&nxn);
        bt_tier1_error err;
        BT_CHECK_EQ(bt_verify_tier1(&eng, "R U R' U'", &err), (int)BT_TIER1_OK);
        BT_CHECK_EQ(bt_verify_tier1(&eng, "", &err), (int)BT_TIER1_OK);
    }
    bt_pz_nxn n4;
    bt_pz_nxn_init(&n4, 4);
    bt_pz_engine eng4 = nxn_engine_of(&n4);
    bt_tier1_error err;
    static const char *parity = "Rw U2 x Rw U2 Rw U2 Rw' U2 Lw U2 Rw' U2 Rw U2 Rw' U2 Rw'";
    BT_CHECK_EQ(bt_verify_tier1(&eng4, parity, &err), (int)BT_TIER1_OK);
    bt_pz_nxn n5;
    bt_pz_nxn_init(&n5, 5);
    bt_pz_engine eng5 = nxn_engine_of(&n5);
    static const char *l2e = "Rw' U' R' U R' F R F' Rw";
    BT_CHECK_EQ(bt_verify_tier1(&eng5, l2e, &err), (int)BT_TIER1_OK);
    BT_CHECK_EQ(bt_verify_tier1(&eng5, "R wut", &err), (int)BT_TIER1_PARSE);
}

BT_TEST(nxn_oracle, bigcube_pack_tokens_parse_at_their_registered_n) {
    static const char *parity_tokens[] = {
        "R'", "U",   "3Rw'", "F2", "Lw", "R", "Rw", "Rw'", "U'",
        "U2", "Uw2", "l'",   "l",  "r'", "r", "r2", "u2",  "x",
    };
    for (size_t n = 4; n <= 7; n++) {
        bt_pz_nxn nxn;
        bt_pz_nxn_init(&nxn, n);
        for (size_t i = 0; i < sizeof parity_tokens / sizeof parity_tokens[0]; i++) {
            bt_pz_nxn_move mv;
            BT_CHECK(nxn_move_ok(&nxn, parity_tokens[i], &mv));
        }
    }
    static const char *l2e_tokens[] = {
        "3Lw", "3Lw'", "3Rw'", "B2",   "F", "F'", "F2", "Lw",  "Lw'", "R",  "R'",
        "Rw",  "Rw'",  "Rw2",  "Rw2'", "U", "U'", "U2", "U2'", "x",   "x'",
    };
    for (size_t n = 5; n <= 7; n++) {
        bt_pz_nxn nxn;
        bt_pz_nxn_init(&nxn, n);
        for (size_t i = 0; i < sizeof l2e_tokens / sizeof l2e_tokens[0]; i++) {
            bt_pz_nxn_move mv;
            BT_CHECK(nxn_move_ok(&nxn, l2e_tokens[i], &mv));
        }
    }
    bt_pz_nxn n4;
    bt_pz_nxn_init(&n4, 4);
    bt_pz_nxn_move mv;
    BT_CHECK(!nxn_move_ok(&n4, "X", &mv));
    bt_pz_nxn n5;
    bt_pz_nxn_init(&n5, 5);
    BT_CHECK(!nxn_move_ok(&n5, "X,", &mv));
}

static size_t slot_of(size_t face, size_t r, size_t c) {
    if (r == 1 && c == 1) {
        return 48 + face;
    }
    if (r % 2 == 0 && c % 2 == 0) {
        size_t pos = r == 0 ? c / 2 : 3 - c / 2;
        return face * 4 + pos;
    }
    size_t pos = r == 1 ? 3 - c : r;
    return 24 + face * 4 + pos;
}

static size_t home_face_of(size_t sticker) {
    if (sticker < 24) {
        return sticker / 4;
    }
    if (sticker < 48) {
        return (sticker - 24) / 4;
    }
    return sticker - 48;
}

static void cube_grid(const bt_cube *cube, bt_pz_grid *out) {
    out->face_count = 6;
    for (size_t face = 0; face < 6; face++) {
        out->faces[face].row_count = 3;
        for (size_t r = 0; r < 3; r++) {
            out->faces[face].rows[r].cols = 3;
            for (size_t c = 0; c < 3; c++) {
                out->faces[face].rows[r].cells[c] =
                    (uint8_t)home_face_of(bt_cube_sticker_at(cube, (uint8_t)slot_of(face, r, c)));
            }
        }
    }
}

static size_t nxn_pack_corpus(char rows[][NX_TEXT_MAX]) {
    static const char *files[] = {
        "bld_3style_specials.tsv",
        "bld_m2.tsv",
        "twoby2_cll.tsv",
        "bld_oldpoch.tsv",
    };
    size_t count = 0;
    for (size_t i = 0; i < sizeof files / sizeof files[0]; i++) {
        char rel[96];
        snprintf(rel, sizeof rel, "packs/%s", files[i]);
        tc_lines lines;
        tc_sheet_lines(rel, &lines);
        for (size_t at = 1; at < lines.count; at++) {
            char copy[TC_LINE_MAX];
            snprintf(copy, sizeof copy, "%s", lines.lines[at]);
            char *fields[8];
            size_t nfields = tc_split(copy, fields, 8);
            if (nfields < 2) {
                continue;
            }
            char *alg = fields[1];
            char *arrow = strstr(alg, "->");
            if (arrow != NULL) {
                *arrow = '\0';
            }
            char cleaned[TC_LINE_MAX];
            size_t used = 0;
            cleaned[0] = '\0';
            char *walker = alg;
            while (*walker != '\0') {
                while (*walker == ' ' || *walker == '\t' || *walker == ',' || *walker == '(' ||
                       *walker == ')') {
                    walker++;
                }
                if (*walker == '\0') {
                    break;
                }
                if (used > 0) {
                    cleaned[used++] = ' ';
                }
                while (*walker != '\0' && *walker != ' ' && *walker != '\t' && *walker != ',' &&
                       *walker != '(' && *walker != ')') {
                    cleaned[used++] = *walker++;
                }
            }
            cleaned[used] = '\0';
            if (used == 0) {
                continue;
            }
            bt_alg parsed;
            bt_alg_init(&parsed);
            if (!bt_alg_try_parse(cleaned, &parsed)) {
                bt_alg_free(&parsed);
                continue;
            }
            bt_alg_free(&parsed);
            if (count >= NX_MAX_ROWS) {
                BT_ABORT("corpus exceeds %d rows", NX_MAX_ROWS);
            }
            snprintf(rows[count], NX_TEXT_MAX, "%s", cleaned);
            count++;
        }
    }
    return count;
}

BT_TEST(nxn_oracle, n3_agrees_with_bt_core_on_the_pack_corpus) {
    bt_pz_nxn nxn;
    bt_pz_nxn_init(&nxn, 3);
    static char rows[NX_MAX_ROWS][NX_TEXT_MAX];
    size_t count = nxn_pack_corpus(rows);
    BT_CHECK(count >= 150);
    for (size_t i = 0; i < count; i++) {
        bt_alg alg;
        bt_alg_init(&alg);
        char err_token[128];
        if (!bt_alg_parse(rows[i], &alg, err_token, sizeof err_token)) {
            BT_ABORT("core parse %s failed: %s", rows[i], err_token);
        }
        bt_cube cube = bt_cube_solved();
        bt_cube_apply_alg(&cube, &alg);
        bt_alg_free(&alg);
        bt_pz_nxn_state state;
        nxn_applied(&nxn, rows[i], &state);
        bt_pz_grid mine;
        bt_pz_nxn_facelets(&nxn, &state, &mine);
        bt_pz_grid theirs;
        cube_grid(&cube, &theirs);
        BT_CHECK(bt_pz_grid_eq(&mine, &theirs));
    }
}

BT_TEST(nxn_oracle, n3_agrees_with_bt_core_on_generated_algs) {
    static const char faces[] = "ULFRBD";
    static const char *core_suffixes[4] = {"", "'", "2", "2'"};
    static const char *extra[3] = {"M", "E", "S"};
    static const char *rotations[3] = {"x", "y", "z"};
    for (uint64_t seed = 0; seed < 400; seed++) {
        bt_rng rng;
        bt_rng_seed(&rng, seed);
        size_t count = 1 + (size_t)bt_rng_below(&rng, 30);
        char text[NX_TEXT_MAX];
        size_t used = 0;
        text[0] = '\0';
        for (size_t i = 0; i < count; i++) {
            uint8_t cls = (uint8_t)bt_rng_below(&rng, 6);
            size_t face = (size_t)bt_rng_below(&rng, 6);
            size_t suffix = (size_t)bt_rng_below(&rng, 4);
            char token[8];
            if (cls <= 1) {
                snprintf(token, sizeof token, "%c%s", faces[face], core_suffixes[suffix]);
            } else if (cls == 2) {
                snprintf(token, sizeof token, "2%c%s", faces[face], core_suffixes[suffix]);
            } else if (cls == 3) {
                char lower = (char)((unsigned char)faces[face] | 0x20U);
                snprintf(token, sizeof token, "%c%s", lower, core_suffixes[suffix]);
            } else if (cls == 4) {
                snprintf(token, sizeof token, "%s%s", extra[face % 3], core_suffixes[suffix]);
            } else {
                snprintf(token, sizeof token, "%s%s", rotations[face % 3], core_suffixes[suffix]);
            }
            int written =
                snprintf(text + used, sizeof text - used, "%s%s", i == 0 ? "" : " ", token);
            if (written < 0 || (size_t)written >= sizeof text - used) {
                BT_ABORT("token text overflow at %zu", i);
            }
            used += (size_t)written;
        }
        bt_alg alg;
        bt_alg_init(&alg);
        char err_token[128];
        if (!bt_alg_parse(text, &alg, err_token, sizeof err_token)) {
            BT_ABORT("core parse %s failed: %s", text, err_token);
        }
        bt_cube cube = bt_cube_solved();
        bt_cube_apply_alg(&cube, &alg);
        bt_alg_free(&alg);
        bt_pz_nxn nxn;
        bt_pz_nxn_init(&nxn, 3);
        bt_pz_nxn_state state;
        nxn_applied(&nxn, text, &state);
        bt_pz_grid mine;
        bt_pz_nxn_facelets(&nxn, &state, &mine);
        bt_pz_grid theirs;
        cube_grid(&cube, &theirs);
        BT_CHECK(bt_pz_grid_eq(&mine, &theirs));
    }
}

BT_TEST(nxn_oracle, quarter_one_direction_is_pinned_by_the_side_top_rows) {
    bt_pz_nxn nxn;
    bt_pz_nxn_init(&nxn, 3);
    bt_pz_nxn_state state;
    nxn_applied(&nxn, "U", &state);
    bt_pz_grid grid;
    bt_pz_nxn_facelets(&nxn, &state, &grid);
    static const size_t faces[4] = {1, 2, 3, 4};
    static const uint8_t values[4] = {2, 3, 4, 1};
    for (size_t k = 0; k < 4; k++) {
        for (size_t c = 0; c < 3; c++) {
            BT_CHECK_EQ(grid.faces[faces[k]].rows[0].cells[c], values[k]);
        }
    }
}

BT_TEST(nxn_oracle, is_solved_detects_a_turned_state) {
    bt_pz_nxn nxn;
    bt_pz_nxn_init(&nxn, 3);
    bt_pz_nxn_state solved;
    bt_pz_nxn_solved_state(&nxn, &solved);
    BT_CHECK(bt_pz_nxn_is_solved(&nxn, &solved));
    bt_pz_nxn_state turned;
    nxn_applied(&nxn, "U", &turned);
    BT_CHECK(!bt_pz_nxn_is_solved(&nxn, &turned));
}

BT_TEST(nxn_oracle, invert_replaces_a_reused_out_alg) {
    bt_pz_nxn nxn;
    bt_pz_nxn_init(&nxn, 3);
    void *alg = bt_pz_alg_create();
    nxn_parse_ok(&nxn, "R U'", alg);
    void *reused = bt_pz_alg_create();
    nxn_parse_ok(&nxn, "x", reused);
    BT_CHECK_EQ(bt_pz_alg_len(reused), (size_t)1);
    bt_pz_nxn_invert_alg(&nxn, alg, reused);
    BT_CHECK_EQ(bt_pz_alg_len(reused), (size_t)2);
    void *want = bt_pz_alg_create();
    nxn_parse_ok(&nxn, "U R'", want);
    BT_CHECK(nxn_alg_moves_eq(&nxn, reused, want));
    void *fresh = bt_pz_alg_create();
    bt_pz_nxn_invert_alg(&nxn, alg, fresh);
    BT_CHECK(nxn_alg_moves_eq(&nxn, fresh, want));
    bt_pz_alg_destroy(alg);
    bt_pz_alg_destroy(reused);
    bt_pz_alg_destroy(want);
    bt_pz_alg_destroy(fresh);
}

static void nxn_die_on_hand_built_n(void) {
    bt_pz_nxn bad = {100};
    bt_pz_engine eng = bt_pz_nxn_engine(&bad);
    bt_pz_nxn_state state;
    eng.solved_state(&eng, &state);
}

BT_TEST(nxn_oracle, engine_rejects_a_hand_built_n) {
    bt_death_expect("nxn_oracle.engine_rejects_a_hand_built_n", "n out of range",
                    nxn_die_on_hand_built_n);
}

BT_TEST(nxn_oracle, unicode_whitespace_separates_tokens) {
    bt_pz_nxn nxn;
    bt_pz_nxn_init(&nxn, 3);
    static const char *texts[] = {
        "R\xC2\x85 U",     "R\xC2\xA0 U",     "R\xE1\x9A\x80 U",
        "R\xE2\x80\x80 U", "R\xE2\x80\xA8 U", "R\xE2\x80\xAF U",
        "R\xE2\x81\x9F U", "R\xE3\x80\x80 U", "R\v U",
    };
    bt_pz_nxn_state want;
    nxn_applied(&nxn, "R U", &want);
    for (size_t i = 0; i < sizeof texts / sizeof texts[0]; i++) {
        bt_pz_error err;
        void *alg = bt_pz_alg_create();
        BT_CHECK(bt_pz_nxn_parse_alg(&nxn, texts[i], strlen(texts[i]), alg, &err));
        BT_CHECK_EQ(bt_pz_alg_len(alg), (size_t)2);
        bt_pz_alg_destroy(alg);
        bt_pz_nxn_state state;
        nxn_applied(&nxn, texts[i], &state);
        BT_CHECK(bt_pz_nxn_state_eq(&state, &want));
    }
}

BT_TEST(nxn_oracle, is_empty_reflects_the_parsed_move_count) {
    bt_pz_nxn nxn;
    bt_pz_nxn_init(&nxn, 3);
    void *alg = bt_pz_alg_create();
    nxn_parse_ok(&nxn, "U", alg);
    BT_CHECK(!bt_pz_alg_is_empty(alg));
    bt_pz_error err;
    BT_CHECK(bt_pz_nxn_parse_alg(&nxn, "", strlen(""), alg, &err));
    BT_CHECK(bt_pz_alg_is_empty(alg));
    bt_pz_alg_destroy(alg);
}
