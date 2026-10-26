#include "common.h"
#include "core/analysis.h"
#include "core/cube.h"
#include "core/moves.h"

static void identity_occ(uint8_t occ[54]) {
    for (size_t i = 0; i < 54; i++) {
        occ[i] = (uint8_t)i;
    }
}

static void all_corners_twisted(uint8_t occ[54], size_t w) {
    identity_occ(occ);
    for (uint8_t k = 0; k < 8; k++) {
        uint8_t triple[3];
        bt_corner_stickers(k, triple);
        for (size_t j = 0; j < 3; j++) {
            occ[triple[(j + w) % 3]] = triple[j];
        }
    }
}

static void all_edges_flipped(uint8_t occ[54]) {
    identity_occ(occ);
    for (uint8_t e = 0; e < 12; e++) {
        uint8_t pair[2];
        bt_edge_stickers(e, pair);
        occ[pair[0]] = pair[1];
        occ[pair[1]] = pair[0];
    }
}

static void single_corner_twisted(uint8_t occ[54], uint8_t k, size_t w) {
    identity_occ(occ);
    uint8_t triple[3];
    bt_corner_stickers(k, triple);
    for (size_t j = 0; j < 3; j++) {
        occ[triple[(j + w) % 3]] = triple[j];
    }
}

static void single_edge_flipped(uint8_t occ[54], uint8_t e) {
    identity_occ(occ);
    uint8_t pair[2];
    bt_edge_stickers(e, pair);
    occ[pair[0]] = pair[1];
    occ[pair[1]] = pair[0];
}

typedef struct {
    bt_cube items[14];
    size_t count;
} state_list;

static void states(state_list *out) {
    out->count = 0;
    out->items[out->count++] = bt_cube_solved();
    const char *texts[] = {
        "R U R' F2", "M E S", "R U", "F B' D2", "R U R' U' R U R' U'", "2B l' x S2",
    };
    for (size_t t = 0; t < 6; t++) {
        bt_alg alg;
        bt_alg_init(&alg);
        char err[64];
        BT_CHECK(bt_alg_parse(texts[t], &alg, err, sizeof err));
        bt_cube cube = bt_cube_solved();
        bt_cube_apply_alg(&cube, &alg);
        out->items[out->count++] = cube;
        bt_alg_free(&alg);
    }
    uint8_t occ[54];
    all_corners_twisted(occ, 1);
    out->items[out->count++] = bt_cube_from_occ(occ);
    all_corners_twisted(occ, 2);
    out->items[out->count++] = bt_cube_from_occ(occ);
    all_edges_flipped(occ);
    out->items[out->count++] = bt_cube_from_occ(occ);
    single_corner_twisted(occ, 0, 1);
    out->items[out->count++] = bt_cube_from_occ(occ);
    single_corner_twisted(occ, 5, 2);
    out->items[out->count++] = bt_cube_from_occ(occ);
    single_edge_flipped(occ, 3);
    out->items[out->count++] = bt_cube_from_occ(occ);
    single_edge_flipped(occ, 9);
    out->items[out->count++] = bt_cube_from_occ(occ);
}

BT_TEST(fuzzkill_analysis, is_empty_tracks_len) {
    bt_alg alg;
    bt_alg_init(&alg);
    char err[64];
    BT_CHECK(bt_alg_parse("", &alg, err, sizeof err));
    BT_CHECK(bt_alg_is_empty(&alg));
    BT_CHECK_EQ(bt_alg_len(&alg), (size_t)0);
    bt_alg_free(&alg);
    bt_alg_init(&alg);
    BT_CHECK(bt_alg_parse("U", &alg, err, sizeof err));
    BT_CHECK(!bt_alg_is_empty(&alg));
    BT_CHECK_EQ(bt_alg_len(&alg), (size_t)1);
    bt_alg_free(&alg);
    bt_alg_init(&alg);
    BT_CHECK(bt_alg_parse("R U r' 2F M2 x", &alg, err, sizeof err));
    BT_CHECK(!bt_alg_is_empty(&alg));
    BT_CHECK_EQ(bt_alg_len(&alg), (size_t)6);
    bt_alg_free(&alg);
}

BT_TEST(fuzzkill_analysis, orientation_is_rotation_invariant) {
    state_list list;
    states(&list);
    for (size_t s = 0; s < list.count; s++) {
        for (uint8_t axis = 0; axis < 3; axis++) {
            for (uint8_t quarter = 1; quarter <= 3; quarter++) {
                bt_move rotation = bt_move_of_rotation(axis, quarter);
                uint8_t table[54];
                bt_move_table(rotation, table);
                bt_cube rotated = list.items[s];
                bt_cube_apply(&rotated, rotation);
                for (uint8_t e = 0; e < 12; e++) {
                    uint8_t pair[2];
                    bt_edge_stickers(e, pair);
                    uint8_t image = bt_piece_of(table[pair[0]]).index;
                    BT_CHECK_EQ(bt_edge_flip(&rotated, image), bt_edge_flip(&list.items[s], e));
                }
                for (uint8_t k = 0; k < 8; k++) {
                    uint8_t triple[3];
                    bt_corner_stickers(k, triple);
                    uint8_t image = bt_piece_of(table[triple[0]]).index;
                    BT_CHECK_EQ(bt_corner_twist(&rotated, image),
                                bt_corner_twist(&list.items[s], k));
                }
            }
        }
    }
}
