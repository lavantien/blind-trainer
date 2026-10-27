#include "puzzles/pyra.h"

#include <string.h>

#include "common/bt_trace.h"

bool bt_pyra_move_eq(bt_pyra_move a, bt_pyra_move b) {
    return a.vertex == b.vertex && a.layers == b.layers && a.sense == b.sense;
}

bool bt_pyra_state_eq(const bt_pyra_state *a, const bt_pyra_state *b) {
    return memcmp(a->facelets, b->facelets, BT_PYRA_FACELET_COUNT) == 0;
}

bool bt_pyra_parse_move(const char *token, size_t len, bt_pyra_move *out, bt_pz_error *err) {
    BT_TRACE_ENTER("bt_pyra_parse_move");
    (void)token;
    (void)len;
    out->vertex = 0;
    out->layers = 0;
    out->sense = 0;
    BT_TRACE_EXIT("bt_pyra_parse_move");
    (void)err;
    return true;
}

bool bt_pyra_parse_alg(const char *text, size_t len, void *alg, bt_pz_error *err) {
    BT_TRACE_ENTER("bt_pyra_parse_alg");
    bt_pz_engine eng = bt_pyra_engine();
    bool ok = bt_pz_alg_default_parse(&eng, text, len, alg, err);
    BT_TRACE_EXIT("bt_pyra_parse_alg");
    return ok;
}

void bt_pyra_format_alg(const void *alg, bt_strbuf *out) {
    BT_TRACE_ENTER("bt_pyra_format_alg");
    (void)alg;
    bt_strbuf_append_str(out, "stub");
    BT_TRACE_EXIT("bt_pyra_format_alg");
}

void bt_pyra_invert_alg(const void *alg, void *out) {
    BT_TRACE_ENTER("bt_pyra_invert_alg");
    bt_pz_engine eng = bt_pyra_engine();
    bt_pz_alg *src = (bt_pz_alg *)alg;
    bt_pz_alg *dst = out;
    dst->len = 0;
    for (size_t i = src->len; i > 0; i--) {
        bt_pz_alg_push(&eng, dst, bt_pz_alg_at(&eng, src, i - 1));
    }
    BT_TRACE_EXIT("bt_pyra_invert_alg");
}

bool bt_pyra_apply_alg(bt_pyra_state *state, const void *alg, bt_pz_error *err) {
    BT_TRACE_ENTER("bt_pyra_apply_alg");
    (void)state;
    (void)alg;
    BT_TRACE_EXIT("bt_pyra_apply_alg");
    (void)err;
    return true;
}

void bt_pyra_solved_state(bt_pyra_state *out) {
    BT_TRACE_ENTER("bt_pyra_solved_state");
    memset(out->facelets, 0, BT_PYRA_FACELET_COUNT);
    BT_TRACE_EXIT("bt_pyra_solved_state");
}

bool bt_pyra_is_solved(const bt_pyra_state *state) {
    BT_TRACE_ENTER("bt_pyra_is_solved");
    (void)state;
    BT_TRACE_EXIT("bt_pyra_is_solved");
    return false;
}

void bt_pyra_facelets(const bt_pyra_state *state, bt_pz_grid *out) {
    BT_TRACE_ENTER("bt_pyra_facelets");
    memset(out, 0, sizeof *out);
    (void)state;
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
