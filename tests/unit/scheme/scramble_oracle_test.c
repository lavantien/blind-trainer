#include "scheme/classifier.h"
#include "scheme/memo.h"
#include "scheme/pairs.h"
#include "scheme/scheme.h"
#include "scheme/scramble.h"

#include "common/bt_harness.h"
#include "common/rng.h"

#include <stdlib.h>
#include <string.h>

static void random_cube(uint64_t seed, bt_cube *cube) {
    bt_rng rng;
    bt_rng_seed(&rng, seed);
    const char *faces[6] = {"U", "D", "L", "R", "F", "B"};
    const char *suffixes[3] = {"", "'", "2"};
    size_t len = 8 + (size_t)bt_rng_below(&rng, 17);
    char text[256];
    size_t at = 0;
    size_t last = 6;
    for (size_t i = 0; i < len; i++) {
        size_t f = (size_t)bt_rng_below(&rng, 6);
        while (f == last) {
            f = (size_t)bt_rng_below(&rng, 6);
        }
        last = f;
        at += (size_t)snprintf(text + at, sizeof text - at, "%s%s ", faces[f],
                               suffixes[bt_rng_below(&rng, 3)]);
    }
    if (at > 0) {
        text[at - 1] = '\0';
    }
    bt_alg alg;
    bt_alg_init(&alg);
    char err[128];
    if (!bt_alg_parse(text, &alg, err, sizeof err)) {
        BT_ABORT("parse %s failed: %s", text, err);
    }
    *cube = bt_cube_solved();
    bt_cube_apply_alg(cube, &alg);
    bt_alg_free(&alg);
}

BT_TEST(scramble_oracle, occ_orientation_predicates_match_bt_core) {
    for (uint64_t seed = 0; seed < 400; seed++) {
        bt_cube cube;
        random_cube(seed, &cube);
        uint8_t occ[54];
        bt_scheme_state_array(&cube, occ);
        for (uint8_t slot = 0; slot < 12; slot++) {
            BT_CHECK_EQ(bt_scheme_edge_flipped(occ, slot), bt_edge_flip(&cube, slot));
        }
        for (uint8_t slot = 0; slot < 8; slot++) {
            int8_t twist = bt_corner_twist(&cube, slot);
            uint8_t mapped;
            if (twist == 0) {
                mapped = 0;
            } else if (twist == 1) {
                mapped = 1;
            } else {
                mapped = 2;
            }
            BT_CHECK_EQ(bt_scheme_corner_twist_state(occ, slot), mapped);
        }
    }
}

static bool case_set_contains(const bt_case *cases, size_t count, uint8_t t1, uint8_t t2) {
    for (size_t i = 0; i < count; i++) {
        if (cases[i].t1 == t1 && cases[i].t2 == t2) {
            return true;
        }
    }
    return false;
}

BT_TEST(scramble_oracle, random_occ_states_round_trip_through_memo) {
    bt_case edge_cases[BT_PAIRS_MAX];
    bt_case corner_cases[BT_PAIRS_MAX];
    size_t edge_count = bt_scheme_enumerate_cases(BT_ORBIT_EDGE, edge_cases);
    size_t corner_count = bt_scheme_enumerate_cases(BT_ORBIT_CORNER, corner_cases);
    for (uint64_t seed = 0; seed < 2000; seed++) {
        uint8_t occ[54];
        bt_scheme_random_occ(seed, occ);
        bt_memo memo;
        bt_scheme_memo_from_array(occ, &memo);
        uint8_t work[54];
        memcpy(work, occ, sizeof work);
        bt_scheme_apply_memo(work, &memo);
        for (int i = 0; i < 54; i++) {
            BT_CHECK_EQ(work[i], (uint8_t)i);
        }
        bt_memo again;
        bt_scheme_memo_from_array(occ, &again);
        BT_CHECK(bt_scheme_memo_eq(&memo, &again));
        for (size_t i = 0; i < memo.edge_pair_count; i++) {
            BT_CHECK(case_set_contains(edge_cases, edge_count, memo.edge_pairs[i].t1,
                                       memo.edge_pairs[i].t2));
        }
        for (size_t i = 0; i < memo.corner_pair_count; i++) {
            BT_CHECK(case_set_contains(corner_cases, corner_count, memo.corner_pairs[i].t1,
                                       memo.corner_pairs[i].t2));
        }
        BT_CHECK_EQ(memo.edge_pair_count, memo.edge_buffer_count);
        BT_CHECK_EQ(memo.corner_pair_count, memo.corner_buffer_count);
        BT_CHECK(memo.edge_letter_count == 2 * memo.edge_pair_count ||
                 memo.edge_letter_count == 2 * memo.edge_pair_count + 1);
        BT_CHECK(memo.corner_letter_count == 2 * memo.corner_pair_count ||
                 memo.corner_letter_count == 2 * memo.corner_pair_count + 1);
        BT_CHECK(bt_scheme_occ_is_legal(occ));
    }
}

BT_TEST(scramble_oracle, random_occ_covers_orientation_and_parity_shapes) {
    uint32_t with_flips = 0;
    uint32_t with_twists = 0;
    uint32_t with_parity_memo = 0;
    for (uint64_t seed = 0; seed < 500; seed++) {
        uint8_t occ[54];
        bt_scheme_random_occ(seed, occ);
        bool flipped = false;
        for (uint8_t slot = 0; slot < 12; slot++) {
            if (bt_scheme_edge_flipped(occ, slot)) {
                flipped = true;
            }
        }
        bool twisted = false;
        for (uint8_t slot = 0; slot < 8; slot++) {
            if (bt_scheme_corner_twist_state(occ, slot) != 0) {
                twisted = true;
            }
        }
        bt_memo memo;
        bt_scheme_memo_from_array(occ, &memo);
        with_flips += flipped ? 1 : 0;
        with_twists += twisted ? 1 : 0;
        with_parity_memo += memo.residue.has_parity ? 1 : 0;
    }
    BT_CHECK(with_flips > 300);
    BT_CHECK(with_twists > 200);
    BT_CHECK(with_parity_memo > 150);
}

BT_TEST(scramble_oracle, random_state_cubes_are_legal_unsolved_and_center_fixed) {
    for (uint64_t seed = 0; seed < 2000; seed++) {
        bt_cube cube;
        bt_scheme_random_state(seed, &cube);
        BT_CHECK(!bt_cube_is_solved(&cube));
        BT_CHECK(bt_cube_centers_fixed(&cube));
        uint8_t occ[54];
        bt_scheme_state_array(&cube, occ);
        uint8_t want[54];
        bt_scheme_random_occ(seed, want);
        BT_CHECK(memcmp(occ, want, sizeof occ) == 0);
        int twist = 0;
        for (uint8_t corner = 0; corner < 8; corner++) {
            twist += bt_corner_twist(&cube, corner);
        }
        BT_CHECK_EQ(twist % 3, 0);
        int flips = 0;
        for (uint8_t edge = 0; edge < 12; edge++) {
            flips += bt_edge_flip(&cube, edge) ? 1 : 0;
        }
        BT_CHECK_EQ(flips % 2, 0);
    }
}

BT_TEST(scramble_oracle, prop_random_occ_is_legal_and_round_trips) {
    for (uint64_t seed = 0; seed < 512; seed++) {
        uint8_t occ[54];
        bt_scheme_random_occ(seed, occ);
        BT_CHECK(bt_scheme_occ_is_legal(occ));
        bool is_identity = true;
        for (int i = 0; i < 54; i++) {
            if (occ[i] != (uint8_t)i) {
                is_identity = false;
            }
        }
        BT_CHECK(!is_identity);
        bt_memo memo;
        bt_scheme_memo_from_array(occ, &memo);
        uint8_t work[54];
        memcpy(work, occ, sizeof work);
        bt_scheme_apply_memo(work, &memo);
        for (int i = 0; i < 54; i++) {
            BT_CHECK_EQ(work[i], (uint8_t)i);
        }
        bt_memo again;
        bt_scheme_memo_from_array(occ, &again);
        BT_CHECK(bt_scheme_memo_eq(&memo, &again));
        BT_CHECK_EQ(memo.edge_pair_count, memo.edge_buffer_count);
        BT_CHECK_EQ(memo.corner_pair_count, memo.corner_buffer_count);
        BT_CHECK(memo.edge_letter_count == 2 * memo.edge_pair_count ||
                 memo.edge_letter_count == 2 * memo.edge_pair_count + 1);
        BT_CHECK(memo.corner_letter_count == 2 * memo.corner_pair_count ||
                 memo.corner_letter_count == 2 * memo.corner_pair_count + 1);
    }
}
