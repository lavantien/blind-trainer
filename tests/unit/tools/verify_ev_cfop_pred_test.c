#include "verify_test_util.h"

#include <string.h>

#include "core/alg.h"
#include "core/cube.h"
#include "verify/ev_bld.h"
#include "verify/ev_cfop.h"
#include "verify/packs.h"

static void state_of(const char *alg_text, bt_cube *out) {
    bt_alg alg;
    bt_alg_init(&alg);
    char token[64];
    if (!bt_alg_parse(alg_text, &alg, token, sizeof token)) {
        BT_ABORT("parse failed: %s", alg_text);
    }
    *out = bt_cube_solved();
    bt_cube_apply_alg(out, &alg);
    bt_alg_free(&alg);
}

BT_TEST(verify_ev_cfop_pred, rewrite_names_each_rejection) {
    char out[512];
    BT_CHECK(verify_cfop_rewrite_to_fixed("U3", out, sizeof out) != 0);
    BT_CHECK(strstr(out, "no canonical move") != NULL);
    BT_CHECK(verify_cfop_rewrite_to_fixed("A", out, sizeof out) != 0);
    BT_CHECK(strstr(out, "unknown token") != NULL);
    char tiny[8];
    BT_CHECK(verify_cfop_rewrite_to_fixed("R U R' U' R U R' U'", tiny, sizeof tiny) != 0);
    BT_CHECK(strncmp(tiny, "rewritt", sizeof tiny - 1) == 0);
    BT_CHECK_EQ(verify_cfop_rewrite_to_fixed("", out, sizeof out), 0);
    BT_CHECK_EQ(out, "");
}

BT_TEST(verify_ev_cfop_pred, rewrite_carries_a_rotation_frame) {
    char out[512];
    BT_CHECK_EQ(verify_cfop_rewrite_to_fixed("y R", out, sizeof out), 0);
    BT_CHECK(out[0] != '\0');
    BT_CHECK(strstr(out, "y") == NULL);
    BT_CHECK(verify_cfop_rewrite_to_fixed("x Q", out, sizeof out) != 0);
    BT_CHECK(verify_cfop_rewrite_to_fixed("x A", out, sizeof out) != 0);
}

BT_TEST(verify_ev_cfop_pred, rewrite_keeps_slice_and_wide_tokens_canonical) {
    char out[512];
    BT_CHECK_EQ(verify_cfop_rewrite_to_fixed("M E S", out, sizeof out), 0);
    BT_CHECK_EQ(strcmp(out, "M E S"), 0);
    BT_CHECK_EQ(verify_cfop_rewrite_to_fixed("u2'", out, sizeof out), 0);
    BT_CHECK_EQ(strcmp(out, "u2"), 0);
}

BT_TEST(verify_ev_cfop_pred, stage_of_rejects_unknown_names) {
    verify_cfop_stage stage;
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_cfop_stage_of("f2l", &stage, err, sizeof err), 0);
    BT_CHECK_EQ(stage, VERIFY_CFOP_F2L);
    BT_CHECK(verify_cfop_stage_of("roux", &stage, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "no cfop step") != NULL);
}

BT_TEST(verify_ev_cfop_pred, stage_outcome_names_the_pll_and_oll_failures) {
    verify_bld_layout layout;
    verify_bld_layout_probe(&layout);
    bt_cube state;
    state_of("R U R' U' R U R' U'", &state);
    verify_tier2 pll = verify_cfop_stage_outcome(VERIFY_CFOP_PLL, &state, &layout, "{}");
    BT_CHECK(strstr(pll.fail, "no rotation frames") != NULL);
    state_of("F R U R' U' F'", &state);
    verify_tier2 oll_pass = verify_cfop_stage_outcome(VERIFY_CFOP_OLL, &state, &layout, "{}");
    BT_CHECK_EQ(oll_pass.kind, VERIFY_TIER2_PASS);
    verify_tier2 pll_fail = verify_cfop_stage_outcome(VERIFY_CFOP_PLL, &state, &layout, "{}");
    BT_CHECK(strstr(pll_fail.fail, "oriented last layer") != NULL);
}

BT_TEST(verify_ev_cfop_pred, f2l_outcome_names_the_slot_failures) {
    verify_bld_layout layout;
    verify_bld_layout_probe(&layout);
    bt_cube state;
    state_of("U R U' R'", &state);
    verify_tier2 slot_json =
        verify_cfop_stage_outcome(VERIFY_CFOP_F2L, &state, &layout, "not json");
    BT_CHECK(slot_json.fail[0] != '\0');
    verify_tier2 slot_missing = verify_cfop_stage_outcome(VERIFY_CFOP_F2L, &state, &layout, "{}");
    BT_CHECK(strstr(slot_missing.fail, "names no slot") != NULL);
    verify_tier2 slot_number =
        verify_cfop_stage_outcome(VERIFY_CFOP_F2L, &state, &layout, "{\"slot\": 3}");
    BT_CHECK(strstr(slot_number.fail, "names no slot") != NULL);
    verify_tier2 slot_bad =
        verify_cfop_stage_outcome(VERIFY_CFOP_F2L, &state, &layout, "{\"slot\": \"frx\"}");
    BT_CHECK(strstr(slot_bad.fail, "non-face letter") != NULL);
    state_of("R U R' U R U2 R'", &state);
    verify_tier2 solved_layers =
        verify_cfop_stage_outcome(VERIFY_CFOP_F2L, &state, &layout, "{\"slot\": \"fr\"}");
    BT_CHECK(strstr(solved_layers.fail, "first two layers are already solved") != NULL);
}

BT_TEST(verify_ev_cfop_pred, rewrite_names_a_leading_digit_token) {
    char out[512];
    BT_CHECK(verify_cfop_rewrite_to_fixed("2R", out, sizeof out) != 0);
    BT_CHECK(strstr(out, "no canonical move") != NULL);
}

BT_TEST(verify_ev_cfop_pred, rewrite_tolerates_trailing_space_and_stacked_rotations) {
    char out[512];
    BT_CHECK_EQ(verify_cfop_rewrite_to_fixed("R U ", out, sizeof out), 0);
    BT_CHECK_EQ(out, "R U");
    BT_CHECK_EQ(verify_cfop_rewrite_to_fixed("x y R", out, sizeof out), 0);
    BT_CHECK(out[0] != '\0');
}

BT_TEST(verify_ev_cfop_pred, f2l_reads_the_u_and_d_slot_letters_and_home_pairs) {
    verify_bld_layout layout;
    verify_bld_layout_probe(&layout);
    bt_cube state;
    state_of("R", &state);
    const char *home_slots[2] = {"UF", "fl"};
    for (size_t at = 0; at < 2; at++) {
        char front_json[32];
        snprintf(front_json, sizeof front_json, "{\"slot\":\"%s\"}", home_slots[at]);
        verify_tier2 outcome =
            verify_cfop_stage_outcome(VERIFY_CFOP_F2L, &state, &layout, front_json);
        BT_CHECK_EQ(outcome.kind, VERIFY_TIER2_FAIL);
        BT_CHECK(strstr(outcome.fail, "holds its pair solved") != NULL);
    }
    verify_tier2 displaced_slot =
        verify_cfop_stage_outcome(VERIFY_CFOP_F2L, &state, &layout, "{\"slot\":\"DF\"}");
    BT_CHECK_EQ(displaced_slot.kind, VERIFY_TIER2_PASS);
}

BT_TEST(verify_ev_cfop_pred, f2l_names_a_state_with_no_solved_cross) {
    verify_bld_layout layout;
    verify_bld_layout_probe(&layout);
    bt_cube state;
    state_of("U D'", &state);
    verify_tier2 no_cross =
        verify_cfop_stage_outcome(VERIFY_CFOP_F2L, &state, &layout, "{\"slot\":\"fr\"}");
    BT_CHECK_EQ(no_cross.kind, VERIFY_TIER2_FAIL);
    BT_CHECK(strstr(no_cross.fail, "no solved cross") != NULL);
}

static bt_seed_rows cfop_rows_holder;

static void cfop_one_row(const char *alg) {
    static bt_seed_row row;
    memset(&row, 0, sizeof row);
    snprintf(row.case_key, sizeof row.case_key, "%s", "pll.1");
    row.alg = (char *)alg;
    row.front_json = (char *)"{}";
    row.source_url = (char *)"https://example.test";
    row.source_at = (char *)"2026-10-10";
    cfop_rows_holder.items = &row;
    cfop_rows_holder.count = 1;
}

BT_TEST(verify_ev_cfop_pred, pll_rows_name_blank_and_parenthesized_rotation_algs) {
    verify_section_result section;
    cfop_one_row("");
    verify_cfop_verify_pll(&cfop_rows_holder, &section);
    BT_CHECK_EQ(section.count, 1);
    BT_CHECK(!section.rows[0].tier1_ok);
    BT_CHECK_EQ(section.rows[0].tier1_err, "alg is empty");
    BT_CHECK_EQ(section.rows[0].tier2.kind, VERIFY_TIER2_SKIPPED);
    verify_section_free(&section);
    cfop_one_row("(x)");
    verify_cfop_verify_pll(&cfop_rows_holder, &section);
    BT_CHECK_EQ(section.count, 1);
    BT_CHECK(section.rows[0].tier1_ok);
    BT_CHECK_EQ(section.rows[0].tier2.kind, VERIFY_TIER2_PASS);
    verify_section_free(&section);
}
