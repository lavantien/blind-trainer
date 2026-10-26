#include "core/cube.h"

#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"

uint8_t bt_corner_id(bt_face face, uint8_t pos) {
    BT_TRACE_ENTER("bt_corner_id");
    uint8_t id = (uint8_t)((size_t)face * 4U + pos);
    BT_TRACE_EXIT("bt_corner_id");
    return id;
}

uint8_t bt_edge_id(bt_face face, uint8_t pos) {
    BT_TRACE_ENTER("bt_edge_id");
    uint8_t id = (uint8_t)(24 + (size_t)face * 4U + pos);
    BT_TRACE_EXIT("bt_edge_id");
    return id;
}

uint8_t bt_center_id(bt_face face) {
    BT_TRACE_ENTER("bt_center_id");
    uint8_t id = (uint8_t)(48 + face);
    BT_TRACE_EXIT("bt_center_id");
    return id;
}

bt_cube bt_cube_solved(void) {
    BT_TRACE_ENTER("bt_cube_solved");
    bt_cube cube;
    for (size_t i = 0; i < 54; i++) {
        cube.occ[i] = (uint8_t)i;
    }
    BT_TRACE_EXIT("bt_cube_solved");
    return cube;
}

static size_t band_of(size_t value) {
    if (value < 24) {
        return 0;
    }
    if (value < 48) {
        return 1;
    }
    return 2;
}

bool bt_cube_try_from_occ(const uint8_t occ[54], bt_cube *out) {
    BT_TRACE_ENTER("bt_cube_try_from_occ");
    bool seen[54] = {false};
    for (size_t i = 0; i < 54; i++) {
        seen[occ[i]] = true;
    }
    for (size_t i = 0; i < 54; i++) {
        if (!seen[i]) {
            BT_TRACE_FAIL("bt_cube_try_from_occ", "occ must be a permutation of 0..54");
            BT_TRACE_EXIT("bt_cube_try_from_occ");
            return false;
        }
    }
    for (size_t slot = 0; slot < 54; slot++) {
        uint8_t s = occ[slot];
        size_t sticker_band = band_of(s);
        size_t slot_band = band_of(slot);
        if (sticker_band != slot_band) {
            BT_TRACE_FAIL("bt_cube_try_from_occ", "sticker %u in slot %u crosses orbit bands",
                          (unsigned)s, (unsigned)slot);
            BT_TRACE_EXIT("bt_cube_try_from_occ");
            return false;
        }
    }
    for (size_t i = 0; i < 54; i++) {
        out->occ[i] = occ[i];
    }
    BT_TRACE_EXIT("bt_cube_try_from_occ");
    return true;
}

bt_cube bt_cube_from_occ(const uint8_t occ[54]) {
    BT_TRACE_ENTER("bt_cube_from_occ");
    bt_cube out;
    if (!bt_cube_try_from_occ(occ, &out)) {
        bt_trace_dump();
        abort();
    }
    BT_TRACE_EXIT("bt_cube_from_occ");
    return out;
}

void bt_cube_apply(bt_cube *cube, bt_move mv) {
    BT_TRACE_ENTER("bt_cube_apply");
    uint8_t table[54];
    bt_move_table(mv, table);
    uint8_t next[54];
    for (size_t i = 0; i < 54; i++) {
        next[table[i]] = cube->occ[i];
    }
    for (size_t i = 0; i < 54; i++) {
        cube->occ[i] = next[i];
    }
    BT_TRACE_EXIT("bt_cube_apply");
}

void bt_cube_apply_alg(bt_cube *cube, const bt_alg *alg) {
    BT_TRACE_ENTER("bt_cube_apply_alg");
    for (size_t i = 0; i < alg->len; i++) {
        bt_cube_apply(cube, alg->items[i]);
    }
    BT_TRACE_EXIT("bt_cube_apply_alg");
}

uint8_t bt_cube_sticker_at(const bt_cube *cube, uint8_t pos) {
    BT_TRACE_ENTER("bt_cube_sticker_at");
    uint8_t sticker = cube->occ[pos];
    BT_TRACE_EXIT("bt_cube_sticker_at");
    return sticker;
}

uint8_t bt_cube_position_of(const bt_cube *cube, uint8_t sticker) {
    BT_TRACE_ENTER("bt_cube_position_of");
    for (size_t i = 0; i < 54; i++) {
        if (cube->occ[i] == sticker) {
            BT_TRACE_EXIT("bt_cube_position_of");
            return (uint8_t)i;
        }
    }
    BT_TRACE_FAIL("bt_cube_position_of", "sticker %u not present", (unsigned)sticker);
    abort();
}

bt_cube bt_cube_inverse_state(const bt_cube *cube) {
    BT_TRACE_ENTER("bt_cube_inverse_state");
    bt_cube inv;
    for (size_t i = 0; i < 54; i++) {
        inv.occ[cube->occ[i]] = (uint8_t)i;
    }
    BT_TRACE_EXIT("bt_cube_inverse_state");
    return inv;
}

bool bt_cube_is_solved(const bt_cube *cube) {
    BT_TRACE_ENTER("bt_cube_is_solved");
    for (size_t i = 0; i < 54; i++) {
        if (cube->occ[i] != (uint8_t)i) {
            BT_TRACE_EXIT("bt_cube_is_solved");
            return false;
        }
    }
    BT_TRACE_EXIT("bt_cube_is_solved");
    return true;
}

bool bt_cube_centers_fixed(const bt_cube *cube) {
    BT_TRACE_ENTER("bt_cube_centers_fixed");
    for (size_t f = 0; f < 6; f++) {
        if (cube->occ[48 + f] != (uint8_t)(48 + f)) {
            BT_TRACE_EXIT("bt_cube_centers_fixed");
            return false;
        }
    }
    BT_TRACE_EXIT("bt_cube_centers_fixed");
    return true;
}

bool bt_cube_eq(const bt_cube *a, const bt_cube *b) {
    BT_TRACE_ENTER("bt_cube_eq");
    bool equal = memcmp(a->occ, b->occ, sizeof a->occ) == 0;
    BT_TRACE_EXIT("bt_cube_eq");
    return equal;
}
