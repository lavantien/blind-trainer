#include <string.h>

#include "common/bt_harness.h"
#include "core/alg.h"
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

BT_TEST(analysis, piece_tables_partition_the_sticker_space) {
    uint8_t corner_slots[8] = {0};
    uint8_t edge_slots[12] = {0};
    for (uint8_t sticker = 0; sticker < 48; sticker++) {
        bt_piece piece = bt_piece_of(sticker);
        if (piece.kind == BT_PIECE_CORNER) {
            BT_CHECK(sticker < 24);
            uint8_t triple[3];
            bt_corner_stickers(piece.index, triple);
            BT_CHECK(triple[0] == sticker || triple[1] == sticker || triple[2] == sticker);
            corner_slots[piece.index] += 1;
        } else {
            BT_CHECK(sticker >= 24);
            uint8_t pair[2];
            bt_edge_stickers(piece.index, pair);
            BT_CHECK(pair[0] == sticker || pair[1] == sticker);
            edge_slots[piece.index] += 1;
        }
    }
    for (size_t i = 0; i < 8; i++) {
        BT_CHECK_EQ(corner_slots[i], 3);
    }
    for (size_t i = 0; i < 12; i++) {
        BT_CHECK_EQ(edge_slots[i], 2);
    }
}

BT_TEST(analysis, head_stickers_are_ud_or_equator_fb) {
    for (uint8_t corner = 0; corner < 8; corner++) {
        uint8_t head = bt_head_sticker((bt_piece){BT_PIECE_CORNER, corner});
        uint8_t triple[3];
        bt_corner_stickers(corner, triple);
        BT_CHECK(head < 24 && head / 4U == triple[0] / 4U);
        BT_CHECK(head / 4U == 0 || head / 4U == 5);
    }
    for (uint8_t edge = 0; edge < 12; edge++) {
        uint8_t head = bt_head_sticker((bt_piece){BT_PIECE_EDGE, edge});
        uint8_t face = (uint8_t)((head - 24U) / 4U);
        BT_CHECK(face == 0 || face == 5 || face == 2 || face == 4);
    }
}

BT_TEST(analysis, cycles_follow_position_of_in_sorted_order) {
    const char *texts[] = {"R", "F2", "x'", "u", "M E S"};
    for (size_t t = 0; t < 5; t++) {
        bt_cube cube = apply(texts[t]);
        bt_cycles cycles = bt_sticker_cycles(&cube);
        BT_CHECK(cycles.count > 0);
        uint8_t firsts[BT_CYCLES_MAX];
        size_t cursor = 0;
        for (size_t i = 0; i < cycles.count; i++) {
            uint8_t min = 255;
            size_t base = cursor;
            for (size_t j = 0; j < cycles.lens[i]; j++) {
                uint8_t s = cycles.items[base + j];
                if (s < min) {
                    min = s;
                }
            }
            BT_CHECK_EQ(cycles.items[base], min);
            firsts[i] = cycles.items[base];
            for (size_t j = 0; j + 1 < cycles.lens[i]; j++) {
                BT_CHECK_EQ(bt_cube_position_of(&cube, cycles.items[base + j]),
                            cycles.items[base + j + 1]);
            }
            BT_CHECK_EQ(bt_cube_position_of(&cube, cycles.items[base + cycles.lens[i] - 1]),
                        cycles.items[base]);
            cursor += cycles.lens[i];
        }
        for (size_t i = 0; i + 1 < cycles.count; i++) {
            BT_CHECK(firsts[i] < firsts[i + 1]);
        }
    }
}

BT_TEST(analysis, cycle_count_matches_nonfixed_stickers) {
    const char *texts[] = {"U", "R2", "M", "z2"};
    for (size_t t = 0; t < 4; t++) {
        bt_cube cube = apply(texts[t]);
        size_t moved = 0;
        for (uint8_t s = 0; s < 48; s++) {
            if (bt_cube_position_of(&cube, s) != s) {
                moved += 1;
            }
        }
        bt_cycles cycles = bt_sticker_cycles(&cube);
        size_t cycled = 0;
        for (size_t i = 0; i < cycles.count; i++) {
            cycled += cycles.lens[i];
        }
        BT_CHECK_EQ(moved, cycled);
    }
}
