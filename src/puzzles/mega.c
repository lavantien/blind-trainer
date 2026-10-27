#include "puzzles/mega.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"

const uint8_t BT_MEGA_NEIGHBOR_CW[BT_MEGA_FACES][BT_MEGA_RING] = {
    {BT_MEGA_F, BT_MEGA_L, BT_MEGA_BL, BT_MEGA_BR, BT_MEGA_R},
    {BT_MEGA_U, BT_MEGA_R, BT_MEGA_DR, BT_MEGA_DL, BT_MEGA_L},
    {BT_MEGA_U, BT_MEGA_BR, BT_MEGA_UR, BT_MEGA_DR, BT_MEGA_F},
    {BT_MEGA_U, BT_MEGA_BL, BT_MEGA_B, BT_MEGA_UR, BT_MEGA_R},
    {BT_MEGA_U, BT_MEGA_L, BT_MEGA_UL, BT_MEGA_B, BT_MEGA_BR},
    {BT_MEGA_U, BT_MEGA_F, BT_MEGA_DL, BT_MEGA_UL, BT_MEGA_BL},
    {BT_MEGA_B, BT_MEGA_UL, BT_MEGA_DL, BT_MEGA_DR, BT_MEGA_UR},
    {BT_MEGA_BR, BT_MEGA_BL, BT_MEGA_UL, BT_MEGA_D, BT_MEGA_UR},
    {BT_MEGA_R, BT_MEGA_BR, BT_MEGA_B, BT_MEGA_D, BT_MEGA_DR},
    {BT_MEGA_F, BT_MEGA_R, BT_MEGA_UR, BT_MEGA_D, BT_MEGA_DL},
    {BT_MEGA_F, BT_MEGA_DR, BT_MEGA_D, BT_MEGA_UL, BT_MEGA_L},
    {BT_MEGA_BL, BT_MEGA_L, BT_MEGA_DL, BT_MEGA_D, BT_MEGA_B},
};

const uint8_t BT_MEGA_Y_PERM[BT_MEGA_FACES] = {
    BT_MEGA_U, BT_MEGA_L, BT_MEGA_F,     BT_MEGA_R,  BT_MEGA_BR,
    BT_MEGA_BL, BT_MEGA_D, BT_MEGA_UR, BT_MEGA_DR, BT_MEGA_DL,
    BT_MEGA_UL, BT_MEGA_B,
};

static const struct {
    const char *text;
    bt_mega_move mv;
} PINNED[16] = {
    {"U", {0, BT_MEGA_U, 1}},
    {"U2", {0, BT_MEGA_U, 2}},
    {"U2'", {0, BT_MEGA_U, 3}},
    {"U'", {0, BT_MEGA_U, 4}},
    {"R", {0, BT_MEGA_R, 1}},
    {"R2", {0, BT_MEGA_R, 2}},
    {"R2'", {0, BT_MEGA_R, 3}},
    {"R'", {0, BT_MEGA_R, 4}},
    {"L", {0, BT_MEGA_L, 1}},
    {"L'", {0, BT_MEGA_L, 4}},
    {"F", {0, BT_MEGA_F, 1}},
    {"F'", {0, BT_MEGA_F, 4}},
    {"BR", {0, BT_MEGA_BR, 1}},
    {"BR'", {0, BT_MEGA_BR, 4}},
    {"y", {1, 0, 1}},
    {"y'", {1, 0, 4}},
};

static size_t slot(uint8_t face, uint8_t kind) {
    return (size_t)face * BT_MEGA_PER_FACE + kind;
}

uint8_t bt_mega_corner_slot(size_t k) {
    return (uint8_t)(BT_MEGA_CORNER_BASE + k);
}

uint8_t bt_mega_edge_slot(size_t k) {
    return (uint8_t)(BT_MEGA_EDGE_BASE + k);
}

uint8_t bt_mega_color(const bt_mega_state *state, uint8_t face, uint8_t slot_kind) {
    return state->colors[slot(face, slot_kind)];
}

bool bt_mega_move_eq(bt_mega_move a, bt_mega_move b) {
    if (a.spin != b.spin) {
        return false;
    }
    if (a.spin == 0 && a.face != b.face) {
        return false;
    }
    return a.steps == b.steps;
}

bool bt_mega_state_eq(const bt_mega_state *a, const bt_mega_state *b) {
    return memcmp(a->colors, b->colors, BT_MEGA_FACELETS) == 0;
}

bool bt_mega_is_pinned_move(bt_mega_move mv) {
    for (size_t i = 0; i < 16; i++) {
        if (bt_mega_move_eq(PINNED[i].mv, mv)) {
            return true;
        }
    }
    return false;
}

static size_t ring_pos(uint8_t g, uint8_t h) {
    const uint8_t *ring = BT_MEGA_NEIGHBOR_CW[g];
    for (size_t k = 0; k < BT_MEGA_RING; k++) {
        if (ring[k] == h) {
            return k;
        }
    }
    BT_TRACE_FAIL("ring_pos", "face %u not in ring of %u", h, g);
    bt_trace_dump();
    abort();
}

static size_t corner_pos(uint8_t g, uint8_t a, uint8_t b) {
    const uint8_t *ring = BT_MEGA_NEIGHBOR_CW[g];
    for (size_t k = 0; k < BT_MEGA_RING; k++) {
        uint8_t cur = ring[k];
        uint8_t next = ring[(k + 1) % BT_MEGA_RING];
        if ((cur == a && next == b) || (cur == b && next == a)) {
            return k;
        }
    }
    BT_TRACE_FAIL("corner_pos", "pair %u %u not adjacent on %u", a, b, g);
    bt_trace_dump();
    abort();
}

static void turn_cw(uint8_t colors[BT_MEGA_FACELETS], uint8_t face) {
    uint8_t src[BT_MEGA_FACELETS];
    memcpy(src, colors, sizeof src);
    const uint8_t *ring = BT_MEGA_NEIGHBOR_CW[face];
    for (size_t k = 0; k < BT_MEGA_RING; k++) {
        uint8_t a = ring[k];
        uint8_t b = ring[(k + 1) % BT_MEGA_RING];
        uint8_t c = ring[(k + 2) % BT_MEGA_RING];
        size_t next = (k + 1) % BT_MEGA_RING;
        colors[slot(face, bt_mega_corner_slot(next))] = src[slot(face, bt_mega_corner_slot(k))];
        colors[slot(face, bt_mega_edge_slot(next))] = src[slot(face, bt_mega_edge_slot(k))];
        colors[slot(b, bt_mega_corner_slot(corner_pos(b, face, c)))] =
            src[slot(a, bt_mega_corner_slot(corner_pos(a, face, b)))];
        colors[slot(c, bt_mega_corner_slot(corner_pos(c, face, b)))] =
            src[slot(b, bt_mega_corner_slot(corner_pos(b, face, a)))];
        colors[slot(b, bt_mega_edge_slot((ring_pos(b, face) + BT_MEGA_RING - 1) %
                                         BT_MEGA_RING))] =
            src[slot(a,
                     bt_mega_edge_slot((ring_pos(a, face) + BT_MEGA_RING - 1) % BT_MEGA_RING))];
    }
}

static void spin_cw(uint8_t colors[BT_MEGA_FACELETS]) {
    uint8_t src[BT_MEGA_FACELETS];
    memcpy(src, colors, sizeof src);
    for (size_t g = 0; g < BT_MEGA_FACES; g++) {
        uint8_t face = (uint8_t)g;
        uint8_t img = BT_MEGA_Y_PERM[g];
        colors[slot(img, BT_MEGA_CENTER)] = src[slot(face, BT_MEGA_CENTER)];
        for (size_t k = 0; k < BT_MEGA_RING; k++) {
            uint8_t h = BT_MEGA_NEIGHBOR_CW[g][k];
            uint8_t h2 = BT_MEGA_NEIGHBOR_CW[g][(k + 1) % BT_MEGA_RING];
            colors[slot(img, bt_mega_edge_slot(ring_pos(img, BT_MEGA_Y_PERM[h])))] =
                src[slot(face, bt_mega_edge_slot(k))];
            colors[slot(img,
                        bt_mega_corner_slot(corner_pos(img, BT_MEGA_Y_PERM[h],
                                                       BT_MEGA_Y_PERM[h2])))] =
                src[slot(face, bt_mega_corner_slot(k))];
        }
    }
}

static void describe_move(bt_mega_move mv, char out[32]) {
    if (mv.spin == 0) {
        snprintf(out, 32, "turn %u %u", mv.face, mv.steps);
    } else {
        snprintf(out, 32, "spin %u", mv.steps);
    }
}

bool bt_mega_parse_move(const char *token, size_t len, bt_mega_move *out, bt_pz_error *err) {
    BT_TRACE_ENTER("bt_mega_parse_move");
    for (size_t i = 0; i < 16; i++) {
        if (strlen(PINNED[i].text) == len && strncmp(PINNED[i].text, token, len) == 0) {
            *out = PINNED[i].mv;
            BT_TRACE_EXIT("bt_mega_parse_move");
            return true;
        }
    }
    bt_pz_error_set(err, BT_PZ_ERR_PARSE, token, len);
    BT_TRACE_FAIL("bt_mega_parse_move", "token rejected");
    BT_TRACE_EXIT("bt_mega_parse_move");
    return false;
}

bool bt_mega_parse_alg(const char *text, size_t len, void *alg, bt_pz_error *err) {
    BT_TRACE_ENTER("bt_mega_parse_alg");
    bt_pz_engine eng = bt_mega_engine();
    bool ok = bt_pz_alg_default_parse(&eng, text, len, alg, err);
    if (!ok) {
        BT_TRACE_FAIL("bt_mega_parse_alg", "alg rejected");
    }
    BT_TRACE_EXIT("bt_mega_parse_alg");
    return ok;
}

void bt_mega_format_alg(const void *alg, bt_strbuf *out) {
    BT_TRACE_ENTER("bt_mega_format_alg");
    bt_pz_engine eng = bt_mega_engine();
    size_t len = bt_pz_alg_len(alg);
    for (size_t i = 0; i < len; i++) {
        if (i > 0) {
            bt_strbuf_append_str(out, " ");
        }
        bt_mega_move mv = *(const bt_mega_move *)bt_pz_alg_at(&eng, alg, i);
        const char *text = NULL;
        for (size_t p = 0; p < 16; p++) {
            if (bt_mega_move_eq(PINNED[p].mv, mv)) {
                text = PINNED[p].text;
                break;
            }
        }
        if (text == NULL) {
            BT_TRACE_FAIL("bt_mega_format_alg", "move outside the pinned table");
            bt_trace_dump();
            abort();
        }
        bt_strbuf_append_str(out, text);
    }
    BT_TRACE_EXIT("bt_mega_format_alg");
}

void bt_mega_invert_alg(const void *alg, void *out) {
    BT_TRACE_ENTER("bt_mega_invert_alg");
    bt_pz_engine eng = bt_mega_engine();
    bt_pz_alg *src = (bt_pz_alg *)alg;
    bt_pz_alg *dst = out;
    dst->len = 0;
    for (size_t i = src->len; i > 0; i--) {
        bt_mega_move mv = *(const bt_mega_move *)bt_pz_alg_at(&eng, src, i - 1);
        mv.steps = (uint8_t)(BT_MEGA_RING - mv.steps);
        bt_pz_alg_push(&eng, dst, &mv);
    }
    BT_TRACE_EXIT("bt_mega_invert_alg");
}

bool bt_mega_apply_alg(bt_mega_state *state, const void *alg, bt_pz_error *err) {
    BT_TRACE_ENTER("bt_mega_apply_alg");
    bt_pz_engine eng = bt_mega_engine();
    size_t len = bt_pz_alg_len(alg);
    for (size_t i = 0; i < len; i++) {
        bt_mega_move mv = *(const bt_mega_move *)bt_pz_alg_at(&eng, alg, i);
        if (!bt_mega_is_pinned_move(mv)) {
            char text[32];
            describe_move(mv, text);
            bt_pz_error_set(err, BT_PZ_ERR_APPLY, text, strlen(text));
            BT_TRACE_FAIL("bt_mega_apply_alg", "move rejected: %s", text);
            BT_TRACE_EXIT("bt_mega_apply_alg");
            return false;
        }
        if (mv.spin == 0) {
            for (uint8_t step = 0; step < mv.steps; step++) {
                turn_cw(state->colors, mv.face);
            }
        } else {
            for (uint8_t step = 0; step < mv.steps; step++) {
                spin_cw(state->colors);
            }
        }
    }
    BT_TRACE_EXIT("bt_mega_apply_alg");
    return true;
}

void bt_mega_solved_state(bt_mega_state *out) {
    BT_TRACE_ENTER("bt_mega_solved_state");
    for (size_t i = 0; i < BT_MEGA_FACELETS; i++) {
        out->colors[i] = (uint8_t)(i / BT_MEGA_PER_FACE);
    }
    BT_TRACE_EXIT("bt_mega_solved_state");
}

bool bt_mega_is_solved(const bt_mega_state *state) {
    BT_TRACE_ENTER("bt_mega_is_solved");
    bool solved = true;
    for (size_t i = 0; i < BT_MEGA_FACELETS; i++) {
        solved = solved && state->colors[i] == (uint8_t)(i / BT_MEGA_PER_FACE);
    }
    BT_TRACE_EXIT("bt_mega_is_solved");
    return solved;
}

void bt_mega_facelets(const bt_mega_state *state, bt_pz_grid *out) {
    BT_TRACE_ENTER("bt_mega_facelets");
    out->face_count = BT_MEGA_FACES;
    for (size_t face = 0; face < BT_MEGA_FACES; face++) {
        uint8_t f = (uint8_t)face;
        bt_pz_face *view = &out->faces[face];
        view->row_count = 5;
        view->rows[0].cols = 1;
        view->rows[0].cells[0] = state->colors[slot(f, bt_mega_corner_slot(0))];
        view->rows[1].cols = 2;
        view->rows[1].cells[0] = state->colors[slot(f, bt_mega_edge_slot(BT_MEGA_RING - 1))];
        view->rows[1].cells[1] = state->colors[slot(f, bt_mega_edge_slot(0))];
        view->rows[2].cols = 3;
        view->rows[2].cells[0] = state->colors[slot(f, bt_mega_corner_slot(BT_MEGA_RING - 1))];
        view->rows[2].cells[1] = state->colors[slot(f, BT_MEGA_CENTER)];
        view->rows[2].cells[2] = state->colors[slot(f, bt_mega_corner_slot(1))];
        view->rows[3].cols = 3;
        view->rows[3].cells[0] = state->colors[slot(f, bt_mega_edge_slot(BT_MEGA_RING - 2))];
        view->rows[3].cells[1] = state->colors[slot(f, bt_mega_edge_slot(2))];
        view->rows[3].cells[2] = state->colors[slot(f, bt_mega_edge_slot(1))];
        view->rows[4].cols = 2;
        view->rows[4].cells[0] = state->colors[slot(f, bt_mega_corner_slot(BT_MEGA_RING - 2))];
        view->rows[4].cells[1] = state->colors[slot(f, bt_mega_corner_slot(2))];
    }
    BT_TRACE_EXIT("bt_mega_facelets");
}

static bool mega_vt_parse_move(const bt_pz_engine *eng, const char *token, size_t len,
                               void *out_move, bt_pz_error *err) {
    (void)eng;
    return bt_mega_parse_move(token, len, out_move, err);
}

static bool mega_vt_parse_alg(const bt_pz_engine *eng, const char *text, size_t len, void *alg,
                              bt_pz_error *err) {
    (void)eng;
    return bt_mega_parse_alg(text, len, alg, err);
}

static void mega_vt_format_alg(const bt_pz_engine *eng, const void *alg, bt_strbuf *out) {
    (void)eng;
    bt_mega_format_alg(alg, out);
}

static void mega_vt_invert_alg(const bt_pz_engine *eng, const void *alg, void *out) {
    (void)eng;
    bt_mega_invert_alg(alg, out);
}

static bool mega_vt_apply_alg(const bt_pz_engine *eng, void *state, const void *alg,
                              bt_pz_error *err) {
    (void)eng;
    return bt_mega_apply_alg(state, alg, err);
}

static void mega_vt_solved_state(const bt_pz_engine *eng, void *out_state) {
    (void)eng;
    bt_mega_solved_state(out_state);
}

static bool mega_vt_is_solved(const bt_pz_engine *eng, const void *state) {
    (void)eng;
    return bt_mega_is_solved(state);
}

static void mega_vt_facelets(const bt_pz_engine *eng, const void *state, bt_pz_grid *out) {
    (void)eng;
    bt_mega_facelets(state, out);
}

bt_pz_engine bt_mega_engine(void) {
    BT_TRACE_ENTER("bt_mega_engine");
    bt_pz_engine eng = {
        .name = "mega",
        .self = NULL,
        .move_size = sizeof(bt_mega_move),
        .state_size = sizeof(bt_mega_state),
        .parse_move = mega_vt_parse_move,
        .parse_alg = mega_vt_parse_alg,
        .format_alg = mega_vt_format_alg,
        .invert_alg = mega_vt_invert_alg,
        .apply_alg = mega_vt_apply_alg,
        .solved_state = mega_vt_solved_state,
        .is_solved = mega_vt_is_solved,
        .facelets = mega_vt_facelets,
    };
    BT_TRACE_EXIT("bt_mega_engine");
    return eng;
}
