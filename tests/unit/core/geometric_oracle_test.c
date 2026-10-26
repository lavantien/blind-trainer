#include "common.h"

static const int32_t O_NORMALS[6][3] = {
    {0, 1, 0}, {-1, 0, 0}, {0, 0, 1}, {1, 0, 0}, {0, 0, -1}, {0, -1, 0},
};
static const size_t O_AXES[6] = {1, 0, 2, 0, 2, 1};
static const int32_t O_SIGNS[6] = {1, -1, 1, 1, -1, -1};

static void o_grid(size_t face, int32_t r, int32_t c, int32_t out[3]) {
    int32_t v[3];
    switch (face) {
    case 0:
        v[0] = c - 1;
        v[1] = 1;
        v[2] = r - 1;
        break;
    case 1:
        v[0] = -1;
        v[1] = 1 - r;
        v[2] = c - 1;
        break;
    case 2:
        v[0] = c - 1;
        v[1] = 1 - r;
        v[2] = 1;
        break;
    case 3:
        v[0] = 1;
        v[1] = 1 - r;
        v[2] = 1 - c;
        break;
    case 4:
        v[0] = 1 - c;
        v[1] = 1 - r;
        v[2] = -1;
        break;
    default:
        v[0] = c - 1;
        v[1] = -1;
        v[2] = 1 - r;
        break;
    }
    out[0] = v[0];
    out[1] = v[1];
    out[2] = v[2];
}

static void o_rot(int32_t v[3], size_t axis, size_t times) {
    for (size_t i = 0; i < times; i++) {
        int32_t x = v[0];
        int32_t y = v[1];
        int32_t z = v[2];
        if (axis == 0) {
            v[0] = x;
            v[1] = -z;
            v[2] = y;
        } else if (axis == 1) {
            v[0] = z;
            v[1] = y;
            v[2] = -x;
        } else {
            v[0] = -y;
            v[1] = x;
            v[2] = z;
        }
    }
}

static size_t o_corner_pos(size_t r, size_t c) {
    if (r == 0 && c == 0) {
        return 0;
    }
    if (r == 0 && c == 2) {
        return 1;
    }
    if (r == 2 && c == 2) {
        return 2;
    }
    return 3;
}

static size_t o_edge_pos(size_t r, size_t c) {
    if (r == 0) {
        return 0;
    }
    if (c == 2) {
        return 1;
    }
    if (r == 2) {
        return 2;
    }
    return 3;
}

static size_t o_sticker_id(size_t face, size_t r, size_t c) {
    if (r == 1 && c == 1) {
        return 48 + face;
    }
    if (r % 2U == 0 && c % 2U == 0) {
        return face * 4U + o_corner_pos(r, c);
    }
    return 24 + face * 4U + o_edge_pos(r, c);
}

static size_t o_face_of_normal(const int32_t n[3]) {
    for (size_t f = 0; f < 6; f++) {
        if (O_NORMALS[f][0] == n[0] && O_NORMALS[f][1] == n[1] && O_NORMALS[f][2] == n[2]) {
            return f;
        }
    }
    BT_ABORT("no face for normal");
    abort();
}

static void o_cell_of(size_t face, const int32_t p[3], size_t *r, size_t *c) {
    for (size_t row = 0; row < 3; row++) {
        for (size_t col = 0; col < 3; col++) {
            int32_t g[3];
            o_grid(face, (int32_t)row, (int32_t)col, g);
            if (g[0] == p[0] && g[1] == p[1] && g[2] == p[2]) {
                *r = row;
                *c = col;
                return;
            }
        }
    }
    BT_ABORT("no cell on face %zu", face);
    abort();
}

typedef struct {
    int32_t pos[3];
    int32_t n[3];
} o_sticker;

typedef struct {
    size_t face;
    int32_t depth;
    bool whole;
} o_layer;

static void o_turn_layer(o_sticker stickers[54], const o_layer *layer, size_t quarter) {
    const int32_t *n = O_NORMALS[layer->face];
    size_t applied = O_SIGNS[layer->face] < 0 ? quarter : (4U - quarter) % 4U;
    for (size_t i = 0; i < 54; i++) {
        int32_t d =
            stickers[i].pos[0] * n[0] + stickers[i].pos[1] * n[1] + stickers[i].pos[2] * n[2];
        if (layer->whole || d == layer->depth) {
            o_rot(stickers[i].pos, O_AXES[layer->face], applied);
            o_rot(stickers[i].n, O_AXES[layer->face], applied);
        }
    }
}

static void o_oracle_perm(const o_layer *layers, size_t count, size_t quarter, uint8_t perm[54]) {
    o_sticker stickers[54];
    size_t index = 0;
    for (size_t face = 0; face < 6; face++) {
        for (size_t r = 0; r < 3; r++) {
            for (size_t c = 0; c < 3; c++) {
                o_grid(face, (int32_t)r, (int32_t)c, stickers[index].pos);
                stickers[index].n[0] = O_NORMALS[face][0];
                stickers[index].n[1] = O_NORMALS[face][1];
                stickers[index].n[2] = O_NORMALS[face][2];
                index += 1;
            }
        }
    }
    for (size_t i = 0; i < count; i++) {
        o_turn_layer(stickers, &layers[i], quarter);
    }
    for (size_t face = 0; face < 6; face++) {
        for (size_t r = 0; r < 3; r++) {
            for (size_t c = 0; c < 3; c++) {
                size_t id = o_sticker_id(face, r, c);
                o_sticker *st = &stickers[face * 9U + r * 3U + c];
                size_t dst = o_face_of_normal(st->n);
                size_t cr;
                size_t cc;
                o_cell_of(dst, st->pos, &cr, &cc);
                perm[id] = (uint8_t)o_sticker_id(dst, cr, cc);
            }
        }
    }
}

static void check_tables_match(const o_layer *layers, size_t count, bt_move mv, const char *label) {
    for (uint8_t q = 1; q <= 3; q++) {
        bt_move graded = mv;
        graded.quarter = q;
        uint8_t table[54];
        bt_move_table(graded, table);
        uint8_t oracle[54];
        o_oracle_perm(layers, count, q, oracle);
        if (memcmp(table, oracle, sizeof table) != 0) {
            BT_ABORT("%s quarter %u table mismatch", label, q);
        }
    }
}

BT_TEST(geometric_oracle, face_tables_match_pysim_geometry) {
    const char letters[6] = {'U', 'L', 'F', 'R', 'B', 'D'};
    for (size_t f = 0; f < 6; f++) {
        char label[2] = {letters[f], '\0'};
        o_layer layers[1] = {{f, 1, false}};
        check_tables_match(layers, 1, bt_move_of_face((uint8_t)f, 1), label);
    }
}

BT_TEST(geometric_oracle, inner_tables_match_slice_geometry) {
    const char letters[6] = {'U', 'L', 'F', 'R', 'B', 'D'};
    for (size_t f = 0; f < 6; f++) {
        char label[4] = {'2', letters[f], '\0'};
        o_layer layers[1] = {{f, 0, false}};
        check_tables_match(layers, 1, bt_move_of_inner((uint8_t)f, 1), label);
    }
}

BT_TEST(geometric_oracle, slice_tables_match_following_face_geometry) {
    const size_t follows[3][2] = {{BT_SLICE_M, 1}, {BT_SLICE_E, 5}, {BT_SLICE_S, 2}};
    const char *names[3] = {"M", "E", "S"};
    for (size_t i = 0; i < 3; i++) {
        o_layer layers[1] = {{follows[i][1], 0, false}};
        check_tables_match(layers, 1, bt_move_of_slice((uint8_t)follows[i][0], 1), names[i]);
    }
}

BT_TEST(geometric_oracle, wide_tables_match_face_plus_inner_geometry) {
    const char letters[6] = {'U', 'L', 'F', 'R', 'B', 'D'};
    for (size_t f = 0; f < 6; f++) {
        char label[2] = {(char)((unsigned char)letters[f] | 0x20U), '\0'};
        o_layer layers[2] = {{f, 1, false}, {f, 0, false}};
        check_tables_match(layers, 2, bt_move_of_wide((uint8_t)f, 1), label);
    }
}

BT_TEST(geometric_oracle, rotation_tables_match_whole_cube_geometry) {
    const size_t follows[3][2] = {{BT_AXIS_X, 3}, {BT_AXIS_Y, 0}, {BT_AXIS_Z, 2}};
    const char *names[3] = {"x", "y", "z"};
    for (size_t i = 0; i < 3; i++) {
        o_layer layers[1] = {{follows[i][1], 0, true}};
        check_tables_match(layers, 1, bt_move_of_rotation((uint8_t)follows[i][0], 1), names[i]);
    }
}
