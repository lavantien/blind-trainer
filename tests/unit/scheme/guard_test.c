#include <string.h>

#include "common/bt_harness.h"
#include "core/analysis.h"
#include "scheme/classifier.h"
#include "scheme/memo.h"
#include "scheme/scheme.h"
#include "scheme/scramble.h"
#include "unit/core/death_test.h"

static void identity_occ(uint8_t occ[54]) {
    for (uint8_t i = 0; i < 54; i++) {
        occ[i] = i;
    }
}

BT_TEST(guard, swap_twist_block_rejects_both_stickers_of_one_piece) {
    uint8_t head = BT_SCHEME_EDGE_BUFFER_STICKER;
    uint8_t mate = bt_scheme_edge_mate(head);
    uint8_t block[7] = {head, mate, head, mate, head, mate, head};
    BT_CHECK(!bt_scheme_is_swap_twist_block(block, 7));
}

BT_TEST(guard, residue_eq_separates_equal_counts_with_different_content) {
    bt_residue left = {0};
    left.flip_count = 1;
    left.flips[0] = 5;
    bt_residue right = {0};
    right.flip_count = 1;
    right.flips[0] = 7;
    BT_CHECK(!bt_scheme_residue_eq(&left, &right));
    right.flips[0] = 5;
    left.twist_count = 1;
    left.twists[0] = (bt_twist){.corner = 2, .quarter = 1};
    right.twist_count = 1;
    right.twists[0] = (bt_twist){.corner = 3, .quarter = 1};
    BT_CHECK(!bt_scheme_residue_eq(&left, &right));
    right.twists[0] = left.twists[0];
    left.has_parity = true;
    right.has_parity = true;
    left.parity = (bt_parity){.edge = {1, 2}, .corner = {3, 4}};
    right.parity = (bt_parity){.edge = {1, 2}, .corner = {3, 5}};
    BT_CHECK(!bt_scheme_residue_eq(&left, &right));
    right.parity = left.parity;
    BT_CHECK(bt_scheme_residue_eq(&left, &right));
}

BT_TEST(guard, occ_is_legal_accepts_identity_and_rejects_moved_centers) {
    uint8_t occ[54];
    identity_occ(occ);
    BT_CHECK(bt_scheme_occ_is_legal(occ));
    uint8_t moved[54];
    identity_occ(moved);
    moved[48] = 49;
    moved[49] = 48;
    BT_CHECK(!bt_scheme_occ_is_legal(moved));
}

BT_TEST(guard, occ_is_legal_rejects_a_band_crossed_permutation) {
    uint8_t occ[54];
    identity_occ(occ);
    occ[0] = 24;
    occ[24] = 0;
    BT_CHECK(!bt_scheme_occ_is_legal(occ));
}

BT_TEST(guard, piece_perm_rejects_a_cross_band_head_slot) {
    uint8_t occ[54];
    identity_occ(occ);
    uint8_t corner_head = BT_SCHEME_CORNER_SUCC[0][0];
    uint8_t edge_pair[2];
    bt_edge_stickers(0, edge_pair);
    occ[corner_head] = edge_pair[0];
    occ[edge_pair[0]] = corner_head;
    uint8_t out[12];
    BT_CHECK(!bt_scheme_piece_perm(occ, BT_ORBIT_EDGE, out));
    BT_CHECK(!bt_scheme_piece_perm(occ, BT_ORBIT_CORNER, out));
}

static void guard_death_trace_non_permutation(void) {
    uint8_t occ[54];
    identity_occ(occ);
    occ[1] = 0;
    bt_trace trace;
    bt_scheme_trace(occ, BT_ORBIT_EDGE, &trace);
}

static void guard_death_same_piece_center_id(void) {
    (void)bt_scheme_same_piece(48, 0);
}

static void guard_death_twist_head_missing(void) {
    uint8_t occ[54];
    identity_occ(occ);
    const uint8_t *cyc = BT_SCHEME_CORNER_SUCC[0];
    uint8_t stickers[3];
    bt_corner_stickers(1, stickers);
    uint8_t head = bt_head_sticker((bt_piece){BT_PIECE_CORNER, 1});
    occ[cyc[0]] = stickers[0] == head ? stickers[1] : stickers[0];
    bt_corner_stickers(2, stickers);
    occ[cyc[1]] = stickers[0];
    bt_corner_stickers(3, stickers);
    occ[cyc[2]] = stickers[0];
    (void)bt_scheme_twist_sum(occ);
}

static void guard_death_head_sticker_short_name(void) {
    (void)bt_scheme_head_sticker("U");
}

BT_TEST(guard, contract_aborts_run_through_the_death_path) {
    bt_death_expect_code("guard.trace_aborts_on_a_non_permutation", "state must be a permutation",
                         134, guard_death_trace_non_permutation);
    bt_death_expect_code("guard.same_piece_aborts_on_a_center_id", "sticker id out of range", 134,
                         guard_death_same_piece_center_id);
    bt_death_expect_code("guard.head_sticker_aborts_on_a_short_name",
                         "piece name must have 2 or 3 faces", 134,
                         guard_death_head_sticker_short_name);
    bt_death_expect_code("guard.twist_state_aborts_when_the_head_sticker_is_missing", "twist head",
                         134, guard_death_twist_head_missing);
}
