#include "puzzles/skewb.h"

#include <string.h>

#include "common/bt_trace.h"

const char *const BT_SKEWB_FACE_NAMES[BT_SKEWB_FACE_COUNT] = {"U", "F", "R", "L", "B", "D"};
const char *const BT_SKEWB_VERTEX_NAMES[BT_SKEWB_VERTEX_COUNT] = {"F", "L", "R", "B"};

bt_skewb bt_skewb_new(void) {
    BT_TRACE_ENTER("bt_skewb_new");
    bt_skewb skewb;
    memset(&skewb, 0, sizeof skewb);
    BT_TRACE_EXIT("bt_skewb_new");
    return skewb;
}

void bt_skewb_turn_table(const bt_skewb *skewb, uint8_t vertex, uint8_t quarter,
                         uint8_t out[BT_SKEWB_FACELET_COUNT]) {
    BT_TRACE_ENTER("bt_skewb_turn_table");
    memcpy(out, skewb->turns[vertex * 2 + quarter - 1], BT_SKEWB_FACELET_COUNT);
    BT_TRACE_EXIT("bt_skewb_turn_table");
}

bool bt_skewb_move_eq(bt_skewb_move a, bt_skewb_move b) {
    return a.vertex == b.vertex && a.quarter == b.quarter;
}

bool bt_skewb_state_eq(const bt_skewb_state *a, const bt_skewb_state *b) {
    return memcmp(a->facelets, b->facelets, BT_SKEWB_FACELET_COUNT) == 0;
}

bool bt_skewb_parse_move(const char *token, size_t len, bt_skewb_move *out, bt_pz_error *err) {
    BT_TRACE_ENTER("bt_skewb_parse_move");
    (void)token;
    (void)len;
    out->vertex = 0;
    out->quarter = 1;
    BT_TRACE_EXIT("bt_skewb_parse_move");
    (void)err;
    return true;
}

bool bt_skewb_parse_alg(const bt_skewb *skewb, const char *text, size_t len, void *alg,
                        bt_pz_error *err) {
    BT_TRACE_ENTER("bt_skewb_parse_alg");
    bt_pz_engine eng = bt_skewb_engine(skewb);
    bool ok = bt_pz_alg_default_parse(&eng, text, len, alg, err);
    BT_TRACE_EXIT("bt_skewb_parse_alg");
    return ok;
}

void bt_skewb_format_alg(const bt_skewb *skewb, const void *alg, bt_strbuf *out) {
    BT_TRACE_ENTER("bt_skewb_format_alg");
    (void)skewb;
    (void)alg;
    bt_strbuf_append_str(out, "stub");
    BT_TRACE_EXIT("bt_skewb_format_alg");
}

void bt_skewb_invert_alg(const bt_skewb *skewb, const void *alg, void *out) {
    BT_TRACE_ENTER("bt_skewb_invert_alg");
    bt_pz_engine eng = bt_skewb_engine(skewb);
    bt_pz_alg *src = (bt_pz_alg *)alg;
    bt_pz_alg *dst = out;
    dst->len = 0;
    for (size_t i = src->len; i > 0; i--) {
        bt_pz_alg_push(&eng, dst, bt_pz_alg_at(&eng, src, i - 1));
    }
    BT_TRACE_EXIT("bt_skewb_invert_alg");
}

bool bt_skewb_apply_alg(const bt_skewb *skewb, bt_skewb_state *state, const void *alg,
                        bt_pz_error *err) {
    BT_TRACE_ENTER("bt_skewb_apply_alg");
    (void)skewb;
    (void)state;
    (void)alg;
    BT_TRACE_EXIT("bt_skewb_apply_alg");
    (void)err;
    return true;
}

void bt_skewb_solved_state(const bt_skewb *skewb, bt_skewb_state *out) {
    BT_TRACE_ENTER("bt_skewb_solved_state");
    (void)skewb;
    memset(out->facelets, 0, BT_SKEWB_FACELET_COUNT);
    BT_TRACE_EXIT("bt_skewb_solved_state");
}

bool bt_skewb_is_solved(const bt_skewb *skewb, const bt_skewb_state *state) {
    BT_TRACE_ENTER("bt_skewb_is_solved");
    (void)skewb;
    (void)state;
    BT_TRACE_EXIT("bt_skewb_is_solved");
    return false;
}

void bt_skewb_facelets(const bt_skewb *skewb, const bt_skewb_state *state, bt_pz_grid *out) {
    BT_TRACE_ENTER("bt_skewb_facelets");
    (void)skewb;
    (void)state;
    memset(out, 0, sizeof *out);
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
