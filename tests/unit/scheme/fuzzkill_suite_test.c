#include "common.h"
#include "scheme/classifier.h"
#include "scheme/memo.h"
#include "scheme/scheme.h"
#include "scheme/scramble.h"

#include "common/bt_harness.h"

#include <stdlib.h>
#include <string.h>

enum { FK_ROWS_MAX = 1100 };

typedef struct {
    char sheet[16];
    char alg[256];
    char rowid[16];
} fk_row;

static fk_row fk_rows[FK_ROWS_MAX];
static size_t fk_row_count;

static void fk_load_rows(void) {
    fk_row_count = 0;
    char path[BT_PATH_MAX];
    bt_test_ref_path(path, "rows.tsv");
    bt_test_lines lines;
    bt_test_lines_begin(&lines, path);
    char *line;
    while ((line = bt_test_lines_next(&lines)) != NULL) {
        if (*line == '\0') {
            continue;
        }
        char *cols[5];
        BT_CHECK_EQ(bt_tsv_split(line, cols, 5), (size_t)5);
        BT_CHECK(fk_row_count < FK_ROWS_MAX);
        fk_row *row = &fk_rows[fk_row_count++];
        snprintf(row->sheet, sizeof row->sheet, "%s", cols[1]);
        snprintf(row->alg, sizeof row->alg, "%s", cols[4]);
        snprintf(row->rowid, sizeof row->rowid, "%s", cols[2]);
    }
    bt_test_lines_end(&lines);
}

static void identity(uint8_t occ[54]) {
    for (uint8_t i = 0; i < 54; i++) {
        occ[i] = i;
    }
}

static void build_occ(const uint8_t edge_perm[12], const bool edge_flip[12],
                      const uint8_t corner_perm[8], const uint8_t corner_twist[8],
                      uint8_t occ[54]) {
    identity(occ);
    for (uint8_t slot = 0; slot < 12; slot++) {
        uint8_t occupants[2];
        uint8_t positions[2];
        bt_edge_stickers(edge_perm[slot], occupants);
        bt_edge_stickers(slot, positions);
        if (edge_flip[slot]) {
            occ[positions[0]] = occupants[1];
            occ[positions[1]] = occupants[0];
        } else {
            occ[positions[0]] = occupants[0];
            occ[positions[1]] = occupants[1];
        }
    }
    for (uint8_t slot = 0; slot < 8; slot++) {
        uint8_t piece[3];
        uint8_t positions[3];
        bt_corner_stickers(corner_perm[slot], piece);
        bt_corner_stickers(slot, positions);
        uint8_t w = corner_twist[slot] % 3;
        for (uint8_t j = 0; j < 3; j++) {
            occ[positions[(j + w) % 3]] = piece[j];
        }
    }
}

static void swap_pair(uint8_t *values, size_t n, size_t a, size_t b) {
    (void)n;
    uint8_t tmp = values[a];
    values[a] = values[b];
    values[b] = tmp;
}

static void cycle_three(uint8_t *values, size_t a, size_t b, size_t c) {
    uint8_t pa = values[a];
    uint8_t pb = values[b];
    uint8_t pc = values[c];
    values[a] = pb;
    values[b] = pc;
    values[c] = pa;
}

static uint8_t head_of_edge(uint8_t edge) {
    bt_piece piece = {BT_PIECE_EDGE, edge};
    return bt_head_sticker(piece);
}

static uint8_t head_of_corner(uint8_t corner) {
    bt_piece piece = {BT_PIECE_CORNER, corner};
    return bt_head_sticker(piece);
}

static void residue_of(const bt_twist *twists, uint8_t twist_count, const uint8_t *flips,
                       uint8_t flip_count, bool has_parity, bt_parity parity, bt_residue *out) {
    memset(out, 0, sizeof *out);
    out->has_parity = has_parity;
    if (has_parity) {
        out->parity = parity;
    }
    out->flip_count = flip_count;
    if (flip_count > 0) {
        memcpy(out->flips, flips, flip_count);
    }
    out->twist_count = twist_count;
    if (twist_count > 0) {
        memcpy(out->twists, twists, twist_count * sizeof(bt_twist));
    }
}

static void assert_round_trip(const uint8_t occ[54]) {
    BT_CHECK(bt_scheme_occ_is_legal(occ));
    bt_memo memo;
    bt_scheme_memo_from_array(occ, &memo);
    uint8_t work[54];
    memcpy(work, occ, sizeof work);
    bt_scheme_apply_memo(work, &memo);
    for (int i = 0; i < 54; i++) {
        BT_CHECK_EQ(work[i], (uint8_t)i);
    }
}

BT_TEST(fuzzkill, classification_shapes_are_pinned) {
    uint8_t edges[12];
    uint8_t corners[8];
    for (uint8_t i = 0; i < 12; i++) {
        edges[i] = i;
    }
    for (uint8_t i = 0; i < 8; i++) {
        corners[i] = i;
    }
    bool no_flips[12] = {false};
    uint8_t no_twists[8] = {0};
    uint8_t occ[54];

    build_occ(edges, no_flips, corners, no_twists, occ);
    bt_report solved;
    bt_scheme_classify(occ, &solved);
    BT_CHECK_EQ((int)solved.class.kind, (int)BT_CLASS_SOLVED);

    uint8_t cycled[12];
    memcpy(cycled, edges, sizeof edges);
    cycle_three(cycled, 5, 6, 7);
    build_occ(cycled, no_flips, corners, no_twists, occ);
    bt_report edge_cycle;
    bt_scheme_classify(occ, &edge_cycle);
    BT_CHECK_EQ((int)edge_cycle.class.kind, (int)BT_CLASS_THREE_STYLE);
    BT_CHECK_EQ((int)edge_cycle.class.orbit, (int)BT_ORBIT_EDGE);
    BT_CHECK_EQ(edge_cycle.class.case_count, (size_t)1);

    uint8_t cycled_corners[8];
    memcpy(cycled_corners, corners, sizeof corners);
    cycle_three(cycled_corners, 5, 6, 7);
    build_occ(edges, no_flips, cycled_corners, no_twists, occ);
    bt_report corner_cycle;
    bt_scheme_classify(occ, &corner_cycle);
    BT_CHECK_EQ((int)corner_cycle.class.kind, (int)BT_CLASS_THREE_STYLE);
    BT_CHECK_EQ((int)corner_cycle.class.orbit, (int)BT_ORBIT_CORNER);
    BT_CHECK_EQ(corner_cycle.class.case_count, (size_t)1);

    build_occ(cycled, no_flips, cycled_corners, no_twists, occ);
    bt_report both_orbits;
    bt_scheme_classify(occ, &both_orbits);
    BT_CHECK_EQ((int)both_orbits.class.kind, (int)BT_CLASS_UNCLASSIFIED);

    bool single_flip[12] = {false};
    single_flip[5] = true;
    build_occ(edges, single_flip, corners, no_twists, occ);
    bt_report one_flip;
    bt_scheme_classify(occ, &one_flip);
    BT_CHECK_EQ((int)one_flip.class.kind, (int)BT_CLASS_UNCLASSIFIED);

    bool flip_pair[12] = {false};
    flip_pair[4] = true;
    flip_pair[7] = true;
    build_occ(edges, flip_pair, corners, no_twists, occ);
    bt_report flips;
    bt_scheme_classify(occ, &flips);
    BT_CHECK_EQ((int)flips.class.kind, (int)BT_CLASS_FLIP_PAIR);
    if (flips.class.kind == BT_CLASS_FLIP_PAIR) {
        BT_CHECK((flips.class.first == head_of_edge(4) && flips.class.second == head_of_edge(7)) ||
                 (flips.class.first == head_of_edge(7) && flips.class.second == head_of_edge(4)));
    }

    uint8_t twist_state[54];
    identity(twist_state);
    bt_twist twists[2];
    twists[0].corner = head_of_corner(3);
    twists[0].quarter = 1;
    twists[1].corner = head_of_corner(4);
    twists[1].quarter = 2;
    bt_residue residue;
    residue_of(twists, 2, NULL, 0, false, (bt_parity){{0}, {0}}, &residue);
    bt_scheme_apply_residue(twist_state, &residue);
    bt_report twists_report;
    bt_scheme_classify(twist_state, &twists_report);
    BT_CHECK_EQ((int)twists_report.class.kind, (int)BT_CLASS_TWIST_PAIR);
    BT_CHECK_EQ(twists_report.class.first, head_of_corner(3));
    BT_CHECK_EQ(twists_report.class.second, head_of_corner(4));

    uint8_t twist_with_edges[54];
    memcpy(twist_with_edges, twist_state, sizeof twist_state);
    uint8_t ea[2];
    uint8_t eb[2];
    bt_edge_stickers(3, ea);
    bt_edge_stickers(4, eb);
    swap_pair(twist_with_edges, 54, ea[0], eb[0]);
    swap_pair(twist_with_edges, 54, ea[1], eb[1]);
    bt_report twist_with_edges_report;
    bt_scheme_classify(twist_with_edges, &twist_with_edges_report);
    BT_CHECK_EQ((int)twist_with_edges_report.class.kind, (int)BT_CLASS_UNCLASSIFIED);

    uint8_t parity_state[54];
    identity(parity_state);
    bt_parity parity;
    parity.edge[0] = head_of_edge(3);
    parity.edge[1] = head_of_edge(4);
    parity.corner[0] = head_of_corner(3);
    parity.corner[1] = head_of_corner(4);
    residue_of(NULL, 0, NULL, 0, true, parity, &residue);
    bt_scheme_apply_residue(parity_state, &residue);
    bt_report parity_report;
    bt_scheme_classify(parity_state, &parity_report);
    BT_CHECK_EQ((int)parity_report.class.kind, (int)BT_CLASS_PARITY);
    BT_CHECK_EQ(parity_report.class.e1, head_of_edge(3));
    BT_CHECK_EQ(parity_report.class.e2, head_of_edge(4));
    BT_CHECK_EQ(parity_report.class.c1, head_of_corner(3));
    BT_CHECK_EQ(parity_report.class.c2, head_of_corner(4));

    uint8_t swap_plus_cycle[54];
    build_occ(cycled, no_flips, corners, no_twists, swap_plus_cycle);
    uint8_t a[2];
    uint8_t b[2];
    bt_edge_stickers(3, a);
    bt_edge_stickers(4, b);
    swap_pair(swap_plus_cycle, 54, a[0], b[0]);
    swap_pair(swap_plus_cycle, 54, a[1], b[1]);
    uint8_t slot3[3];
    uint8_t slot4[3];
    bt_corner_stickers(3, slot3);
    bt_corner_stickers(4, slot4);
    for (int i = 0; i < 3; i++) {
        swap_pair(swap_plus_cycle, 54, slot3[i], slot4[i]);
    }
    bt_report classified;
    bt_scheme_classify(swap_plus_cycle, &classified);
    BT_CHECK_EQ((int)classified.class.kind, (int)BT_CLASS_UNCLASSIFIED);

    uint8_t swap_plus_corner_cycle[54];
    build_occ(edges, no_flips, cycled_corners, no_twists, swap_plus_corner_cycle);
    swap_pair(swap_plus_corner_cycle, 54, a[0], b[0]);
    swap_pair(swap_plus_corner_cycle, 54, a[1], b[1]);
    bt_report edge_swap_corner_cycle;
    bt_scheme_classify(swap_plus_corner_cycle, &edge_swap_corner_cycle);
    BT_CHECK_EQ((int)edge_swap_corner_cycle.class.kind, (int)BT_CLASS_UNCLASSIFIED);

    uint8_t twisted_swap_state[54];
    identity(twisted_swap_state);
    residue_of(twists, 2, NULL, 0, true, parity, &residue);
    bt_scheme_apply_residue(twisted_swap_state, &residue);
    bt_report twisted_swap;
    bt_scheme_classify(twisted_swap_state, &twisted_swap);
    BT_CHECK_EQ((int)twisted_swap.class.kind, (int)BT_CLASS_PARITY);
    bt_corner_twist_note twisted_pieces[8];
    BT_CHECK_EQ(bt_scheme_twisted_pieces(&twisted_swap.defects, twisted_pieces), (size_t)2);

    uint8_t edge_swap_twist_pair[54];
    identity(edge_swap_twist_pair);
    uint8_t pa[2];
    uint8_t pb[2];
    bt_edge_stickers(3, pa);
    bt_edge_stickers(4, pb);
    swap_pair(edge_swap_twist_pair, 54, pa[0], pb[0]);
    swap_pair(edge_swap_twist_pair, 54, pa[1], pb[1]);
    residue_of(twists, 2, NULL, 0, false, (bt_parity){{0}, {0}}, &residue);
    bt_scheme_apply_residue(edge_swap_twist_pair, &residue);
    bt_report swap_with_twists;
    bt_scheme_classify(edge_swap_twist_pair, &swap_with_twists);
    BT_CHECK_EQ((int)swap_with_twists.class.kind, (int)BT_CLASS_UNCLASSIFIED);
}

BT_TEST(fuzzkill, defects_list_flipped_and_twisted_pieces) {
    uint8_t edges[12];
    uint8_t corners[8];
    for (uint8_t i = 0; i < 12; i++) {
        edges[i] = i;
    }
    for (uint8_t i = 0; i < 8; i++) {
        corners[i] = i;
    }
    bool flips[12] = {false};
    flips[4] = true;
    flips[7] = true;
    uint8_t no_twists[8] = {0};
    uint8_t occ[54];
    build_occ(edges, flips, corners, no_twists, occ);
    bt_report report;
    bt_scheme_classify(occ, &report);
    uint8_t flipped[12];
    BT_CHECK_EQ(bt_scheme_flipped_pieces(&report.defects, flipped), (size_t)2);
    BT_CHECK_EQ(flipped[0], head_of_edge(4));
    BT_CHECK_EQ(flipped[1], head_of_edge(7));

    uint8_t twisted[54];
    identity(twisted);
    bt_twist twists[2];
    twists[0].corner = head_of_corner(3);
    twists[0].quarter = 1;
    twists[1].corner = head_of_corner(4);
    twists[1].quarter = 2;
    bt_residue residue;
    residue_of(twists, 2, NULL, 0, false, (bt_parity){{0}, {0}}, &residue);
    bt_scheme_apply_residue(twisted, &residue);
    bt_report twisted_report;
    bt_scheme_classify(twisted, &twisted_report);
    bt_corner_twist_note twisted_pieces[8];
    size_t count = bt_scheme_twisted_pieces(&twisted_report.defects, twisted_pieces);
    BT_CHECK_EQ(count, (size_t)2);
    bool has_3 = false;
    bool has_4 = false;
    for (size_t i = 0; i < count; i++) {
        if (twisted_pieces[i].head == head_of_corner(3)) {
            has_3 = true;
        }
        if (twisted_pieces[i].head == head_of_corner(4)) {
            has_4 = true;
        }
    }
    BT_CHECK(has_3 && has_4);
}

BT_TEST(fuzzkill, residue_classes_are_pinned) {
    uint8_t c3 = head_of_corner(3);
    uint8_t c4 = head_of_corner(4);
    uint8_t c5 = head_of_corner(5);
    uint8_t e3 = head_of_edge(3);
    uint8_t e4 = head_of_edge(4);
    bt_residue residue;
    bt_residue_class class;
    bt_twist twists[3];
    uint8_t flips[3];
    bt_parity parity;
    parity.edge[0] = e3;
    parity.edge[1] = e4;
    parity.corner[0] = c3;
    parity.corner[1] = c4;

    memset(&residue, 0, sizeof residue);
    class = bt_scheme_residue_class(&residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_NONE);

    flips[0] = e3;
    flips[1] = e4;
    residue_of(NULL, 0, flips, 2, false, parity, &residue);
    class = bt_scheme_residue_class(&residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_FLIP_PAIR);
    BT_CHECK_EQ(class.a, e3);
    BT_CHECK_EQ(class.b, e4);

    flips[2] = head_of_edge(5);
    residue_of(NULL, 0, flips, 3, false, parity, &residue);
    class = bt_scheme_residue_class(&residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_COMPOUND);

    twists[0].corner = c3;
    twists[0].quarter = 1;
    twists[1].corner = c4;
    twists[1].quarter = 2;
    residue_of(twists, 2, NULL, 0, false, parity, &residue);
    class = bt_scheme_residue_class(&residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_TWIST_PAIR);
    BT_CHECK_EQ(class.a, c3);
    BT_CHECK_EQ(class.b, c4);

    twists[1].quarter = 1;
    residue_of(twists, 2, NULL, 0, false, parity, &residue);
    class = bt_scheme_residue_class(&residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_COMPOUND);

    twists[1].corner = c4;
    twists[1].quarter = 1;
    twists[2].corner = c5;
    twists[2].quarter = 1;
    residue_of(twists, 3, NULL, 0, false, parity, &residue);
    class = bt_scheme_residue_class(&residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_TWIST_TRIPLE);
    BT_CHECK_EQ(class.a, c3);
    BT_CHECK_EQ(class.b, c4);
    BT_CHECK_EQ(class.c, c5);

    twists[2].quarter = 2;
    residue_of(twists, 3, NULL, 0, false, parity, &residue);
    class = bt_scheme_residue_class(&residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_COMPOUND);

    residue_of(NULL, 0, NULL, 0, true, parity, &residue);
    class = bt_scheme_residue_class(&residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_PARITY);

    residue_of(NULL, 0, flips, 2, true, parity, &residue);
    class = bt_scheme_residue_class(&residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_COMPOUND);

    twists[0].corner = c3;
    twists[0].quarter = 1;
    twists[1].corner = c4;
    twists[1].quarter = 2;
    residue_of(twists, 2, NULL, 0, true, parity, &residue);
    class = bt_scheme_residue_class(&residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_PARITY);

    twists[0].corner = c5;
    twists[0].quarter = 1;
    twists[1].corner = c3;
    twists[1].quarter = 2;
    residue_of(twists, 2, NULL, 0, true, parity, &residue);
    class = bt_scheme_residue_class(&residue);
    BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_LTCT);
}

BT_TEST(fuzzkill, scramble_legality_helpers_are_pinned) {
    uint8_t edges[12];
    uint8_t corners[8];
    for (uint8_t i = 0; i < 12; i++) {
        edges[i] = i;
    }
    for (uint8_t i = 0; i < 8; i++) {
        corners[i] = i;
    }
    bool no_flips[12] = {false};
    uint8_t no_twists[8] = {0};

    uint8_t swapped[12];
    memcpy(swapped, edges, sizeof edges);
    swap_pair(swapped, 12, 0, 9);
    cycle_three(swapped, 3, 4, 5);
    uint8_t cycled_corners[8];
    memcpy(cycled_corners, corners, sizeof corners);
    cycle_three(cycled_corners, 1, 6, 7);
    uint8_t perm_state[54];
    build_occ(swapped, no_flips, cycled_corners, no_twists, perm_state);
    uint8_t edge_perm[12];
    BT_CHECK(bt_scheme_piece_perm(perm_state, BT_ORBIT_EDGE, edge_perm));
    uint8_t edge_want[12] = {9, 1, 2, 4, 5, 3, 6, 7, 8, 0, 10, 11};
    for (int i = 0; i < 12; i++) {
        BT_CHECK_EQ(edge_perm[i], edge_want[i]);
    }
    uint8_t corner_perm[12];
    BT_CHECK(bt_scheme_piece_perm(perm_state, BT_ORBIT_CORNER, corner_perm));
    uint8_t corner_want[8] = {0, 6, 2, 3, 4, 5, 7, 1};
    for (int i = 0; i < 8; i++) {
        BT_CHECK_EQ(corner_perm[i], corner_want[i]);
    }

    uint8_t edge_swap_only[54];
    build_occ(edges, no_flips, corners, no_twists, edge_swap_only);
    uint8_t a[2];
    uint8_t b[2];
    bt_edge_stickers(3, a);
    bt_edge_stickers(4, b);
    swap_pair(edge_swap_only, 54, a[0], b[0]);
    swap_pair(edge_swap_only, 54, a[1], b[1]);
    BT_CHECK(!bt_scheme_occ_is_legal(edge_swap_only));

    uint8_t twisted[54];
    identity(twisted);
    bt_twist twists[1];
    twists[0].corner = head_of_corner(5);
    twists[0].quarter = 1;
    bt_residue residue;
    residue_of(twists, 1, NULL, 0, false, (bt_parity){{0}, {0}}, &residue);
    bt_scheme_apply_residue(twisted, &residue);
    uint8_t plain[54];
    identity(plain);
    BT_CHECK_EQ(bt_scheme_twist_sum(plain), 0ULL);
    BT_CHECK(bt_scheme_twist_sum(twisted) % 3 != 0);
    BT_CHECK(!bt_scheme_occ_is_legal(twisted));

    uint8_t duplicate[54];
    identity(duplicate);
    duplicate[11] = duplicate[10];
    BT_CHECK(!bt_scheme_occ_is_legal(duplicate));

    uint8_t out_of_range[54];
    identity(out_of_range);
    out_of_range[11] = 60;
    BT_CHECK(!bt_scheme_occ_is_legal(out_of_range));

    uint8_t non_cohesive[54];
    identity(non_cohesive);
    uint8_t slot3[3];
    uint8_t slot4[3];
    bt_corner_stickers(3, slot3);
    bt_corner_stickers(4, slot4);
    swap_pair(non_cohesive, 54, slot3[0], slot4[1]);
    BT_CHECK(!bt_scheme_occ_is_legal(non_cohesive));

    uint8_t mixed_type[54];
    identity(mixed_type);
    swap_pair(mixed_type, 54, slot3[0], 24);
    uint8_t scratch[12];
    BT_CHECK(!bt_scheme_piece_perm(mixed_type, BT_ORBIT_EDGE, scratch));
    BT_CHECK(!bt_scheme_piece_perm(mixed_type, BT_ORBIT_CORNER, scratch));
}

BT_TEST(fuzzkill, scramble_edge_sampling_stays_uniform) {
    uint32_t counts[12] = {0};
    for (uint64_t seed = 0; seed < 20000; seed++) {
        bt_cube cube;
        bt_scheme_random_state(seed, &cube);
        uint8_t occ[54];
        bt_scheme_state_array(&cube, occ);
        uint8_t perm[12];
        BT_CHECK(bt_scheme_piece_perm(occ, BT_ORBIT_EDGE, perm));
        counts[perm[0]]++;
    }
    for (int piece = 0; piece < 12; piece++) {
        BT_CHECK(counts[piece] >= 1200);
        BT_CHECK(counts[piece] < 2200);
    }
}

BT_TEST(fuzzkill, memo_restores_synthetic_legal_states) {
    uint8_t edges[12];
    uint8_t corners[8];
    for (uint8_t i = 0; i < 12; i++) {
        edges[i] = i;
    }
    for (uint8_t i = 0; i < 8; i++) {
        corners[i] = i;
    }
    bool no_flips[12] = {false};
    uint8_t no_twists[8] = {0};
    bool flips[12] = {false};
    flips[4] = true;
    flips[7] = true;
    uint8_t occ[54];

    build_occ(edges, no_flips, corners, no_twists, occ);
    assert_round_trip(occ);

    uint8_t cycled[12];
    memcpy(cycled, edges, sizeof edges);
    cycle_three(cycled, 5, 6, 7);
    build_occ(cycled, no_flips, corners, no_twists, occ);
    assert_round_trip(occ);

    memcpy(cycled, edges, sizeof edges);
    cycle_three(cycled, 2, 5, 9);
    build_occ(cycled, no_flips, corners, no_twists, occ);
    assert_round_trip(occ);

    build_occ(edges, flips, corners, no_twists, occ);
    assert_round_trip(occ);

    uint8_t twist_pair[54];
    identity(twist_pair);
    bt_twist twists[2];
    twists[0].corner = head_of_corner(3);
    twists[0].quarter = 1;
    twists[1].corner = head_of_corner(4);
    twists[1].quarter = 2;
    bt_residue residue;
    residue_of(twists, 2, NULL, 0, false, (bt_parity){{0}, {0}}, &residue);
    bt_scheme_apply_residue(twist_pair, &residue);
    assert_round_trip(twist_pair);

    uint8_t parity_state[54];
    identity(parity_state);
    bt_parity parity;
    parity.edge[0] = head_of_edge(3);
    parity.edge[1] = head_of_edge(4);
    parity.corner[0] = head_of_corner(3);
    parity.corner[1] = head_of_corner(4);
    residue_of(NULL, 0, NULL, 0, true, parity, &residue);
    bt_scheme_apply_residue(parity_state, &residue);
    assert_round_trip(parity_state);

    uint8_t ltct_state[54];
    identity(ltct_state);
    bt_twist ltct_twists[2];
    ltct_twists[0].corner = head_of_corner(5);
    ltct_twists[0].quarter = 1;
    ltct_twists[1].corner = head_of_corner(3);
    ltct_twists[1].quarter = 2;
    residue_of(ltct_twists, 2, NULL, 0, true, parity, &residue);
    bt_scheme_apply_residue(ltct_state, &residue);
    assert_round_trip(ltct_state);

    uint8_t mixed[54];
    memcpy(cycled, edges, sizeof edges);
    cycle_three(cycled, 2, 5, 9);
    build_occ(cycled, flips, corners, no_twists, mixed);
    residue_of(NULL, 0, NULL, 0, true, parity, &residue);
    bt_scheme_apply_residue(mixed, &residue);
    assert_round_trip(mixed);

    for (uint64_t seed = 0; seed < 400; seed++) {
        bt_cube cube;
        bt_scheme_random_state(seed, &cube);
        bt_scheme_state_array(&cube, occ);
        bt_memo memo;
        bt_scheme_memo_from_array(occ, &memo);
        uint8_t work[54];
        memcpy(work, occ, sizeof work);
        bt_scheme_apply_memo(work, &memo);
        for (int i = 0; i < 54; i++) {
            BT_CHECK_EQ(work[i], (uint8_t)i);
        }
    }
}

BT_TEST(fuzzkill, vendor_twist_pairs_are_opposite) {
    fk_load_rows();
    int twist_rows = 0;
    for (size_t i = 0; i < fk_row_count; i++) {
        const fk_row *row = &fk_rows[i];
        bt_alg alg;
        bt_alg_init(&alg);
        char err[128];
        if (!bt_alg_parse(row->alg, &alg, err, sizeof err)) {
            BT_ABORT("simcheck alg %s does not parse: %s", row->alg, err);
        }
        bt_report report;
        bt_scheme_classify_alg(&alg, &report);
        bt_alg_free(&alg);
        if (report.class.kind != BT_CLASS_TWIST_PAIR) {
            if (strcmp(row->sheet, "twists") == 0) {
                BT_ABORT("twists row %s classified kind %d", row->rowid, (int)report.class.kind);
            }
            continue;
        }
        int t1 = (int)bt_scheme_defect_twist(&report.defects, report.class.first);
        int t2 = (int)bt_scheme_defect_twist(&report.defects, report.class.second);
        t1 = ((t1 % 3) + 3) % 3;
        t2 = ((t2 % 3) + 3) % 3;
        if (strcmp(row->sheet, "twists") == 0) {
            BT_CHECK_EQ((t1 + t2) % 3, 0);
            twist_rows++;
        }
        BT_CHECK(!(t1 == 1 && t2 == 1));
        BT_CHECK(!(t1 == 2 && t2 == 2));
    }
    BT_CHECK(twist_rows >= 20);
}
