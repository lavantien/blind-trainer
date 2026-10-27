#include "verify_test_util.h"

#include <string.h>

#include "verify/ev_bld.h"

static bool slot_index_ok(const verify_bld_layout *layout, const char *name) {
    char err[VERIFY_ERR_MAX];
    return verify_bld_slot_index(layout, name, err, sizeof err) >= 0;
}

BT_TEST(verify_ev_bld, wide_suffix_tokens_map_to_lowercase_wide_moves) {
    char out[128];
    verify_bld_map_wide_suffix("Lw' D L2", out, sizeof out);
    BT_CHECK_EQ(out, "l' D L2");
    verify_bld_map_wide_suffix("Dw2 L", out, sizeof out);
    BT_CHECK_EQ(out, "d2 L");
    verify_bld_map_wide_suffix("R U R'", out, sizeof out);
    BT_CHECK_EQ(out, "R U R'");
}

BT_TEST(verify_ev_bld, the_probe_yields_eight_corners_twelve_edges_and_named_slots) {
    verify_bld_layout layout;
    verify_bld_layout_probe(&layout);
    size_t corners[VERIFY_BLD_MAX_BAND];
    size_t edges[VERIFY_BLD_MAX_BAND];
    BT_CHECK_EQ(verify_bld_corner_pieces(&layout, corners, VERIFY_BLD_MAX_BAND), 8);
    BT_CHECK_EQ(verify_bld_edge_pieces(&layout, edges, VERIFY_BLD_MAX_BAND), 12);
    const uint8_t *corner_slot = NULL;
    size_t corner_len = 0;
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_bld_slot(&layout, "UFR", &corner_slot, &corner_len, err, sizeof err), 0);
    BT_CHECK_EQ(corner_len, 3);
    const uint8_t *edge_slot = NULL;
    size_t edge_len = 0;
    BT_CHECK_EQ(verify_bld_slot(&layout, "UR", &edge_slot, &edge_len, err, sizeof err), 0);
    BT_CHECK_EQ(edge_len, 2);
    BT_CHECK(!slot_index_ok(&layout, "UD"));
    BT_CHECK(!slot_index_ok(&layout, "UFRF"));
}

BT_TEST(verify_ev_bld, solved_cube_audits_with_no_disturbance) {
    verify_bld_layout layout;
    verify_bld_layout_probe(&layout);
    bt_cube solved = bt_cube_solved();
    verify_bld_state_audit audit;
    verify_bld_audit(&solved, &layout, &audit);
    BT_CHECK_EQ(audit.disturbed_count, 0);
    BT_CHECK(verify_bld_zero_permutation(&audit, &layout));
}

BT_TEST(verify_ev_bld, a_t_perm_case_state_transposes_the_named_edges) {
    verify_bld_layout layout;
    verify_bld_layout_probe(&layout);
    char mapped[128];
    verify_bld_map_wide_suffix("R U R' U' R' F R2 U' R' U' R U R' F'", mapped, sizeof mapped);
    bt_alg alg;
    bt_alg_init(&alg);
    BT_CHECK(bt_alg_try_parse(mapped, &alg));
    bt_cube state;
    verify_bld_case_cube(&alg, &state);
    verify_bld_state_audit audit;
    verify_bld_audit(&state, &layout, &audit);
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_bld_transposed(&audit, &layout, "UR", "UL", err, sizeof err), 0);
    BT_CHECK(verify_bld_transposed(&audit, &layout, "UR", "UF", err, sizeof err) != 0);
    bt_alg_free(&alg);
}

BT_TEST(verify_ev_bld, plain_m2_transposes_df_and_ub_with_four_displaced_edges) {
    verify_bld_layout layout;
    verify_bld_layout_probe(&layout);
    bt_alg alg;
    bt_alg_init(&alg);
    BT_CHECK(bt_alg_try_parse("M2", &alg));
    bt_cube state;
    verify_bld_case_cube(&alg, &state);
    verify_bld_state_audit audit;
    verify_bld_audit(&state, &layout, &audit);
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_bld_transposed(&audit, &layout, "DF", "UB", err, sizeof err), 0);
    size_t edges[VERIFY_BLD_MAX_BAND];
    size_t band[VERIFY_BLD_MAX_BAND];
    size_t band_count = verify_bld_edge_pieces(&layout, band, VERIFY_BLD_MAX_BAND);
    BT_CHECK_EQ(verify_bld_displaced(&audit, band, band_count, edges, VERIFY_BLD_MAX_BAND), 4);
    bt_alg_free(&alg);
}

BT_TEST(verify_ev_bld, pair_and_swap_specs_parse) {
    char pair[2][VERIFY_BLD_NAME_MAX];
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_bld_parse_pair("UF & UB", '&', pair, err, sizeof err), 0);
    BT_CHECK_EQ(pair[0], "UF");
    BT_CHECK_EQ(pair[1], "UB");
    char edge[2][VERIFY_BLD_NAME_MAX];
    char corner[2][VERIFY_BLD_NAME_MAX];
    BT_CHECK_EQ(verify_bld_parity_swaps("UF-UR+UFR-UBR", edge, corner, err, sizeof err), 0);
    BT_CHECK_EQ(edge[0], "UF");
    BT_CHECK_EQ(corner[1], "UBR");
    BT_CHECK(verify_bld_parity_swaps("UF-UR", edge, corner, err, sizeof err) != 0);
}

BT_TEST(verify_ev_bld, corner_names_match_slots_in_any_letter_order) {
    verify_bld_layout layout;
    verify_bld_layout_probe(&layout);
    char err[VERIFY_ERR_MAX];
    int ufr = verify_bld_slot_index(&layout, "UFR", err, sizeof err);
    int urf = verify_bld_slot_index(&layout, "URF", err, sizeof err);
    int ur = verify_bld_slot_index(&layout, "UR", err, sizeof err);
    int ru = verify_bld_slot_index(&layout, "RU", err, sizeof err);
    int ufl = verify_bld_slot_index(&layout, "UFL", err, sizeof err);
    BT_CHECK_EQ(ufr, urf);
    BT_CHECK_EQ(ur, ru);
    BT_CHECK(ufr != ufl);
}
