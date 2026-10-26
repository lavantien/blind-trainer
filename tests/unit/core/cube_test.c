#include "common/bt_harness.h"
#include "core/alg.h"
#include "core/cube.h"

static bt_cube cube_of(const char *text) {
    bt_alg alg;
    bt_alg_init(&alg);
    char err[64];
    if (!bt_alg_parse(text, &alg, err, sizeof err)) {
        BT_ABORT("parse failed for %s: %s", text, err);
    }
    bt_cube cube = bt_cube_solved();
    bt_cube_apply_alg(&cube, &alg);
    bt_alg_free(&alg);
    return cube;
}

BT_TEST(cube, solved_state_is_identity) {
    bt_cube cube = bt_cube_solved();
    BT_CHECK(bt_cube_is_solved(&cube));
    BT_CHECK(bt_cube_centers_fixed(&cube));
    bt_cube inverse = bt_cube_inverse_state(&cube);
    BT_CHECK(bt_cube_eq(&cube, &inverse));
    for (uint8_t pos = 0; pos < 54; pos++) {
        BT_CHECK_EQ(cube.occ[pos], pos);
        BT_CHECK_EQ(bt_cube_sticker_at(&cube, pos), pos);
        BT_CHECK_EQ(bt_cube_position_of(&cube, pos), pos);
    }
}

BT_TEST(cube, apply_then_inverse_alg_restores_solved) {
    bt_cube cube = cube_of("R U r' 2F x2 M2 S'");
    BT_CHECK(!bt_cube_is_solved(&cube));
    bt_alg alg;
    bt_alg_init(&alg);
    char err[64];
    BT_CHECK(bt_alg_parse("R U r' 2F x2 M2 S'", &alg, err, sizeof err));
    bt_alg inverse = bt_alg_inverse(&alg);
    bt_cube_apply_alg(&cube, &inverse);
    BT_CHECK(bt_cube_is_solved(&cube));
    bt_alg_free(&alg);
    bt_alg_free(&inverse);
}

BT_TEST(cube, inverse_state_matches_inverse_alg_state) {
    bt_cube forward = cube_of("F B' l2 M E' 2R z");
    bt_alg alg;
    bt_alg_init(&alg);
    char err[64];
    BT_CHECK(bt_alg_parse("F B' l2 M E' 2R z", &alg, err, sizeof err));
    bt_alg inverse = bt_alg_inverse(&alg);
    bt_cube backward = bt_cube_solved();
    bt_cube_apply_alg(&backward, &inverse);
    bt_cube flipped = bt_cube_inverse_state(&forward);
    BT_CHECK(bt_cube_eq(&flipped, &backward));
    bt_alg_free(&alg);
    bt_alg_free(&inverse);
}

BT_TEST(cube, sticker_at_and_position_of_are_inverses_after_moves) {
    bt_cube cube = cube_of("R U R' U' M x' 2B2");
    for (uint8_t sticker = 0; sticker < 54; sticker++) {
        BT_CHECK_EQ(bt_cube_sticker_at(&cube, bt_cube_position_of(&cube, sticker)), sticker);
    }
    for (uint8_t pos = 0; pos < 54; pos++) {
        BT_CHECK_EQ(bt_cube_position_of(&cube, bt_cube_sticker_at(&cube, pos)), pos);
    }
}

BT_TEST(cube, only_face_turns_keep_centers_fixed) {
    const char *kept[] = {"U", "D", "L", "R", "F", "B"};
    for (size_t i = 0; i < sizeof kept / sizeof kept[0]; i++) {
        bt_cube cube = cube_of(kept[i]);
        BT_CHECK(bt_cube_centers_fixed(&cube));
    }
    const char *moved[] = {"x", "M", "l", "2F"};
    for (size_t i = 0; i < sizeof moved / sizeof moved[0]; i++) {
        bt_cube cube = cube_of(moved[i]);
        BT_CHECK(!bt_cube_centers_fixed(&cube));
    }
}

BT_TEST(cube, sticker_id_layout_matches_position_map) {
    BT_CHECK_EQ(bt_corner_id(BT_FACE_U, 2), 2);
    BT_CHECK_EQ(bt_edge_id(BT_FACE_U, 2), 26);
    BT_CHECK_EQ(bt_center_id(BT_FACE_U), 48);
    BT_CHECK_EQ(bt_corner_id(BT_FACE_D, 0), 20);
    BT_CHECK_EQ(bt_edge_id(BT_FACE_D, 0), 44);
    BT_CHECK_EQ(bt_center_id(BT_FACE_D), 53);
}

BT_TEST(cube, from_occ_rejects_non_permutation) {
    uint8_t occ[54];
    for (size_t i = 0; i < 54; i++) {
        occ[i] = (uint8_t)i;
    }
    occ[53] = 0;
    bt_cube out;
    BT_CHECK(!bt_cube_try_from_occ(occ, &out));
}

BT_TEST(cube, from_occ_rejects_bijective_orbit_mixed_input) {
    uint8_t occ[54];
    for (size_t i = 0; i < 54; i++) {
        occ[i] = (uint8_t)i;
    }
    uint8_t tmp = occ[0];
    occ[0] = occ[24];
    occ[24] = tmp;
    bt_cube out;
    BT_CHECK(!bt_cube_try_from_occ(occ, &out));
}

BT_TEST(cube, from_occ_accepts_rotation_states_with_moved_centers) {
    bt_cube cube = cube_of("z");
    uint8_t occ[54];
    for (size_t i = 0; i < 54; i++) {
        occ[i] = cube.occ[i];
    }
    bt_cube rebuilt;
    BT_CHECK(bt_cube_try_from_occ(occ, &rebuilt));
    for (uint8_t pos = 0; pos < 54; pos++) {
        BT_CHECK_EQ(bt_cube_sticker_at(&cube, pos), bt_cube_sticker_at(&rebuilt, pos));
    }
}

BT_TEST(cube, from_occ_rebuilds_equivalent_state) {
    bt_cube cube = cube_of("R U F2 M x' 2L S2");
    uint8_t occ[54];
    for (size_t i = 0; i < 54; i++) {
        occ[i] = (uint8_t)i;
    }
    for (uint8_t sticker = 0; sticker < 54; sticker++) {
        occ[bt_cube_position_of(&cube, sticker)] = sticker;
    }
    bt_cube rebuilt;
    BT_CHECK(bt_cube_try_from_occ(occ, &rebuilt));
    for (uint8_t pos = 0; pos < 54; pos++) {
        BT_CHECK_EQ(bt_cube_sticker_at(&rebuilt, pos), bt_cube_sticker_at(&cube, pos));
    }
    BT_CHECK(bt_cube_eq(&rebuilt, &cube));
    uint8_t ident[54];
    for (size_t i = 0; i < 54; i++) {
        ident[i] = (uint8_t)i;
    }
    bt_cube solved;
    BT_CHECK(bt_cube_try_from_occ(ident, &solved));
    BT_CHECK(bt_cube_is_solved(&solved));
}
