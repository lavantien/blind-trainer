#include "puzzles/pyra.h"

#include <string.h>

#include "common/bt_trace.h"

static const size_t CYCLES[4][4][3] = {
    {{0, 9, 18}, {1, 10, 19}, {2, 11, 20}, {3, 12, 21}},
    {{4, 35, 17}, {5, 34, 16}, {1, 33, 15}, {6, 30, 12}},
    {{8, 31, 22}, {7, 32, 23}, {3, 33, 24}, {6, 28, 19}},
    {{27, 26, 13}, {29, 25, 14}, {15, 28, 21}, {10, 30, 24}},
};

static void spin_cycle(uint8_t facelets[BT_PYRA_FACELET_COUNT], const size_t *cycle,
                       uint8_t sense) {
    uint8_t a = cycle[0];
    uint8_t b = cycle[1];
    uint8_t c = cycle[2];
    uint8_t carried = facelets[a];
    if (sense == BT_PYRA_SENSE_CLOCK) {
        facelets[a] = facelets[c];
        facelets[c] = facelets[b];
        facelets[b] = carried;
    } else {
        facelets[a] = facelets[b];
        facelets[b] = facelets[c];
        facelets[c] = carried;
    }
}

static void token_text(bt_pyra_move mv, char out[3]) {
    char base = 'U';
    if (mv.vertex == BT_PYRA_VERTEX_U) {
        base = mv.layers == BT_PYRA_LAYERS_TWIN ? 'U' : 'u';
    } else if (mv.vertex == BT_PYRA_VERTEX_L) {
        base = mv.layers == BT_PYRA_LAYERS_TWIN ? 'L' : 'l';
    } else if (mv.vertex == BT_PYRA_VERTEX_R) {
        base = mv.layers == BT_PYRA_LAYERS_TWIN ? 'R' : 'r';
    } else {
        base = mv.layers == BT_PYRA_LAYERS_TWIN ? 'B' : 'b';
    }
    out[0] = base;
    if (mv.sense == BT_PYRA_SENSE_COUNTER) {
        out[1] = '\'';
        out[2] = '\0';
    } else {
        out[1] = '\0';
    }
}

bool bt_pyra_move_eq(bt_pyra_move a, bt_pyra_move b) {
    bool eq = (a.vertex == b.vertex && a.layers == b.layers && a.sense == b.sense) != 0;
    return eq;
}

bool bt_pyra_state_eq(const bt_pyra_state *a, const bt_pyra_state *b) {
    bool eq = (memcmp(a->facelets, b->facelets, BT_PYRA_FACELET_COUNT) == 0) != 0;
    return eq;
}

bool bt_pyra_parse_move(const char *token, size_t len, bt_pyra_move *out, bt_pz_error *err) {
    BT_TRACE_ENTER("bt_pyra_parse_move");
    uint8_t vertex = 0;
    uint8_t layers = 0;
    bool matched = false;
    if (len >= 1) {
        switch (token[0]) {
        case 'U':
            vertex = BT_PYRA_VERTEX_U;
            layers = BT_PYRA_LAYERS_TWIN;
            matched = true;
            break;
        case 'u':
            vertex = BT_PYRA_VERTEX_U;
            layers = BT_PYRA_LAYERS_TIP;
            matched = true;
            break;
        case 'L':
            vertex = BT_PYRA_VERTEX_L;
            layers = BT_PYRA_LAYERS_TWIN;
            matched = true;
            break;
        case 'l':
            vertex = BT_PYRA_VERTEX_L;
            layers = BT_PYRA_LAYERS_TIP;
            matched = true;
            break;
        case 'R':
            vertex = BT_PYRA_VERTEX_R;
            layers = BT_PYRA_LAYERS_TWIN;
            matched = true;
            break;
        case 'r':
            vertex = BT_PYRA_VERTEX_R;
            layers = BT_PYRA_LAYERS_TIP;
            matched = true;
            break;
        case 'B':
            vertex = BT_PYRA_VERTEX_B;
            layers = BT_PYRA_LAYERS_TWIN;
            matched = true;
            break;
        case 'b':
            vertex = BT_PYRA_VERTEX_B;
            layers = BT_PYRA_LAYERS_TIP;
            matched = true;
            break;
        default:
            break;
        }
    }
    if (!matched) {
        bt_pz_error_set(err, BT_PZ_ERR_PARSE, token, len);
        BT_TRACE_FAIL("bt_pyra_parse_move", "token rejected");
        BT_TRACE_EXIT("bt_pyra_parse_move");
        return false;
    }
    uint8_t sense = BT_PYRA_SENSE_CLOCK;
    if (len == 2 && token[1] == '\'') {
        sense = BT_PYRA_SENSE_COUNTER;
    } else if (len != 1) {
        bt_pz_error_set(err, BT_PZ_ERR_PARSE, token, len);
        BT_TRACE_FAIL("bt_pyra_parse_move", "token rejected");
        BT_TRACE_EXIT("bt_pyra_parse_move");
        return false;
    }
    out->vertex = vertex;
    out->layers = layers;
    out->sense = sense;
    BT_TRACE_EXIT("bt_pyra_parse_move");
    return true;
}

bool bt_pyra_parse_alg(const char *text, size_t len, void *alg, bt_pz_error *err) {
    BT_TRACE_ENTER("bt_pyra_parse_alg");
    bt_pz_engine eng = bt_pyra_engine();
    bool ok = bt_pz_alg_default_parse(&eng, text, len, alg, err);
    if (!ok) {
        BT_TRACE_FAIL("bt_pyra_parse_alg", "alg rejected");
    }
    BT_TRACE_EXIT("bt_pyra_parse_alg");
    return ok;
}

void bt_pyra_format_alg(const void *alg, bt_strbuf *out) {
    BT_TRACE_ENTER("bt_pyra_format_alg");
    bt_pz_engine eng = bt_pyra_engine();
    size_t len = bt_pz_alg_len(alg);
    for (size_t i = 0; i < len; i++) {
        if (i > 0) {
            bt_strbuf_append_str(out, " ");
        }
        bt_pyra_move mv = *(const bt_pyra_move *)bt_pz_alg_at(&eng, alg, i);
        char text[3];
        token_text(mv, text);
        bt_strbuf_append_str(out, text);
    }
    BT_TRACE_EXIT("bt_pyra_format_alg");
}

void bt_pyra_invert_alg(const void *alg, void *out) {
    BT_TRACE_ENTER("bt_pyra_invert_alg");
    bt_pz_engine eng = bt_pyra_engine();
    bt_pz_alg *src = (bt_pz_alg *)alg;
    bt_pz_alg *dst = out;
    dst->len = 0;
    for (size_t i = src->len; i > 0; i--) {
        bt_pyra_move mv = *(const bt_pyra_move *)bt_pz_alg_at(&eng, src, i - 1);
        mv.sense = mv.sense == BT_PYRA_SENSE_CLOCK ? BT_PYRA_SENSE_COUNTER : BT_PYRA_SENSE_CLOCK;
        bt_pz_alg_push(&eng, dst, &mv);
    }
    BT_TRACE_EXIT("bt_pyra_invert_alg");
}

bool bt_pyra_apply_alg(bt_pyra_state *state, const void *alg, bt_pz_error *err) {
    BT_TRACE_ENTER("bt_pyra_apply_alg");
    (void)err;
    bt_pz_engine eng = bt_pyra_engine();
    size_t len = bt_pz_alg_len(alg);
    for (size_t i = 0; i < len; i++) {
        bt_pyra_move mv = *(const bt_pyra_move *)bt_pz_alg_at(&eng, alg, i);
        size_t depth = mv.layers == BT_PYRA_LAYERS_TWIN ? 4 : 1;
        for (size_t cycle = 0; cycle < depth; cycle++) {
            spin_cycle(state->facelets, CYCLES[mv.vertex][cycle], mv.sense);
        }
    }
    BT_TRACE_EXIT("bt_pyra_apply_alg");
    return true;
}

void bt_pyra_solved_state(bt_pyra_state *out) {
    BT_TRACE_ENTER("bt_pyra_solved_state");
    for (size_t i = 0; i < BT_PYRA_FACELET_COUNT; i++) {
        out->facelets[i] = (uint8_t)(i / 9);
    }
    BT_TRACE_EXIT("bt_pyra_solved_state");
}

bool bt_pyra_is_solved(const bt_pyra_state *state) {
    BT_TRACE_ENTER("bt_pyra_is_solved");
    for (size_t i = 0; i < BT_PYRA_FACELET_COUNT; i++) {
        if (state->facelets[i] != (uint8_t)(i / 9)) {
            BT_TRACE_EXIT("bt_pyra_is_solved");
            return false;
        }
    }
    BT_TRACE_EXIT("bt_pyra_is_solved");
    return true;
}

void bt_pyra_facelets(const bt_pyra_state *state, bt_pz_grid *out) {
    BT_TRACE_ENTER("bt_pyra_facelets");
    out->face_count = 4;
    for (size_t face = 0; face < 4; face++) {
        size_t base = face * 9;
        bt_pz_face *view = &out->faces[face];
        view->row_count = 3;
        view->rows[0].cols = 1;
        view->rows[0].cells[0] = state->facelets[base];
        view->rows[1].cols = 3;
        for (size_t i = 0; i < 3; i++) {
            view->rows[1].cells[i] = state->facelets[base + 1 + i];
        }
        view->rows[2].cols = 5;
        for (size_t i = 0; i < 5; i++) {
            view->rows[2].cells[i] = state->facelets[base + 4 + i];
        }
    }
    BT_TRACE_EXIT("bt_pyra_facelets");
}

static bool pyra_vt_parse_move(const bt_pz_engine *eng, const char *token, size_t len,
                               void *out_move, bt_pz_error *err) {
    (void)eng;
    return bt_pyra_parse_move(token, len, out_move, err);
}

static bool pyra_vt_parse_alg(const bt_pz_engine *eng, const char *text, size_t len, void *alg,
                              bt_pz_error *err) {
    (void)eng;
    return bt_pyra_parse_alg(text, len, alg, err);
}

static void pyra_vt_format_alg(const bt_pz_engine *eng, const void *alg, bt_strbuf *out) {
    (void)eng;
    bt_pyra_format_alg(alg, out);
}

static void pyra_vt_invert_alg(const bt_pz_engine *eng, const void *alg, void *out) {
    (void)eng;
    bt_pyra_invert_alg(alg, out);
}

static bool pyra_vt_apply_alg(const bt_pz_engine *eng, void *state, const void *alg,
                              bt_pz_error *err) {
    (void)eng;
    return bt_pyra_apply_alg(state, alg, err);
}

static void pyra_vt_solved_state(const bt_pz_engine *eng, void *out_state) {
    (void)eng;
    bt_pyra_solved_state(out_state);
}

static bool pyra_vt_is_solved(const bt_pz_engine *eng, const void *state) {
    (void)eng;
    return bt_pyra_is_solved(state);
}

static void pyra_vt_facelets(const bt_pz_engine *eng, const void *state, bt_pz_grid *out) {
    (void)eng;
    bt_pyra_facelets(state, out);
}

bt_pz_engine bt_pyra_engine(void) {
    BT_TRACE_ENTER("bt_pyra_engine");
    bt_pz_engine eng = {
        .name = "pyra",
        .self = NULL,
        .move_size = sizeof(bt_pyra_move),
        .state_size = sizeof(bt_pyra_state),
        .parse_move = pyra_vt_parse_move,
        .parse_alg = pyra_vt_parse_alg,
        .format_alg = pyra_vt_format_alg,
        .invert_alg = pyra_vt_invert_alg,
        .apply_alg = pyra_vt_apply_alg,
        .solved_state = pyra_vt_solved_state,
        .is_solved = pyra_vt_is_solved,
        .facelets = pyra_vt_facelets,
    };
    BT_TRACE_EXIT("bt_pyra_engine");
    return eng;
}
