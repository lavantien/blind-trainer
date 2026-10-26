#include "scheme/classifier.h"
#include "scheme/memo.h"
#include "scheme/scheme.h"
#include "scheme/scramble.h"

#include "common/bt_harness.h"

#include <stdlib.h>
#include <string.h>

static int compare_u8(const void *a, const void *b) {
    return (int)*(const uint8_t *)a - (int)*(const uint8_t *)b;
}

static void identity(uint8_t occ[54]) {
    for (uint8_t i = 0; i < 54; i++) {
        occ[i] = i;
    }
}

static void parse_or_abort(const char *text, bt_alg *alg) {
    bt_alg_init(alg);
    char err[128];
    if (!bt_alg_parse(text, alg, err, sizeof err)) {
        BT_ABORT("parse %s failed: %s", text, err);
    }
}

static void after(const char *text, bt_cube *cube) {
    *cube = bt_cube_solved();
    bt_alg alg;
    parse_or_abort(text, &alg);
    bt_cube_apply_alg(cube, &alg);
    bt_alg_free(&alg);
}

static void assert_solves(const uint8_t occ[54], const bt_memo *memo) {
    uint8_t work[54];
    memcpy(work, occ, sizeof work);
    bt_scheme_apply_memo(work, memo);
    for (int i = 0; i < 54; i++) {
        BT_CHECK_EQ(work[i], (uint8_t)i);
    }
}

static bt_case edge_case(uint8_t t1, uint8_t t2) {
    bt_case case_id = {BT_ORBIT_EDGE, t1, t2};
    return case_id;
}

static bt_case corner_case(uint8_t t1, uint8_t t2) {
    bt_case case_id = {BT_ORBIT_CORNER, t1, t2};
    return case_id;
}

static bool cycles_eq(const bt_scheme_cycles *a, const bt_scheme_cycles *b) {
    if (a->count != b->count) {
        return false;
    }
    for (uint8_t i = 0; i < a->count; i++) {
        for (uint8_t k = 0; k < 3; k++) {
            if (a->items[i][k] != b->items[i][k]) {
                return false;
            }
        }
    }
    return true;
}

BT_TEST(memo_more, ideal_cycle_matches_standard_buffer_for_both_orbits) {
    bt_case edge = edge_case(30, 47);
    bt_scheme_cycles edge_cycles;
    bt_scheme_ideal_cycle(&edge, &edge_cycles);
    bt_scheme_cycles edge_at;
    bt_scheme_ideal_cycle_at(BT_ORBIT_EDGE, BT_SCHEME_EDGE_BUFFER_STICKER, 30, 47, &edge_at);
    BT_CHECK(cycles_eq(&edge_cycles, &edge_at));
    bt_case corner = corner_case(1, 0);
    bt_scheme_cycles corner_cycles;
    bt_scheme_ideal_cycle(&corner, &corner_cycles);
    bt_scheme_cycles corner_at;
    bt_scheme_ideal_cycle_at(BT_ORBIT_CORNER, BT_SCHEME_CORNER_BUFFER_STICKER, 1, 0, &corner_at);
    BT_CHECK(cycles_eq(&corner_cycles, &corner_at));
}

BT_TEST(memo_more, ideal_cycle_at_corner_buffer_pins_cycles) {
    bt_scheme_cycles cycles;
    bt_scheme_ideal_cycle_at(BT_ORBIT_CORNER, 3, 1, 0, &cycles);
    BT_CHECK_EQ(cycles.count, 3);
    BT_CHECK(cycles.items[0][0] == 3 && cycles.items[0][1] == 1 && cycles.items[0][2] == 0);
    BT_CHECK(cycles.items[1][0] == 5 && cycles.items[1][1] == 13 && cycles.items[1][2] == 17);
    BT_CHECK(cycles.items[2][0] == 8 && cycles.items[2][1] == 16 && cycles.items[2][2] == 4);
    uint8_t occ[54];
    identity(occ);
    bt_scheme_apply_sticker_cycles(occ, &cycles);
    bt_cube cube = bt_cube_from_occ(occ);
    BT_CHECK_EQ(bt_cube_position_of(&cube, 3), 1);
    BT_CHECK_EQ(bt_cube_position_of(&cube, 1), 0);
    BT_CHECK_EQ(bt_cube_position_of(&cube, 0), 3);
    BT_CHECK_EQ(bt_cube_position_of(&cube, 5), 13);
    BT_CHECK_EQ(bt_cube_position_of(&cube, 13), 17);
    BT_CHECK_EQ(bt_cube_position_of(&cube, 17), 5);
    BT_CHECK_EQ(bt_cube_position_of(&cube, 2), 2);
    BT_CHECK_EQ(bt_cube_position_of(&cube, 21), 21);
}

BT_TEST(memo_more, edge_three_style_case_from_ideal_cycle_round_trips) {
    uint8_t occ[54];
    identity(occ);
    bt_scheme_cycles cycles;
    bt_scheme_ideal_cycle_at(BT_ORBIT_EDGE, 26, 24, 25, &cycles);
    bt_scheme_apply_sticker_cycles(occ, &cycles);
    BT_CHECK(bt_scheme_occ_is_legal(occ));
    bt_cube cube = bt_cube_from_occ(occ);
    BT_CHECK_EQ(bt_cube_position_of(&cube, 26), 24);
    BT_CHECK_EQ(bt_cube_position_of(&cube, 24), 25);
    BT_CHECK_EQ(bt_cube_position_of(&cube, 25), 26);
    BT_CHECK_EQ(bt_cube_position_of(&cube, 0), 0);
    BT_CHECK_EQ(bt_cube_position_of(&cube, 47), 47);
    bt_memo memo;
    bt_scheme_memo_from_array(occ, &memo);
    BT_CHECK_EQ(memo.edge_letter_count, (size_t)2);
    BT_CHECK_EQ(memo.edge_letters[0], 'B');
    BT_CHECK_EQ(memo.edge_letters[1], 'A');
    bt_case want = edge_case(25, 24);
    BT_CHECK_EQ(memo.edge_pair_count, (size_t)1);
    BT_CHECK(memo.edge_pairs[0].orbit == want.orbit && memo.edge_pairs[0].t1 == want.t1 &&
             memo.edge_pairs[0].t2 == want.t2);
    BT_CHECK_EQ(memo.edge_buffers[0], 26);
    BT_CHECK_EQ(memo.corner_pair_count, (size_t)0);
    BT_CHECK_EQ(memo.corner_letter_count, (size_t)0);
    bt_residue empty;
    memset(&empty, 0, sizeof empty);
    BT_CHECK(bt_scheme_residue_eq(&memo.residue, &empty));
    assert_solves(occ, &memo);
    bt_report report;
    bt_scheme_classify(occ, &report);
    BT_CHECK_EQ((int)report.class.kind, (int)BT_CLASS_THREE_STYLE);
    if (report.class.kind == BT_CLASS_THREE_STYLE) {
        BT_CHECK_EQ((int)report.class.orbit, (int)BT_ORBIT_EDGE);
        BT_CHECK_EQ(report.class.case_count, (size_t)1);
        bt_case class_want = edge_case(24, 25);
        BT_CHECK(report.class.cases[0].orbit == class_want.orbit &&
                 report.class.cases[0].t1 == class_want.t1 &&
                 report.class.cases[0].t2 == class_want.t2);
    }
    char position[BT_CLASS_POSITION_MAX];
    BT_CHECK(bt_scheme_report_position(&report, position));
    BT_CHECK_EQ(position, "UF-UB-UR");
}

BT_TEST(memo_more, corner_three_style_case_from_ideal_cycle_round_trips) {
    uint8_t occ[54];
    identity(occ);
    bt_scheme_cycles cycles;
    bt_scheme_ideal_cycle_at(BT_ORBIT_CORNER, 2, 0, 1, &cycles);
    bt_scheme_apply_sticker_cycles(occ, &cycles);
    BT_CHECK(bt_scheme_occ_is_legal(occ));
    bt_cube cube = bt_cube_from_occ(occ);
    BT_CHECK_EQ(bt_cube_position_of(&cube, 2), 0);
    BT_CHECK_EQ(bt_cube_position_of(&cube, 0), 1);
    BT_CHECK_EQ(bt_cube_position_of(&cube, 1), 2);
    BT_CHECK_EQ(bt_cube_position_of(&cube, 3), 3);
    bt_memo memo;
    bt_scheme_memo_from_array(occ, &memo);
    BT_CHECK_EQ(memo.corner_letter_count, (size_t)2);
    BT_CHECK_EQ(memo.corner_letters[0], 'B');
    BT_CHECK_EQ(memo.corner_letters[1], 'A');
    bt_case want = corner_case(1, 0);
    BT_CHECK_EQ(memo.corner_pair_count, (size_t)1);
    BT_CHECK(memo.corner_pairs[0].orbit == want.orbit && memo.corner_pairs[0].t1 == want.t1 &&
             memo.corner_pairs[0].t2 == want.t2);
    BT_CHECK_EQ(memo.corner_buffers[0], 2);
    BT_CHECK_EQ(memo.edge_pair_count, (size_t)0);
    BT_CHECK_EQ(memo.edge_letter_count, (size_t)0);
    bt_residue empty;
    memset(&empty, 0, sizeof empty);
    BT_CHECK(bt_scheme_residue_eq(&memo.residue, &empty));
    assert_solves(occ, &memo);
    bt_report report;
    bt_scheme_classify(occ, &report);
    BT_CHECK_EQ((int)report.class.kind, (int)BT_CLASS_THREE_STYLE);
    if (report.class.kind == BT_CLASS_THREE_STYLE) {
        BT_CHECK_EQ((int)report.class.orbit, (int)BT_ORBIT_CORNER);
        BT_CHECK_EQ(report.class.case_count, (size_t)1);
        bt_case class_want = corner_case(0, 1);
        BT_CHECK(report.class.cases[0].orbit == class_want.orbit &&
                 report.class.cases[0].t1 == class_want.t1 &&
                 report.class.cases[0].t2 == class_want.t2);
    }
    char position[BT_CLASS_POSITION_MAX];
    BT_CHECK(bt_scheme_report_position(&report, position));
    BT_CHECK_EQ(position, "UFR-UBL-UBR");
}

BT_TEST(memo_more, edge_buffers_float_through_the_documented_order) {
    uint8_t occ[54];
    identity(occ);
    bt_scheme_cycles first;
    bt_scheme_cycles second;
    bt_scheme_cycles third;
    bt_scheme_ideal_cycle_at(BT_ORBIT_EDGE, 26, 24, 25, &first);
    bt_scheme_ideal_cycle_at(BT_ORBIT_EDGE, 33, 35, 44, &second);
    bt_scheme_ideal_cycle_at(BT_ORBIT_EDGE, 46, 47, 43, &third);
    bt_scheme_apply_sticker_cycles(occ, &first);
    bt_scheme_apply_sticker_cycles(occ, &second);
    bt_scheme_apply_sticker_cycles(occ, &third);
    BT_CHECK(bt_scheme_occ_is_legal(occ));
    bt_memo memo;
    bt_scheme_memo_from_array(occ, &memo);
    BT_CHECK_EQ(memo.edge_letter_count, (size_t)6);
    BT_CHECK(memcmp(memo.edge_letters, "BAULTX", 6) == 0);
    bt_case want[3] = {edge_case(25, 24), edge_case(44, 35), edge_case(43, 47)};
    BT_CHECK_EQ(memo.edge_pair_count, (size_t)3);
    for (int i = 0; i < 3; i++) {
        BT_CHECK(memo.edge_pairs[i].orbit == want[i].orbit && memo.edge_pairs[i].t1 == want[i].t1 &&
                 memo.edge_pairs[i].t2 == want[i].t2);
    }
    BT_CHECK_EQ(memo.edge_buffers[0], 26);
    BT_CHECK_EQ(memo.edge_buffers[1], 33);
    BT_CHECK_EQ(memo.edge_buffers[2], 46);
    bt_residue empty;
    memset(&empty, 0, sizeof empty);
    BT_CHECK(bt_scheme_residue_eq(&memo.residue, &empty));
    assert_solves(occ, &memo);
}

BT_TEST(memo_more, corner_buffers_float_through_the_documented_order) {
    uint8_t occ[54];
    identity(occ);
    bt_scheme_cycles first;
    bt_scheme_cycles second;
    bt_scheme_ideal_cycle_at(BT_ORBIT_CORNER, 2, 1, 0, &first);
    bt_scheme_ideal_cycle_at(BT_ORBIT_CORNER, 3, 21, 22, &second);
    bt_scheme_apply_sticker_cycles(occ, &first);
    bt_scheme_apply_sticker_cycles(occ, &second);
    BT_CHECK(bt_scheme_occ_is_legal(occ));
    bt_memo memo;
    bt_scheme_memo_from_array(occ, &memo);
    BT_CHECK_EQ(memo.corner_letter_count, (size_t)4);
    BT_CHECK(memcmp(memo.corner_letters, "ABWV", 4) == 0);
    bt_case want[2] = {corner_case(0, 1), corner_case(22, 21)};
    BT_CHECK_EQ(memo.corner_pair_count, (size_t)2);
    for (int i = 0; i < 2; i++) {
        BT_CHECK(memo.corner_pairs[i].orbit == want[i].orbit &&
                 memo.corner_pairs[i].t1 == want[i].t1 && memo.corner_pairs[i].t2 == want[i].t2);
    }
    BT_CHECK_EQ(memo.corner_buffers[0], 2);
    BT_CHECK_EQ(memo.corner_buffers[1], 3);
    bt_residue empty;
    memset(&empty, 0, sizeof empty);
    BT_CHECK(bt_scheme_residue_eq(&memo.residue, &empty));
    assert_solves(occ, &memo);
    bt_report report;
    bt_scheme_classify(occ, &report);
    BT_CHECK_EQ((int)report.class.kind, (int)BT_CLASS_UNCLASSIFIED);
}

BT_TEST(memo_more, flip_pair_state_leaves_flip_residue_and_classifies) {
    uint8_t occ[54];
    identity(occ);
    bt_residue residue;
    memset(&residue, 0, sizeof residue);
    residue.flip_count = 2;
    residue.flips[0] = 26;
    residue.flips[1] = 24;
    bt_scheme_apply_residue(occ, &residue);
    BT_CHECK(bt_scheme_occ_is_legal(occ));
    bt_memo memo;
    bt_scheme_memo_from_array(occ, &memo);
    BT_CHECK_EQ(memo.edge_letter_count, (size_t)0);
    BT_CHECK_EQ(memo.edge_pair_count, (size_t)0);
    BT_CHECK_EQ(memo.residue.flip_count, 2);
    BT_CHECK_EQ(memo.residue.flips[0], 24);
    BT_CHECK_EQ(memo.residue.flips[1], 26);
    BT_CHECK(!memo.residue.has_parity);
    BT_CHECK_EQ(memo.residue.twist_count, 0);
    bt_residue_class class = bt_scheme_residue_class(&memo.residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_FLIP_PAIR);
    BT_CHECK_EQ(class.a, 24);
    BT_CHECK_EQ(class.b, 26);
    assert_solves(occ, &memo);
    bt_report report;
    bt_scheme_classify(occ, &report);
    BT_CHECK_EQ((int)report.class.kind, (int)BT_CLASS_FLIP_PAIR);
    BT_CHECK_EQ(report.class.first, 26);
    BT_CHECK_EQ(report.class.second, 24);
    char position[BT_CLASS_POSITION_MAX];
    BT_CHECK(bt_scheme_report_position(&report, position));
    BT_CHECK_EQ(position, "UF & UB");
    uint8_t flipped[12];
    BT_CHECK_EQ(bt_scheme_flipped_pieces(&report.defects, flipped), (size_t)2);
    BT_CHECK_EQ(flipped[0], 24);
    BT_CHECK_EQ(flipped[1], 26);
    BT_CHECK(bt_scheme_defect_flip(&report.defects, bt_scheme_head_sticker("UF")));
    BT_CHECK(!bt_scheme_defect_flip(&report.defects, bt_scheme_head_sticker("UR")));
    bt_corner_twist_note twisted[8];
    BT_CHECK_EQ(bt_scheme_twisted_pieces(&report.defects, twisted), (size_t)0);
}

BT_TEST(memo_more, twist_pair_state_leaves_twist_residue_and_classifies) {
    uint8_t occ[54];
    identity(occ);
    bt_residue residue;
    memset(&residue, 0, sizeof residue);
    residue.twist_count = 2;
    residue.twists[0].corner = 2;
    residue.twists[0].quarter = 1;
    residue.twists[1].corner = 21;
    residue.twists[1].quarter = 2;
    bt_scheme_apply_residue(occ, &residue);
    BT_CHECK(bt_scheme_occ_is_legal(occ));
    bt_memo memo;
    bt_scheme_memo_from_array(occ, &memo);
    BT_CHECK_EQ(memo.corner_letter_count, (size_t)0);
    BT_CHECK_EQ(memo.corner_pair_count, (size_t)0);
    uint8_t corners[BT_MEMO_MAX_TWISTS];
    uint8_t quarters[BT_MEMO_MAX_TWISTS];
    for (uint8_t t = 0; t < memo.residue.twist_count; t++) {
        corners[t] = memo.residue.twists[t].corner;
        quarters[t] = memo.residue.twists[t].quarter;
    }
    qsort(corners, memo.residue.twist_count, 1, compare_u8);
    BT_CHECK_EQ(memo.residue.twist_count, 2);
    BT_CHECK_EQ(corners[0], 2);
    BT_CHECK_EQ(corners[1], 21);
    bool has_2_2 = false;
    bool has_21_1 = false;
    for (uint8_t t = 0; t < memo.residue.twist_count; t++) {
        if (memo.residue.twists[t].corner == 2 && memo.residue.twists[t].quarter == 2) {
            has_2_2 = true;
        }
        if (memo.residue.twists[t].corner == 21 && memo.residue.twists[t].quarter == 1) {
            has_21_1 = true;
        }
    }
    BT_CHECK(has_2_2 && has_21_1);
    bt_residue_class class = bt_scheme_residue_class(&memo.residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_TWIST_PAIR);
    BT_CHECK_EQ(class.a, 2);
    BT_CHECK_EQ(class.b, 21);
    assert_solves(occ, &memo);
    bt_report report;
    bt_scheme_classify(occ, &report);
    BT_CHECK_EQ((int)report.class.kind, (int)BT_CLASS_TWIST_PAIR);
    BT_CHECK_EQ(report.class.first, 2);
    BT_CHECK_EQ(report.class.second, 21);
    char position[BT_CLASS_POSITION_MAX];
    BT_CHECK(bt_scheme_report_position(&report, position));
    char first[16];
    char second[16];
    BT_CHECK(sscanf(position, "%15s & %15s", first, second) == 2);
    BT_CHECK((strcmp(first, "UFR") == 0 || strcmp(first, "DFR") == 0) &&
             (strcmp(second, "UFR") == 0 || strcmp(second, "DFR") == 0) &&
             strcmp(first, second) != 0);
    BT_CHECK_EQ((int)bt_scheme_defect_twist(&report.defects, bt_scheme_head_sticker(first)), 1);
    BT_CHECK_EQ((int)bt_scheme_defect_twist(&report.defects, bt_scheme_head_sticker(second)), -1);
}

BT_TEST(memo_more, twist_triple_state_leaves_equal_quarter_residue) {
    uint8_t occ[54];
    identity(occ);
    bt_residue residue;
    memset(&residue, 0, sizeof residue);
    residue.twist_count = 3;
    residue.twists[0].corner = 2;
    residue.twists[0].quarter = 1;
    residue.twists[1].corner = 3;
    residue.twists[1].quarter = 1;
    residue.twists[2].corner = 21;
    residue.twists[2].quarter = 1;
    bt_scheme_apply_residue(occ, &residue);
    BT_CHECK(bt_scheme_occ_is_legal(occ));
    bt_memo memo;
    bt_scheme_memo_from_array(occ, &memo);
    BT_CHECK_EQ(memo.corner_letter_count, (size_t)0);
    BT_CHECK_EQ(memo.corner_pair_count, (size_t)0);
    bt_residue_class class = bt_scheme_residue_class(&memo.residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_TWIST_TRIPLE);
    BT_CHECK_EQ(class.a, 2);
    BT_CHECK_EQ(class.b, 3);
    BT_CHECK_EQ(class.c, 21);
    assert_solves(occ, &memo);
    bt_report report;
    bt_scheme_classify(occ, &report);
    BT_CHECK_EQ((int)report.class.kind, (int)BT_CLASS_UNCLASSIFIED);
}

BT_TEST(memo_more, parity_state_leaves_parity_residue_and_classifies) {
    uint8_t occ[54];
    identity(occ);
    bt_residue residue;
    memset(&residue, 0, sizeof residue);
    residue.has_parity = true;
    residue.parity.edge[0] = 26;
    residue.parity.edge[1] = 24;
    residue.parity.corner[0] = 2;
    residue.parity.corner[1] = 1;
    bt_scheme_apply_residue(occ, &residue);
    BT_CHECK(bt_scheme_occ_is_legal(occ));
    bt_memo memo;
    bt_scheme_memo_from_array(occ, &memo);
    BT_CHECK_EQ(memo.edge_letter_count, (size_t)1);
    BT_CHECK_EQ(memo.edge_letters[0], 'A');
    BT_CHECK_EQ(memo.corner_letter_count, (size_t)1);
    BT_CHECK_EQ(memo.corner_letters[0], 'B');
    BT_CHECK_EQ(memo.edge_pair_count, (size_t)0);
    BT_CHECK_EQ(memo.corner_pair_count, (size_t)0);
    BT_CHECK(memo.residue.has_parity);
    BT_CHECK_EQ(memo.residue.parity.edge[0], 26);
    BT_CHECK_EQ(memo.residue.parity.edge[1], 24);
    BT_CHECK_EQ(memo.residue.parity.corner[0], 2);
    BT_CHECK_EQ(memo.residue.parity.corner[1], 1);
    BT_CHECK_EQ(memo.residue.flip_count, 0);
    BT_CHECK_EQ(memo.residue.twist_count, 0);
    bt_residue_class class = bt_scheme_residue_class(&memo.residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_PARITY);
    assert_solves(occ, &memo);
    bt_report report;
    bt_scheme_classify(occ, &report);
    BT_CHECK_EQ((int)report.class.kind, (int)BT_CLASS_PARITY);
    BT_CHECK_EQ(report.class.e1, 26);
    BT_CHECK_EQ(report.class.e2, 24);
    BT_CHECK_EQ(report.class.c1, 2);
    BT_CHECK_EQ(report.class.c2, 1);
    char position[BT_CLASS_POSITION_MAX];
    BT_CHECK(bt_scheme_report_position(&report, position));
    BT_CHECK_EQ(position, "UF-UB, UFR-UBR");
    uint8_t flipped[12];
    BT_CHECK_EQ(bt_scheme_flipped_pieces(&report.defects, flipped), (size_t)0);
    bt_corner_twist_note twisted[8];
    BT_CHECK_EQ(bt_scheme_twisted_pieces(&report.defects, twisted), (size_t)0);
}

BT_TEST(memo_more, ltct_state_leaves_parity_and_outside_twists) {
    uint8_t occ[54];
    identity(occ);
    bt_residue residue;
    memset(&residue, 0, sizeof residue);
    residue.has_parity = true;
    residue.parity.edge[0] = 26;
    residue.parity.edge[1] = 24;
    residue.parity.corner[0] = 2;
    residue.parity.corner[1] = 1;
    residue.twist_count = 2;
    residue.twists[0].corner = 2;
    residue.twists[0].quarter = 1;
    residue.twists[1].corner = 3;
    residue.twists[1].quarter = 2;
    bt_scheme_apply_residue(occ, &residue);
    BT_CHECK(bt_scheme_occ_is_legal(occ));
    bt_memo memo;
    bt_scheme_memo_from_array(occ, &memo);
    BT_CHECK_EQ(memo.edge_letter_count, (size_t)1);
    BT_CHECK_EQ(memo.edge_letters[0], 'A');
    BT_CHECK_EQ(memo.corner_letter_count, (size_t)1);
    BT_CHECK_EQ(memo.corner_letters[0], 'Q');
    BT_CHECK_EQ(memo.edge_pair_count, (size_t)0);
    BT_CHECK_EQ(memo.corner_pair_count, (size_t)0);
    BT_CHECK(memo.residue.has_parity);
    BT_CHECK_EQ(memo.residue.parity.edge[0], 26);
    BT_CHECK_EQ(memo.residue.parity.edge[1], 24);
    BT_CHECK_EQ(memo.residue.parity.corner[0], 2);
    BT_CHECK_EQ(memo.residue.parity.corner[1], 1);
    BT_CHECK_EQ(memo.residue.twist_count, 2);
    bool has_1_2 = false;
    bool has_3_1 = false;
    for (uint8_t t = 0; t < memo.residue.twist_count; t++) {
        if (memo.residue.twists[t].corner == 1 && memo.residue.twists[t].quarter == 2) {
            has_1_2 = true;
        }
        if (memo.residue.twists[t].corner == 3 && memo.residue.twists[t].quarter == 1) {
            has_3_1 = true;
        }
    }
    BT_CHECK(has_1_2 && has_3_1);
    BT_CHECK_EQ(memo.residue.flip_count, 0);
    bt_residue_class class = bt_scheme_residue_class(&memo.residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_LTCT);
    assert_solves(occ, &memo);
    bt_report report;
    bt_scheme_classify(occ, &report);
    BT_CHECK_EQ((int)report.class.kind, (int)BT_CLASS_LTCT);
    BT_CHECK_EQ(report.class.e1, 26);
    BT_CHECK_EQ(report.class.e2, 24);
    BT_CHECK_EQ(report.class.c1, 2);
    BT_CHECK_EQ(report.class.c2, 1);
    BT_CHECK_EQ(report.class.bracket, 5);
    char position[BT_CLASS_POSITION_MAX];
    BT_CHECK(bt_scheme_report_position(&report, position));
    BT_CHECK_EQ(position, "UF-UB, UFR-UBR[LUF]");
    BT_CHECK(!bt_scheme_defect_flip(&report.defects, 26));
    BT_CHECK(!bt_scheme_defect_flip(&report.defects, 24));
}

BT_TEST(memo_more, flips_plus_twists_state_is_compound) {
    uint8_t occ[54];
    identity(occ);
    bt_residue residue;
    memset(&residue, 0, sizeof residue);
    residue.flip_count = 2;
    residue.flips[0] = 26;
    residue.flips[1] = 24;
    residue.twist_count = 2;
    residue.twists[0].corner = 2;
    residue.twists[0].quarter = 1;
    residue.twists[1].corner = 21;
    residue.twists[1].quarter = 2;
    bt_scheme_apply_residue(occ, &residue);
    BT_CHECK(bt_scheme_occ_is_legal(occ));
    bt_memo memo;
    bt_scheme_memo_from_array(occ, &memo);
    BT_CHECK_EQ(memo.edge_letter_count, (size_t)0);
    BT_CHECK_EQ(memo.corner_letter_count, (size_t)0);
    BT_CHECK(memo.residue.flip_count > 0);
    BT_CHECK(memo.residue.twist_count > 0);
    bt_residue_class class = bt_scheme_residue_class(&memo.residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_COMPOUND);
    assert_solves(occ, &memo);
}

BT_TEST(memo_more, residue_class_covers_every_arm_from_first_principles) {
    bt_residue residue;
    memset(&residue, 0, sizeof residue);
    bt_residue_class class = bt_scheme_residue_class(&residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_NONE);
    memset(&residue, 0, sizeof residue);
    residue.flip_count = 2;
    residue.flips[0] = 26;
    residue.flips[1] = 24;
    class = bt_scheme_residue_class(&residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_FLIP_PAIR);
    BT_CHECK_EQ(class.a, 26);
    BT_CHECK_EQ(class.b, 24);
    residue.flip_count = 3;
    residue.flips[2] = 46;
    class = bt_scheme_residue_class(&residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_COMPOUND);
    memset(&residue, 0, sizeof residue);
    residue.twist_count = 2;
    residue.twists[0].corner = 2;
    residue.twists[0].quarter = 1;
    residue.twists[1].corner = 21;
    residue.twists[1].quarter = 2;
    class = bt_scheme_residue_class(&residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_TWIST_PAIR);
    BT_CHECK_EQ(class.a, 2);
    BT_CHECK_EQ(class.b, 21);
    residue.twists[1].quarter = 1;
    class = bt_scheme_residue_class(&residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_COMPOUND);
    residue.twist_count = 3;
    residue.twists[1].corner = 3;
    residue.twists[2].corner = 21;
    residue.twists[2].quarter = 1;
    class = bt_scheme_residue_class(&residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_TWIST_TRIPLE);
    BT_CHECK_EQ(class.a, 2);
    BT_CHECK_EQ(class.b, 3);
    BT_CHECK_EQ(class.c, 21);
    memset(&residue, 0, sizeof residue);
    residue.twist_count = 2;
    residue.twists[0].corner = 2;
    residue.twists[0].quarter = 2;
    residue.twists[1].corner = 3;
    residue.twists[1].quarter = 2;
    class = bt_scheme_residue_class(&residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_COMPOUND);
    memset(&residue, 0, sizeof residue);
    residue.flip_count = 1;
    residue.flips[0] = 26;
    residue.twist_count = 1;
    residue.twists[0].corner = 2;
    residue.twists[0].quarter = 1;
    class = bt_scheme_residue_class(&residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_COMPOUND);
    memset(&residue, 0, sizeof residue);
    residue.has_parity = true;
    residue.parity.edge[0] = 26;
    residue.parity.edge[1] = 24;
    residue.parity.corner[0] = 2;
    residue.parity.corner[1] = 1;
    class = bt_scheme_residue_class(&residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_PARITY);
    residue.twist_count = 2;
    residue.twists[0].corner = 2;
    residue.twists[0].quarter = 1;
    residue.twists[1].corner = 1;
    residue.twists[1].quarter = 2;
    class = bt_scheme_residue_class(&residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_PARITY);
    residue.twist_count = 1;
    residue.twists[0].corner = 3;
    residue.twists[0].quarter = 1;
    class = bt_scheme_residue_class(&residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_LTCT);
    residue.twist_count = 0;
    residue.flip_count = 2;
    residue.flips[0] = 26;
    residue.flips[1] = 24;
    class = bt_scheme_residue_class(&residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_COMPOUND);
}

BT_TEST(memo_more, u_turn_position_stays_none_when_unclassified) {
    bt_cube cube;
    after("U", &cube);
    uint8_t occ[54];
    bt_scheme_state_array(&cube, occ);
    bt_report report;
    bt_scheme_classify(occ, &report);
    BT_CHECK_EQ((int)report.class.kind, (int)BT_CLASS_UNCLASSIFIED);
    char position[BT_CLASS_POSITION_MAX];
    BT_CHECK(!bt_scheme_report_position(&report, position));
}

BT_TEST(memo_more, random_legal_states_round_trip_with_monotonic_buffers) {
    uint8_t edge_heads[12];
    uint8_t corner_heads[8];
    bt_scheme_edge_floating_heads(edge_heads);
    bt_scheme_corner_floating_heads(corner_heads);
    for (uint64_t seed = 0; seed < 400; seed++) {
        uint8_t occ[54];
        bt_scheme_random_occ(seed, occ);
        BT_CHECK(bt_scheme_occ_is_legal(occ));
        bt_memo memo;
        bt_scheme_memo_from_array(occ, &memo);
        assert_solves(occ, &memo);
        BT_CHECK_EQ(memo.edge_pair_count, memo.edge_buffer_count);
        BT_CHECK_EQ(memo.corner_pair_count, memo.corner_buffer_count);
        int previous = -1;
        for (size_t i = 0; i < memo.edge_buffer_count; i++) {
            int index = -1;
            for (int h = 0; h < 12; h++) {
                if (edge_heads[h] == memo.edge_buffers[i]) {
                    index = h;
                }
            }
            BT_CHECK(index >= 0);
            BT_CHECK(index >= previous);
            previous = index;
        }
        previous = -1;
        for (size_t i = 0; i < memo.corner_buffer_count; i++) {
            int index = -1;
            for (int h = 0; h < 8; h++) {
                if (corner_heads[h] == memo.corner_buffers[i]) {
                    index = h;
                }
            }
            BT_CHECK(index >= 0);
            BT_CHECK(index >= previous);
            previous = index;
        }
        size_t edge_slack = memo.edge_letter_count - 2 * memo.edge_pair_count;
        size_t corner_slack = memo.corner_letter_count - 2 * memo.corner_pair_count;
        BT_CHECK(edge_slack <= 1);
        BT_CHECK(corner_slack <= 1);
    }
}

BT_TEST(memo_more, letters_follow_arrival_order_at_each_buffer) {
    uint8_t occ[54];
    identity(occ);
    bt_scheme_cycles first;
    bt_scheme_cycles second;
    bt_scheme_ideal_cycle_at(BT_ORBIT_EDGE, 26, 24, 25, &first);
    bt_scheme_ideal_cycle_at(BT_ORBIT_EDGE, 33, 35, 44, &second);
    bt_scheme_apply_sticker_cycles(occ, &first);
    bt_scheme_apply_sticker_cycles(occ, &second);
    bt_memo memo;
    bt_scheme_memo_from_array(occ, &memo);
    for (size_t i = 0; i < memo.edge_pair_count; i++) {
        char letters[2];
        bt_scheme_case_letters(&memo.edge_pairs[i], letters);
        BT_CHECK_EQ(letters[0], bt_scheme_letter_of(memo.edge_pairs[i].t1));
        BT_CHECK_EQ(letters[1], bt_scheme_letter_of(memo.edge_pairs[i].t2));
    }
    BT_CHECK_EQ(memo.edge_pair_count, (size_t)2);
    BT_CHECK_EQ(bt_scheme_letter_of(memo.edge_pairs[0].t1), 'B');
    BT_CHECK_EQ(bt_scheme_letter_of(memo.edge_pairs[0].t2), 'A');
    BT_CHECK_EQ(bt_scheme_letter_of(memo.edge_pairs[1].t1), 'U');
    BT_CHECK_EQ(bt_scheme_letter_of(memo.edge_pairs[1].t2), 'L');
}
