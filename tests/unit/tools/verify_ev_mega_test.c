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
    BT_CHECK(verify_mega_tag_of("{bad", tag, sizeof tag) != 0);
    BT_CHECK(tag[0] != '\0');
}

BT_TEST(verify_ev_mega, empty_algs_fail_case_state) {
    bt_mega_state state;
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_mega_case_state("", &state, err, sizeof err), 1);
    BT_CHECK_EQ(err, "alg is empty");
}

BT_TEST(verify_ev_mega, oriented_or_placed_start_states_fail_their_stage) {
    char err[VERIFY_ERR_MAX];
    bt_mega_state oriented;
    BT_CHECK_EQ(verify_mega_case_state("U", &oriented, err, sizeof err), 0);
    verify_tier2 co = verify_mega_stage_outcome(VERIFY_MEGA_STAGE_CO, &oriented);
    BT_CHECK_EQ(co.kind, VERIFY_TIER2_FAIL);
    BT_CHECK_EQ(co.fail, "corners already oriented");
    bt_mega_state solved;
    bt_mega_solved_state(&solved);
    verify_tier2 cp = verify_mega_stage_outcome(VERIFY_MEGA_STAGE_CP, &solved);
    BT_CHECK_EQ(cp.kind, VERIFY_TIER2_FAIL);
    BT_CHECK_EQ(cp.fail, "corners already placed");
    verify_tier2 ep = verify_mega_stage_outcome(VERIFY_MEGA_STAGE_EP, &solved);
    BT_CHECK_EQ(ep.kind, VERIFY_TIER2_FAIL);
    BT_CHECK_EQ(ep.fail, "start state is already solved");
}

BT_TEST(verify_ev_mega, a_displaced_center_never_finds_a_home_frame) {
    bt_mega_state state;
    bt_mega_solved_state(&state);
    state.colors[BT_MEGA_U * BT_MEGA_PER_FACE + BT_MEGA_CENTER] = BT_MEGA_F;
    verify_tier2 cp = verify_mega_stage_outcome(VERIFY_MEGA_STAGE_CP, &state);
    BT_CHECK_EQ(cp.kind, VERIFY_TIER2_PASS);
}

static bt_seed_rows mega_rows_holder;

static void mega_one_row(const char *alg, const char *front_json) {
    static bt_seed_row row;
    memset(&row, 0, sizeof row);
    snprintf(row.case_key, sizeof row.case_key, "%s", "4lll.1");
    row.alg = (char *)alg;
    row.front_json = (char *)front_json;
    row.source_url = (char *)"https://example.test";
    row.source_at = (char *)"2026-10-10";
    mega_rows_holder.items = &row;
    mega_rows_holder.count = 1;
}

BT_TEST(verify_ev_mega, verify_4lll_skips_rows_that_fail_their_gates) {
    verify_section_result section;
    mega_one_row("", "{\"case\":\"EP5\"}");
    verify_mega_verify_4lll(&mega_rows_holder, &section);
    BT_CHECK_EQ(section.count, 1);
    BT_CHECK(!section.rows[0].tier1_ok);
    BT_CHECK_EQ(section.rows[0].tier1_err, "alg is empty");
    BT_CHECK_EQ(section.rows[0].tier2.kind, VERIFY_TIER2_SKIPPED);
    verify_section_free(&section);
    mega_one_row("R", "{bad");
    verify_mega_verify_4lll(&mega_rows_holder, &section);
    BT_CHECK_EQ(section.count, 1);
    BT_CHECK(section.rows[0].tier1_ok);
    BT_CHECK_EQ(section.rows[0].tier2.kind, VERIFY_TIER2_SKIPPED);
    verify_section_free(&section);
}
