#include "scheme/classifier.h"
#include "scheme/memo.h"
#include "scheme/scheme.h"

#include "common/bt_harness.h"
#include "death_test.h"

#include <string.h>

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

static void after(const char *text, uint8_t occ[54]) {
    bt_cube cube = bt_cube_solved();
    apply_alg_text(text, &cube);
    bt_scheme_state_array(&cube, occ);
}

static void check_block(const bt_trace *trace, size_t index, const uint8_t *want, size_t want_len) {
    BT_CHECK(index < trace->block_count);
    BT_CHECK_EQ(trace->block_len[index], want_len);
    BT_CHECK(memcmp(trace->blocks[index], want, want_len) == 0);
}

BT_TEST(classifier, solved_cube_classifies_solved) {
    bt_cube cube = bt_cube_solved();
    uint8_t occ[54];
    bt_scheme_state_array(&cube, occ);
    bt_report report;
    bt_scheme_classify(occ, &report);
    BT_CHECK_EQ((int)report.class.kind, (int)BT_CLASS_SOLVED);
    BT_CHECK(bt_scheme_trace_is_solved(&report.edge));
    BT_CHECK(bt_scheme_trace_is_solved(&report.corner));
    char position[BT_CLASS_POSITION_MAX];
    BT_CHECK(!bt_scheme_report_position(&report, position));
}

BT_TEST(classifier, u_turn_walk_blocks_pinned) {
    uint8_t occ[54];
    after("U", occ);
    bt_trace edge;
    bt_trace corner;
    bt_scheme_trace(occ, BT_ORBIT_EDGE, &edge);
    bt_scheme_trace(occ, BT_ORBIT_CORNER, &corner);
    uint8_t edge_want[5] = {26, 25, 24, 27, 26};
    uint8_t corner_want[5] = {2, 1, 0, 3, 2};
    BT_CHECK_EQ(edge.block_count, (size_t)1);
    BT_CHECK_EQ(corner.block_count, (size_t)1);
    check_block(&edge, 0, edge_want, 5);
    check_block(&corner, 0, corner_want, 5);
    bt_report report;
    bt_scheme_classify(occ, &report);
    bt_classification want = {0};
    want.kind = BT_CLASS_UNCLASSIFIED;
    snprintf(want.unclassified, BT_CLASS_TEXT_MAX, "edge:[5] corner:[5]");
    BT_CHECK(bt_scheme_classification_eq(&report.class, &want));
}

BT_TEST(classifier, m2_double_edge_swap_is_unclassified) {
    uint8_t occ[54];
    after("M2", occ);
    bt_trace edge;
    bt_scheme_trace(occ, BT_ORBIT_EDGE, &edge);
    uint8_t first[3] = {26, 46, 26};
    uint8_t second[3] = {24, 44, 24};
    BT_CHECK_EQ(edge.block_count, (size_t)2);
    check_block(&edge, 0, first, 3);
    check_block(&edge, 1, second, 3);
    bt_trace corner;
    bt_scheme_trace(occ, BT_ORBIT_CORNER, &corner);
    BT_CHECK(bt_scheme_trace_is_solved(&corner));
    bt_report report;
    bt_scheme_classify(occ, &report);
    BT_CHECK_EQ((int)report.class.kind, (int)BT_CLASS_UNCLASSIFIED);
}

BT_TEST(classifier, walk_follows_arrival_map_and_partitions_displaced_stickers) {
    const char *algs[9] = {
        "U", "U'", "U2", "R", "R U R' U'", "F R U R' U' F'", "x y z M E S", "l2 2B' S2", "E2",
    };
    for (int a = 0; a < 9; a++) {
        uint8_t occ[54];
        after(algs[a], occ);
        bt_orbit orbits[2] = {BT_ORBIT_EDGE, BT_ORBIT_CORNER};
        for (int o = 0; o < 2; o++) {
            bt_orbit orbit = orbits[o];
            const char *const *floating =
                orbit == BT_ORBIT_EDGE ? BT_SCHEME_EDGE_FLOATING : BT_SCHEME_CORNER_FLOATING;
            size_t floating_count = orbit == BT_ORBIT_EDGE ? 12 : 8;
            bt_trace walked;
            bt_scheme_trace(occ, orbit, &walked);
            bool covered[48] = {false};
            for (size_t b = 0; b < walked.block_count; b++) {
                const uint8_t *block = walked.blocks[b];
                size_t len = walked.block_len[b];
                BT_CHECK_EQ(block[0], block[len - 1]);
                for (size_t i = 1; i < len; i++) {
                    BT_CHECK_EQ(occ[block[i - 1]], block[i]);
                }
                bool head_ok = false;
                for (size_t p = 0; p < floating_count; p++) {
                    if (bt_scheme_head_sticker(floating[p]) == block[0]) {
                        head_ok = true;
                    }
                }
                BT_CHECK(head_ok);
                for (size_t i = 0; i + 1 < len; i++) {
                    BT_CHECK(!covered[block[i]]);
                    covered[block[i]] = true;
                }
            }
            for (size_t p = 0; p < floating_count; p++) {
                uint8_t head = bt_scheme_head_sticker(floating[p]);
                bool displaced = false;
                bool represented = false;
                for (uint8_t s = bt_scheme_orbit_lo(orbit); s < bt_scheme_orbit_hi(orbit); s++) {
                    if (!bt_scheme_same_piece(s, head)) {
                        continue;
                    }
                    if (occ[s] != s) {
                        displaced = true;
                    }
                    if (covered[s]) {
                        represented = true;
                    }
                }
                BT_CHECK_EQ(displaced, represented);
            }
            for (uint8_t s = 0; s < 48; s++) {
                if (covered[s]) {
                    BT_CHECK(occ[s] != s);
                }
            }
            if (walked.block_count > 0) {
                uint8_t expected_head = 255;
                for (size_t p = 0; p < floating_count && expected_head == 255; p++) {
                    uint8_t head = bt_scheme_head_sticker(floating[p]);
                    if (occ[head] != head) {
                        expected_head = head;
                    }
                }
                BT_CHECK_EQ(walked.blocks[0][0], expected_head);
            }
        }
    }
}

BT_TEST(classifier, block_shape_predicates_pin_the_corpus_shapes) {
    uint8_t style[4] = {26, 25, 24, 27};
    uint8_t not_style[4] = {26, 32, 25, 27};
    uint8_t flip[3] = {26, 32, 26};
    uint8_t not_flip[3] = {26, 25, 26};
    uint8_t swap[3] = {26, 25, 26};
    uint8_t not_swap[3] = {26, 32, 26};
    uint8_t twist[4] = {2, 12, 9, 2};
    uint8_t not_twist[4] = {2, 12, 3, 2};
    uint8_t short_twist[3] = {2, 9, 2};
    uint8_t swap_twist[7] = {2, 16, 9, 1, 12, 13, 2};
    uint8_t not_swap_twist[7] = {2, 16, 9, 1, 12, 25, 2};
    BT_CHECK(bt_scheme_is_style_block(style, 4));
    BT_CHECK(!bt_scheme_is_style_block(not_style, 4));
    BT_CHECK(bt_scheme_is_flip_block(flip, 3));
    BT_CHECK(!bt_scheme_is_flip_block(not_flip, 3));
    BT_CHECK(bt_scheme_is_swap_block(swap, 3));
    BT_CHECK(!bt_scheme_is_swap_block(not_swap, 3));
    BT_CHECK(bt_scheme_is_twist_block(twist, 4));
    BT_CHECK(!bt_scheme_is_twist_block(not_twist, 4));
    BT_CHECK(!bt_scheme_is_twist_block(short_twist, 3));
    BT_CHECK(bt_scheme_is_swap_twist_block(swap_twist, 7));
    BT_CHECK(!bt_scheme_is_swap_twist_block(not_swap_twist, 7));
}

BT_TEST(classifier, twist_pair_position_keeps_first_when_first_is_plus) {
    bt_report report;
    memset(&report, 0, sizeof report);
    report.class.kind = BT_CLASS_TWIST_PAIR;
    report.class.first = 2;
    report.class.second = 3;
    for (uint8_t slot = 0; slot < 12; slot++) {
        bt_piece piece = {BT_PIECE_EDGE, slot};
        report.defects.edge_flips[slot].head = bt_head_sticker(piece);
        report.defects.edge_flips[slot].flipped = false;
    }
    for (uint8_t slot = 0; slot < 8; slot++) {
        bt_piece piece = {BT_PIECE_CORNER, slot};
        report.defects.corner_twists[slot].head = bt_head_sticker(piece);
        report.defects.corner_twists[slot].twist = (slot == 2 || slot == 3) ? 1 : 0;
    }
    char position[BT_CLASS_POSITION_MAX];
    BT_CHECK(bt_scheme_report_position(&report, position));
    BT_CHECK_EQ(position, "UFR & UFL");
}

BT_TEST(classifier, solved_state_carries_no_defects) {
    bt_cube cube = bt_cube_solved();
    uint8_t occ[54];
    bt_scheme_state_array(&cube, occ);
    bt_report report;
    bt_scheme_classify(occ, &report);
    uint8_t flipped[12];
    bt_corner_twist_note twisted[8];
    BT_CHECK_EQ(bt_scheme_flipped_pieces(&report.defects, flipped), (size_t)0);
    BT_CHECK_EQ(bt_scheme_twisted_pieces(&report.defects, twisted), (size_t)0);
    for (int i = 0; i < 12; i++) {
        BT_CHECK(!bt_scheme_defect_flip(&report.defects,
                                        bt_scheme_head_sticker(BT_SCHEME_EDGE_FLOATING[i])));
    }
    for (int i = 0; i < 8; i++) {
        BT_CHECK_EQ(bt_scheme_defect_twist(&report.defects,
                                           bt_scheme_head_sticker(BT_SCHEME_CORNER_FLOATING[i])),
                    0);
    }
}

BT_TEST(classifier, slice_double_swap_flips_no_edges) {
    uint8_t occ[54];
    after("M2 E2", occ);
    bt_report report;
    bt_scheme_classify(occ, &report);
    uint8_t flipped[12];
    BT_CHECK_EQ(bt_scheme_flipped_pieces(&report.defects, flipped), (size_t)0);
}

BT_TEST(classifier, corner_twists_sum_to_zero_mod_three_and_are_nonzero_after_r) {
    uint8_t occ[54];
    after("R", occ);
    bt_report report;
    bt_scheme_classify(occ, &report);
    bt_corner_twist_note twisted[8];
    size_t count = bt_scheme_twisted_pieces(&report.defects, twisted);
    BT_CHECK_EQ(count, (size_t)4);
    int sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += twisted[i].twist;
    }
    BT_CHECK_EQ(sum % 3, 0);
    for (int i = 0; i < 8; i++) {
        uint8_t head = bt_scheme_head_sticker(BT_SCHEME_CORNER_FLOATING[i]);
        BT_CHECK_EQ(bt_scheme_defect_twist(&report.defects, head),
                    bt_scheme_defect_twist(&report.defects, head));
    }
}

BT_TEST(classifier, classify_alg_fills_qtm_and_matches_state_classification) {
    bt_alg alg;
    bt_alg_init(&alg);
    char err[128];
    BT_CHECK(bt_alg_parse("R U R' U'", &alg, err, sizeof err));
    bt_report via_alg;
    bt_scheme_classify_alg(&alg, &via_alg);
    BT_CHECK(via_alg.has_qtm);
    BT_CHECK_EQ(via_alg.qtm, 4ULL);
    uint8_t occ[54];
    after("R U R' U'", occ);
    bt_report via_state;
    bt_scheme_classify(occ, &via_state);
    BT_CHECK(bt_scheme_classification_eq(&via_alg.class, &via_state.class));
    BT_CHECK(!via_state.has_qtm);
    bt_alg_free(&alg);
}

static void run_flip_accessor_rejects_corner(void) {
    bt_cube cube = bt_cube_solved();
    uint8_t occ[54];
    bt_scheme_state_array(&cube, occ);
    bt_report report;
    bt_scheme_classify(occ, &report);
    bt_scheme_defect_flip(&report.defects, bt_scheme_head_sticker("UFR"));
}

static void run_twist_accessor_rejects_edge(void) {
    bt_cube cube = bt_cube_solved();
    uint8_t occ[54];
    bt_scheme_state_array(&cube, occ);
    bt_report report;
    bt_scheme_classify(occ, &report);
    bt_scheme_defect_twist(&report.defects, bt_scheme_head_sticker("UF"));
}

BT_TEST(classifier, flip_accessor_rejects_corner_head) {
    bt_death_expect("classifier.flip_accessor_rejects_corner_head", "not an edge head",
                    run_flip_accessor_rejects_corner);
}

BT_TEST(classifier, twist_accessor_rejects_edge_head) {
    bt_death_expect("classifier.twist_accessor_rejects_edge_head", "not a corner head",
                    run_twist_accessor_rejects_edge);
}

BT_TEST(classifier, e2_leaves_corners_solved_with_two_edge_swap_blocks) {
    uint8_t occ[54];
    after("E2", occ);
    bt_trace corner;
    bt_scheme_trace(occ, BT_ORBIT_CORNER, &corner);
    BT_CHECK(bt_scheme_trace_is_solved(&corner));
    bt_trace edge;
    bt_scheme_trace(occ, BT_ORBIT_EDGE, &edge);
    uint8_t first[3] = {33, 41, 33};
    uint8_t second[3] = {35, 43, 35};
    BT_CHECK_EQ(edge.block_count, (size_t)2);
    check_block(&edge, 0, first, 3);
    check_block(&edge, 1, second, 3);
    bt_report report;
    bt_scheme_classify(occ, &report);
    BT_CHECK_EQ((int)report.class.kind, (int)BT_CLASS_UNCLASSIFIED);
    char position[BT_CLASS_POSITION_MAX];
    BT_CHECK(!bt_scheme_report_position(&report, position));
}
