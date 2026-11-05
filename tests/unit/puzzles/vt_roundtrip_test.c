#include <string.h>

#include "common/bt_harness.h"
#include "common/strbuf.h"
#include "puzzles/fto.h"
#include "puzzles/geometry.h"
#include "puzzles/mega.h"
#include "puzzles/nxn.h"
#include "puzzles/pyra.h"
#include "puzzles/skewb.h"
#include "puzzles/sq1.h"
#include "unit/core/death_test.h"

static void vt_check_roundtrip(const bt_pz_engine *eng, const char *text) {
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    BT_CHECK(eng->parse_alg(eng, text, strlen(text), alg, &err));
    bt_strbuf sb;
    BT_CHECK_EQ(bt_strbuf_init(&sb, 64), 0);
    eng->format_alg(eng, alg, &sb);
    void *again = bt_pz_alg_create();
    bt_pz_error err2;
    BT_CHECK(eng->parse_alg(eng, sb.data, sb.len, again, &err2));
    BT_CHECK_EQ(bt_pz_alg_len(again), bt_pz_alg_len(alg));
    for (size_t i = 0; i < bt_pz_alg_len(alg); i++) {
        BT_CHECK(memcmp(bt_pz_alg_at(eng, alg, i), bt_pz_alg_at(eng, again, i), eng->move_size) ==
                 0);
    }
    void *state_a = malloc(eng->state_size);
    void *state_b = malloc(eng->state_size);
    memset(state_a, 0, eng->state_size);
    memset(state_b, 0, eng->state_size);
    eng->solved_state(eng, state_a);
    eng->solved_state(eng, state_b);
    BT_CHECK(eng->apply_alg(eng, state_a, alg, &err));
    BT_CHECK(eng->apply_alg(eng, state_b, again, &err2));
    BT_CHECK(memcmp(state_a, state_b, eng->state_size) == 0);
    BT_CHECK(eng->is_solved(eng, state_b) == false);
    bt_pz_grid solved_grid;
    bt_pz_grid moved_grid;
    eng->facelets(eng, state_a, &moved_grid);
    eng->solved_state(eng, state_b);
    eng->facelets(eng, state_b, &solved_grid);
    BT_CHECK(!bt_pz_grid_eq(&moved_grid, &solved_grid));
    eng->facelets(eng, state_b, &moved_grid);
    BT_CHECK(bt_pz_grid_eq(&moved_grid, &solved_grid));
    free(state_a);
    free(state_b);
    bt_pz_alg_destroy(again);
    bt_pz_alg_destroy(alg);
    bt_strbuf_free(&sb);
}

static void vt_state_eq_pair(const bt_pz_engine *eng, bool (*eq)(const void *, const void *),
                            size_t state_size, const char *text) {
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    BT_CHECK(eng->parse_alg(eng, text, strlen(text), alg, &err));
    void *a = malloc(state_size);
    void *b = malloc(state_size);
    memset(a, 0, state_size);
    memset(b, 0, state_size);
    eng->solved_state(eng, a);
    eng->solved_state(eng, b);
    BT_CHECK(eq(a, b));
    BT_CHECK(eng->apply_alg(eng, b, alg, &err));
    BT_CHECK(!eq(a, b));
    free(a);
    free(b);
    bt_pz_alg_destroy(alg);
}

BT_TEST(vt, nxn_formats_reparse_and_apply_identically) {
    for (size_t n = 2; n <= 7; n++) {
        bt_pz_nxn nxn;
        BT_CHECK(bt_pz_nxn_init(&nxn, n));
        bt_pz_engine eng = bt_pz_nxn_engine(&nxn);
        vt_check_roundtrip(&eng, "R U R' F2");
        if (n >= 4) {
            vt_check_roundtrip(&eng, "2R 3L' 2F2");
        }
    }
}

BT_TEST(vt, sq1_formats_reparse_and_apply_identically) {
    bt_pz_engine eng = bt_pz_sq1_engine();
    vt_check_roundtrip(&eng, "(1,0)");
    vt_check_roundtrip(&eng, "/");
    vt_check_roundtrip(&eng, "(1,0)/(3,0)/(-1,0)");
}

BT_TEST(vt, mega_formats_reparse_and_apply_identically) {
    bt_pz_engine eng = bt_mega_engine();
    vt_check_roundtrip(&eng, "R2' BR'");
    vt_check_roundtrip(&eng, "U2 L y'");
}

BT_TEST(vt, pyra_formats_reparse_and_apply_identically) {
    bt_pz_engine eng = bt_pyra_engine();
    vt_check_roundtrip(&eng, "U L' R B");
    vt_check_roundtrip(&eng, "u b'");
}

BT_TEST(vt, skewb_formats_reparse_and_apply_identically) {
    bt_skewb skewb = bt_skewb_new();
    bt_pz_engine eng = bt_skewb_engine(&skewb);
    vt_check_roundtrip(&eng, "F L R' B");
}

BT_TEST(vt, fto_formats_reparse_and_apply_identically) {
    bt_fto fto = bt_fto_new();
    bt_pz_engine eng = bt_fto_engine(&fto);
    vt_check_roundtrip(&eng, "U BR U BR' F U F'");
}

BT_TEST(vt, state_eq_separates_solved_from_moved) {
    bt_pz_nxn nxn;
    BT_CHECK(bt_pz_nxn_init(&nxn, 3));
    bt_pz_engine eng = bt_pz_nxn_engine(&nxn);
    vt_state_eq_pair(&eng, (bool (*)(const void *, const void *))bt_pz_nxn_state_eq,
                     sizeof(bt_pz_nxn_state), "R");
    bt_pz_engine sq1 = bt_pz_sq1_engine();
    vt_state_eq_pair(&sq1, (bool (*)(const void *, const void *))bt_pz_sq1_state_eq,
                     sizeof(bt_pz_sq1_state), "(1,0)");
    bt_pz_engine mega = bt_mega_engine();
    vt_state_eq_pair(&mega, (bool (*)(const void *, const void *))bt_mega_state_eq,
                     sizeof(bt_mega_state), "R");
    bt_pz_engine pyra = bt_pyra_engine();
    vt_state_eq_pair(&pyra, (bool (*)(const void *, const void *))bt_pyra_state_eq,
                     sizeof(bt_pyra_state), "U");
    bt_skewb skewb = bt_skewb_new();
    bt_pz_engine skewb_eng = bt_skewb_engine(&skewb);
    vt_state_eq_pair(&skewb_eng, (bool (*)(const void *, const void *))bt_skewb_state_eq,
                     sizeof(bt_skewb_state), "F");
    bt_fto fto = bt_fto_new();
    bt_pz_engine fto_eng = bt_fto_engine(&fto);
    vt_state_eq_pair(&fto_eng, (bool (*)(const void *, const void *))bt_fto_state_eq,
                     sizeof(bt_fto_state), "U");
}

BT_TEST(vt, grid_eq_separates_every_shape_of_drift) {
    bt_pz_nxn nxn;
    BT_CHECK(bt_pz_nxn_init(&nxn, 3));
    bt_pz_engine eng = bt_pz_nxn_engine(&nxn);
    bt_pz_nxn_state state;
    bt_pz_nxn_solved_state(&nxn, &state);
    bt_pz_grid base;
    eng.facelets(&eng, &state, &base);
    bt_pz_grid drift = base;
    drift.faces[0].rows[0].cells[0] ^= 1;
    BT_CHECK(!bt_pz_grid_eq(&drift, &base));
    drift = base;
    drift.faces[1].rows[1].cols = 1;
    BT_CHECK(!bt_pz_grid_eq(&drift, &base));
    drift = base;
    drift.faces[2].row_count = 2;
    BT_CHECK(!bt_pz_grid_eq(&drift, &base));
    bt_pz_engine pyra = bt_pyra_engine();
    bt_pyra_state pyra_state;
    bt_pyra_solved_state(&pyra_state);
    bt_pz_grid pyra_grid;
    pyra.facelets(&pyra, &pyra_state, &pyra_grid);
    BT_CHECK(!bt_pz_grid_eq(&pyra_grid, &base));
}

BT_TEST(vt, error_helpers_cover_null_truncation_and_both_kinds) {
    bt_pz_error err;
    bt_pz_error_set(NULL, BT_PZ_ERR_PARSE, "R", 1);
    bt_pz_error_set(&err, BT_PZ_ERR_PARSE, NULL, 4);
    BT_CHECK_EQ(err.token, "");
    char long_token[200];
    memset(long_token, 'R', sizeof long_token);
    bt_pz_error_set(&err, BT_PZ_ERR_APPLY, long_token, sizeof long_token);
    BT_CHECK_EQ(strlen(err.token), (size_t)127);
    BT_CHECK_EQ(err.kind, (int)BT_PZ_ERR_APPLY);
    bt_strbuf sb;
    BT_CHECK_EQ(bt_strbuf_init(&sb, 16), 0);
    bt_pz_error_message(&err, &sb);
    char expected[160];
    memcpy(expected, "illegal move: ", 14);
    memset(expected + 14, 'R', 127);
    expected[141] = '\0';
    BT_CHECK_EQ(sb.data, expected);
    bt_pz_error_set(&err, BT_PZ_ERR_PARSE, "z", 1);
    bt_strbuf clear;
    BT_CHECK_EQ(bt_strbuf_init(&clear, 16), 0);
    bt_pz_error_message(&err, &clear);
    BT_CHECK_EQ(clear.data, "unknown token: z");
    bt_strbuf_free(&clear);
    bt_strbuf_free(&sb);
}

static void vt_death_alg_at_out_of_range(void) {
    bt_pz_engine eng = bt_pyra_engine();
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    eng.parse_alg(&eng, "U", 1, alg, &err);
    (void)bt_pz_alg_at(&eng, alg, bt_pz_alg_len(alg));
    bt_pz_alg_destroy(alg);
}

static void vt_death_default_parse_move_size_over_cap(void) {
    bt_pz_engine eng = bt_pyra_engine();
    eng.move_size = 33;
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    bt_pz_alg_default_parse(&eng, "U", 1, alg, &err);
    bt_pz_alg_destroy(alg);
}

BT_TEST(vt, geometry_guards_abort_through_the_fatal_path) {
    bt_death_expect_abort("vt.alg_at_rejects_an_out_of_range_index", "index out of range",
                          vt_death_alg_at_out_of_range);
    bt_death_expect_abort("vt.default_parse_rejects_an_oversized_move", "move size over the scratch cap",
                          vt_death_default_parse_move_size_over_cap);
}
