#include "verify_test_util.h"

#include <string.h>

#include "verify/ev_cfop.h"

static void after(const char *text, bt_cube *state) {
    bt_alg alg;
    bt_alg_init(&alg);
    char token[64];
    BT_CHECK(bt_alg_parse(text, &alg, token, sizeof token));
    *state = bt_cube_solved();
    bt_cube_apply_alg(state, &alg);
    bt_alg_free(&alg);
}

BT_TEST(verify_ev_cfop, the_u_band_holds_four_corners_and_four_edges) {
    verify_bld_layout layout;
    verify_bld_layout_probe(&layout);
    uint8_t band[VERIFY_BLD_POSITION_COUNT];
    size_t band_len = verify_cfop_u_band(&layout, band, VERIFY_BLD_POSITION_COUNT);
    BT_CHECK_EQ(band_len, 20);
    bt_cube after_u;
    after("U", &after_u);
    BT_CHECK(verify_cfop_f2l_exact(&after_u, band, band_len));
    bt_cube after_r;
    after("R", &after_r);
    BT_CHECK(!verify_cfop_f2l_exact(&after_r, band, band_len));
}

BT_TEST(verify_ev_cfop, regrasp_moves_rewrite_to_their_fixed_frame_moves) {
    char out[256];
    BT_CHECK_EQ(verify_cfop_rewrite_to_fixed("y L'", out, sizeof out), 0);
    BT_CHECK_EQ(out, "F'");
    BT_CHECK_EQ(verify_cfop_rewrite_to_fixed("x U'", out, sizeof out), 0);
    BT_CHECK_EQ(out, "F'");
    BT_CHECK_EQ(verify_cfop_rewrite_to_fixed("R U R'", out, sizeof out), 0);
    BT_CHECK_EQ(out, "R U R'");
    BT_CHECK(verify_cfop_rewrite_to_fixed("Q'", out, sizeof out) != 0);
}

BT_TEST(verify_ev_cfop, a_t_perm_case_state_is_a_clean_pll) {
    verify_bld_layout layout;
    verify_bld_layout_probe(&layout);
    bt_alg alg;
    bt_alg_init(&alg);
    char token[64];
    BT_CHECK(bt_alg_parse("R U R' U' R' F R2 U' R' U' R U R' F'", &alg, token, sizeof token));
    bt_cube state;
    verify_bld_case_cube(&alg, &state);
    bt_alg_free(&alg);
    verify_tier2 pll =
        verify_cfop_stage_outcome(VERIFY_CFOP_PLL, &state, &layout, "{\"case\":\"T\"}");
    BT_CHECK_EQ(pll.kind, VERIFY_TIER2_PASS);
    verify_tier2 oll =
        verify_cfop_stage_outcome(VERIFY_CFOP_OLL, &state, &layout, "{\"case\":\"T\"}");
    BT_CHECK_EQ(oll.kind, VERIFY_TIER2_FAIL);
}

BT_TEST(verify_ev_cfop, a_sune_case_state_is_a_clean_oll) {
    verify_bld_layout layout;
    verify_bld_layout_probe(&layout);
    bt_alg alg;
    bt_alg_init(&alg);
    char token[64];
    BT_CHECK(bt_alg_parse("R U R' U R U2 R'", &alg, token, sizeof token));
    bt_cube state;
    verify_bld_case_cube(&alg, &state);
    bt_alg_free(&alg);
    verify_tier2 oll =
        verify_cfop_stage_outcome(VERIFY_CFOP_OLL, &state, &layout, "{\"case\":\"Sune\"}");
    BT_CHECK_EQ(oll.kind, VERIFY_TIER2_PASS);
    verify_tier2 pll =
        verify_cfop_stage_outcome(VERIFY_CFOP_PLL, &state, &layout, "{\"case\":\"Sune\"}");
    BT_CHECK_EQ(pll.kind, VERIFY_TIER2_FAIL);
}

BT_TEST(verify_ev_cfop, a_simple_insert_case_keeps_the_cross_and_displaces_the_slot) {
    verify_bld_layout layout;
    verify_bld_layout_probe(&layout);
    bt_alg alg;
    bt_alg_init(&alg);
    char token[64];
    BT_CHECK(bt_alg_parse("U R U' R'", &alg, token, sizeof token));
    bt_cube state;
    verify_bld_case_cube(&alg, &state);
    verify_tier2 f2l = verify_cfop_stage_outcome(VERIFY_CFOP_F2L, &state, &layout,
                                                 "{\"case\":\"01\",\"slot\":\"fr\"}");
    BT_CHECK_EQ(f2l.kind, VERIFY_TIER2_PASS);
    bt_alg oll;
    bt_alg_init(&oll);
    BT_CHECK(bt_alg_parse("R U R' U R U2 R'", &oll, token, sizeof token));
    bt_cube oll_state;
    verify_bld_case_cube(&oll, &oll_state);
    bt_alg_free(&oll);
    verify_tier2 oll_f2l = verify_cfop_stage_outcome(VERIFY_CFOP_F2L, &oll_state, &layout,
                                                     "{\"case\":\"01\",\"slot\":\"fr\"}");
    BT_CHECK_EQ(oll_f2l.kind, VERIFY_TIER2_FAIL);
    bt_alg_free(&alg);
}

BT_TEST(verify_ev_cfop, a_y_regrasp_insert_still_displaces_the_named_slot) {
    verify_bld_layout layout;
    verify_bld_layout_probe(&layout);
    char fixed[256];
    BT_CHECK_EQ(verify_cfop_rewrite_to_fixed("y L' U' L", fixed, sizeof fixed), 0);
    bt_alg alg;
    bt_alg_init(&alg);
    char token[64];
    BT_CHECK(bt_alg_parse(fixed, &alg, token, sizeof token));
    bt_cube state;
    verify_bld_case_cube(&alg, &state);
    bt_alg_free(&alg);
    verify_tier2 f2l = verify_cfop_stage_outcome(VERIFY_CFOP_F2L, &state, &layout,
                                                 "{\"case\":\"10\",\"slot\":\"fr\"}");
    BT_CHECK_EQ(f2l.kind, VERIFY_TIER2_PASS);
}

BT_TEST(verify_ev_cfop, solved_and_stage_names_resolve) {
    verify_bld_layout layout;
    verify_bld_layout_probe(&layout);
    bt_cube solved = bt_cube_solved();
    verify_tier2 pll = verify_cfop_stage_outcome(VERIFY_CFOP_PLL, &solved, &layout, "{}");
    BT_CHECK_EQ(pll.kind, VERIFY_TIER2_FAIL);
    verify_cfop_stage stage;
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_cfop_stage_of("pll", &stage, err, sizeof err), 0);
    BT_CHECK_EQ(stage, VERIFY_CFOP_PLL);
    BT_CHECK_EQ(verify_cfop_stage_of("oll", &stage, err, sizeof err), 0);
    BT_CHECK_EQ(stage, VERIFY_CFOP_OLL);
    BT_CHECK_EQ(verify_cfop_stage_of("f2l", &stage, err, sizeof err), 0);
    BT_CHECK_EQ(stage, VERIFY_CFOP_F2L);
    BT_CHECK(verify_cfop_stage_of("x", &stage, err, sizeof err) != 0);
}

BT_TEST(verify_ev_cfop, an_unknown_alg_token_names_the_core_error) {
    bt_seed_row storage;
    memset(&storage, 0, sizeof storage);
    snprintf(storage.case_key, sizeof storage.case_key, "%s", "pll.probe");
    storage.alg = (char *)"R X";
    storage.front_json = (char *)"{\"case\":\"P\"}";
    storage.source_url = (char *)"https://example.test";
    storage.source_at = (char *)"2026-10-10";
    bt_seed_rows rows;
    rows.items = &storage;
    rows.count = 1;
    verify_section_result section;
    verify_cfop_verify_pll(&rows, &section);
    BT_CHECK_EQ(section.count, 1);
    BT_CHECK(!section.rows[0].tier1_ok);
    BT_CHECK_EQ(section.rows[0].tier1_err, "unknown token: X");
    BT_CHECK(section.rows[0].tier2.kind == VERIFY_TIER2_SKIPPED);
    verify_section_free(&section);
}

BT_TEST(verify_ev_cfop, a_single_frame_must_satisfy_both_conjuncts) {
    verify_bld_layout layout;
    verify_bld_layout_probe(&layout);
    bt_cube state;
    after("F R U R' U' F'", &state);
    verify_tier2 pll =
        verify_cfop_stage_outcome(VERIFY_CFOP_PLL, &state, &layout, "{\"case\":\"line\"}");
    BT_CHECK_EQ(pll.kind, VERIFY_TIER2_FAIL);
    BT_CHECK_EQ(pll.fail, "no rotation frames a solved first two layers with an oriented last "
                          "layer");
    verify_tier2 oll =
        verify_cfop_stage_outcome(VERIFY_CFOP_OLL, &state, &layout, "{\"case\":\"line\"}");
    BT_CHECK_EQ(oll.kind, VERIFY_TIER2_PASS);
}
