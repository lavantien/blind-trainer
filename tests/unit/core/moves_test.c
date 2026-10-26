#include "common/bt_harness.h"
#include "core/cube.h"
#include "core/moves.h"

enum { GENERATOR_MAX = (6 * 3 * 3 + 3 * 3 + 3 * 3) };

static size_t all_generators(bt_move *out, size_t cap) {
    size_t n = 0;
    for (uint8_t face = 0; face < BT_FACE_COUNT; face++) {
        for (uint8_t q = 1; q <= 3; q++) {
            if (n < cap) {
                out[n++] = bt_move_of_face(face, q);
            }
            if (n < cap) {
                out[n++] = bt_move_of_wide(face, q);
            }
            if (n < cap) {
                out[n++] = bt_move_of_inner(face, q);
            }
        }
    }
    for (uint8_t slice = 0; slice < 3; slice++) {
        for (uint8_t q = 1; q <= 3; q++) {
            if (n < cap) {
                out[n++] = bt_move_of_slice(slice, q);
            }
        }
    }
    for (uint8_t axis = 0; axis < 3; axis++) {
        for (uint8_t q = 1; q <= 3; q++) {
            if (n < cap) {
                out[n++] = bt_move_of_rotation(axis, q);
            }
        }
    }
    return n;
}

BT_TEST(moves, all_generator_tables_are_bijections) {
    bt_move gens[GENERATOR_MAX];
    size_t count = all_generators(gens, GENERATOR_MAX);
    BT_CHECK_EQ(count, (size_t)72);
    for (size_t g = 0; g < count; g++) {
        uint8_t table[54];
        bt_move_table(gens[g], table);
        bool seen[54] = {false};
        for (size_t i = 0; i < 54; i++) {
            BT_CHECK(!seen[table[i]]);
            seen[table[i]] = true;
        }
    }
}

BT_TEST(moves, face_turn_cycles_its_own_stickers_and_fixes_centers) {
    for (uint8_t face = 0; face < BT_FACE_COUNT; face++) {
        uint8_t table[54];
        bt_move_table(bt_move_of_face(face, 1), table);
        size_t corner_base = (size_t)face * 4U;
        size_t edge_base = 24 + (size_t)face * 4U;
        bool corner_dsts[4] = {false};
        bool edge_dsts[4] = {false};
        for (size_t k = 0; k < 4; k++) {
            size_t c = table[corner_base + k];
            size_t e = table[edge_base + k];
            BT_CHECK(c >= corner_base && c < corner_base + 4 && c != corner_base + k);
            BT_CHECK(e >= edge_base && e < edge_base + 4 && e != edge_base + k);
            corner_dsts[c - corner_base] = true;
            edge_dsts[e - edge_base] = true;
        }
        for (size_t k = 0; k < 4; k++) {
            BT_CHECK(corner_dsts[k]);
            BT_CHECK(edge_dsts[k]);
        }
        for (size_t center = 48; center < 54; center++) {
            BT_CHECK_EQ(table[center], (uint8_t)center);
        }
    }
}

BT_TEST(moves, slices_equal_matching_inners) {
    const uint8_t follows[3] = {BT_FACE_L, BT_FACE_D, BT_FACE_F};
    for (uint8_t slice = 0; slice < 3; slice++) {
        for (uint8_t q = 1; q <= 3; q++) {
            uint8_t slice_table[54];
            uint8_t inner_table[54];
            bt_move_table(bt_move_of_slice(slice, q), slice_table);
            bt_move_table(bt_move_of_inner(follows[slice], q), inner_table);
            BT_CHECK(memcmp(slice_table, inner_table, sizeof slice_table) == 0);
        }
    }
}

BT_TEST(moves, rotations_equal_face_slice_counter_face_composition) {
    for (uint8_t q = 1; q <= 3; q++) {
        uint8_t inv = (uint8_t)(4 - q);
        const uint8_t axes[3] = {BT_AXIS_X, BT_AXIS_Y, BT_AXIS_Z};
        const uint8_t faces[3] = {BT_FACE_R, BT_FACE_U, BT_FACE_F};
        const uint8_t slices[3] = {BT_SLICE_M, BT_SLICE_E, BT_SLICE_S};
        const uint8_t slice_quarters[3] = {inv, inv, q};
        const uint8_t backs[3] = {BT_FACE_L, BT_FACE_D, BT_FACE_B};
        for (size_t i = 0; i < 3; i++) {
            bt_cube composed = bt_cube_solved();
            bt_cube_apply(&composed, bt_move_of_face(faces[i], q));
            bt_cube_apply(&composed, bt_move_of_slice(slices[i], slice_quarters[i]));
            bt_cube_apply(&composed, bt_move_of_face(backs[i], inv));
            bt_cube rotation = bt_cube_solved();
            bt_cube_apply(&rotation, bt_move_of_rotation(axes[i], q));
            BT_CHECK(bt_cube_eq(&composed, &rotation));
        }
    }
}

BT_TEST(moves, wide_equals_face_plus_following_inner) {
    for (uint8_t face = 0; face < BT_FACE_COUNT; face++) {
        bt_cube composed = bt_cube_solved();
        bt_cube_apply(&composed, bt_move_of_face(face, 1));
        bt_cube_apply(&composed, bt_move_of_inner(face, 1));
        bt_cube wide = bt_cube_solved();
        bt_cube_apply(&wide, bt_move_of_wide(face, 1));
        BT_CHECK(bt_cube_eq(&composed, &wide));
    }
}

BT_TEST(moves, invert_is_an_involution) {
    bt_move gens[GENERATOR_MAX];
    size_t count = all_generators(gens, GENERATOR_MAX);
    for (size_t g = 0; g < count; g++) {
        bt_move twice = bt_move_invert(bt_move_invert(gens[g]));
        BT_CHECK(bt_move_eq(twice, gens[g]));
        bt_cube cube = bt_cube_solved();
        bt_cube_apply(&cube, gens[g]);
        bt_cube_apply(&cube, bt_move_invert(gens[g]));
        BT_CHECK(bt_cube_is_solved(&cube));
    }
}

BT_TEST(moves, geometry_helpers_return_sentinels_off_the_surface) {
    const int32_t none[3] = {2, 2, 2};
    BT_CHECK_EQ(bt_moves_face_of_normal(none), 6);
    const int32_t high[3] = {0, 1, 2};
    size_t r = 0;
    size_t c = 0;
    bt_moves_cell_of(0, high, &r, &c);
    BT_CHECK_EQ(r, (size_t)9);
    BT_CHECK_EQ(c, (size_t)9);
    const int32_t edge[3] = {2, 1, 0};
    bt_moves_cell_of(0, edge, &r, &c);
    BT_CHECK_EQ(r, (size_t)9);
    BT_CHECK_EQ(c, (size_t)9);
}
