#include "puzzles/mega.h"

#include <string.h>

#include "common/bt_trace.h"

const uint8_t BT_MEGA_NEIGHBOR_CW[BT_MEGA_FACES][BT_MEGA_RING] = {{0}};
const uint8_t BT_MEGA_Y_PERM[BT_MEGA_FACES] = {0};

uint8_t bt_mega_corner_slot(size_t k) {
    return (uint8_t)(BT_MEGA_CORNER_BASE + k);
}

uint8_t bt_mega_edge_slot(size_t k) {
    return (uint8_t)(BT_MEGA_EDGE_BASE + k);
}

uint8_t bt_mega_color(const bt_mega_state *state, uint8_t face, uint8_t slot) {
    return state->colors[(size_t)face * BT_MEGA_PER_FACE + slot];
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
    (void)mv;
    return false;
}

bool bt_mega_parse_move(const char *token, size_t len, bt_mega_move *out, bt_pz_error *err) {
    BT_TRACE_ENTER("bt_mega_parse_move");
    (void)token;
    (void)len;
    out->spin = 0;
    out->face = 0;
    out->steps = 1;
    BT_TRACE_EXIT("bt_mega_parse_move");
    (void)err;
    return true;
}

bool bt_mega_parse_alg(const char *text, size_t len, void *alg, bt_pz_error *err) {
    BT_TRACE_ENTER("bt_mega_parse_alg");
    bt_pz_engine eng = bt_mega_engine();
    bool ok = bt_pz_alg_default_parse(&eng, text, len, alg, err);
    BT_TRACE_EXIT("bt_mega_parse_alg");
    return ok;
}

void bt_mega_format_alg(const void *alg, bt_strbuf *out) {
    BT_TRACE_ENTER("bt_mega_format_alg");
    (void)alg;
    bt_strbuf_append_str(out, "stub");
    BT_TRACE_EXIT("bt_mega_format_alg");
}

void bt_mega_invert_alg(const void *alg, void *out) {
    BT_TRACE_ENTER("bt_mega_invert_alg");
    bt_pz_engine eng = bt_mega_engine();
    bt_pz_alg *src = (bt_pz_alg *)alg;
    bt_pz_alg *dst = out;
    dst->len = 0;
    for (size_t i = src->len; i > 0; i--) {
        bt_pz_alg_push(&eng, dst, bt_pz_alg_at(&eng, src, i - 1));
    }
    BT_TRACE_EXIT("bt_mega_invert_alg");
}

bool bt_mega_apply_alg(bt_mega_state *state, const void *alg, bt_pz_error *err) {
    BT_TRACE_ENTER("bt_mega_apply_alg");
    (void)state;
    (void)alg;
    BT_TRACE_EXIT("bt_mega_apply_alg");
    (void)err;
    return true;
}

void bt_mega_solved_state(bt_mega_state *out) {
    BT_TRACE_ENTER("bt_mega_solved_state");
    memset(out->colors, 0, BT_MEGA_FACELETS);
    BT_TRACE_EXIT("bt_mega_solved_state");
}

bool bt_mega_is_solved(const bt_mega_state *state) {
    BT_TRACE_ENTER("bt_mega_is_solved");
    (void)state;
    BT_TRACE_EXIT("bt_mega_is_solved");
    return false;
}

void bt_mega_facelets(const bt_mega_state *state, bt_pz_grid *out) {
    BT_TRACE_ENTER("bt_mega_facelets");
    (void)state;
    memset(out, 0, sizeof *out);
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
