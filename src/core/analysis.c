#include "core/analysis.h"

#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"

static const uint8_t CORNER_STICKERS[8][3] = {
    {1, 16, 13},  {0, 17, 4},  {2, 9, 12},   {3, 8, 5},
    {22, 19, 14}, {23, 18, 7}, {21, 10, 15}, {20, 11, 6},
};

static const uint8_t EDGE_STICKERS[12][2] = {
    {24, 40}, {25, 36}, {26, 32}, {27, 28}, {41, 31}, {43, 37},
    {33, 39}, {35, 29}, {46, 42}, {45, 38}, {44, 34}, {47, 30},
};

static uint8_t g_piece_of_sticker[48];
static bool g_analysis_ready;

static void analysis_ensure(void) {
    if (g_analysis_ready) {
        return;
    }
    for (size_t piece = 0; piece < 8; piece++) {
        for (size_t i = 0; i < 3; i++) {
            g_piece_of_sticker[CORNER_STICKERS[piece][i]] = (uint8_t)piece;
        }
    }
    for (size_t piece = 0; piece < 12; piece++) {
        for (size_t i = 0; i < 2; i++) {
            g_piece_of_sticker[EDGE_STICKERS[piece][i]] = (uint8_t)piece;
        }
    }
    g_analysis_ready = true;
}

bt_piece bt_piece_of(uint8_t sticker) {
    BT_TRACE_ENTER("bt_piece_of");
    analysis_ensure();
    if (sticker >= 48) {
        BT_TRACE_FAIL("bt_piece_of", "sticker %u is not a cubie sticker", (unsigned)sticker);
        bt_trace_dump();
        abort();
    }
    bt_piece piece;
    if (sticker < 24) {
        piece.kind = BT_PIECE_CORNER;
        piece.index = g_piece_of_sticker[sticker];
    } else {
        piece.kind = BT_PIECE_EDGE;
        piece.index = g_piece_of_sticker[sticker];
    }
    BT_TRACE_EXIT("bt_piece_of");
    return piece;
}

uint8_t bt_head_sticker(bt_piece piece) {
    BT_TRACE_ENTER("bt_head_sticker");
    analysis_ensure();
    if (piece.kind == BT_PIECE_CORNER && piece.index >= 8) {
        BT_TRACE_FAIL("bt_head_sticker", "corner %u out of range", (unsigned)piece.index);
        BT_TRACE_EXIT("bt_head_sticker");
        return 255;
    }
    if (piece.kind == BT_PIECE_EDGE && piece.index >= 12) {
        BT_TRACE_FAIL("bt_head_sticker", "edge %u out of range", (unsigned)piece.index);
        BT_TRACE_EXIT("bt_head_sticker");
        return 255;
    }
    uint8_t head;
    if (piece.kind == BT_PIECE_CORNER) {
        head = CORNER_STICKERS[piece.index][0];
    } else {
        head = EDGE_STICKERS[piece.index][0];
    }
    BT_TRACE_EXIT("bt_head_sticker");
    return head;
}

void bt_corner_stickers(uint8_t corner, uint8_t out[3]) {
    BT_TRACE_ENTER("bt_corner_stickers");
    analysis_ensure();
    if (corner >= 8) {
        BT_TRACE_FAIL("bt_corner_stickers", "corner %u out of range", (unsigned)corner);
        bt_trace_dump();
        abort();
    }
    memcpy(out, CORNER_STICKERS[corner], 3);
    BT_TRACE_EXIT("bt_corner_stickers");
}

void bt_edge_stickers(uint8_t edge, uint8_t out[2]) {
    BT_TRACE_ENTER("bt_edge_stickers");
    analysis_ensure();
    if (edge >= 12) {
        BT_TRACE_FAIL("bt_edge_stickers", "edge %u out of range", (unsigned)edge);
        bt_trace_dump();
        abort();
    }
    memcpy(out, EDGE_STICKERS[edge], 2);
    BT_TRACE_EXIT("bt_edge_stickers");
}

bt_cycles bt_sticker_cycles(const bt_cube *cube) {
    BT_TRACE_ENTER("bt_sticker_cycles");
    typedef struct {
        uint8_t first;
        uint8_t len;
        size_t offset;
    } cycle_entry;
    bool visited[48] = {false};
    cycle_entry entries[BT_CYCLES_MAX];
    uint8_t count = 0;
    size_t filled = 0;
    uint8_t items[48];
    uint8_t buffer[48];
    for (uint8_t start = 0; start < 48; start++) {
        if (visited[start]) {
            continue;
        }
        size_t len = 0;
        uint8_t current = start;
        while (!visited[current]) {
            visited[current] = true;
            buffer[len++] = current;
            current = bt_cube_position_of(cube, current);
        }
        if (len > 1) {
            uint8_t min = 255;
            size_t split = 0;
            for (size_t i = 0; i < len; i++) {
                if (buffer[i] < min) {
                    min = buffer[i];
                    split = i;
                }
            }
            for (size_t i = 0; i < len; i++) {
                items[filled + i] = buffer[(split + i) % len];
            }
            entries[count].first = items[filled];
            entries[count].len = (uint8_t)len;
            entries[count].offset = filled;
            count += 1;
            filled += len;
        }
    }
    for (size_t i = 1; i < count; i++) {
        cycle_entry key = entries[i];
        size_t j = i;
        while (j > 0 && entries[j - 1].first > key.first) {
            entries[j] = entries[j - 1];
            j -= 1;
        }
        entries[j] = key;
    }
    bt_cycles cycles;
    cycles.count = count;
    size_t out_filled = 0;
    for (size_t i = 0; i < count; i++) {
        for (size_t k = 0; k < entries[i].len; k++) {
            cycles.items[out_filled + k] = items[entries[i].offset + k];
        }
        cycles.lens[i] = entries[i].len;
        out_filled += entries[i].len;
    }
    BT_TRACE_EXIT("bt_sticker_cycles");
    return cycles;
}

static const int8_t ANALYSIS_NORMAL[6][3] = {
    {0, 1, 0}, {-1, 0, 0}, {0, 0, 1}, {1, 0, 0}, {0, 0, -1}, {0, -1, 0},
};

static uint8_t axis_priority(size_t face) {
    if (face == 0 || face == 5) {
        return 2;
    }
    if (face == 2 || face == 4) {
        return 1;
    }
    return 0;
}

static size_t center_home_face(const bt_cube *cube, size_t face) {
    return (size_t)(cube->occ[48 + face] - 48U);
}

static void cross3(const int8_t a[3], const int8_t b[3], int8_t out[3]) {
    out[0] = (int8_t)(a[1] * b[2] - a[2] * b[1]);
    out[1] = (int8_t)(a[2] * b[0] - a[0] * b[2]);
    out[2] = (int8_t)(a[0] * b[1] - a[1] * b[0]);
}

static int32_t dot3(const int8_t a[3], const int8_t b[3]) {
    int32_t total = 0;
    for (size_t i = 0; i < 3; i++) {
        total += (int32_t)a[i] * (int32_t)b[i];
    }
    return total;
}

bool bt_edge_flip(const bt_cube *cube, uint8_t edge) {
    BT_TRACE_ENTER("bt_edge_flip");
    analysis_ensure();
    if (edge >= 12) {
        BT_TRACE_FAIL("bt_edge_flip", "edge %u out of range", (unsigned)edge);
        BT_TRACE_EXIT("bt_edge_flip");
        return false;
    }
    uint8_t pair[2];
    memcpy(pair, EDGE_STICKERS[edge], 2);
    size_t f1 = ((size_t)pair[0] - 24U) / 4U;
    size_t f2 = ((size_t)pair[1] - 24U) / 4U;
    bool slot_high =
        axis_priority(center_home_face(cube, f1)) > axis_priority(center_home_face(cube, f2));
    uint8_t t1 = cube->occ[pair[0]];
    uint8_t t2 = cube->occ[pair[1]];
    bool piece_high =
        axis_priority(((size_t)t1 - 24U) / 4U) > axis_priority(((size_t)t2 - 24U) / 4U);
    bool flipped = piece_high != slot_high;
    BT_TRACE_EXIT("bt_edge_flip");
    return flipped;
}

int8_t bt_corner_twist(const bt_cube *cube, uint8_t corner) {
    BT_TRACE_ENTER("bt_corner_twist");
    analysis_ensure();
    if (corner >= 8) {
        BT_TRACE_FAIL("bt_corner_twist", "corner %u out of range", (unsigned)corner);
        BT_TRACE_EXIT("bt_corner_twist");
        return 0;
    }
    uint8_t slot[3];
    memcpy(slot, CORNER_STICKERS[corner], 3);
    bool has_ud = false;
    size_t ud_face = 0;
    int8_t diagonal[3] = {0, 0, 0};
    for (size_t i = 0; i < 3; i++) {
        size_t face = slot[i] / 4U;
        for (size_t k = 0; k < 3; k++) {
            diagonal[k] = (int8_t)(diagonal[k] + ANALYSIS_NORMAL[face][k]);
        }
        size_t home = center_home_face(cube, face);
        if (home == 0 || home == 5) {
            if (has_ud) {
                BT_TRACE_FAIL("bt_corner_twist", "two ud centers on corner %u", (unsigned)corner);
                bt_trace_dump();
                abort();
            }
            has_ud = true;
            ud_face = face;
        }
    }
    if (!has_ud) {
        BT_TRACE_FAIL("bt_corner_twist", "no ud center on a corner slot");
        bt_trace_dump();
        abort();
    }
    bool found = false;
    uint8_t ud_sticker = 0;
    for (size_t i = 0; i < 3; i++) {
        uint8_t occupant = cube->occ[slot[i]];
        if (occupant / 4U == 0 || occupant / 4U == 5) {
            found = true;
            ud_sticker = occupant;
        }
    }
    if (!found) {
        BT_TRACE_FAIL("bt_corner_twist", "no ud sticker on a corner piece");
        bt_trace_dump();
        abort();
    }
    size_t land = bt_cube_position_of(cube, ud_sticker) / 4U;
    int8_t twist = 0;
    if (land != ud_face) {
        int8_t c[3];
        cross3(ANALYSIS_NORMAL[ud_face], ANALYSIS_NORMAL[land], c);
        twist = dot3(diagonal, c) > 0 ? 1 : -1;
    }
    BT_TRACE_EXIT("bt_corner_twist");
    return twist;
}

bt_state_summary bt_summarize(const bt_cube *cube) {
    BT_TRACE_ENTER("bt_summarize");
    analysis_ensure();
    bt_state_summary summary;
    for (size_t slot = 0; slot < 8; slot++) {
        uint8_t occupant = cube->occ[CORNER_STICKERS[slot][0]];
        summary.corners[slot].piece = g_piece_of_sticker[occupant];
        summary.corners[slot].twist = bt_corner_twist(cube, (uint8_t)slot);
    }
    for (size_t slot = 0; slot < 12; slot++) {
        uint8_t occupant = cube->occ[EDGE_STICKERS[slot][0]];
        summary.edges[slot].piece = g_piece_of_sticker[occupant];
        summary.edges[slot].flipped = bt_edge_flip(cube, (uint8_t)slot);
    }
    summary.cycles = bt_sticker_cycles(cube);
    BT_TRACE_EXIT("bt_summarize");
    return summary;
}
