#include "puzzles/skewb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"

const char *const BT_SKEWB_FACE_NAMES[BT_SKEWB_FACE_COUNT] = {"U", "F", "R", "L", "B", "D"};
const char *const BT_SKEWB_VERTEX_NAMES[BT_SKEWB_VERTEX_COUNT] = {"F", "L", "R", "B"};

enum { BT_SKEWB_TEXT_MAX = 32 };

static const int8_t VERTEX_SIGNS[4][3] = {
    {1, 1, 1},
    {-1, 1, 1},
    {1, 1, -1},
    {1, -1, 1},
};

static const int8_t FACE_NORMALS[6][3] = {
    {0, 1, 0}, {0, 0, 1}, {1, 0, 0}, {-1, 0, 0}, {0, 0, -1}, {0, -1, 0},
};

static const int8_t FACE_UPS[6][3] = {
    {0, 0, -1}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 0, 1},
};

static const int8_t FACE_RIGHTS[6][3] = {
    {1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0},
};

static void rotate_point(const int8_t signs[3], uint8_t quarter, const int8_t point[3],
                         int8_t out[3]) {
    int8_t framed[3] = {
        (int8_t)(signs[0] * point[0]),
        (int8_t)(signs[1] * point[1]),
        (int8_t)(signs[2] * point[2]),
    };
    int8_t cycled[3];
    if (quarter == 1) {
        cycled[0] = framed[1];
        cycled[1] = framed[2];
        cycled[2] = framed[0];
    } else {
        cycled[0] = framed[2];
        cycled[1] = framed[0];
        cycled[2] = framed[1];
    }
    out[0] = (int8_t)(signs[0] * cycled[0]);
    out[1] = (int8_t)(signs[1] * cycled[1]);
    out[2] = (int8_t)(signs[2] * cycled[2]);
}

static int32_t dot3(const int8_t a[3], const int8_t b[3]) {
    return (int32_t)a[0] * b[0] + (int32_t)a[1] * b[1] + (int32_t)a[2] * b[2];
}

static void cross3(const int8_t a[3], const int8_t b[3], int8_t out[3]) {
    out[0] = (int8_t)(a[1] * b[2] - a[2] * b[1]);
    out[1] = (int8_t)(a[2] * b[0] - a[0] * b[2]);
    out[2] = (int8_t)(a[0] * b[1] - a[1] * b[0]);
}

static void anchors(int8_t out[BT_SKEWB_FACELET_COUNT][3]) {
    for (size_t face = 0; face < 6; face++) {
        const int8_t *normal = FACE_NORMALS[face];
        const int8_t *up = FACE_UPS[face];
        const int8_t *right = FACE_RIGHTS[face];
        for (size_t row = 0; row < 3; row++) {
            for (size_t col = 0; col < 3; col++) {
                int8_t rise = (int8_t)((1 - (int8_t)row) * 2);
                int8_t run = (int8_t)(((int8_t)col - 1) * 2);
                size_t slot = face * 9 + row * 3 + col;
                out[slot][0] = (int8_t)(normal[0] * 3 + up[0] * rise + right[0] * run);
                out[slot][1] = (int8_t)(normal[1] * 3 + up[1] * rise + right[1] * run);
                out[slot][2] = (int8_t)(normal[2] * 3 + up[2] * rise + right[2] * run);
            }
        }
    }
}

static size_t slot_of(const int8_t table[BT_SKEWB_FACELET_COUNT][3], const int8_t point[3]) {
    for (size_t slot = 0; slot < BT_SKEWB_FACELET_COUNT; slot++) {
        if (table[slot][0] == point[0] && table[slot][1] == point[1] &&
            table[slot][2] == point[2]) {
            return slot;
        }
    }
    BT_TRACE_FAIL("slot_of", "anchor point has no slot");
    bt_trace_dump();
    abort();
}

static bool starts_with(const char *token, size_t len, const char *prefix) {
    size_t plen = strlen(prefix);
    bool match = (len >= plen && len <= plen + 1 && strncmp(token, prefix, plen) == 0) != 0;
    return match;
}

bt_skewb bt_skewb_new(void) {
    BT_TRACE_ENTER("bt_skewb_new");
    bt_skewb skewb;
    int8_t table[BT_SKEWB_FACELET_COUNT][3];
    anchors(table);
    for (size_t vertex = 0; vertex < 4; vertex++) {
        const int8_t *signs = VERTEX_SIGNS[vertex];
        int8_t probe[3] = {0, 1, 0};
        int8_t rotated[3];
        rotate_point(signs, 1, probe, rotated);
        int8_t product[3];
        cross3(probe, rotated, product);
        uint8_t clockwise = dot3(product, signs) < 0 ? 1 : 2;
        uint8_t turn[BT_SKEWB_FACELET_COUNT];
        for (size_t i = 0; i < BT_SKEWB_FACELET_COUNT; i++) {
            turn[i] = (uint8_t)i;
        }
        for (size_t slot = 0; slot < BT_SKEWB_FACELET_COUNT; slot++) {
            if (dot3(table[slot], signs) > 0) {
                rotate_point(signs, clockwise, table[slot], rotated);
                turn[slot] = (uint8_t)slot_of(table, rotated);
            }
        }
        memcpy(skewb.turns[vertex * 2], turn, BT_SKEWB_FACELET_COUNT);
        for (size_t i = 0; i < BT_SKEWB_FACELET_COUNT; i++) {
            skewb.turns[vertex * 2 + 1][i] = turn[turn[i]];
        }
    }
    BT_TRACE_EXIT("bt_skewb_new");
    return skewb;
}

void bt_skewb_turn_table(const bt_skewb *skewb, uint8_t vertex, uint8_t quarter,
                         uint8_t out[BT_SKEWB_FACELET_COUNT]) {
    BT_TRACE_ENTER("bt_skewb_turn_table");
    memcpy(out, skewb->turns[(size_t)vertex * 2 + quarter - 1], BT_SKEWB_FACELET_COUNT);
    BT_TRACE_EXIT("bt_skewb_turn_table");
}

bool bt_skewb_move_eq(bt_skewb_move a, bt_skewb_move b) {
    bool eq = (a.vertex == b.vertex && a.quarter == b.quarter) != 0;
    return eq;
}

bool bt_skewb_state_eq(const bt_skewb_state *a, const bt_skewb_state *b) {
    bool eq = (memcmp(a->facelets, b->facelets, BT_SKEWB_FACELET_COUNT) == 0) != 0;
    return eq;
}

bool bt_skewb_parse_move(const char *token, size_t len, bt_skewb_move *out, bt_pz_error *err) {
    BT_TRACE_ENTER("bt_skewb_parse_move");
    for (size_t vertex = 0; vertex < BT_SKEWB_VERTEX_COUNT; vertex++) {
        const char *name = BT_SKEWB_VERTEX_NAMES[vertex];
        if (!starts_with(token, len, name)) {
            continue;
        }
        size_t plen = strlen(name);
        uint8_t quarter = 1;
        if (len == plen + 1) {
            if (token[plen] == '\'') {
                quarter = 2;
            } else {
                continue;
            }
        }
        out->vertex = (uint8_t)vertex;
        out->quarter = quarter;
        BT_TRACE_EXIT("bt_skewb_parse_move");
        return true;
    }
    bt_pz_error_set(err, BT_PZ_ERR_PARSE, token, len);
    BT_TRACE_FAIL("bt_skewb_parse_move", "token rejected");
    BT_TRACE_EXIT("bt_skewb_parse_move");
    return false;
}

bool bt_skewb_parse_alg(const bt_skewb *skewb, const char *text, size_t len, void *alg,
                        bt_pz_error *err) {
    BT_TRACE_ENTER("bt_skewb_parse_alg");
    bt_pz_engine eng = bt_skewb_engine(skewb);
    bool ok = bt_pz_alg_default_parse(&eng, text, len, alg, err);
    if (!ok) {
        BT_TRACE_FAIL("bt_skewb_parse_alg", "alg rejected");
    }
    BT_TRACE_EXIT("bt_skewb_parse_alg");
    return ok;
}

static void format_move(bt_skewb_move mv, char out[4]) {
    const char *name = BT_SKEWB_VERTEX_NAMES[mv.vertex];
    size_t at = 0;
    while (name[at] != '\0') {
        out[at] = name[at];
        at++;
    }
    if (mv.quarter == 2) {
        out[at] = '\'';
        at++;
    }
    out[at] = '\0';
}

void bt_skewb_format_alg(const bt_skewb *skewb, const void *alg, bt_strbuf *out) {
    BT_TRACE_ENTER("bt_skewb_format_alg");
    (void)skewb;
    bt_pz_engine eng = bt_skewb_engine(skewb);
    size_t len = bt_pz_alg_len(alg);
    for (size_t i = 0; i < len; i++) {
        if (i > 0) {
            bt_strbuf_append_str(out, " ");
        }
        bt_skewb_move mv = *(const bt_skewb_move *)bt_pz_alg_at(&eng, alg, i);
        char text[4];
        format_move(mv, text);
        bt_strbuf_append_str(out, text);
    }
    BT_TRACE_EXIT("bt_skewb_format_alg");
}

void bt_skewb_invert_alg(const bt_skewb *skewb, const void *alg, void *out) {
    BT_TRACE_ENTER("bt_skewb_invert_alg");
    bt_pz_engine eng = bt_skewb_engine(skewb);
    bt_pz_alg *src = (bt_pz_alg *)alg;
    bt_pz_alg *dst = out;
    dst->len = 0;
    for (size_t i = src->len; i > 0; i--) {
        bt_skewb_move mv = *(const bt_skewb_move *)bt_pz_alg_at(&eng, src, i - 1);
        mv.quarter = (uint8_t)(3 - mv.quarter);
        bt_pz_alg_push(&eng, dst, &mv);
    }
    BT_TRACE_EXIT("bt_skewb_invert_alg");
}

static const uint8_t *table_for(const bt_skewb *skewb, bt_skewb_move mv, bt_pz_error *err) {
    if (mv.vertex >= 4 || mv.quarter == 0 || mv.quarter > 2) {
        char text[BT_SKEWB_TEXT_MAX];
        snprintf(text, sizeof text, "vertex %u quarter %u", mv.vertex, mv.quarter);
        bt_pz_error_set(err, BT_PZ_ERR_APPLY, text, strlen(text));
        return NULL;
    }
    return skewb->turns[(size_t)mv.vertex * 2 + mv.quarter - 1];
}

bool bt_skewb_apply_alg(const bt_skewb *skewb, bt_skewb_state *state, const void *alg,
                        bt_pz_error *err) {
    BT_TRACE_ENTER("bt_skewb_apply_alg");
    bt_pz_engine eng = bt_skewb_engine(skewb);
    size_t len = bt_pz_alg_len(alg);
    for (size_t i = 0; i < len; i++) {
        bt_skewb_move mv = *(const bt_skewb_move *)bt_pz_alg_at(&eng, alg, i);
        const uint8_t *table = table_for(skewb, mv, err);
        if (table == NULL) {
            BT_TRACE_FAIL("bt_skewb_apply_alg", "move rejected");
            BT_TRACE_EXIT("bt_skewb_apply_alg");
            return false;
        }
        uint8_t before[BT_SKEWB_FACELET_COUNT];
        memcpy(before, state->facelets, BT_SKEWB_FACELET_COUNT);
        for (size_t slot = 0; slot < BT_SKEWB_FACELET_COUNT; slot++) {
            state->facelets[table[slot]] = before[slot];
        }
    }
    BT_TRACE_EXIT("bt_skewb_apply_alg");
    return true;
}

void bt_skewb_solved_state(const bt_skewb *skewb, bt_skewb_state *out) {
    BT_TRACE_ENTER("bt_skewb_solved_state");
    (void)skewb;
    for (size_t i = 0; i < BT_SKEWB_FACELET_COUNT; i++) {
        out->facelets[i] = (uint8_t)(i / 9);
    }
    BT_TRACE_EXIT("bt_skewb_solved_state");
}

bool bt_skewb_is_solved(const bt_skewb *skewb, const bt_skewb_state *state) {
    BT_TRACE_ENTER("bt_skewb_is_solved");
    (void)skewb;
    for (size_t i = 0; i < BT_SKEWB_FACELET_COUNT; i++) {
        if (state->facelets[i] != (uint8_t)(i / 9)) {
            BT_TRACE_EXIT("bt_skewb_is_solved");
            return false;
        }
    }
    BT_TRACE_EXIT("bt_skewb_is_solved");
    return true;
}

void bt_skewb_facelets(const bt_skewb *skewb, const bt_skewb_state *state, bt_pz_grid *out) {
    BT_TRACE_ENTER("bt_skewb_facelets");
    (void)skewb;
    out->face_count = 6;
    for (size_t face = 0; face < 6; face++) {
        bt_pz_face *view = &out->faces[face];
        view->row_count = 3;
        for (size_t row = 0; row < 3; row++) {
            view->rows[row].cols = 3;
            for (size_t col = 0; col < 3; col++) {
                view->rows[row].cells[col] = state->facelets[face * 9 + row * 3 + col];
            }
        }
    }
    BT_TRACE_EXIT("bt_skewb_facelets");
}

static bool skewb_vt_parse_move(const bt_pz_engine *eng, const char *token, size_t len,
                                void *out_move, bt_pz_error *err) {
    (void)eng;
    return bt_skewb_parse_move(token, len, out_move, err);
}

static bool skewb_vt_parse_alg(const bt_pz_engine *eng, const char *text, size_t len, void *alg,
                               bt_pz_error *err) {
    return bt_skewb_parse_alg(eng->self, text, len, alg, err);
}

static void skewb_vt_format_alg(const bt_pz_engine *eng, const void *alg, bt_strbuf *out) {
    bt_skewb_format_alg(eng->self, alg, out);
}

static void skewb_vt_invert_alg(const bt_pz_engine *eng, const void *alg, void *out) {
    bt_skewb_invert_alg(eng->self, alg, out);
}

static bool skewb_vt_apply_alg(const bt_pz_engine *eng, void *state, const void *alg,
                               bt_pz_error *err) {
    return bt_skewb_apply_alg(eng->self, state, alg, err);
}

static void skewb_vt_solved_state(const bt_pz_engine *eng, void *out_state) {
    bt_skewb_solved_state(eng->self, out_state);
}

static bool skewb_vt_is_solved(const bt_pz_engine *eng, const void *state) {
    return bt_skewb_is_solved(eng->self, state);
}

static void skewb_vt_facelets(const bt_pz_engine *eng, const void *state, bt_pz_grid *out) {
    bt_skewb_facelets(eng->self, state, out);
}

bt_pz_engine bt_skewb_engine(const bt_skewb *skewb) {
    BT_TRACE_ENTER("bt_skewb_engine");
    bt_pz_engine eng = {
        .name = "skewb",
        .self = skewb,
        .move_size = sizeof(bt_skewb_move),
        .state_size = sizeof(bt_skewb_state),
        .parse_move = skewb_vt_parse_move,
        .parse_alg = skewb_vt_parse_alg,
        .format_alg = skewb_vt_format_alg,
        .invert_alg = skewb_vt_invert_alg,
        .apply_alg = skewb_vt_apply_alg,
        .solved_state = skewb_vt_solved_state,
        .is_solved = skewb_vt_is_solved,
        .facelets = skewb_vt_facelets,
    };
    BT_TRACE_EXIT("bt_skewb_engine");
    return eng;
}
