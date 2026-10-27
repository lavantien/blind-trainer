#include "verify_test_util.h"

#include <string.h>

#include "verify/ev_pyra.h"

static const size_t TIP_LOCALS[3] = {0, 4, 8};
static const size_t EDGE_LOCALS[3] = {1, 3, 6};

static bool locals_hold(const size_t *locals, size_t local) {
    for (size_t at = 0; at < 3; at++) {
        if (locals[at] == local) {
            return true;
        }
    }
    return false;
}

BT_TEST(verify_ev_pyra, the_solved_grid_renders_one_color_per_face) {
    bt_pyra_state solved;
    bt_pyra_solved_state(&solved);
    bt_pz_grid grid;
    bt_pyra_facelets(&solved, &grid);
    for (size_t face = 0; face < 4; face++) {
        for (size_t local = 0; local < 9; local++) {
            BT_CHECK_EQ(verify_pyra_flat(&grid, face, local), (uint8_t)face);
        }
    }
}

BT_TEST(verify_ev_pyra, tip_turns_touch_only_tip_local_stickers) {
    bt_pyra_state solved;
    bt_pyra_solved_state(&solved);
    bt_pz_grid solved_grid;
    bt_pyra_facelets(&solved, &solved_grid);
    const char *tokens[8] = {"u", "u'", "l", "l'", "r", "r'", "b", "b'"};
    bt_pz_error err;
    for (size_t at = 0; at < 8; at++) {
        void *alg = bt_pz_alg_create();
        bt_pz_engine eng = bt_pyra_engine();
        BT_CHECK(eng.parse_alg(&eng, tokens[at], strlen(tokens[at]), alg, &err));
        bt_pyra_state state;
        bt_pyra_solved_state(&state);
        BT_CHECK(bt_pyra_apply_alg(&state, alg, &err));
        bt_pz_grid grid;
        bt_pyra_facelets(&state, &grid);
        for (size_t face = 0; face < 4; face++) {
            for (size_t local = 0; local < 9; local++) {
                bool moved = verify_pyra_flat(&grid, face, local) !=
                             verify_pyra_flat(&solved_grid, face, local);
                BT_CHECK(!moved || locals_hold(TIP_LOCALS, local));
            }
        }
        bt_pz_alg_destroy(alg);
    }
}

BT_TEST(verify_ev_pyra, a_vertex_turn_moves_edges_and_corners_but_never_only_edges) {
    bt_pyra_state solved;
    bt_pyra_solved_state(&solved);
    bt_pz_grid solved_grid;
    bt_pyra_facelets(&solved, &solved_grid);
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    bt_pz_engine eng = bt_pyra_engine();
    BT_CHECK(eng.parse_alg(&eng, "R", 1, alg, &err));
    bt_pyra_state state;
    bt_pyra_solved_state(&state);
    BT_CHECK(bt_pyra_apply_alg(&state, alg, &err));
    bt_pz_grid grid;
    bt_pyra_facelets(&state, &grid);
    bool edge_moved = false;
    bool other_moved = false;
    for (size_t face = 0; face < 4; face++) {
        for (size_t local = 0; local < 9; local++) {
            bool moved =
                verify_pyra_flat(&grid, face, local) != verify_pyra_flat(&solved_grid, face, local);
            if (locals_hold(EDGE_LOCALS, local)) {
                edge_moved = (bool)(edge_moved || moved);
            } else {
                other_moved = (bool)(other_moved || moved);
            }
        }
    }
    bt_pz_alg_destroy(alg);
    BT_CHECK(edge_moved && other_moved);
}

BT_TEST(verify_ev_pyra, solved_state_rejects_the_l4e_predicate) {
    bt_pyra_state solved;
    bt_pyra_solved_state(&solved);
    char err[VERIFY_ERR_MAX];
    int moved = verify_pyra_l4e_disturbance(&solved, err, sizeof err);
    BT_CHECK_EQ(moved, -1);
    BT_CHECK_EQ(err, "0 edge facelets moved, a last four edges case holds 4 to 8");
}

BT_TEST(verify_ev_pyra, an_edge_case_alg_yields_an_edges_only_start_state) {
    bt_pyra_state state;
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_pyra_case_state("R U' R' U' R U' R'", &state, err, sizeof err), 0);
    BT_CHECK(verify_pyra_l4e_disturbance(&state, err, sizeof err) >= 0);
}

BT_TEST(verify_ev_pyra, a_u_framed_edge_case_reports_its_framed_edge_count) {
    bt_pyra_state state;
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_pyra_case_state("L' U L", &state, err, sizeof err), 0);
    BT_CHECK_EQ(verify_pyra_l4e_disturbance(&state, err, sizeof err), 6);
}

BT_TEST(verify_ev_pyra, a_tip_twisted_edge_case_is_rejected_outside_the_u_layer_frame) {
    bt_pyra_state state;
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_pyra_case_state("u R U' R'", &state, err, sizeof err), 0);
    int moved = verify_pyra_l4e_disturbance(&state, err, sizeof err);
    BT_CHECK_EQ(moved, -1);
    BT_CHECK_EQ(err, "corners and centers are not home in any U layer frame");
}

BT_TEST(verify_ev_pyra, a_tip_turn_case_state_is_rejected_as_outside_l4e) {
    bt_pyra_state state;
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_pyra_case_state("u", &state, err, sizeof err), 0);
    BT_CHECK(verify_pyra_l4e_disturbance(&state, err, sizeof err) < 0);
}

BT_TEST(verify_ev_pyra, a_full_scramble_case_state_exceeds_the_edge_bound) {
    bt_pyra_state state;
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_pyra_case_state("L R B U", &state, err, sizeof err), 0);
    BT_CHECK(verify_pyra_l4e_disturbance(&state, err, sizeof err) < 0);
}

BT_TEST(verify_ev_pyra, l4e_admission_is_closed_under_the_u_layer_frame) {
    bt_pz_error pz_err;
    void *quarter = bt_pz_alg_create();
    bt_pz_engine eng = bt_pyra_engine();
    BT_CHECK(eng.parse_alg(&eng, "U", 1, quarter, &pz_err));
    bt_pyra_state state;
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_pyra_case_state("R U' R' U' R U' R'", &state, err, sizeof err), 0);
    for (size_t turns = 0; turns < VERIFY_PYRA_U_FRAME_WORDS; turns++) {
        bt_pyra_state framed = state;
        for (size_t at = 0; at < turns; at++) {
            BT_CHECK(bt_pyra_apply_alg(&framed, quarter, &pz_err));
        }
        BT_CHECK_EQ(verify_pyra_l4e_disturbance(&framed, err, sizeof err), 6);
    }
    bt_pz_alg_destroy(quarter);
}
