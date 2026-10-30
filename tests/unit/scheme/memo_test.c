#include "common.h"
#include "death_test.h"
#include "scheme/memo.h"
#include "scheme/scramble.h"

#include "common/rng.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t pos;
    uint8_t sticker;
} bt_assign;

static void identity(uint8_t occ[54]) {
    for (uint8_t i = 0; i < 54; i++) {
        occ[i] = i;
    }
}

static void apply_alg_text(const char *text, bt_cube *cube) {
    bt_alg alg;
    bt_alg_init(&alg);
    char err[128];
    if (!bt_alg_parse(text, &alg, err, sizeof err)) {
        BT_ABORT("parse %s failed: %s", text, err);
    }
    bt_cube_apply_alg(cube, &alg);
    bt_alg_free(&alg);
}

static void after(const char *text, bt_cube *cube) {
    *cube = bt_cube_solved();
    apply_alg_text(text, cube);
}

static void occ_of(const bt_assign *assignments, size_t count, uint8_t occ[54]) {
    identity(occ);
    for (size_t i = 0; i < count; i++) {
        occ[assignments[i].pos] = assignments[i].sticker;
    }
}

static void round_trip(const uint8_t occ[54]) {
    bt_memo memo;
    bt_scheme_memo_occ(occ, &memo);
    uint8_t work[54];
    memcpy(work, occ, sizeof work);
    bt_scheme_apply_memo(work, &memo);
    uint8_t want[54];
    identity(want);
    BT_CHECK(memcmp(work, want, sizeof work) == 0);
}

static bool case_eq(const bt_case *a, const bt_case *b) {
    return (a->orbit == b->orbit && a->t1 == b->t1 && a->t2 == b->t2) != 0;
}

static void check_case_vec(const bt_case *got, size_t got_count, const bt_case *want,
                           size_t want_count) {
    BT_CHECK_EQ(got_count, want_count);
    for (size_t i = 0; i < want_count && i < got_count; i++) {
        BT_CHECK(case_eq(&got[i], &want[i]));
    }
}

static void check_letters(const char *got, size_t got_count, const char *want) {
    BT_CHECK_EQ(got_count, strlen(want));
    BT_CHECK(memcmp(got, want, strlen(want)) == 0);
}

static void run_memo_rejects_orientation_illegal(void) {
    uint8_t occ[54];
    identity(occ);
    uint8_t tmp = occ[24];
    occ[24] = occ[25];
    occ[25] = tmp;
    bt_memo memo;
    bt_scheme_memo_from_array(occ, &memo);
}

BT_TEST(memo, memo_rejects_orientation_illegal_cohesive_state) {
    bt_death_expect("memo.memo_rejects_orientation_illegal_cohesive_state", "legal state",
                    run_memo_rejects_orientation_illegal);
}

static void run_memo_rejects_parity_illegal(void) {
    uint8_t occ[54];
    identity(occ);
    uint8_t tmp = occ[24];
    occ[24] = occ[26];
    occ[26] = tmp;
    tmp = occ[25];
    occ[25] = occ[27];
    occ[27] = tmp;
    bt_memo memo;
    bt_scheme_memo_from_array(occ, &memo);
}

BT_TEST(memo, memo_rejects_parity_illegal_cohesive_state) {
    bt_death_expect("memo.memo_rejects_parity_illegal_cohesive_state", "legal state",
                    run_memo_rejects_parity_illegal);
}

BT_TEST(memo, corner_succ_cycles_are_pinned) {
    for (int p = 0; p < 8; p++) {
        BT_CHECK_EQ(bt_scheme_corner_succ(BT_SCHEME_CORNER_SUCC[p][0]),
                    BT_SCHEME_CORNER_SUCC[p][1]);
        BT_CHECK_EQ(bt_scheme_corner_succ(BT_SCHEME_CORNER_SUCC[p][1]),
                    BT_SCHEME_CORNER_SUCC[p][2]);
        BT_CHECK_EQ(bt_scheme_corner_succ(BT_SCHEME_CORNER_SUCC[p][2]),
                    BT_SCHEME_CORNER_SUCC[p][0]);
    }
    BT_CHECK_EQ(BT_SCHEME_CORNER_SUCC[2][0], 2);
    BT_CHECK_EQ(BT_SCHEME_CORNER_SUCC[2][1], 9);
    BT_CHECK_EQ(BT_SCHEME_CORNER_SUCC[2][2], 12);
    BT_CHECK_EQ(BT_SCHEME_CORNER_SUCC[0][0], 1);
    BT_CHECK_EQ(BT_SCHEME_CORNER_SUCC[0][1], 13);
    BT_CHECK_EQ(BT_SCHEME_CORNER_SUCC[0][2], 16);
    BT_CHECK_EQ(BT_SCHEME_CORNER_SUCC[3][0], 3);
    BT_CHECK_EQ(BT_SCHEME_CORNER_SUCC[3][1], 5);
    BT_CHECK_EQ(BT_SCHEME_CORNER_SUCC[3][2], 8);
    BT_CHECK_EQ(BT_SCHEME_CORNER_SUCC[5][0], 23);
    BT_CHECK_EQ(BT_SCHEME_CORNER_SUCC[5][1], 7);
    BT_CHECK_EQ(BT_SCHEME_CORNER_SUCC[5][2], 18);
}

BT_TEST(memo, corner_succ_conjugates_under_rotations) {
    const char *rotations[9] = {"x", "x2", "x'", "y", "y2", "y'", "z", "z2", "z'"};
    for (int r = 0; r < 9; r++) {
        bt_cube cube;
        after(rotations[r], &cube);
        for (uint8_t p = 0; p < 24; p++) {
            uint8_t moved = bt_cube_position_of(&cube, p);
            BT_CHECK_EQ(bt_cube_position_of(&cube, bt_scheme_corner_succ(p)),
                        bt_scheme_corner_succ(moved));
        }
    }
}

BT_TEST(memo, ideal_cycle_structure_pinned) {
    bt_scheme_cycles cycles;
    bt_case edge_case = {BT_ORBIT_EDGE, 24, 25};
    bt_scheme_ideal_cycle(&edge_case, &cycles);
    BT_CHECK_EQ(cycles.count, 2);
    BT_CHECK(cycles.items[0][0] == 26 && cycles.items[0][1] == 24 && cycles.items[0][2] == 25);
    BT_CHECK(cycles.items[1][0] == 32 && cycles.items[1][1] == 40 && cycles.items[1][2] == 36);
    bt_case corner_case = {BT_ORBIT_CORNER, 0, 1};
    bt_scheme_ideal_cycle(&corner_case, &cycles);
    BT_CHECK_EQ(cycles.count, 3);
    BT_CHECK(cycles.items[0][0] == 2 && cycles.items[0][1] == 0 && cycles.items[0][2] == 1);
    BT_CHECK(cycles.items[1][0] == 9 && cycles.items[1][1] == 17 && cycles.items[1][2] == 13);
    BT_CHECK(cycles.items[2][0] == 12 && cycles.items[2][1] == 4 && cycles.items[2][2] == 16);
    bt_scheme_ideal_cycle_at(BT_ORBIT_EDGE, 24, 27, 43, &cycles);
    BT_CHECK_EQ(cycles.count, 2);
    BT_CHECK(cycles.items[0][0] == 24 && cycles.items[0][1] == 27 && cycles.items[0][2] == 43);
    BT_CHECK(cycles.items[1][0] == 40 && cycles.items[1][1] == 28 && cycles.items[1][2] == 37);
}

BT_TEST(memo, memo_of_solved_is_empty) {
    bt_cube cube = bt_cube_solved();
    bt_memo memo;
    bt_scheme_memo(&cube, &memo);
    BT_CHECK_EQ(memo.edge_letter_count, (size_t)0);
    BT_CHECK_EQ(memo.corner_letter_count, (size_t)0);
    BT_CHECK_EQ(memo.edge_pair_count, (size_t)0);
    BT_CHECK_EQ(memo.corner_pair_count, (size_t)0);
    BT_CHECK_EQ(memo.edge_buffer_count, (size_t)0);
    BT_CHECK_EQ(memo.corner_buffer_count, (size_t)0);
    bt_residue empty;
    memset(&empty, 0, sizeof empty);
    BT_CHECK(bt_scheme_residue_eq(&memo.residue, &empty));
    bt_residue_class class = bt_scheme_residue_class(&memo.residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_NONE);
}

BT_TEST(memo, memo_of_four_cycles_is_pair_plus_parity_per_orbit) {
    const bt_assign assigns[] = {
        {26, 27}, {32, 28}, {24, 26}, {40, 32}, {25, 24}, {36, 40}, {27, 25},
        {28, 36}, {3, 2},   {5, 9},   {8, 12},  {0, 3},   {17, 5},  {4, 8},
        {1, 0},   {13, 17}, {16, 4},  {2, 1},   {9, 13},  {12, 16},
    };
    uint8_t occ[54];
    occ_of(assigns, sizeof assigns / sizeof assigns[0], occ);
    bt_memo memo;
    bt_scheme_memo_occ(occ, &memo);
    check_letters(memo.edge_letters, memo.edge_letter_count, "DBA");
    bt_case edge_want[1] = {{BT_ORBIT_EDGE, 27, 25}};
    check_case_vec(memo.edge_pairs, memo.edge_pair_count, edge_want, 1);
    BT_CHECK_EQ(memo.edge_buffers[0], 26);
    BT_CHECK_EQ(memo.edge_buffer_count, (size_t)1);
    check_letters(memo.corner_letters, memo.corner_letter_count, "BAD");
    bt_case corner_want[1] = {{BT_ORBIT_CORNER, 1, 0}};
    check_case_vec(memo.corner_pairs, memo.corner_pair_count, corner_want, 1);
    BT_CHECK_EQ(memo.corner_buffers[0], 2);
    BT_CHECK_EQ(memo.corner_buffer_count, (size_t)1);
    BT_CHECK(memo.residue.has_parity);
    BT_CHECK_EQ(memo.residue.parity.edge[0], 26);
    BT_CHECK_EQ(memo.residue.parity.edge[1], 24);
    BT_CHECK_EQ(memo.residue.parity.corner[0], 2);
    BT_CHECK_EQ(memo.residue.parity.corner[1], 3);
    BT_CHECK_EQ(memo.residue.flip_count, 0);
    BT_CHECK_EQ(memo.residue.twist_count, 0);
    bt_residue_class class = bt_scheme_residue_class(&memo.residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_PARITY);
    round_trip(occ);
}

BT_TEST(memo, memo_of_double_edge_swap_crosses_blocks) {
    const bt_assign assigns[] = {
        {26, 24}, {32, 40}, {24, 26}, {40, 32}, {25, 27}, {36, 28}, {27, 25}, {28, 36},
    };
    uint8_t occ[54];
    occ_of(assigns, sizeof assigns / sizeof assigns[0], occ);
    bt_memo memo;
    bt_scheme_memo_occ(occ, &memo);
    check_letters(memo.edge_letters, memo.edge_letter_count, "ADBD");
    bt_case edge_want[2] = {{BT_ORBIT_EDGE, 24, 27}, {BT_ORBIT_EDGE, 25, 27}};
    check_case_vec(memo.edge_pairs, memo.edge_pair_count, edge_want, 2);
    BT_CHECK_EQ(memo.edge_buffers[0], 26);
    BT_CHECK_EQ(memo.edge_buffers[1], 26);
    bt_residue empty;
    memset(&empty, 0, sizeof empty);
    BT_CHECK(bt_scheme_residue_eq(&memo.residue, &empty));
    BT_CHECK_EQ(memo.corner_pair_count, (size_t)0);
    round_trip(occ);
}

static int compare_u8(const void *a, const void *b) {
    return (int)*(const uint8_t *)a - (int)*(const uint8_t *)b;
}

BT_TEST(memo, memo_of_two_flips_is_residue_only) {
    const bt_assign assigns[] = {{26, 32}, {32, 26}, {27, 28}, {28, 27}};
    uint8_t occ[54];
    occ_of(assigns, sizeof assigns / sizeof assigns[0], occ);
    bt_memo memo;
    bt_scheme_memo_occ(occ, &memo);
    BT_CHECK_EQ(memo.edge_letter_count, (size_t)0);
    BT_CHECK_EQ(memo.edge_pair_count, (size_t)0);
    qsort(memo.residue.flips, memo.residue.flip_count, 1, compare_u8);
    BT_CHECK_EQ(memo.residue.flip_count, 2);
    BT_CHECK_EQ(memo.residue.flips[0], 26);
    BT_CHECK_EQ(memo.residue.flips[1], 27);
    BT_CHECK(!memo.residue.has_parity);
    bt_residue_class class = bt_scheme_residue_class(&memo.residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_FLIP_PAIR);
    BT_CHECK_EQ(class.a, 26);
    BT_CHECK_EQ(class.b, 27);
    round_trip(occ);
}

BT_TEST(memo, memo_of_flip_mid_cycle_absorbs_into_pair) {
    const bt_assign assigns[] = {
        {26, 25}, {32, 36}, {24, 26}, {40, 32}, {36, 24}, {25, 40}, {27, 28}, {28, 27},
    };
    uint8_t occ[54];
    occ_of(assigns, sizeof assigns / sizeof assigns[0], occ);
    bt_memo memo;
    bt_scheme_memo_occ(occ, &memo);
    check_letters(memo.edge_letters, memo.edge_letter_count, "BQ");
    bt_case edge_want[1] = {{BT_ORBIT_EDGE, 25, 40}};
    check_case_vec(memo.edge_pairs, memo.edge_pair_count, edge_want, 1);
    qsort(memo.residue.flips, memo.residue.flip_count, 1, compare_u8);
    BT_CHECK_EQ(memo.residue.flip_count, 2);
    BT_CHECK_EQ(memo.residue.flips[0], 26);
    BT_CHECK_EQ(memo.residue.flips[1], 27);
    BT_CHECK(!memo.residue.has_parity);
    round_trip(occ);
}

BT_TEST(memo, memo_of_two_disjoint_cycles_floats_the_buffer) {
    const bt_assign assigns[] = {
        {26, 25}, {32, 36}, {25, 24}, {36, 40}, {24, 26}, {40, 32},
        {46, 45}, {42, 38}, {45, 44}, {38, 34}, {44, 46}, {34, 42},
    };
    uint8_t occ[54];
    occ_of(assigns, sizeof assigns / sizeof assigns[0], occ);
    bt_memo memo;
    bt_scheme_memo_occ(occ, &memo);
    check_letters(memo.edge_letters, memo.edge_letter_count, "BAWV");
    bt_case edge_want[2] = {{BT_ORBIT_EDGE, 25, 24}, {BT_ORBIT_EDGE, 46, 45}};
    check_case_vec(memo.edge_pairs, memo.edge_pair_count, edge_want, 2);
    BT_CHECK_EQ(memo.edge_buffers[0], 26);
    BT_CHECK_EQ(memo.edge_buffers[1], 44);
    bt_residue empty;
    memset(&empty, 0, sizeof empty);
    BT_CHECK(bt_scheme_residue_eq(&memo.residue, &empty));
    round_trip(occ);
}

BT_TEST(memo, round_trip_on_face_turns) {
    const char *texts[8] = {"U", "R", "F", "L2", "D'", "B2 U", "R U R' U'", "F2 D2 L2"};
    for (int i = 0; i < 8; i++) {
        bt_cube cube;
        after(texts[i], &cube);
        bt_memo memo;
        bt_scheme_memo(&cube, &memo);
        uint8_t work[54];
        bt_scheme_state_array(&cube, work);
        bt_scheme_apply_memo(work, &memo);
        uint8_t want[54];
        identity(want);
        BT_CHECK(memcmp(work, want, sizeof work) == 0);
    }
}

static void run_corner_succ_rejects_edge(void) {
    bt_scheme_corner_succ(26);
}

static void run_edge_mate_rejects_corner(void) {
    bt_scheme_edge_mate(2);
}

static void run_walk_parity_one_orbit(void) {
    uint8_t occ[54];
    identity(occ);
    bt_scheme_apply_aligned_swap(occ, BT_ORBIT_EDGE, 26, 24);
    bt_memo memo;
    bt_scheme_memo_occ(occ, &memo);
}

BT_TEST(memo, corner_succ_rejects_edge_sticker) {
    bt_death_expect("memo.corner_succ_rejects_edge_sticker", "corner_succ on non corner sticker 26",
                    run_corner_succ_rejects_edge);
}

BT_TEST(memo, edge_mate_rejects_corner_sticker) {
    bt_death_expect("memo.edge_mate_rejects_corner_sticker", "edge_mate on non edge sticker 2",
                    run_edge_mate_rejects_corner);
}

BT_TEST(memo, walk_panics_when_parity_hits_one_orbit) {
    bt_death_expect("memo.walk_panics_when_parity_hits_one_orbit", "parity must hit both orbits",
                    run_walk_parity_one_orbit);
}

BT_TEST(memo, stale_pending_pointing_at_reserved_piece_is_dropped) {
    uint8_t occ[54];
    identity(occ);
    bt_scheme_cycles cycles;
    bt_scheme_ideal_cycle_at(BT_ORBIT_EDGE, 26, 24, 25, &cycles);
    bt_scheme_apply_sticker_cycles(occ, &cycles);
    bt_orbit_walk expected;
    uint8_t probe[54];
    memcpy(probe, occ, sizeof probe);
    bt_scheme_walk_init(&expected, BT_ORBIT_EDGE, probe);
    bt_scheme_walk_run(&expected);
    bt_orbit_walk walk;
    uint8_t work[54];
    memcpy(work, occ, sizeof work);
    bt_scheme_walk_init(&walk, BT_ORBIT_EDGE, work);
    walk.pending_count = 1;
    walk.pending[0] = 33;
    walk.reserved[bt_piece_of(33).index] = true;
    bt_scheme_walk_run(&walk);
    BT_CHECK_EQ(walk.letter_count, expected.letter_count);
    BT_CHECK(memcmp(walk.letters, expected.letters, walk.letter_count) == 0);
    BT_CHECK_EQ(walk.pair_count, expected.pair_count);
    for (size_t i = 0; i < walk.pair_count; i++) {
        BT_CHECK(case_eq(&walk.pairs[i], &expected.pairs[i]));
    }
    BT_CHECK_EQ(walk.buffer_count, expected.buffer_count);
    BT_CHECK(memcmp(walk.buffers, expected.buffers, walk.buffer_count) == 0);
    BT_CHECK_EQ(walk.has_parity, expected.has_parity);
}

static void run_walk_pair_guard_same_piece(void) {
    const bt_assign assigns[] = {{26, 24}, {24, 24}};
    uint8_t occ[54];
    occ_of(assigns, 2, occ);
    bt_orbit_walk walk;
    bt_scheme_walk_init(&walk, BT_ORBIT_EDGE, occ);
    bt_scheme_walk_run(&walk);
}

static void run_walk_spin_budget(void) {
    const bt_assign assigns[] = {{2, 9}, {9, 9}};
    uint8_t occ[54];
    occ_of(assigns, 2, occ);
    bt_orbit_walk walk;
    bt_scheme_walk_init(&walk, BT_ORBIT_CORNER, occ);
    bt_scheme_walk_run(&walk);
}

static void run_walk_letter_budget(void) {
    const bt_assign assigns[] = {
        {24, 36}, {25, 27}, {26, 28}, {27, 33}, {28, 40},
        {32, 25}, {33, 24}, {36, 39}, {39, 24}, {40, 39},
    };
    uint8_t occ[54];
    occ_of(assigns, 10, occ);
    bt_orbit_walk walk;
    bt_scheme_walk_init(&walk, BT_ORBIT_EDGE, occ);
    bt_scheme_walk_run(&walk);
}

BT_TEST(memo, walk_pair_guard_rejects_same_piece_targets) {
    bt_death_expect("memo.walk_pair_guard_rejects_same_piece_targets", "t2 on t1 piece",
                    run_walk_pair_guard_same_piece);
}

BT_TEST(memo, walk_spin_budget_stops_a_stalled_zero_letter_loop) {
    bt_death_expect("memo.walk_spin_budget_stops_a_stalled_zero_letter_loop",
                    "memo walk spin budget exceeded", run_walk_spin_budget);
}

BT_TEST(memo, walk_letter_budget_stops_an_unbounded_pair_loop) {
    bt_death_expect("memo.walk_letter_budget_stops_an_unbounded_pair_loop",
                    "memo walk letter budget exceeded", run_walk_letter_budget);
}

BT_TEST(memo, walk_skips_reserved_displaced_pieces_in_both_scans) {
    const bt_assign assigns[] = {
        {24, 47}, {40, 30}, {47, 24}, {30, 40}, {25, 27}, {36, 28}, {27, 25}, {28, 36},
    };
    uint8_t occ[54];
    occ_of(assigns, sizeof assigns / sizeof assigns[0], occ);
    bt_orbit_walk walk;
    bt_scheme_walk_init(&walk, BT_ORBIT_EDGE, occ);
    walk.reserved[0] = true;
    bt_scheme_walk_run(&walk);
    check_letters(walk.letters, walk.letter_count, "DAXA");
    bt_case want[2] = {{BT_ORBIT_EDGE, 27, 24}, {BT_ORBIT_EDGE, 47, 24}};
    check_case_vec(walk.pairs, walk.pair_count, want, 2);
    BT_CHECK_EQ(walk.buffers[0], 25);
    BT_CHECK_EQ(walk.buffers[1], 25);
    BT_CHECK(!walk.has_parity);
}

BT_TEST(memo, residue_class_eq_distinguishes_every_field) {
    bt_residue_class base = {BT_RESIDUE_TWIST_PAIR, 26, 32, 9};
    bt_residue_class copy = base;
    BT_CHECK(bt_scheme_residue_class_eq(base, copy));
    bt_residue_class other_kind = base;
    other_kind.kind = BT_RESIDUE_PARITY;
    BT_CHECK(!bt_scheme_residue_class_eq(base, other_kind));
    bt_residue_class other_a = base;
    other_a.a = 25;
    BT_CHECK(!bt_scheme_residue_class_eq(base, other_a));
    bt_residue_class other_b = base;
    other_b.b = 25;
    BT_CHECK(!bt_scheme_residue_class_eq(base, other_b));
    bt_residue_class other_c = base;
    other_c.c = 3;
    BT_CHECK(!bt_scheme_residue_class_eq(base, other_c));
}
