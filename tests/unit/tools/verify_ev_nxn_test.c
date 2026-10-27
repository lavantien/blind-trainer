#include "verify_test_util.h"

#include <string.h>

#include "verify/ev_nxn.h"

static bt_pz_nxn engine(void) {
    bt_pz_nxn nxn;
    BT_CHECK(bt_pz_nxn_init(&nxn, 2));
    return nxn;
}

BT_TEST(verify_ev_nxn, rotation_words_dedupe_to_the_24_cube_orientations) {
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

BT_TEST(verify_ev_nxn, empty_algs_fail_tier1) {
    bt_pz_nxn nxn = engine();
    char err[VERIFY_ERR_MAX];
    int rc = verify_nxn_tier1_nonempty(&nxn, "", err, sizeof err);
    BT_CHECK_EQ(rc, 1);
    BT_CHECK_EQ(err, "alg is empty");
}
