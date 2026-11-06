#include "verify_test_util.h"

#include <string.h>

#include "common/alloc.h"
#include "unit/core/death_test.h"
#include "verify/ev_nxn.h"

static bt_pz_nxn engine(void) {
    bt_pz_nxn nxn;
    BT_CHECK(bt_pz_nxn_init(&nxn, 2));
    return nxn;
}

static bt_seed_row cll_row(const char *key, const char *alg) {
    bt_seed_row row = {0};
    snprintf(row.event, sizeof row.event, "%s", "2x2");
    snprintf(row.method, sizeof row.method, "%s", "cll");
    snprintf(row.stage, sizeof row.stage, "%s", "cll");
    snprintf(row.case_key, sizeof row.case_key, "%s", key);
    row.alg = (char *)alg;
    row.front_json = (char *)"{\"case\":\"probe\"}";
    row.source_url = (char *)"https://example.test";
    row.source_at = (char *)"2026-10-10";
    row.content_hash[0] = '\0';
    return row;
}

static void nxn_death_push_realloc(void) {
    bt_pz_nxn nxn;
    bt_pz_nxn_init(&nxn, 2);
    verify_nxn_rotations rotations;
    verify_nxn_rotations_init(&rotations);
    bt_alloc_fail_arm(1);
    verify_nxn_rotations_build(&nxn, &rotations);
}

BT_TEST(verify_ev_nxn, rotation_words_yield_the_24_cube_orientations) {
    bt_pz_nxn nxn = engine();
    verify_nxn_rotations rotations;
    verify_nxn_rotations_init(&rotations);
    verify_nxn_rotations_build(&nxn, &rotations);
    BT_CHECK_EQ(rotations.count, VERIFY_NXN_ROTATION_COUNT);
    verify_nxn_rotations_free(&rotations);
    bt_pz_nxn big;
    BT_CHECK(bt_pz_nxn_init(&big, 7));
    verify_nxn_rotations_init(&rotations);
    verify_nxn_rotations_build(&big, &rotations);
    BT_CHECK_EQ(rotations.count, VERIFY_NXN_ROTATION_COUNT);
    verify_nxn_rotations_free(&rotations);
}

BT_TEST(verify_ev_nxn, rotations_build_aborts_when_its_push_realloc_starves) {
    bt_death_expect_abort("verify_ev_nxn.push_realloc_starves", "malloc failed",
                          nxn_death_push_realloc);
}

BT_TEST(verify_ev_nxn, solved_state_has_a_solved_bottom_layer) {
    bt_pz_nxn nxn = engine();
    bt_pz_nxn_state solved;
    bt_pz_nxn_solved_state(&nxn, &solved);
    BT_CHECK(verify_nxn_d_layer_solved(&nxn, &solved));
}

BT_TEST(verify_ev_nxn, a_mere_u_turn_still_leaves_the_bottom_layer_solved) {
    bt_pz_nxn nxn = engine();
    verify_nxn_rotations rotations;
    verify_nxn_rotations_init(&rotations);
    verify_nxn_rotations_build(&nxn, &rotations);
    verify_tier2 outcome = verify_nxn_tier2_cll(&nxn, &rotations, "U");
    BT_CHECK_EQ(outcome.kind, VERIFY_TIER2_PASS);
    verify_nxn_rotations_free(&rotations);
}

BT_TEST(verify_ev_nxn, a_corner_three_cycle_has_no_solved_layer) {
    bt_pz_nxn nxn = engine();
    verify_nxn_rotations rotations;
    verify_nxn_rotations_init(&rotations);
    verify_nxn_rotations_build(&nxn, &rotations);
    verify_tier2 outcome = verify_nxn_tier2_cll(&nxn, &rotations, "R U R'");
    BT_CHECK_EQ(outcome.kind, VERIFY_TIER2_FAIL);
    verify_nxn_rotations_free(&rotations);
}

BT_TEST(verify_ev_nxn, a_rotation_only_row_is_rejected_as_already_solved) {
    bt_pz_nxn nxn = engine();
    verify_nxn_rotations rotations;
    verify_nxn_rotations_init(&rotations);
    verify_nxn_rotations_build(&nxn, &rotations);
    const char *algs[3] = {"x", "z'", "x y2"};
    for (size_t at = 0; at < 3; at++) {
        verify_tier2 outcome = verify_nxn_tier2_cll(&nxn, &rotations, algs[at]);
        BT_CHECK_EQ(outcome.kind, VERIFY_TIER2_FAIL);
        BT_CHECK_EQ(outcome.fail, "start state is already solved");
    }
    verify_nxn_rotations_free(&rotations);
}

BT_TEST(verify_ev_nxn, an_unknown_token_alg_fails_tier1_with_the_engine_message) {
    bt_pz_nxn nxn = engine();
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_nxn_tier1_nonempty(&nxn, "Z", err, sizeof err), 1);
    BT_CHECK(err[0] != '\0');
    BT_CHECK(strstr(err, "Z") != NULL);
}

BT_TEST(verify_ev_nxn, an_unknown_token_alg_fails_tier2_with_the_engine_message) {
    bt_pz_nxn nxn = engine();
    verify_nxn_rotations rotations;
    verify_nxn_rotations_init(&rotations);
    verify_nxn_rotations_build(&nxn, &rotations);
    verify_tier2 outcome = verify_nxn_tier2_cll(&nxn, &rotations, "Z");
    BT_CHECK_EQ(outcome.kind, VERIFY_TIER2_FAIL);
    BT_CHECK(outcome.fail != NULL);
    BT_CHECK(strstr(outcome.fail, "Z") != NULL);
    verify_nxn_rotations_free(&rotations);
}

BT_TEST(verify_ev_nxn, empty_algs_fail_tier1) {
    bt_pz_nxn nxn = engine();
    char err[VERIFY_ERR_MAX];
    int rc = verify_nxn_tier1_nonempty(&nxn, "", err, sizeof err);
    BT_CHECK_EQ(rc, 1);
    BT_CHECK_EQ(err, "alg is empty");
}

BT_TEST(verify_ev_nxn, twoby2_reports_the_tier1_and_tier2_parse_failures) {
    bt_seed_row rows_items[2] = {cll_row("cll.probe", "Z"), cll_row("cll.ok", "R U R'")};
    bt_seed_rows rows = {rows_items, 2};
    verify_section_result result;
    verify_nxn_verify_twoby2(&rows, &result);
    BT_CHECK_EQ(result.count, 2);
    BT_CHECK(!result.rows[0].tier1_ok);
    BT_CHECK(result.rows[0].tier1_err[0] != '\0');
    BT_CHECK(strstr(result.rows[0].tier1_err, "Z") != NULL);
    BT_CHECK_EQ(result.rows[0].tier2.kind, VERIFY_TIER2_FAIL);
    BT_CHECK(result.rows[0].tier2.fail != NULL);
    BT_CHECK(strstr(result.rows[0].tier2.fail, "Z") != NULL);
    BT_CHECK(result.rows[1].tier1_ok);
    BT_CHECK_EQ(result.rows[1].tier2.kind, VERIFY_TIER2_FAIL);
    verify_section_free(&result);
}
