#include "verify_test_util.h"

#include <string.h>

#include "core/alg.h"
#include "core/cube.h"
#include "verify/ev_bld.h"
#include "verify/ev_cfop.h"

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
