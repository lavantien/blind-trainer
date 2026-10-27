#include "verify_test_util.h"

#include <string.h>

#include "verify/ev_mega.h"

static void after(const char *text, bt_mega_state *state) {
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    bt_pz_engine eng = bt_mega_engine();
    BT_CHECK(eng.parse_alg(&eng, text, strlen(text), alg, &err));
    bt_mega_solved_state(state);
    BT_CHECK(bt_mega_apply_alg(state, alg, &err));
    bt_pz_alg_destroy(alg);
}

BT_TEST(verify_ev_mega, the_u_ring_holds_five_slots) {
    BT_CHECK_EQ(verify_mega_ring(), 5);
}

BT_TEST(verify_ev_mega, solved_state_orients_and_places_everything) {
    bt_mega_state state;
    bt_mega_solved_state(&state);
    BT_CHECK(verify_mega_edges_oriented(&state));
    BT_CHECK(verify_mega_corners_oriented(&state));
    BT_CHECK(verify_mega_corners_placed(&state));
}

BT_TEST(verify_ev_mega, a_side_turn_disorients_the_u_ring) {
    bt_mega_state state;
    after("R", &state);
    BT_CHECK(!verify_mega_edges_oriented(&state));
    BT_CHECK(!verify_mega_corners_oriented(&state));
    BT_CHECK(!verify_mega_corners_placed(&state));
}

BT_TEST(verify_ev_mega, a_u_turn_keeps_the_u_face_uniform_but_unplaces_corners) {
    bt_mega_state state;
    after("U", &state);
    BT_CHECK(verify_mega_edges_oriented(&state));
    BT_CHECK(verify_mega_corners_oriented(&state));
    BT_CHECK(!verify_mega_corners_placed(&state));
}

BT_TEST(verify_ev_mega, tags_map_to_their_stages) {
    verify_mega_ll_stage stage;
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_mega_stage_of("EO1", &stage, err, sizeof err), 0);
    BT_CHECK_EQ(stage, VERIFY_MEGA_STAGE_EO);
    BT_CHECK_EQ(verify_mega_stage_of("CO4", &stage, err, sizeof err), 0);
    BT_CHECK_EQ(stage, VERIFY_MEGA_STAGE_CO);
    BT_CHECK_EQ(verify_mega_stage_of("CP12", &stage, err, sizeof err), 0);
    BT_CHECK_EQ(stage, VERIFY_MEGA_STAGE_CP);
    BT_CHECK_EQ(verify_mega_stage_of("EP5", &stage, err, sizeof err), 0);
    BT_CHECK_EQ(stage, VERIFY_MEGA_STAGE_EP);
    BT_CHECK_EQ(verify_mega_stage_of("EO", &stage, err, sizeof err), 0);
    BT_CHECK_EQ(stage, VERIFY_MEGA_STAGE_EO);
    BT_CHECK(verify_mega_stage_of("Sune", &stage, err, sizeof err) != 0);
}

BT_TEST(verify_ev_mega, a_u_turn_case_passes_cp_and_fails_eo) {
    bt_mega_state state;
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_mega_case_state("U", &state, err, sizeof err), 0);
    verify_tier2 cp = verify_mega_stage_outcome(VERIFY_MEGA_STAGE_CP, &state);
    BT_CHECK_EQ(cp.kind, VERIFY_TIER2_PASS);
    verify_tier2 ep = verify_mega_stage_outcome(VERIFY_MEGA_STAGE_EP, &state);
    BT_CHECK_EQ(ep.kind, VERIFY_TIER2_FAIL);
    verify_tier2 eo = verify_mega_stage_outcome(VERIFY_MEGA_STAGE_EO, &state);
    BT_CHECK_EQ(eo.kind, VERIFY_TIER2_FAIL);
}

BT_TEST(verify_ev_mega, a_rollback_r_start_case_passes_the_eo_and_co_conditions) {
    bt_mega_state state;
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_mega_case_state("R", &state, err, sizeof err), 0);
    verify_tier2 eo = verify_mega_stage_outcome(VERIFY_MEGA_STAGE_EO, &state);
    BT_CHECK_EQ(eo.kind, VERIFY_TIER2_PASS);
    verify_tier2 co = verify_mega_stage_outcome(VERIFY_MEGA_STAGE_CO, &state);
    BT_CHECK_EQ(co.kind, VERIFY_TIER2_PASS);
    verify_tier2 ep = verify_mega_stage_outcome(VERIFY_MEGA_STAGE_EP, &state);
    BT_CHECK_EQ(ep.kind, VERIFY_TIER2_FAIL);
}

BT_TEST(verify_ev_mega, tag_of_reads_the_case_field) {
    char tag[32];
    BT_CHECK_EQ(verify_mega_tag_of("{\"case\":\"EP5\"}", tag, sizeof tag), 0);
    BT_CHECK_EQ(tag, "EP5");
    BT_CHECK(verify_mega_tag_of("{\"case\":3}", tag, sizeof tag) != 0);
    BT_CHECK(verify_mega_tag_of("{}", tag, sizeof tag) != 0);
}
