#include "common/bt_harness.h"
#include "common/strbuf.h"
#include "puzzles/geometry.h"
#include "puzzles/sq1.h"
#include "puzzles/tier1.h"
#include "unit/core/common.h"

#include <string.h>

static const uint8_t SQ1_SOLVED_TOP[12] = {0, 0, 1, 2, 2, 3, 4, 4, 5, 6, 6, 7};
static const uint8_t SQ1_SOLVED_BOTTOM[12] = {8, 9, 9, 10, 11, 11, 12, 13, 13, 14, 15, 15};
static const uint8_t SQ1_KITE_SHAPE[8] = {1, 1, 2, 1, 2, 2, 1, 2};

static bt_pz_engine sq1_engine(void) {
    return bt_pz_sq1_engine();
}

static void sq1_push(void *alg, bt_pz_sq1_move mv) {
    bt_pz_engine eng = sq1_engine();
    bt_pz_alg_push(&eng, alg, &mv);
}

static void sq1_parse_ok(const char *text, void *alg) {
    bt_pz_error err;
    if (!bt_pz_sq1_parse_alg(text, strlen(text), alg, &err)) {
        BT_ABORT("parse %s failed: %s", text, err.token);
    }
}

static bool sq1_parse_fails(const char *text, const char *want_token) {
    void *alg = bt_pz_alg_create();
    bt_pz_error err;
    bool ok = bt_pz_sq1_parse_alg(text, strlen(text), alg, &err);
    bt_pz_alg_destroy(alg);
    return (bool)(!ok && strcmp(err.token, want_token) == 0);
}

static void sq1_apply_ok(bt_pz_sq1_state *state, const void *alg) {
    bt_pz_error err;
    if (!bt_pz_sq1_apply_alg(state, alg, &err)) {
        BT_ABORT("apply failed: %s", err.token);
    }
}

static void sq1_format(const void *alg, char *out, size_t out_size) {
    bt_strbuf sb;
    bt_strbuf_init(&sb, 64);
    bt_pz_sq1_format_alg(alg, &sb);
    snprintf(out, out_size, "%s", sb.data);
    bt_strbuf_free(&sb);
}

static bool sq1_alg_moves_eq(const void *a, const void *b) {
    bt_pz_engine eng = sq1_engine();
    if (bt_pz_alg_len(a) != bt_pz_alg_len(b)) {
        return false;
    }
    for (size_t i = 0; i < bt_pz_alg_len(a); i++) {
        const bt_pz_sq1_move *ma = bt_pz_alg_at(&eng, a, i);
        const bt_pz_sq1_move *mb = bt_pz_alg_at(&eng, b, i);
        if (!bt_pz_sq1_move_eq(*ma, *mb)) {
            return false;
        }
    }
    return true;
}

static size_t sq1_shape(const bt_pz_sq1_state *state, size_t layer, uint8_t *out) {
    return bt_pz_sq1_canonical_shape(state, layer, out, BT_PZ_SQ1_UNITS);
}

static bool shape_eq(const uint8_t *a, size_t alen, const uint8_t *b, size_t blen) {
    if (alen != blen) {
        return false;
    }
    return memcmp(a, b, alen) == 0;
}

static bool widths_less(const uint8_t *a, const uint8_t *b, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (a[i] != b[i]) {
            return a[i] < b[i];
        }
    }
    return false;
}

static size_t min_rotation(const uint8_t *w, size_t len, uint8_t *out) {
    uint8_t best[BT_PZ_SQ1_UNITS];
    memcpy(best, w, len);
    for (size_t rotation = 0; rotation < len; rotation++) {
        uint8_t candidate[BT_PZ_SQ1_UNITS];
        for (size_t unit = 0; unit < len; unit++) {
            candidate[unit] = w[(rotation + unit) % len];
        }
        if (widths_less(candidate, best, len)) {
            memcpy(best, candidate, len);
        }
    }
    memcpy(out, best, len);
    return len;
}

static size_t mirrored(const uint8_t *w, size_t len, uint8_t *out) {
    uint8_t reversed[BT_PZ_SQ1_UNITS];
    for (size_t i = 0; i < len; i++) {
        reversed[i] = w[len - 1 - i];
    }
    return min_rotation(reversed, len, out);
}

static size_t famkey(const uint8_t *w, size_t len, uint8_t *out) {
    uint8_t mirror[BT_PZ_SQ1_UNITS];
    size_t mirror_len = mirrored(w, len, mirror);
    if (widths_less(mirror, w, len)) {
        memcpy(out, mirror, mirror_len);
        return mirror_len;
    }
    memcpy(out, w, len);
    return len;
}

BT_TEST(sq1_oracle, solved_state_matches_the_ported_layout) {
    bt_pz_sq1_state state;
    bt_pz_sq1_solved_state(&state);
    BT_CHECK(memcmp(state.units, SQ1_SOLVED_TOP, sizeof SQ1_SOLVED_TOP) == 0);
    BT_CHECK(memcmp(state.units + 12, SQ1_SOLVED_BOTTOM, sizeof SQ1_SOLVED_BOTTOM) == 0);
    BT_CHECK(!state.slice_flipped);
    BT_CHECK(bt_pz_sq1_is_solved(&state));
    BT_CHECK(bt_pz_sq1_can_slash(&state));
    bt_pz_grid grid;
    bt_pz_sq1_facelets(&state, &grid);
    BT_CHECK_EQ(grid.face_count, (size_t)2);
    BT_CHECK_EQ(grid.faces[0].row_count, (size_t)1);
    BT_CHECK_EQ(grid.faces[0].rows[0].cols, (size_t)12);
    BT_CHECK(memcmp(grid.faces[0].rows[0].cells, SQ1_SOLVED_TOP, sizeof SQ1_SOLVED_TOP) == 0);
    BT_CHECK(memcmp(grid.faces[1].rows[0].cells, SQ1_SOLVED_BOTTOM, sizeof SQ1_SOLVED_BOTTOM) == 0);
    uint8_t shape[BT_PZ_SQ1_UNITS];
    size_t len = sq1_shape(&state, 0, shape);
    BT_CHECK(shape_eq(shape, len, BT_PZ_SQ1_SQUARE_SHAPE, BT_PZ_SQ1_PIECES_PER_LAYER));
    len = sq1_shape(&state, 1, shape);
    BT_CHECK(shape_eq(shape, len, BT_PZ_SQ1_SQUARE_SHAPE, BT_PZ_SQ1_PIECES_PER_LAYER));
}

BT_TEST(sq1_oracle, slash_swaps_ring_halves_and_flips_the_slice) {
    static const uint8_t expected_top[12] = {0, 0, 1, 2, 2, 3, 12, 13, 13, 14, 15, 15};
    static const uint8_t expected_bottom[12] = {8, 9, 9, 10, 11, 11, 4, 4, 5, 6, 6, 7};
    bt_pz_sq1_state state;
    bt_pz_sq1_solved_state(&state);
    void *alg = bt_pz_alg_create();
    sq1_parse_ok("/", alg);
    sq1_apply_ok(&state, alg);
    BT_CHECK(memcmp(state.units, expected_top, sizeof expected_top) == 0);
    BT_CHECK(memcmp(state.units + 12, expected_bottom, sizeof expected_bottom) == 0);
    BT_CHECK(state.slice_flipped);
    BT_CHECK(!bt_pz_sq1_is_solved(&state));
    sq1_apply_ok(&state, alg);
    BT_CHECK(bt_pz_sq1_is_solved(&state));
    bt_pz_alg_destroy(alg);
}

BT_TEST(sq1_oracle, slash_legality_is_state_aware) {
    bt_pz_engine eng = sq1_engine();
    bt_pz_sq1_state aligned;
    bt_pz_sq1_solved_state(&aligned);
    void *alg = bt_pz_alg_create();
    sq1_parse_ok("(1,0) /", alg);
    bt_pz_error err;
    BT_CHECK(bt_pz_sq1_apply_alg(&aligned, alg, &err));
    bt_pz_alg_destroy(alg);
    static const char *bad[] = {"(2,0) /", "(0,1) /"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        bt_pz_sq1_state misaligned;
        bt_pz_sq1_solved_state(&misaligned);
        void *row = bt_pz_alg_create();
        sq1_parse_ok(bad[i], row);
        BT_CHECK(!bt_pz_sq1_apply_alg(&misaligned, row, &err));
        BT_CHECK_EQ(err.kind, (int)BT_PZ_ERR_APPLY);
        BT_CHECK(strcmp(err.token, "/") == 0);
        bt_pz_alg_destroy(row);
    }
    bt_tier1_error tier1_err;
    BT_CHECK_EQ(bt_verify_tier1(&eng, "(2,0) /", &tier1_err), (int)BT_TIER1_APPLY);
    BT_CHECK_EQ(tier1_err.cause.kind, (int)BT_PZ_ERR_APPLY);
    BT_CHECK(strcmp(tier1_err.cause.token, "/") == 0);
}

BT_TEST(sq1_oracle, twist_rotation_direction_follows_the_port) {
    static const uint8_t rotated_top[12] = {7, 0, 0, 1, 2, 2, 3, 4, 4, 5, 6, 6};
    static const uint8_t rotated_bottom[12] = {15, 8, 9, 9, 10, 11, 11, 12, 13, 13, 14, 15};
    bt_pz_sq1_state top_only;
    bt_pz_sq1_solved_state(&top_only);
    void *alg = bt_pz_alg_create();
    sq1_parse_ok("(1,0)", alg);
    sq1_apply_ok(&top_only, alg);
    bt_pz_alg_destroy(alg);
    BT_CHECK(memcmp(top_only.units, rotated_top, sizeof rotated_top) == 0);
    BT_CHECK(memcmp(top_only.units + 12, SQ1_SOLVED_BOTTOM, sizeof SQ1_SOLVED_BOTTOM) == 0);
    bt_pz_sq1_state bottom_only;
    bt_pz_sq1_solved_state(&bottom_only);
    alg = bt_pz_alg_create();
    sq1_parse_ok("(0,1)", alg);
    sq1_apply_ok(&bottom_only, alg);
    bt_pz_alg_destroy(alg);
    BT_CHECK(memcmp(bottom_only.units + 12, rotated_bottom, sizeof rotated_bottom) == 0);
    bt_pz_grid grid;
    bt_pz_sq1_facelets(&bottom_only, &grid);
    BT_CHECK(memcmp(grid.faces[1].rows[0].cells, rotated_bottom, sizeof rotated_bottom) == 0);
}

BT_TEST(sq1_oracle, twist_enumeration_covers_exactly_143_moves) {
    bt_pz_engine eng = sq1_engine();
    BT_CHECK_EQ(BT_PZ_SQ1_TWISTS, 143);
    size_t count = 0;
    for (int32_t top = -5; top <= 6; top++) {
        for (int32_t bottom = -5; bottom <= 6; bottom++) {
            if (top == 0 && bottom == 0) {
                continue;
            }
            count++;
            char parenthesized[16];
            snprintf(parenthesized, sizeof parenthesized, "(%d,%d)", top, bottom);
            char bare[16];
            snprintf(bare, sizeof bare, "%d,%d", top, bottom);
            bt_pz_sq1_move parsed;
            bt_pz_error err;
            BT_CHECK(bt_pz_sq1_parse_move(parenthesized, strlen(parenthesized), &parsed, &err));
            BT_CHECK(bt_pz_sq1_move_eq(parsed, bt_pz_sq1_move_twist(top, bottom)));
            bt_pz_sq1_move bare_parsed;
            BT_CHECK(bt_pz_sq1_parse_move(bare, strlen(bare), &bare_parsed, &err));
            BT_CHECK(bt_pz_sq1_move_eq(bare_parsed, parsed));
            bt_tier1_error tier1_err;
            BT_CHECK_EQ(bt_verify_tier1(&eng, bare, &tier1_err), (int)BT_TIER1_OK);
        }
    }
    BT_CHECK_EQ(count, (size_t)143);
}

BT_TEST(sq1_oracle, grammar_accepts_the_pinned_token_forms) {
    static const char *texts[] = {
        "",        "/",     "//",       "(1,0)",          "1,0",          "  1,0   ",
        "(-5,-5)", "(6,6)", "6,-3",     "(1,0) / (-1,0)", "(1,0)/(-1,0)", "(1,0)/(3,0)/(-1,0)",
        "1,0/",    "/3,3/", "/ -1,2 /",
    };
    size_t case_count = sizeof texts / sizeof texts[0];
    bt_pz_sq1_move want[15][5];
    size_t want_len[15];
    memset(want, 0, sizeof want);
    memset(want_len, 0, sizeof want_len);
    want[1][0] = bt_pz_sq1_move_slash();
    want_len[1] = 1;
    want[2][0] = bt_pz_sq1_move_slash();
    want[2][1] = bt_pz_sq1_move_slash();
    want_len[2] = 2;
    want[3][0] = bt_pz_sq1_move_twist(1, 0);
    want_len[3] = 1;
    want[4][0] = bt_pz_sq1_move_twist(1, 0);
    want_len[4] = 1;
    want[5][0] = bt_pz_sq1_move_twist(1, 0);
    want_len[5] = 1;
    want[6][0] = bt_pz_sq1_move_twist(-5, -5);
    want_len[6] = 1;
    want[7][0] = bt_pz_sq1_move_twist(6, 6);
    want_len[7] = 1;
    want[8][0] = bt_pz_sq1_move_twist(6, -3);
    want_len[8] = 1;
    want[9][0] = bt_pz_sq1_move_twist(1, 0);
    want[9][1] = bt_pz_sq1_move_slash();
    want[9][2] = bt_pz_sq1_move_twist(-1, 0);
    want_len[9] = 3;
    want[10][0] = bt_pz_sq1_move_twist(1, 0);
    want[10][1] = bt_pz_sq1_move_slash();
    want[10][2] = bt_pz_sq1_move_twist(-1, 0);
    want_len[10] = 3;
    want[11][0] = bt_pz_sq1_move_twist(1, 0);
    want[11][1] = bt_pz_sq1_move_slash();
    want[11][2] = bt_pz_sq1_move_twist(3, 0);
    want[11][3] = bt_pz_sq1_move_slash();
    want[11][4] = bt_pz_sq1_move_twist(-1, 0);
    want_len[11] = 5;
    want[12][0] = bt_pz_sq1_move_twist(1, 0);
    want[12][1] = bt_pz_sq1_move_slash();
    want_len[12] = 2;
    want[13][0] = bt_pz_sq1_move_slash();
    want[13][1] = bt_pz_sq1_move_twist(3, 3);
    want[13][2] = bt_pz_sq1_move_slash();
    want_len[13] = 3;
    want[14][0] = bt_pz_sq1_move_slash();
    want[14][1] = bt_pz_sq1_move_twist(-1, 2);
    want[14][2] = bt_pz_sq1_move_slash();
    want_len[14] = 3;
    for (size_t i = 0; i < case_count; i++) {
        void *alg = bt_pz_alg_create();
        sq1_parse_ok(texts[i], alg);
        void *expected = bt_pz_alg_create();
        for (size_t k = 0; k < want_len[i]; k++) {
            sq1_push(expected, want[i][k]);
        }
        BT_CHECK(sq1_alg_moves_eq(alg, expected));
        bt_pz_alg_destroy(alg);
        bt_pz_alg_destroy(expected);
    }
}

BT_TEST(sq1_oracle, grammar_rejects_pack_hazards_and_garbage) {
    static const struct {
        const char *text;
        const char *token;
    } cases[] = {
        {"(U)", "(U)"},
        {"(D)", "(D)"},
        {"(U2)", "(U2)"},
        {"(D2)", "(D2)"},
        {"(D')", "(D')"},
        {"(U D)", "(U D)"},
        {"(U2 D) (1,0)", "(U2 D)"},
        {"(U D)(0,-1)", "(U D)"},
        {"*", "*"},
        {"(1,0) / *", "*"},
        {"/ *", "*"},
        {"(0,0)", "(0,0)"},
        {"(7,0)", "(7,0)"},
        {"(1,7)", "(1,7)"},
        {"(-6,0)", "(-6,0)"},
        {"(05,0)", "(05,0)"},
        {"(-0,1)", "(-0,1)"},
        {"(1,0", "(1,0"},
        {"(1,2,3)", "(1,2,3)"},
        {"( 1,0 )", "( 1,0 )"},
        {"( )", "( )"},
        {"(1,0)x", "x"},
        {"x,1", "x,1"},
        {"1", "1"},
        {"1,0,2", "1,0,2"},
        {"1,0)(2,3)", "1,0)(2,3)"},
        {"1 0", "1"},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        BT_CHECK(sq1_parse_fails(cases[i].text, cases[i].token));
    }
}

BT_TEST(sq1_oracle, parse_move_takes_exactly_one_token) {
    bt_pz_sq1_move mv;
    bt_pz_error err;
    BT_CHECK(bt_pz_sq1_parse_move("/", strlen("/"), &mv, &err));
    BT_CHECK(mv.slash);
    BT_CHECK(bt_pz_sq1_parse_move("1,0", strlen("1,0"), &mv, &err));
    BT_CHECK(bt_pz_sq1_move_eq(mv, bt_pz_sq1_move_twist(1, 0)));
    BT_CHECK(bt_pz_sq1_parse_move("(1,0)", strlen("(1,0)"), &mv, &err));
    BT_CHECK(bt_pz_sq1_move_eq(mv, bt_pz_sq1_move_twist(1, 0)));
    static const char *bad[] = {"", "(1,0)/", "1,0 /", "(1,0) (2,0)", "(U)"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        BT_CHECK(!bt_pz_sq1_parse_move(bad[i], strlen(bad[i]), &mv, &err));
        BT_CHECK(strcmp(err.token, bad[i]) == 0);
    }
}

BT_TEST(sq1_oracle, format_is_canonical_and_inversion_negates_twists) {
    char text[256];
    void *fused = bt_pz_alg_create();
    sq1_parse_ok("(1,0)/(-1,0)", fused);
    sq1_format(fused, text, sizeof text);
    BT_CHECK(strcmp(text, "(1,0) / (-1,0)") == 0);
    void *bare = bt_pz_alg_create();
    sq1_parse_ok("1,0 / 3,3", bare);
    sq1_format(bare, text, sizeof text);
    BT_CHECK(strcmp(text, "(1,0) / (3,3)") == 0);
    void *slashes = bt_pz_alg_create();
    sq1_parse_ok("/3,3/", slashes);
    sq1_format(slashes, text, sizeof text);
    BT_CHECK(strcmp(text, "/ (3,3) /") == 0);
    void *empty = bt_pz_alg_create();
    sq1_parse_ok("", empty);
    sq1_format(empty, text, sizeof text);
    BT_CHECK(strcmp(text, "") == 0);
    void *alg = bt_pz_alg_create();
    sq1_parse_ok("(3,0) /", alg);
    void *inverted = bt_pz_alg_create();
    bt_pz_sq1_invert_alg(alg, inverted);
    void *want = bt_pz_alg_create();
    sq1_push(want, bt_pz_sq1_move_slash());
    sq1_push(want, bt_pz_sq1_move_twist(-3, 0));
    BT_CHECK(sq1_alg_moves_eq(inverted, want));
    sq1_format(inverted, text, sizeof text);
    BT_CHECK(strcmp(text, "/ (-3,0)") == 0);
    void *slash_alg = bt_pz_alg_create();
    sq1_parse_ok("/", slash_alg);
    void *slash_inverted = bt_pz_alg_create();
    bt_pz_sq1_invert_alg(slash_alg, slash_inverted);
    BT_CHECK_EQ(bt_pz_alg_len(slash_inverted), (size_t)1);
    bt_pz_sq1_move only;
    bt_pz_engine eng = sq1_engine();
    const bt_pz_sq1_move *first = bt_pz_alg_at(&eng, slash_inverted, 0);
    only = *first;
    BT_CHECK(only.slash);
    bt_pz_alg_destroy(fused);
    bt_pz_alg_destroy(bare);
    bt_pz_alg_destroy(slashes);
    bt_pz_alg_destroy(empty);
    bt_pz_alg_destroy(alg);
    bt_pz_alg_destroy(inverted);
    bt_pz_alg_destroy(want);
    bt_pz_alg_destroy(slash_alg);
    bt_pz_alg_destroy(slash_inverted);
}

BT_TEST(sq1_oracle, tier1_round_trips_extra_algs_outside_the_pack) {
    bt_pz_engine eng = sq1_engine();
    static const char *texts[] = {"/", "(6,6)", "(1,0)/(3,0)/(-1,0)", "/ 3,3 / 3,3 /", "5,-5"};
    bt_tier1_error err;
    for (size_t i = 0; i < sizeof texts / sizeof texts[0]; i++) {
        BT_CHECK_EQ(bt_verify_tier1(&eng, texts[i], &err), (int)BT_TIER1_OK);
    }
}

BT_TEST(sq1_oracle, forward_from_solved_can_be_illegal_while_the_start_state_round_trip_holds) {
    bt_pz_sq1_state forward;
    bt_pz_sq1_solved_state(&forward);
    void *alg = bt_pz_alg_create();
    sq1_parse_ok("/ 0,1 / 0,3 /", alg);
    bt_pz_error err;
    BT_CHECK(!bt_pz_sq1_apply_alg(&forward, alg, &err));
    BT_CHECK_EQ(err.kind, (int)BT_PZ_ERR_APPLY);
    BT_CHECK(strcmp(err.token, "/") == 0);
    bt_pz_sq1_state through_start;
    bt_pz_sq1_solved_state(&through_start);
    void *inverted = bt_pz_alg_create();
    bt_pz_sq1_invert_alg(alg, inverted);
    sq1_apply_ok(&through_start, inverted);
    uint8_t shape[BT_PZ_SQ1_UNITS];
    size_t len = sq1_shape(&through_start, 0, shape);
    BT_CHECK(shape_eq(shape, len, BT_PZ_SQ1_SQUARE_SHAPE, BT_PZ_SQ1_PIECES_PER_LAYER));
    sq1_apply_ok(&through_start, alg);
    BT_CHECK(bt_pz_sq1_is_solved(&through_start));
    bt_pz_alg_destroy(alg);
    bt_pz_alg_destroy(inverted);
}

BT_TEST(sq1_oracle, cs001_inverse_yields_kite_kite) {
    bt_pz_sq1_state state;
    bt_pz_sq1_solved_state(&state);
    void *alg = bt_pz_alg_create();
    sq1_parse_ok("/", alg);
    void *inverted = bt_pz_alg_create();
    bt_pz_sq1_invert_alg(alg, inverted);
    sq1_apply_ok(&state, inverted);
    uint8_t top[BT_PZ_SQ1_UNITS];
    uint8_t bottom[BT_PZ_SQ1_UNITS];
    size_t top_len = sq1_shape(&state, 0, top);
    size_t bottom_len = sq1_shape(&state, 1, bottom);
    BT_CHECK(shape_eq(top, top_len, SQ1_KITE_SHAPE, sizeof SQ1_KITE_SHAPE));
    BT_CHECK(shape_eq(bottom, bottom_len, SQ1_KITE_SHAPE, sizeof SQ1_KITE_SHAPE));
    bt_pz_alg_destroy(alg);
    bt_pz_alg_destroy(inverted);
}

typedef struct {
    char key[12];
    char alg[128];
    char names[2][24];
} sq1_case_row;

typedef struct {
    char name[24];
    uint8_t key[BT_PZ_SQ1_UNITS];
    size_t len;
    bool set;
} fam_entry;

static const fam_entry *fam_find(const fam_entry *fams, size_t fam_count, const char *name) {
    for (size_t i = 0; i < fam_count; i++) {
        if (strcmp(fams[i].name, name) == 0) {
            return &fams[i];
        }
    }
    return NULL;
}

static size_t fam_set(fam_entry *fams, size_t fam_count, const char *name, const uint8_t *key,
                      size_t len) {
    if (fam_count >= 64) {
        BT_ABORT("family table full");
    }
    snprintf(fams[fam_count].name, sizeof fams[fam_count].name, "%s", name);
    memcpy(fams[fam_count].key, key, len);
    fams[fam_count].len = len;
    fams[fam_count].set = true;
    return fam_count + 1;
}

static size_t sq1_load_cubeshape_rows(sq1_case_row *rows, size_t cap) {
    tc_lines lines;
    tc_sheet_lines("packs/sq1_cubeshape.tsv", &lines);
    size_t count = 0;
    for (size_t at = 1; at < lines.count; at++) {
        if (lines.lines[at][0] == '\0') {
            continue;
        }
        char copy[TC_LINE_MAX];
        snprintf(copy, sizeof copy, "%s", lines.lines[at]);
        char *cols[8];
        size_t nfields = tc_split(copy, cols, 8);
        if (nfields < 3) {
            BT_ABORT("row needs 3 columns: %s", lines.lines[at]);
        }
        const char *marker = "\"case\":\"";
        char *start = strstr(cols[2], marker);
        if (start == NULL) {
            BT_ABORT("case marker missing: %s", lines.lines[at]);
        }
        start += strlen(marker);
        char *end = strchr(start, '"');
        if (end == NULL) {
            BT_ABORT("case closing quote missing: %s", lines.lines[at]);
        }
        char name[48];
        snprintf(name, (size_t)(end - start + 1), "%s", start);
        char *names[2];
        size_t name_count = tc_split_on(name, '/', names, 2);
        if (name_count != 2) {
            BT_ABORT("case name splits in two: %s", cols[2]);
        }
        if (count >= cap) {
            BT_ABORT("rows exceed %zu", cap);
        }
        snprintf(rows[count].key, sizeof rows[count].key, "%s", cols[0]);
        snprintf(rows[count].alg, sizeof rows[count].alg, "%s", cols[1]);
        for (size_t k = 0; k < 2; k++) {
            char *trimmed = names[k];
            while (*trimmed == ' ') {
                trimmed++;
            }
            size_t tlen = strlen(trimmed);
            while (tlen > 0 && trimmed[tlen - 1] == ' ') {
                trimmed[--tlen] = '\0';
            }
            snprintf(rows[count].names[k], sizeof rows[count].names[k], "%s", trimmed);
        }
        count++;
    }
    return count;
}

BT_TEST(sq1_oracle, cubeshape_pack_oracle_sweeps_all_169_rows) {
    static sq1_case_row rows[170];
    size_t row_count = sq1_load_cubeshape_rows(rows, 170);
    BT_CHECK_EQ(row_count, (size_t)169);
    static fam_entry fams[64];
    size_t fam_count = 0;
    char swapped[16][12];
    size_t swapped_count = 0;
    char defective[16][12];
    size_t defective_count = 0;
    for (size_t i = 0; i < row_count; i++) {
        const sq1_case_row *row = &rows[i];
        void *alg = bt_pz_alg_create();
        sq1_parse_ok(row->alg, alg);
        BT_CHECK(!bt_pz_alg_is_empty(alg));
        void *inverted = bt_pz_alg_create();
        bt_pz_sq1_invert_alg(alg, inverted);
        bt_pz_sq1_state state;
        bt_pz_sq1_solved_state(&state);
        bt_pz_error err;
        if (!bt_pz_sq1_apply_alg(&state, inverted, &err)) {
            BT_CHECK_EQ(err.kind, (int)BT_PZ_ERR_APPLY);
            BT_CHECK(strcmp(err.token, "/") == 0);
            snprintf(defective[defective_count], sizeof defective[defective_count], "%s", row->key);
            defective_count++;
            bt_pz_alg_destroy(alg);
            bt_pz_alg_destroy(inverted);
            continue;
        }
        uint8_t shapes[2][BT_PZ_SQ1_UNITS];
        size_t lens[2];
        for (size_t layer = 0; layer < 2; layer++) {
            lens[layer] = sq1_shape(&state, layer, shapes[layer]);
            uint32_t sum = 0;
            for (size_t k = 0; k < lens[layer]; k++) {
                sum += shapes[layer][k];
            }
            BT_CHECK_EQ(sum, (uint32_t)12);
        }
        sq1_apply_ok(&state, alg);
        BT_CHECK(bt_pz_sq1_is_solved(&state));
        size_t corners = 0;
        for (size_t layer = 0; layer < 2; layer++) {
            for (size_t k = 0; k < lens[layer]; k++) {
                if (shapes[layer][k] == 2) {
                    corners++;
                }
            }
        }
        BT_CHECK_EQ(corners, (size_t)8);
        uint8_t observed[2][BT_PZ_SQ1_UNITS];
        size_t observed_lens[2];
        for (size_t layer = 0; layer < 2; layer++) {
            observed_lens[layer] = famkey(shapes[layer], lens[layer], observed[layer]);
        }
        const fam_entry *known[2];
        for (size_t layer = 0; layer < 2; layer++) {
            known[layer] = fam_find(fams, fam_count, row->names[layer]);
        }
        bool direct = true;
        bool crossed = true;
        for (size_t layer = 0; layer < 2; layer++) {
            if (known[layer] != NULL) {
                if (!shape_eq(known[layer]->key, known[layer]->len, observed[layer],
                              observed_lens[layer])) {
                    direct = false;
                }
                if (!shape_eq(known[layer]->key, known[layer]->len, observed[1 - layer],
                              observed_lens[1 - layer])) {
                    crossed = false;
                }
            }
        }
        BT_CHECK(direct || crossed);
        if (direct) {
            for (size_t layer = 0; layer < 2; layer++) {
                if (fam_find(fams, fam_count, row->names[layer]) == NULL) {
                    fam_count = fam_set(fams, fam_count, row->names[layer], observed[layer],
                                        observed_lens[layer]);
                }
            }
        } else {
            snprintf(swapped[swapped_count], sizeof swapped[swapped_count], "%s", row->key);
            swapped_count++;
            for (size_t layer = 0; layer < 2; layer++) {
                if (fam_find(fams, fam_count, row->names[layer]) == NULL) {
                    fam_count = fam_set(fams, fam_count, row->names[layer], observed[1 - layer],
                                        observed_lens[1 - layer]);
                }
            }
        }
        if (strcmp(row->names[0], row->names[1]) == 0) {
            BT_CHECK(shape_eq(shapes[0], lens[0], shapes[1], lens[1]));
        }
        bt_pz_alg_destroy(alg);
        bt_pz_alg_destroy(inverted);
    }
    BT_CHECK_EQ(defective_count, (size_t)1);
    BT_CHECK(strcmp(defective[0], "cs.119") == 0);
    static const char *want_swapped[8] = {
        "cs.075", "cs.078", "cs.084", "cs.089", "cs.097", "cs.109", "cs.118", "cs.129",
    };
    BT_CHECK_EQ(swapped_count, (size_t)8);
    for (size_t i = 0; i < 8; i++) {
        BT_CHECK(strcmp(swapped[i], want_swapped[i]) == 0);
    }
    const fam_entry *square = fam_find(fams, fam_count, "Square");
    BT_CHECK(square != NULL);
    BT_CHECK(
        shape_eq(square->key, square->len, BT_PZ_SQ1_SQUARE_SHAPE, BT_PZ_SQ1_PIECES_PER_LAYER));
    const fam_entry *kite = fam_find(fams, fam_count, "Kite");
    BT_CHECK(kite != NULL);
    BT_CHECK(shape_eq(kite->key, kite->len, SQ1_KITE_SHAPE, sizeof SQ1_KITE_SHAPE));
    const fam_entry *star = fam_find(fams, fam_count, "Star");
    BT_CHECK(star != NULL);
    static const uint8_t star_shape[6] = {2, 2, 2, 2, 2, 2};
    BT_CHECK(shape_eq(star->key, star->len, star_shape, sizeof star_shape));
    const fam_entry *edges = fam_find(fams, fam_count, "8 Edges");
    BT_CHECK(edges != NULL);
    static const uint8_t edges_shape[10] = {1, 1, 1, 1, 1, 1, 1, 1, 2, 2};
    BT_CHECK(shape_eq(edges->key, edges->len, edges_shape, sizeof edges_shape));
    static const char *pairs[5][2] = {
        {"Left Fist", "Right Fist"}, {"Left Pawn", "Right Pawn"},   {"5-1 Left", "5-1 Right"},
        {"4-2 Left", "4-2 Right"},   {"3-2-1 Left", "3-2-1 Right"},
    };
    for (size_t i = 0; i < 5; i++) {
        const fam_entry *left = fam_find(fams, fam_count, pairs[i][0]);
        const fam_entry *right = fam_find(fams, fam_count, pairs[i][1]);
        BT_CHECK(left != NULL && right != NULL);
        BT_CHECK(shape_eq(left->key, left->len, right->key, right->len));
    }
}

BT_TEST(sq1_oracle, inversion_stays_inside_the_parse_domain) {
    void *alg = bt_pz_alg_create();
    sq1_push(alg, bt_pz_sq1_move_twist(0, 6));
    void *inverted = bt_pz_alg_create();
    bt_pz_sq1_invert_alg(alg, inverted);
    void *want = bt_pz_alg_create();
    sq1_push(want, bt_pz_sq1_move_twist(0, 6));
    BT_CHECK(sq1_alg_moves_eq(inverted, want));
    bt_pz_alg_destroy(alg);
    bt_pz_alg_destroy(inverted);
    bt_pz_alg_destroy(want);
    for (int32_t top = -5; top <= 6; top++) {
        for (int32_t bottom = -5; bottom <= 6; bottom++) {
            if (top == 0 && bottom == 0) {
                continue;
            }
            void *single = bt_pz_alg_create();
            sq1_push(single, bt_pz_sq1_move_twist(top, bottom));
            void *inverted_single = bt_pz_alg_create();
            bt_pz_sq1_invert_alg(single, inverted_single);
            char text[16];
            sq1_format(inverted_single, text, sizeof text);
            void *reparsed = bt_pz_alg_create();
            sq1_parse_ok(text, reparsed);
            BT_CHECK(sq1_alg_moves_eq(reparsed, inverted_single));
            bt_pz_alg_destroy(single);
            bt_pz_alg_destroy(inverted_single);
            bt_pz_alg_destroy(reparsed);
        }
    }
}

BT_TEST(sq1_oracle, canonical_shape_merges_the_wrapped_run_and_pins_the_minimum_rotation) {
    static const uint8_t wrapped[12] = {1, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1};
    static const uint8_t want[10] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 3};
    bt_pz_sq1_state state;
    bt_pz_sq1_solved_state(&state);
    memcpy(state.units, wrapped, sizeof wrapped);
    uint8_t shape[BT_PZ_SQ1_UNITS];
    size_t len = sq1_shape(&state, 0, shape);
    BT_CHECK(shape_eq(shape, len, want, sizeof want));
}

BT_TEST(sq1_oracle, canonical_shape_keeps_a_single_run_unmerged) {
    bt_pz_sq1_state state;
    bt_pz_sq1_solved_state(&state);
    memset(state.units, 1, 12);
    uint8_t shape[BT_PZ_SQ1_UNITS];
    size_t len = sq1_shape(&state, 0, shape);
    BT_CHECK_EQ(len, (size_t)1);
    BT_CHECK_EQ(shape[0], (uint8_t)12);
}

BT_TEST(sq1_oracle, invert_replaces_a_reused_out_alg) {
    void *alg = bt_pz_alg_create();
    sq1_parse_ok("(1,0) /", alg);
    void *reused = bt_pz_alg_create();
    sq1_parse_ok("3,3", reused);
    BT_CHECK_EQ(bt_pz_alg_len(reused), (size_t)1);
    bt_pz_sq1_invert_alg(alg, reused);
    BT_CHECK_EQ(bt_pz_alg_len(reused), (size_t)2);
    void *want = bt_pz_alg_create();
    sq1_push(want, bt_pz_sq1_move_slash());
    sq1_push(want, bt_pz_sq1_move_twist(-1, 0));
    BT_CHECK(sq1_alg_moves_eq(reused, want));
    void *fresh = bt_pz_alg_create();
    bt_pz_sq1_invert_alg(alg, fresh);
    BT_CHECK(sq1_alg_moves_eq(fresh, want));
    bt_pz_alg_destroy(alg);
    bt_pz_alg_destroy(reused);
    bt_pz_alg_destroy(want);
    bt_pz_alg_destroy(fresh);
}

BT_TEST(sq1_oracle, vertical_tab_is_not_a_separator) {
    BT_CHECK(sq1_parse_fails("1,0\v", "1,0\v"));
    BT_CHECK(sq1_parse_fails("/\v/", "\v/"));
    BT_CHECK(sq1_parse_fails("1,0\v2,3", "1,0\v2,3"));
}

BT_TEST(sq1_oracle, parse_rejects_a_two_digit_turn_out_of_the_engine_domain) {
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    BT_CHECK(!bt_pz_sq1_parse_alg("(12,3)", strlen("(12,3)"), alg, &err));
    bt_pz_alg_destroy(alg);
    bt_strbuf sb;
    bt_strbuf_init(&sb, 32);
    bt_pz_error_message(&err, &sb);
    BT_CHECK(strstr(sb.data, "unknown token") != NULL);
    bt_strbuf_free(&sb);
}
