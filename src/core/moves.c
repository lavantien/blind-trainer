#include "core/moves.h"

#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "core/cube.h"

static const int32_t MOVE_NORMAL[6][3] = {
    {0, 1, 0}, {-1, 0, 0}, {0, 0, 1}, {1, 0, 0}, {0, 0, -1}, {0, -1, 0},
};
static const size_t MOVE_AXIS[6] = {1, 0, 2, 0, 2, 1};
static const int32_t MOVE_SIGN[6] = {1, -1, 1, 1, -1, -1};

static uint8_t g_face_tables[6][3][54];
static uint8_t g_inner_tables[6][3][54];
static uint8_t g_rotation_tables[3][3][54];
static bool g_tables_ready;

void bt_moves_geometry(size_t face, size_t r, size_t c, int32_t out[3]) {
    BT_TRACE_ENTER("bt_moves_geometry");
    int32_t ri = (int32_t)r;
    int32_t ci = (int32_t)c;
    int32_t v[3];
    switch (face) {
    case 0:
        v[0] = ci - 1;
        v[1] = 1;
        v[2] = ri - 1;
        break;
    case 1:
        v[0] = -1;
        v[1] = 1 - ri;
        v[2] = ci - 1;
        break;
    case 2:
        v[0] = ci - 1;
        v[1] = 1 - ri;
        v[2] = 1;
        break;
    case 3:
        v[0] = 1;
        v[1] = 1 - ri;
        v[2] = 1 - ci;
        break;
    case 4:
        v[0] = 1 - ci;
        v[1] = 1 - ri;
        v[2] = -1;
        break;
    default:
        v[0] = ci - 1;
        v[1] = -1;
        v[2] = 1 - ri;
        break;
    }
    out[0] = v[0];
    out[1] = v[1];
    out[2] = v[2];
    BT_TRACE_EXIT("bt_moves_geometry");
}

static void move_rot(const int32_t v[3], size_t axis, size_t times, int32_t out[3]) {
    int32_t w[3] = {v[0], v[1], v[2]};
    for (size_t i = 0; i < times; i++) {
        int32_t x = w[0];
        int32_t y = w[1];
        int32_t z = w[2];
        if (axis == 0) {
            w[0] = x;
            w[1] = -z;
            w[2] = y;
        } else if (axis == 1) {
            w[0] = z;
            w[1] = y;
            w[2] = -x;
        } else {
            w[0] = -y;
            w[1] = x;
            w[2] = z;
        }
    }
    out[0] = w[0];
    out[1] = w[1];
    out[2] = w[2];
}

static void move_home_cell(size_t id, size_t *face, size_t *r, size_t *c) {
    if (id < 24) {
        size_t row;
        size_t col;
        switch (id % 4U) {
        case 0:
            row = 0;
            col = 0;
            break;
        case 1:
            row = 0;
            col = 2;
            break;
        case 2:
            row = 2;
            col = 2;
            break;
        default:
            row = 2;
            col = 0;
            break;
        }
        *face = id / 4U;
        *r = row;
        *c = col;
    } else if (id < 48) {
        size_t e = id - 24;
        size_t row;
        size_t col;
        switch (e % 4U) {
        case 0:
            row = 0;
            col = 1;
            break;
        case 1:
            row = 1;
            col = 2;
            break;
        case 2:
            row = 2;
            col = 1;
            break;
        default:
            row = 1;
            col = 0;
            break;
        }
        *face = e / 4U;
        *r = row;
        *c = col;
    } else {
        *face = id - 48U;
        *r = 1;
        *c = 1;
    }
}

static size_t move_sticker_id(size_t face, size_t r, size_t c) {
    if (r == 1 && c == 1) {
        return 48 + face;
    }
    if (r % 2U == 0 && c % 2U == 0) {
        size_t pos = r == 0 ? c / 2U : 3U - c / 2U;
        return face * 4U + pos;
    }
    size_t pos = r == 1 ? 3U - c : r;
    return 24 + face * 4U + pos;
}

size_t bt_moves_face_of_normal(const int32_t n[3]) {
    BT_TRACE_ENTER("bt_moves_face_of_normal");
    for (size_t f = 0; f < 6; f++) {
        if (MOVE_NORMAL[f][0] == n[0] && MOVE_NORMAL[f][1] == n[1] && MOVE_NORMAL[f][2] == n[2]) {
            BT_TRACE_EXIT("bt_moves_face_of_normal");
            return f;
        }
    }
    BT_TRACE_EXIT("bt_moves_face_of_normal");
    return 6;
}

void bt_moves_cell_of(size_t face, const int32_t p[3], size_t *r, size_t *c) {
    BT_TRACE_ENTER("bt_moves_cell_of");
    for (size_t row = 0; row < 3; row++) {
        for (size_t col = 0; col < 3; col++) {
            int32_t g[3];
            bt_moves_geometry(face, row, col, g);
            if (g[0] == p[0] && g[1] == p[1] && g[2] == p[2]) {
                *r = row;
                *c = col;
                BT_TRACE_EXIT("bt_moves_cell_of");
                return;
            }
        }
    }
    *r = 9;
    *c = 9;
    BT_TRACE_EXIT("bt_moves_cell_of");
}

static void move_layer_perm(size_t face, int32_t depth, size_t quarter, bool whole,
                            uint8_t perm[54]) {
    const int32_t *n = MOVE_NORMAL[face];
    size_t axis = MOVE_AXIS[face];
    size_t applied = MOVE_SIGN[face] < 0 ? quarter : (4U - quarter) % 4U;
    for (size_t id = 0; id < 54; id++) {
        size_t hf;
        size_t r;
        size_t c;
        move_home_cell(id, &hf, &r, &c);
        int32_t p[3];
        bt_moves_geometry(hf, r, c, p);
        int32_t d = p[0] * n[0] + p[1] * n[1] + p[2] * n[2];
        if (whole || d == depth) {
            int32_t p2[3];
            int32_t n2[3];
            move_rot(p, axis, applied, p2);
            move_rot(MOVE_NORMAL[hf], axis, applied, n2);
            size_t fd = bt_moves_face_of_normal(n2);
            size_t cr;
            size_t cc;
            bt_moves_cell_of(fd, p2, &cr, &cc);
            perm[id] = (uint8_t)move_sticker_id(fd, cr, cc);
        } else {
            perm[id] = (uint8_t)id;
        }
    }
}

static void move_compose(const uint8_t a[54], const uint8_t b[54], uint8_t out[54]) {
    for (size_t i = 0; i < 54; i++) {
        out[i] = a[b[i]];
    }
}

static void move_power(const uint8_t base[54], size_t times, uint8_t out[54]) {
    uint8_t acc[54];
    for (size_t i = 0; i < 54; i++) {
        acc[i] = (uint8_t)i;
    }
    for (size_t i = 0; i < times; i++) {
        uint8_t next[54];
        move_compose(base, acc, next);
        memcpy(acc, next, sizeof acc);
    }
    memcpy(out, acc, 54);
}

static void move_quarter_tables(const uint8_t base[54], uint8_t out[3][54]) {
    move_power(base, 1, out[0]);
    move_power(base, 2, out[1]);
    move_power(base, 3, out[2]);
}

void bt_core_init(void) {
    BT_TRACE_ENTER("bt_core_init");
    if (g_tables_ready) {
        BT_TRACE_EXIT("bt_core_init");
        return;
    }
    for (size_t depth = 0; depth < 2; depth++) {
        uint8_t (*tables)[3][54] = depth == 0 ? g_inner_tables : g_face_tables;
        int32_t layer = depth == 0 ? 0 : 1;
        for (size_t face = 0; face < 6; face++) {
            uint8_t base[54];
            move_layer_perm(face, layer, 1, false, base);
            move_quarter_tables(base, tables[face]);
        }
    }
    const size_t rotation_faces[3] = {3, 0, 2};
    for (size_t axis = 0; axis < 3; axis++) {
        uint8_t base[54];
        move_layer_perm(rotation_faces[axis], 0, 1, true, base);
        move_quarter_tables(base, g_rotation_tables[axis]);
    }
    g_tables_ready = true;
    BT_TRACE_EXIT("bt_core_init");
}

static size_t slice_follows(uint8_t slice) {
    switch (slice) {
    case BT_SLICE_M:
        return BT_FACE_L;
    case BT_SLICE_E:
        return BT_FACE_D;
    default:
        return BT_FACE_F;
    }
}

bt_move bt_move_of_face(uint8_t face, uint8_t quarter) {
    return (bt_move){BT_MOVE_FACE, face, 0, 0, quarter};
}

bt_move bt_move_of_wide(uint8_t face, uint8_t quarter) {
    return (bt_move){BT_MOVE_WIDE, face, 0, 0, quarter};
}

bt_move bt_move_of_slice(uint8_t slice, uint8_t quarter) {
    return (bt_move){BT_MOVE_SLICE, 0, slice, 0, quarter};
}

bt_move bt_move_of_inner(uint8_t face, uint8_t quarter) {
    return (bt_move){BT_MOVE_INNER, face, 0, 0, quarter};
}

bt_move bt_move_of_rotation(uint8_t axis, uint8_t quarter) {
    return (bt_move){BT_MOVE_ROTATION, 0, 0, axis, quarter};
}

uint8_t bt_move_quarter(bt_move mv) {
    return mv.quarter;
}

bt_move bt_move_invert(bt_move mv) {
    mv.quarter = (uint8_t)(4U - mv.quarter);
    return mv;
}

bt_move bt_move_with_quarter(bt_move mv, uint8_t quarter) {
    mv.quarter = quarter;
    return mv;
}

bool bt_move_eq(bt_move a, bt_move b) {
    if (a.kind != b.kind) {
        return false;
    }
    if (a.face != b.face) {
        return false;
    }
    if (a.slice != b.slice) {
        return false;
    }
    if (a.axis != b.axis) {
        return false;
    }
    return a.quarter == b.quarter;
}

void bt_move_table(bt_move mv, uint8_t out[54]) {
    BT_TRACE_ENTER("bt_move_table");
    bt_core_init();
    bool face_used = mv.kind == BT_MOVE_FACE;
    if (!face_used) {
        face_used = mv.kind == BT_MOVE_WIDE;
    }
    if (!face_used) {
        face_used = mv.kind == BT_MOVE_INNER;
    }
    if (mv.quarter < 1 || mv.quarter > 3) {
        BT_TRACE_FAIL("bt_move_table", "quarter %u out of range", (unsigned)mv.quarter);
        bt_trace_dump();
        abort();
    }
    if (face_used && mv.face >= BT_FACE_COUNT) {
        BT_TRACE_FAIL("bt_move_table", "face %u out of range", (unsigned)mv.face);
        bt_trace_dump();
        abort();
    }
    if (mv.kind == BT_MOVE_SLICE && mv.slice > BT_SLICE_S) {
        BT_TRACE_FAIL("bt_move_table", "slice %u out of range", (unsigned)mv.slice);
        bt_trace_dump();
        abort();
    }
    if (mv.kind == BT_MOVE_ROTATION && mv.axis > BT_AXIS_Z) {
        BT_TRACE_FAIL("bt_move_table", "axis %u out of range", (unsigned)mv.axis);
        bt_trace_dump();
        abort();
    }
    size_t q = (size_t)mv.quarter - 1U;
    switch (mv.kind) {
    case BT_MOVE_FACE:
        memcpy(out, g_face_tables[mv.face][q], 54);
        break;
    case BT_MOVE_INNER:
        memcpy(out, g_inner_tables[mv.face][q], 54);
        break;
    case BT_MOVE_WIDE:
        move_compose(g_inner_tables[mv.face][q], g_face_tables[mv.face][q], out);
        break;
    case BT_MOVE_SLICE:
        memcpy(out, g_inner_tables[slice_follows(mv.slice)][q], 54);
        break;
    default:
        memcpy(out, g_rotation_tables[mv.axis][q], 54);
        break;
    }
    BT_TRACE_EXIT("bt_move_table");
}
