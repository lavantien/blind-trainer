#include "common.h"
#include "core/analysis.h"
#include "core/cube.h"

static bt_cube apply(const char *text) {
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

static size_t flipped_set(const bt_cube *cube, uint8_t out[12]) {
    size_t count = 0;
    for (uint8_t e = 0; e < 12; e++) {
        if (bt_edge_flip(cube, e)) {
            out[count++] = e;
        }
    }
    return count;
}

static void twist_map(const bt_cube *cube, int8_t out[8]) {
    for (uint8_t c = 0; c < 8; c++) {
        out[c] = bt_corner_twist(cube, c);
    }
}

static void check_flipped_equals(const bt_cube *cube, const uint8_t *want, size_t want_len) {
    uint8_t got[12];
    size_t got_len = flipped_set(cube, got);
    if (got_len != want_len) {
        BT_ABORT("flipped set size %zu want %zu", got_len, want_len);
    }
    for (size_t i = 0; i < want_len; i++) {
        if (got[i] != want[i]) {
            BT_ABORT("flipped set[%zu] %u want %u", i, got[i], want[i]);
        }
    }
}

static void check_cycles_equals(const bt_cycles *cycles, const uint8_t *lens, size_t lens_len,
                                const uint8_t *items) {
    if (cycles->count != lens_len) {
        BT_ABORT("cycle count %u want %zu", cycles->count, lens_len);
    }
    size_t cursor = 0;
    for (size_t i = 0; i < lens_len; i++) {
        if (cycles->lens[i] != lens[i]) {
            BT_ABORT("cycle %zu len %u want %u", i, cycles->lens[i], lens[i]);
        }
        for (size_t k = 0; k < lens[i]; k++) {
            if (cycles->items[cursor + k] != items[cursor + k]) {
                BT_ABORT("cycle %zu item %zu: %u want %u", i, k, cycles->items[cursor + k],
                         items[cursor + k]);
            }
        }
        cursor += lens[i];
    }
}

BT_TEST(analysis_pins, piece_of_matches_position_map_tsv_for_all_stickers) {
    tc_name_index names;
    tc_position_names(&names);
    bool corner_pieces[8] = {false};
    bool edge_pieces[12] = {false};
    for (uint8_t sticker = 0; sticker < 48; sticker++) {
        bt_piece piece = bt_piece_of(sticker);
        if (piece.kind == BT_PIECE_CORNER) {
            BT_CHECK(sticker < 24);
            BT_CHECK_EQ(piece.index, names.piece_of_sticker[sticker]);
            corner_pieces[piece.index] = true;
        } else {
            BT_CHECK(sticker >= 24 && sticker < 48);
            BT_CHECK_EQ(piece.index, names.piece_of_sticker[sticker]);
            edge_pieces[piece.index] = true;
        }
    }
    for (size_t i = 0; i < 8; i++) {
        BT_CHECK(corner_pieces[i]);
    }
    for (size_t i = 0; i < 12; i++) {
        BT_CHECK(edge_pieces[i]);
    }
}

BT_TEST(analysis_pins, head_sticker_follows_ud_else_fb_rule) {
    tc_name_index names;
    tc_position_names(&names);
    uint8_t corner_groups[8][3];
    for (uint8_t piece = 0; piece < 8; piece++) {
        size_t n = 0;
        for (uint8_t s = 0; s < 24; s++) {
            if (names.piece_of_sticker[s] == piece) {
                corner_groups[piece][n++] = s;
            }
        }
        BT_CHECK_EQ(n, (size_t)3);
    }
    uint8_t edge_groups[12][2];
    for (uint8_t piece = 0; piece < 12; piece++) {
        size_t n = 0;
        for (uint8_t s = 24; s < 48; s++) {
            if (names.piece_of_sticker[s] == piece) {
                edge_groups[piece][n++] = s;
            }
        }
        BT_CHECK_EQ(n, (size_t)2);
    }
    for (uint8_t piece = 0; piece < 8; piece++) {
        uint8_t head = bt_head_sticker((bt_piece){BT_PIECE_CORNER, piece});
        uint8_t ud[3];
        size_t ud_len = 0;
        for (size_t k = 0; k < 3; k++) {
            char first = names.name_of_sticker[corner_groups[piece][k]][0];
            if (first == 'U' || first == 'D') {
                ud[ud_len++] = corner_groups[piece][k];
            }
        }
        uint8_t expected;
        if (ud_len == 0) {
            uint8_t fb[3];
            size_t fb_len = 0;
            for (size_t k = 0; k < 3; k++) {
                char first = names.name_of_sticker[corner_groups[piece][k]][0];
                if (first == 'F' || first == 'B') {
                    fb[fb_len++] = corner_groups[piece][k];
                }
            }
            BT_CHECK_EQ(fb_len, (size_t)1);
            expected = fb[0];
        } else {
            BT_CHECK_EQ(ud_len, (size_t)1);
            expected = ud[0];
        }
        BT_CHECK(head == corner_groups[piece][0] || head == corner_groups[piece][1] ||
                 head == corner_groups[piece][2]);
        BT_CHECK_EQ(head, expected);
    }
    for (uint8_t piece = 0; piece < 12; piece++) {
        uint8_t head = bt_head_sticker((bt_piece){BT_PIECE_EDGE, piece});
        uint8_t ud[2];
        size_t ud_len = 0;
        for (size_t k = 0; k < 2; k++) {
            char first = names.name_of_sticker[edge_groups[piece][k]][0];
            if (first == 'U' || first == 'D') {
                ud[ud_len++] = edge_groups[piece][k];
            }
        }
        uint8_t expected;
        if (ud_len == 0) {
            uint8_t fb[2];
            size_t fb_len = 0;
            for (size_t k = 0; k < 2; k++) {
                char first = names.name_of_sticker[edge_groups[piece][k]][0];
                if (first == 'F' || first == 'B') {
                    fb[fb_len++] = edge_groups[piece][k];
                }
            }
            BT_CHECK_EQ(fb_len, (size_t)1);
            expected = fb[0];
        } else {
            BT_CHECK_EQ(ud_len, (size_t)1);
            expected = ud[0];
        }
        BT_CHECK(head == edge_groups[piece][0] || head == edge_groups[piece][1]);
        BT_CHECK_EQ(head, expected);
    }
}

BT_TEST(analysis_pins, head_sticker_names_are_canonical_piece_names) {
    tc_name_index names;
    tc_position_names(&names);
    const uint8_t corner_idx[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    const char *corner_names[8] = {"UBR", "UBL", "UFR", "UFL", "DBR", "DBL", "DFR", "DFL"};
    for (size_t i = 0; i < 8; i++) {
        uint8_t head = bt_head_sticker((bt_piece){BT_PIECE_CORNER, corner_idx[i]});
        BT_CHECK_EQ((const char *)names.name_of_sticker[head], corner_names[i]);
    }
    const uint8_t edge_idx[8] = {0, 2, 4, 5, 6, 7, 8, 11};
    const char *edge_names[8] = {"UB", "UF", "BL", "BR", "FR", "FL", "DB", "DL"};
    for (size_t i = 0; i < 8; i++) {
        uint8_t head = bt_head_sticker((bt_piece){BT_PIECE_EDGE, edge_idx[i]});
        BT_CHECK_EQ((const char *)names.name_of_sticker[head], edge_names[i]);
    }
}

BT_TEST(analysis_pins, sticker_cycles_on_solved_is_empty) {
    bt_cube cube = bt_cube_solved();
    bt_cycles cycles = bt_sticker_cycles(&cube);
    BT_CHECK_EQ(cycles.count, 0);
}

BT_TEST(analysis_pins, sticker_cycles_single_turn_pins) {
    {
        bt_cube cube = apply("U");
        bt_cycles cycles = bt_sticker_cycles(&cube);
        const uint8_t lens[5] = {4, 4, 4, 4, 4};
        const uint8_t items[20] = {
            0, 1, 2, 3, 4, 16, 12, 8, 5, 17, 13, 9, 24, 25, 26, 27, 28, 40, 36, 32,
        };
        check_cycles_equals(&cycles, lens, 5, items);
    }
    {
        bt_cube cube = apply("U'");
        bt_cycles cycles = bt_sticker_cycles(&cube);
        const uint8_t lens[5] = {4, 4, 4, 4, 4};
        const uint8_t items[20] = {
            0, 3, 2, 1, 4, 8, 12, 16, 5, 9, 13, 17, 24, 27, 26, 25, 28, 32, 36, 40,
        };
        check_cycles_equals(&cycles, lens, 5, items);
    }
    {
        bt_cube cube = apply("D2");
        bt_cycles cycles = bt_sticker_cycles(&cube);
        const uint8_t lens[10] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
        const uint8_t items[20] = {
            6, 14, 7, 15, 10, 18, 11, 19, 20, 22, 21, 23, 30, 38, 34, 42, 44, 46, 45, 47,
        };
        check_cycles_equals(&cycles, lens, 10, items);
    }
}

BT_TEST(analysis_pins, sticker_cycles_rotation_pin_and_coverage) {
    const char *texts[] = {"x", "y", "z", "R U R' U'", "M2", "S'"};
    for (size_t t = 0; t < 6; t++) {
        bt_cube cube = apply(texts[t]);
        bt_cycles cycles = bt_sticker_cycles(&cube);
        bool seen[48] = {false};
        size_t cursor = 0;
        for (size_t i = 0; i < cycles.count; i++) {
            BT_CHECK(cycles.lens[i] > 1);
            uint8_t min = 255;
            for (size_t k = 0; k < cycles.lens[i]; k++) {
                uint8_t s = cycles.items[cursor + k];
                if (s < min) {
                    min = s;
                }
            }
            BT_CHECK_EQ(cycles.items[cursor], min);
            for (size_t k = 0; k < cycles.lens[i]; k++) {
                uint8_t s = cycles.items[cursor + k];
                BT_CHECK(s < 48);
                BT_CHECK(!seen[s]);
                seen[s] = true;
            }
            for (size_t k = 0; k < cycles.lens[i]; k++) {
                uint8_t next = cycles.items[cursor + (k + 1) % cycles.lens[i]];
                BT_CHECK_EQ(bt_cube_position_of(&cube, cycles.items[cursor + k]), next);
            }
            cursor += cycles.lens[i];
        }
        cursor = 0;
        for (size_t i = 0; i + 1 < cycles.count; i++) {
            cursor += cycles.lens[i];
            BT_CHECK(cycles.items[cursor - cycles.lens[i]] < cycles.items[cursor]);
        }
        if (strcmp(texts[t], "x") == 0) {
            bool found = false;
            cursor = 0;
            for (size_t i = 0; i < cycles.count; i++) {
                if (cycles.lens[i] == 4 && cycles.items[cursor] == 26 &&
                    cycles.items[cursor + 1] == 40 && cycles.items[cursor + 2] == 46 &&
                    cycles.items[cursor + 3] == 34) {
                    found = true;
                }
                cursor += cycles.lens[i];
            }
            BT_CHECK(found);
        }
        if (strcmp(texts[t], "y") == 0) {
            bool found_flat = false;
            bool found_ring = false;
            cursor = 0;
            for (size_t i = 0; i < cycles.count; i++) {
                if (cycles.lens[i] == 4 && cycles.items[cursor] == 24 &&
                    cycles.items[cursor + 1] == 25 && cycles.items[cursor + 2] == 26 &&
                    cycles.items[cursor + 3] == 27) {
                    found_flat = true;
                }
                if (cycles.lens[i] == 4 && cycles.items[cursor] == 30 &&
                    cycles.items[cursor + 1] == 42 && cycles.items[cursor + 2] == 38 &&
                    cycles.items[cursor + 3] == 34) {
                    found_ring = true;
                }
                cursor += cycles.lens[i];
            }
            BT_CHECK(found_flat && found_ring);
        }
    }
}

BT_TEST(analysis_pins, solved_state_has_no_flips_or_twists) {
    bt_cube cube = bt_cube_solved();
    for (uint8_t e = 0; e < 12; e++) {
        BT_CHECK(!bt_edge_flip(&cube, e));
    }
    for (uint8_t c = 0; c < 8; c++) {
        BT_CHECK_EQ(bt_corner_twist(&cube, c), 0);
    }
}

BT_TEST(analysis_pins, fb_quarter_turns_flip_their_four_edges) {
    const uint8_t f[4] = {2, 6, 7, 10};
    const uint8_t b[4] = {0, 4, 5, 8};
    bt_cube cube = apply("F");
    check_flipped_equals(&cube, f, 4);
    cube = apply("F'");
    check_flipped_equals(&cube, f, 4);
    cube = apply("B");
    check_flipped_equals(&cube, b, 4);
    cube = apply("B'");
    check_flipped_equals(&cube, b, 4);
    uint8_t out[12];
    cube = apply("F2");
    BT_CHECK_EQ(flipped_set(&cube, out), (size_t)0);
    cube = apply("B2");
    BT_CHECK_EQ(flipped_set(&cube, out), (size_t)0);
}

BT_TEST(analysis_pins, udlr_mes_and_wide_lrud_flip_none) {
    const char *texts[] = {
        "U",  "U'", "U2", "D", "D'", "D2", "L", "L'", "L2", "R", "R'", "R2", "M", "M'", "M2", "E",
        "E'", "E2", "S2", "r", "r'", "r2", "l", "l'", "l2", "u", "u'", "u2", "d", "d'", "d2",
    };
    for (size_t i = 0; i < sizeof texts / sizeof texts[0]; i++) {
        bt_cube cube = apply(texts[i]);
        uint8_t out[12];
        size_t count = flipped_set(&cube, out);
        if (count != 0) {
            BT_ABORT("%s flipped %zu edges", texts[i], count);
        }
    }
}

BT_TEST(analysis_pins, s_flips_the_eight_fb_axis_edge_slots_center_relative) {
    const uint8_t s[8] = {0, 2, 4, 5, 6, 7, 8, 10};
    bt_cube cube = apply("S");
    check_flipped_equals(&cube, s, 8);
    cube = apply("S'");
    check_flipped_equals(&cube, s, 8);
}

BT_TEST(analysis_pins, wide_fb_flip_the_back_four_center_relative) {
    const uint8_t back[4] = {0, 4, 5, 8};
    const uint8_t front[4] = {2, 6, 7, 10};
    bt_cube cube = apply("f");
    check_flipped_equals(&cube, back, 4);
    cube = apply("f'");
    check_flipped_equals(&cube, back, 4);
    cube = apply("b");
    check_flipped_equals(&cube, front, 4);
    cube = apply("b'");
    check_flipped_equals(&cube, front, 4);
}

BT_TEST(analysis_pins, rotations_flip_none_center_relative) {
    const char *texts[] = {"x", "x'", "x2", "y", "y'", "y2", "z", "z'", "z2"};
    for (size_t i = 0; i < 9; i++) {
        bt_cube cube = apply(texts[i]);
        uint8_t out[12];
        BT_CHECK_EQ(flipped_set(&cube, out), (size_t)0);
    }
}

BT_TEST(analysis_pins, center_priority_tie_on_a_slot_resolves_to_not_high) {
    uint8_t occ[54];
    for (size_t i = 0; i < 54; i++) {
        occ[i] = (uint8_t)i;
    }
    uint8_t tmp = occ[50];
    occ[50] = occ[53];
    occ[53] = tmp;
    bt_cube cube;
    BT_CHECK(bt_cube_try_from_occ(occ, &cube));
    BT_CHECK(bt_edge_flip(&cube, 2));
    BT_CHECK(!bt_edge_flip(&cube, 0));
}

BT_TEST(analysis_pins, piece_priority_tie_on_a_slot_resolves_to_not_high) {
    uint8_t occ[54];
    for (size_t i = 0; i < 54; i++) {
        occ[i] = (uint8_t)i;
    }
    uint8_t tmp = occ[24];
    occ[24] = occ[26];
    occ[26] = tmp;
    tmp = occ[32];
    occ[32] = occ[44];
    occ[44] = tmp;
    bt_cube cube;
    BT_CHECK(bt_cube_try_from_occ(occ, &cube));
    BT_CHECK(bt_edge_flip(&cube, 2));
    BT_CHECK(!bt_edge_flip(&cube, 0));
}

BT_TEST(analysis_pins, ud_turns_twist_no_corners) {
    const char *texts[] = {"U", "U'", "U2", "D", "D'", "D2"};
    for (size_t i = 0; i < 6; i++) {
        bt_cube cube = apply(texts[i]);
        int8_t twists[8];
        twist_map(&cube, twists);
        for (size_t k = 0; k < 8; k++) {
            BT_CHECK_EQ(twists[k], 0);
        }
    }
}

BT_TEST(analysis_pins, lrfb_quarter_turns_twist_their_four_corners_with_sum_zero) {
    struct {
        const char *text;
        size_t face;
    } pins[] = {
        {"R", 3}, {"R'", 3}, {"L", 1}, {"L'", 1}, {"F", 2}, {"F'", 2}, {"B", 4}, {"B'", 4},
    };
    for (size_t i = 0; i < 8; i++) {
        bt_cube cube = apply(pins[i].text);
        int8_t twists[8];
        twist_map(&cube, twists);
        int32_t sum = 0;
        for (size_t slot = 0; slot < 8; slot++) {
            uint8_t triple[3];
            bt_corner_stickers((uint8_t)slot, triple);
            bool on_face = triple[0] / 4U == pins[i].face;
            if (!on_face) {
                on_face = triple[1] / 4U == pins[i].face;
            }
            if (!on_face) {
                on_face = triple[2] / 4U == pins[i].face;
            }
            if (on_face) {
                BT_CHECK(twists[slot] != 0);
                BT_CHECK(twists[slot] == 1 || twists[slot] == -1);
            } else {
                BT_CHECK_EQ(twists[slot], 0);
            }
            sum += twists[slot];
        }
        BT_CHECK_EQ(sum % 3, 0);
    }
}

BT_TEST(analysis_pins, rotations_preserve_all_twists) {
    const char *texts[] = {"x", "x'", "x2", "y", "y'", "y2", "z", "z'", "z2"};
    for (size_t i = 0; i < 9; i++) {
        bt_cube cube = apply(texts[i]);
        int8_t twists[8];
        twist_map(&cube, twists);
        for (size_t k = 0; k < 8; k++) {
            BT_CHECK_EQ(twists[k], 0);
        }
    }
}

BT_TEST(analysis_pins, sexy_move_returns_twists_to_zero_at_order_six) {
    bt_alg alg;
    bt_alg_init(&alg);
    char err[64];
    BT_CHECK(bt_alg_parse("R U R' U'", &alg, err, sizeof err));
    for (uint32_t k = 1; k <= 6; k++) {
        bt_cube cube = bt_cube_solved();
        for (uint32_t r = 0; r < k; r++) {
            bt_cube_apply_alg(&cube, &alg);
        }
        if (k == 6) {
            int8_t twists[8];
            twist_map(&cube, twists);
            for (size_t n = 0; n < 8; n++) {
                BT_CHECK_EQ(twists[n], 0);
            }
            uint8_t out[12];
            BT_CHECK_EQ(flipped_set(&cube, out), (size_t)0);
            BT_CHECK(bt_cube_is_solved(&cube));
        } else {
            BT_CHECK(!bt_cube_is_solved(&cube));
        }
    }
    bt_alg_free(&alg);
}

BT_TEST(analysis_pins, random_algs_keep_flip_parity_even_and_twist_sum_zero) {
    bt_rng rng;
    bt_rng_seed(&rng, 20261012);
    char text[256];
    for (size_t case_index = 0; case_index < 2000; case_index++) {
        tc_alg_text(&rng, text, sizeof text);
        bt_alg alg;
        bt_alg_init(&alg);
        char err[64];
        BT_CHECK(bt_alg_parse(text, &alg, err, sizeof err));
        bt_cube cube = bt_cube_solved();
        bt_cube_apply_alg(&cube, &alg);
        bt_state_summary summary = bt_summarize(&cube);
        size_t flips = 0;
        for (size_t e = 0; e < 12; e++) {
            if (summary.edges[e].flipped) {
                flips += 1;
            }
        }
        if (flips % 2 != 0) {
            bt_strbuf sb;
            bt_strbuf_init(&sb, 256);
            bt_alg_format(&alg, &sb);
            BT_ABORT("odd flips for %s", sb.data);
        }
        int32_t twist_sum = 0;
        for (size_t c = 0; c < 8; c++) {
            twist_sum += summary.corners[c].twist;
        }
        if (((twist_sum % 3) + 3) % 3 != 0) {
            bt_strbuf sb;
            bt_strbuf_init(&sb, 256);
            bt_alg_format(&alg, &sb);
            BT_ABORT("twist sum %d for %s", twist_sum, sb.data);
        }
        bt_alg_free(&alg);
    }
}
