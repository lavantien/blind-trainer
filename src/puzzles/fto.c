#include "puzzles/fto.h"

#include <string.h>

#include "common/bt_trace.h"

const char *const BT_FTO_FACE_NAMES[BT_FTO_FACE_COUNT] = {
    "U", "F", "R", "BR", "B", "BL", "L", "D",
};

bt_fto bt_fto_new(void) {
    BT_TRACE_ENTER("bt_fto_new");
    bt_fto fto;
    memset(&fto, 0, sizeof fto);
    BT_TRACE_EXIT("bt_fto_new");
    return fto;
}

void bt_fto_turn_table(const bt_fto *fto, uint8_t face, uint8_t quarter,
                       uint8_t out[BT_FTO_FACELET_COUNT]) {
    BT_TRACE_ENTER("bt_fto_turn_table");
    memcpy(out, fto->turns[face * 2 + quarter - 1], BT_FTO_FACELET_COUNT);
    BT_TRACE_EXIT("bt_fto_turn_table");
}

void bt_fto_neighbors(const bt_fto *fto, uint8_t face, uint8_t out[3]) {
    BT_TRACE_ENTER("bt_fto_neighbors");
    (void)fto;
    (void)face;
    out[0] = 0;
    out[1] = 0;
    out[2] = 0;
    BT_TRACE_EXIT("bt_fto_neighbors");
}

uint8_t bt_fto_opposite(const bt_fto *fto, uint8_t face) {
    BT_TRACE_ENTER("bt_fto_opposite");
    (void)fto;
    (void)face;
    BT_TRACE_EXIT("bt_fto_opposite");
    return 0;
}

bool bt_fto_move_eq(bt_fto_move a, bt_fto_move b) {
    return a.face == b.face && a.quarter == b.quarter;
}

bool bt_fto_state_eq(const bt_fto_state *a, const bt_fto_state *b) {
    return memcmp(a->facelets, b->facelets, BT_FTO_FACELET_COUNT) == 0;
}

bool bt_fto_parse_move(const char *token, size_t len, bt_fto_move *out, bt_pz_error *err) {
    BT_TRACE_ENTER("bt_fto_parse_move");
    (void)token;
    (void)len;
    out->face = 0;
    out->quarter = 1;
    BT_TRACE_EXIT("bt_fto_parse_move");
    (void)err;
    return true;
}

bool bt_fto_parse_alg(const bt_fto *fto, const char *text, size_t len, void *alg,
                      bt_pz_error *err) {
    BT_TRACE_ENTER("bt_fto_parse_alg");
    bt_pz_engine eng = bt_fto_engine(fto);
    bool ok = bt_pz_alg_default_parse(&eng, text, len, alg, err);
    BT_TRACE_EXIT("bt_fto_parse_alg");
    return ok;
}

void bt_fto_format_alg(const bt_fto *fto, const void *alg, bt_strbuf *out) {
    BT_TRACE_ENTER("bt_fto_format_alg");
    (void)fto;
    (void)alg;
    bt_strbuf_append_str(out, "stub");
    BT_TRACE_EXIT("bt_fto_format_alg");
}

void bt_fto_invert_alg(const bt_fto *fto, const void *alg, void *out) {
    BT_TRACE_ENTER("bt_fto_invert_alg");
    bt_pz_engine eng = bt_fto_engine(fto);
    bt_pz_alg *src = (bt_pz_alg *)alg;
    bt_pz_alg *dst = out;
    dst->len = 0;
    for (size_t i = src->len; i > 0; i--) {
        bt_pz_alg_push(&eng, dst, bt_pz_alg_at(&eng, src, i - 1));
    }
    BT_TRACE_EXIT("bt_fto_invert_alg");
}

bool bt_fto_apply_alg(const bt_fto *fto, bt_fto_state *state, const void *alg, bt_pz_error *err) {
    BT_TRACE_ENTER("bt_fto_apply_alg");
    (void)fto;
    (void)state;
    (void)alg;
    BT_TRACE_EXIT("bt_fto_apply_alg");
    (void)err;
    return true;
}

void bt_fto_solved_state(const bt_fto *fto, bt_fto_state *out) {
    BT_TRACE_ENTER("bt_fto_solved_state");
    (void)fto;
    memset(out->facelets, 0, BT_FTO_FACELET_COUNT);
    BT_TRACE_EXIT("bt_fto_solved_state");
}

bool bt_fto_is_solved(const bt_fto *fto, const bt_fto_state *state) {
    BT_TRACE_ENTER("bt_fto_is_solved");
    (void)fto;
    (void)state;
    BT_TRACE_EXIT("bt_fto_is_solved");
    return false;
}

void bt_fto_facelets(const bt_fto *fto, const bt_fto_state *state, bt_pz_grid *out) {
    BT_TRACE_ENTER("bt_fto_facelets");
    (void)fto;
    (void)state;
    memset(out, 0, sizeof *out);
    BT_TRACE_EXIT("bt_fto_facelets");
}

static bool fto_vt_parse_move(const bt_pz_engine *eng, const char *token, size_t len,
                              void *out_move, bt_pz_error *err) {
    (void)eng;
    return bt_fto_parse_move(token, len, out_move, err);
}

static bool fto_vt_parse_alg(const bt_pz_engine *eng, const char *text, size_t len, void *alg,
                             bt_pz_error *err) {
    return bt_fto_parse_alg(eng->self, text, len, alg, err);
}

static void fto_vt_format_alg(const bt_pz_engine *eng, const void *alg, bt_strbuf *out) {
    bt_fto_format_alg(eng->self, alg, out);
}

static void fto_vt_invert_alg(const bt_pz_engine *eng, const void *alg, void *out) {
    bt_fto_invert_alg(eng->self, alg, out);
}

static bool fto_vt_apply_alg(const bt_pz_engine *eng, void *state, const void *alg,
                             bt_pz_error *err) {
    return bt_fto_apply_alg(eng->self, state, alg, err);
}

static void fto_vt_solved_state(const bt_pz_engine *eng, void *out_state) {
    bt_fto_solved_state(eng->self, out_state);
}

static bool fto_vt_is_solved(const bt_pz_engine *eng, const void *state) {
    return bt_fto_is_solved(eng->self, state);
}

static void fto_vt_facelets(const bt_pz_engine *eng, const void *state, bt_pz_grid *out) {
    bt_fto_facelets(eng->self, state, out);
}

bt_pz_engine bt_fto_engine(const bt_fto *fto) {
    BT_TRACE_ENTER("bt_fto_engine");
    bt_pz_engine eng = {
        .name = "fto",
        .self = fto,
        .move_size = sizeof(bt_fto_move),
        .state_size = sizeof(bt_fto_state),
        .parse_move = fto_vt_parse_move,
        .parse_alg = fto_vt_parse_alg,
        .format_alg = fto_vt_format_alg,
        .invert_alg = fto_vt_invert_alg,
        .apply_alg = fto_vt_apply_alg,
        .solved_state = fto_vt_solved_state,
        .is_solved = fto_vt_is_solved,
        .facelets = fto_vt_facelets,
    };
    BT_TRACE_EXIT("bt_fto_engine");
    return eng;
}
