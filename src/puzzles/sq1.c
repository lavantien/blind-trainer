#include "puzzles/sq1.h"

#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"

const uint8_t BT_PZ_SQ1_SQUARE_SHAPE[BT_PZ_SQ1_PIECES_PER_LAYER] = {1, 2, 1, 2, 1, 2, 1, 2};

static const size_t SQ1_CUT_HALF = BT_PZ_SQ1_UNITS / BT_PZ_SQ1_LAYERS;

static bool sq1_is_space(char byte) {
    return (byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r' || byte == '\v' ||
            byte == '\f') != 0;
}

static int32_t sq1_rem_euclid(int32_t value, int32_t modulus) {
    int32_t r = value % modulus;
    return r < 0 ? r + modulus : r;
}

bt_pz_sq1_move bt_pz_sq1_move_twist(int32_t top, int32_t bottom) {
    BT_TRACE_ENTER("bt_pz_sq1_move_twist");
    bt_pz_sq1_move mv = {.top = top, .bottom = bottom, .slash = false};
    BT_TRACE_EXIT("bt_pz_sq1_move_twist");
    return mv;
}

bt_pz_sq1_move bt_pz_sq1_move_slash(void) {
    BT_TRACE_ENTER("bt_pz_sq1_move_slash");
    bt_pz_sq1_move mv = {.top = 0, .bottom = 0, .slash = true};
    BT_TRACE_EXIT("bt_pz_sq1_move_slash");
    return mv;
}

bool bt_pz_sq1_move_eq(bt_pz_sq1_move a, bt_pz_sq1_move b) {
    BT_TRACE_ENTER("bt_pz_sq1_move_eq");
    bool eq = (a.top == b.top && a.bottom == b.bottom && a.slash == b.slash) != 0;
    BT_TRACE_EXIT("bt_pz_sq1_move_eq");
    return eq;
}

bool bt_pz_sq1_state_eq(const bt_pz_sq1_state *a, const bt_pz_sq1_state *b) {
    BT_TRACE_ENTER("bt_pz_sq1_state_eq");
    bool eq = (memcmp(a->units, b->units, sizeof a->units) == 0 &&
               a->slice_flipped == b->slice_flipped) != 0;
    BT_TRACE_EXIT("bt_pz_sq1_state_eq");
    return eq;
}

void bt_pz_sq1_solved_state(bt_pz_sq1_state *out) {
    BT_TRACE_ENTER("bt_pz_sq1_solved_state");
    memset(out->units, 0, sizeof out->units);
    for (size_t layer = 0; layer < BT_PZ_SQ1_LAYERS; layer++) {
        size_t unit = 0;
        for (size_t piece = 0; piece < BT_PZ_SQ1_PIECES_PER_LAYER; piece++) {
            size_t width = piece % BT_PZ_SQ1_LAYERS == layer ? 2 : 1;
            uint8_t id = (uint8_t)(layer * BT_PZ_SQ1_PIECES_PER_LAYER + piece);
            for (size_t k = 0; k < width; k++) {
                out->units[layer * BT_PZ_SQ1_UNITS + unit] = id;
                unit++;
            }
        }
    }
    out->slice_flipped = false;
    BT_TRACE_EXIT("bt_pz_sq1_solved_state");
}

bool bt_pz_sq1_can_slash(const bt_pz_sq1_state *state) {
    BT_TRACE_ENTER("bt_pz_sq1_can_slash");
    for (size_t layer = 0; layer < BT_PZ_SQ1_LAYERS; layer++) {
        const uint8_t *units = state->units + layer * BT_PZ_SQ1_UNITS;
        if (units[0] == units[BT_PZ_SQ1_UNITS - 1] ||
            units[SQ1_CUT_HALF - 1] == units[SQ1_CUT_HALF]) {
            BT_TRACE_EXIT("bt_pz_sq1_can_slash");
            return false;
        }
    }
    BT_TRACE_EXIT("bt_pz_sq1_can_slash");
    return true;
}

static void sq1_rotate(uint8_t *units, int32_t turn) {
    uint8_t rotated[BT_PZ_SQ1_UNITS];
    for (size_t unit = 0; unit < BT_PZ_SQ1_UNITS; unit++) {
        size_t source = (size_t)sq1_rem_euclid((int32_t)unit - turn, (int32_t)BT_PZ_SQ1_UNITS);
        rotated[unit] = units[source];
    }
    memcpy(units, rotated, sizeof rotated);
}

bool bt_pz_sq1_apply(bt_pz_sq1_state *state, bt_pz_sq1_move mv, bt_pz_error *err) {
    BT_TRACE_ENTER("bt_pz_sq1_apply");
    if (mv.slash) {
        if (!bt_pz_sq1_can_slash(state)) {
            bt_pz_error_set(err, BT_PZ_ERR_APPLY, "/", strlen("/"));
            BT_TRACE_FAIL("bt_pz_sq1_apply", "slash rejected");
            BT_TRACE_EXIT("bt_pz_sq1_apply");
            return false;
        }
        for (size_t unit = SQ1_CUT_HALF; unit < BT_PZ_SQ1_UNITS; unit++) {
            uint8_t hold = state->units[unit];
            state->units[unit] = state->units[BT_PZ_SQ1_UNITS + unit];
            state->units[BT_PZ_SQ1_UNITS + unit] = hold;
        }
        state->slice_flipped = !state->slice_flipped != 0;
        BT_TRACE_EXIT("bt_pz_sq1_apply");
        return true;
    }
    sq1_rotate(state->units, mv.top);
    sq1_rotate(state->units + BT_PZ_SQ1_UNITS, mv.bottom);
    BT_TRACE_EXIT("bt_pz_sq1_apply");
    return true;
}

bool bt_pz_sq1_is_solved(const bt_pz_sq1_state *state) {
    BT_TRACE_ENTER("bt_pz_sq1_is_solved");
    bt_pz_sq1_state solved;
    bt_pz_sq1_solved_state(&solved);
    bool eq = bt_pz_sq1_state_eq(state, &solved);
    BT_TRACE_EXIT("bt_pz_sq1_is_solved");
    return eq;
}

void bt_pz_sq1_format_move(bt_pz_sq1_move mv, bt_strbuf *out) {
    BT_TRACE_ENTER("bt_pz_sq1_format_move");
    if (mv.slash) {
        bt_strbuf_append_str(out, "/");
    } else {
        bt_strbuf_appendf(out, "(%d,%d)", (int)mv.top, (int)mv.bottom);
    }
    BT_TRACE_EXIT("bt_pz_sq1_format_move");
}

size_t bt_pz_sq1_canonical_shape(const bt_pz_sq1_state *state, size_t layer, uint8_t *out,
                                 size_t cap) {
    BT_TRACE_ENTER("bt_pz_sq1_canonical_shape");
    const uint8_t *ring = state->units + layer * BT_PZ_SQ1_UNITS;
    uint8_t widths[BT_PZ_SQ1_UNITS];
    size_t count = 0;
    for (size_t unit = 0; unit < BT_PZ_SQ1_UNITS; unit++) {
        if (unit > 0 && ring[unit] == ring[unit - 1]) {
            widths[count - 1]++;
        } else {
            widths[count++] = 1;
        }
    }
    if (count > 1 && ring[0] == ring[BT_PZ_SQ1_UNITS - 1]) {
        count--;
        widths[0] = (uint8_t)(widths[0] + widths[count]);
    }
    if (cap < count) {
        BT_TRACE_FAIL("bt_pz_sq1_canonical_shape", "cap %zu below count %zu", cap, count);
        bt_trace_dump();
        abort();
    }
    memcpy(out, widths, count);
    for (size_t rotation = 0; rotation < count; rotation++) {
        uint8_t candidate[BT_PZ_SQ1_UNITS];
        bool less = false;
        for (size_t unit = 0; unit < count; unit++) {
            candidate[unit] = widths[(rotation + unit) % count];
            if (!less) {
                size_t k = unit;
                if (candidate[k] < out[k]) {
                    less = true;
                } else if (candidate[k] > out[k]) {
                    break;
                }
            }
        }
        if (less) {
            memcpy(out, candidate, count);
        }
    }
    BT_TRACE_EXIT("bt_pz_sq1_canonical_shape");
    return count;
}

static bool sq1_twist_from(int32_t top, int32_t bottom, bt_pz_sq1_move *out) {
    bool in_range = (top >= BT_PZ_SQ1_TURN_MIN && top <= BT_PZ_SQ1_TURN_MAX &&
                     bottom >= BT_PZ_SQ1_TURN_MIN && bottom <= BT_PZ_SQ1_TURN_MAX) != 0;
    if (!in_range || (top == 0 && bottom == 0)) {
        return false;
    }
    out->top = top;
    out->bottom = bottom;
    out->slash = false;
    return true;
}

static bool sq1_parse_int(const char *bytes, size_t len, int32_t *value, size_t *used) {
    bool negative = (len > 0 && bytes[0] == '-') != 0;
    size_t offset = negative ? 1 : 0;
    int32_t acc = 0;
    size_t count = 0;
    for (size_t k = offset; k < len; k++) {
        char byte = bytes[k];
        bool digit = (byte >= '0' && byte <= '9') != 0;
        if (!digit || acc > BT_PZ_SQ1_TURN_MAX) {
            break;
        }
        acc = acc * 10 + (int32_t)(byte - '0');
        count++;
    }
    if (count == 0 || (count > 1 && bytes[offset] == '0') || (negative && acc == 0)) {
        return false;
    }
    *value = negative ? -acc : acc;
    *used = count + (negative ? 1 : 0);
    return true;
}

static bool sq1_parse_paren(const char *segment, size_t len, bt_pz_sq1_move *out,
                            bt_pz_error *err) {
    const char *inner = segment + 1;
    size_t inner_len = len - 2;
    int32_t top = 0;
    size_t used = 0;
    if (!sq1_parse_int(inner, inner_len, &top, &used)) {
        bt_pz_error_set(err, BT_PZ_ERR_PARSE, segment, len);
        return false;
    }
    if (used >= inner_len || inner[used] != ',') {
        bt_pz_error_set(err, BT_PZ_ERR_PARSE, segment, len);
        return false;
    }
    int32_t bottom = 0;
    size_t used_bottom = 0;
    if (!sq1_parse_int(inner + used + 1, inner_len - used - 1, &bottom, &used_bottom)) {
        bt_pz_error_set(err, BT_PZ_ERR_PARSE, segment, len);
        return false;
    }
    if (used + 1 + used_bottom != inner_len) {
        bt_pz_error_set(err, BT_PZ_ERR_PARSE, segment, len);
        return false;
    }
    if (!sq1_twist_from(top, bottom, out)) {
        bt_pz_error_set(err, BT_PZ_ERR_PARSE, segment, len);
        return false;
    }
    return true;
}

static bool sq1_parse_bare(const char *fragment, size_t len, bt_pz_sq1_move *out, size_t *consumed,
                           bt_pz_error *err) {
    size_t end = 0;
    while (end < len && !sq1_is_space(fragment[end])) {
        end++;
    }
    int32_t top = 0;
    size_t used = 0;
    if (!sq1_parse_int(fragment, len, &top, &used)) {
        bt_pz_error_set(err, BT_PZ_ERR_PARSE, fragment, end);
        return false;
    }
    if (used >= len || fragment[used] != ',') {
        bt_pz_error_set(err, BT_PZ_ERR_PARSE, fragment, end);
        return false;
    }
    int32_t bottom = 0;
    size_t used_bottom = 0;
    if (!sq1_parse_int(fragment + used + 1, len - used - 1, &bottom, &used_bottom)) {
        bt_pz_error_set(err, BT_PZ_ERR_PARSE, fragment, end);
        return false;
    }
    size_t ate = used + 1 + used_bottom;
    bool terminated = (ate >= len || fragment[ate] == '/' || sq1_is_space(fragment[ate])) != 0;
    if (!terminated) {
        bt_pz_error_set(err, BT_PZ_ERR_PARSE, fragment, end);
        return false;
    }
    if (!sq1_twist_from(top, bottom, out)) {
        bt_pz_error_set(err, BT_PZ_ERR_PARSE, fragment, end);
        return false;
    }
    *consumed = ate;
    return true;
}

static bool sq1_scan(const char *text, size_t len, void *alg, bt_pz_error *err) {
    for (size_t at = 0; at < len; at++) {
        if ((unsigned char)text[at] > 127) {
            bt_pz_error_set(err, BT_PZ_ERR_PARSE, text, len);
            return false;
        }
    }
    bt_pz_engine eng = bt_pz_sq1_engine();
    size_t index = 0;
    while (index < len) {
        char byte = text[index];
        if (sq1_is_space(byte)) {
            index++;
        } else if (byte == '/') {
            bt_pz_sq1_move slash = bt_pz_sq1_move_slash();
            bt_pz_alg_push(&eng, alg, &slash);
            index++;
        } else if (byte == '(') {
            size_t offset = 0;
            bool closed = false;
            while (index + offset < len) {
                if (text[index + offset] == ')') {
                    closed = true;
                    break;
                }
                offset++;
            }
            if (!closed) {
                bt_pz_error_set(err, BT_PZ_ERR_PARSE, text + index, len - index);
                return false;
            }
            bt_pz_sq1_move mv;
            if (!sq1_parse_paren(text + index, offset + 1, &mv, err)) {
                return false;
            }
            bt_pz_alg_push(&eng, alg, &mv);
            index = index + offset + 1;
        } else {
            bt_pz_sq1_move mv;
            size_t used = 0;
            if (!sq1_parse_bare(text + index, len - index, &mv, &used, err)) {
                return false;
            }
            bt_pz_alg_push(&eng, alg, &mv);
            index += used;
        }
    }
    return true;
}

bool bt_pz_sq1_parse_move(const char *token, size_t len, bt_pz_sq1_move *out, bt_pz_error *err) {
    BT_TRACE_ENTER("bt_pz_sq1_parse_move");
    void *alg = bt_pz_alg_create();
    bool ok = sq1_scan(token, len, alg, err);
    bt_pz_engine eng = bt_pz_sq1_engine();
    if (ok && bt_pz_alg_len(alg) == 1) {
        const bt_pz_sq1_move *only = bt_pz_alg_at(&eng, alg, 0);
        *out = *only;
    } else if (ok) {
        bt_pz_error_set(err, BT_PZ_ERR_PARSE, token, len);
        ok = false;
    }
    bt_pz_alg_destroy(alg);
    if (!ok) {
        BT_TRACE_FAIL("bt_pz_sq1_parse_move", "token rejected");
    }
    BT_TRACE_EXIT("bt_pz_sq1_parse_move");
    return ok;
}

bool bt_pz_sq1_parse_alg(const char *text, size_t len, void *alg, bt_pz_error *err) {
    BT_TRACE_ENTER("bt_pz_sq1_parse_alg");
    bt_pz_alg staged = {0};
    bool ok = sq1_scan(text, len, &staged, err);
    if (!ok) {
        free(staged.items);
        BT_TRACE_FAIL("bt_pz_sq1_parse_alg", "alg rejected");
        BT_TRACE_EXIT("bt_pz_sq1_parse_alg");
        return false;
    }
    bt_pz_alg *vec = alg;
    free(vec->items);
    *vec = staged;
    BT_TRACE_EXIT("bt_pz_sq1_parse_alg");
    return true;
}

void bt_pz_sq1_format_alg(const void *alg, bt_strbuf *out) {
    BT_TRACE_ENTER("bt_pz_sq1_format_alg");
    bt_pz_engine eng = bt_pz_sq1_engine();
    size_t len = bt_pz_alg_len(alg);
    for (size_t i = 0; i < len; i++) {
        if (i > 0) {
            bt_strbuf_append_str(out, " ");
        }
        const bt_pz_sq1_move *mv = bt_pz_alg_at(&eng, alg, i);
        bt_pz_sq1_format_move(*mv, out);
    }
    BT_TRACE_EXIT("bt_pz_sq1_format_alg");
}

void bt_pz_sq1_invert_alg(const void *alg, void *out) {
    BT_TRACE_ENTER("bt_pz_sq1_invert_alg");
    bt_pz_engine eng = bt_pz_sq1_engine();
    size_t len = bt_pz_alg_len(alg);
    for (size_t i = 0; i < len; i++) {
        const bt_pz_sq1_move *src = bt_pz_alg_at(&eng, alg, len - 1 - i);
        bt_pz_sq1_move flipped = *src;
        if (!flipped.slash) {
            int32_t neg_top = -flipped.top;
            int32_t neg_bottom = -flipped.bottom;
            flipped.top = neg_top == -BT_PZ_SQ1_TURN_MAX ? BT_PZ_SQ1_TURN_MAX : neg_top;
            flipped.bottom = neg_bottom == -BT_PZ_SQ1_TURN_MAX ? BT_PZ_SQ1_TURN_MAX : neg_bottom;
        }
        bt_pz_alg_push(&eng, out, &flipped);
    }
    BT_TRACE_EXIT("bt_pz_sq1_invert_alg");
}

bool bt_pz_sq1_apply_alg(bt_pz_sq1_state *state, const void *alg, bt_pz_error *err) {
    BT_TRACE_ENTER("bt_pz_sq1_apply_alg");
    bt_pz_engine eng = bt_pz_sq1_engine();
    size_t len = bt_pz_alg_len(alg);
    for (size_t i = 0; i < len; i++) {
        const bt_pz_sq1_move *mv = bt_pz_alg_at(&eng, alg, i);
        if (!bt_pz_sq1_apply(state, *mv, err)) {
            BT_TRACE_FAIL("bt_pz_sq1_apply_alg", "move %zu rejected", i);
            BT_TRACE_EXIT("bt_pz_sq1_apply_alg");
            return false;
        }
    }
    BT_TRACE_EXIT("bt_pz_sq1_apply_alg");
    return true;
}

void bt_pz_sq1_facelets(const bt_pz_sq1_state *state, bt_pz_grid *out) {
    BT_TRACE_ENTER("bt_pz_sq1_facelets");
    out->face_count = BT_PZ_SQ1_LAYERS;
    for (size_t layer = 0; layer < BT_PZ_SQ1_LAYERS; layer++) {
        out->faces[layer].row_count = 1;
        out->faces[layer].rows[0].cols = BT_PZ_SQ1_UNITS;
        memcpy(out->faces[layer].rows[0].cells, state->units + layer * BT_PZ_SQ1_UNITS,
               BT_PZ_SQ1_UNITS);
    }
    BT_TRACE_EXIT("bt_pz_sq1_facelets");
}

static bool sq1_vt_parse_move(const bt_pz_engine *eng, const char *token, size_t len,
                              void *out_move, bt_pz_error *err) {
    (void)eng;
    return bt_pz_sq1_parse_move(token, len, out_move, err);
}

static bool sq1_vt_parse_alg(const bt_pz_engine *eng, const char *text, size_t len, void *alg,
                             bt_pz_error *err) {
    (void)eng;
    return bt_pz_sq1_parse_alg(text, len, alg, err);
}

static void sq1_vt_format_alg(const bt_pz_engine *eng, const void *alg, bt_strbuf *out) {
    (void)eng;
    bt_pz_sq1_format_alg(alg, out);
}

static void sq1_vt_invert_alg(const bt_pz_engine *eng, const void *alg, void *out) {
    (void)eng;
    bt_pz_sq1_invert_alg(alg, out);
}

static bool sq1_vt_apply_alg(const bt_pz_engine *eng, void *state, const void *alg,
                             bt_pz_error *err) {
    (void)eng;
    return bt_pz_sq1_apply_alg(state, alg, err);
}

static void sq1_vt_solved_state(const bt_pz_engine *eng, void *out_state) {
    (void)eng;
    bt_pz_sq1_solved_state(out_state);
}

static bool sq1_vt_is_solved(const bt_pz_engine *eng, const void *state) {
    (void)eng;
    return bt_pz_sq1_is_solved(state);
}

static void sq1_vt_facelets(const bt_pz_engine *eng, const void *state, bt_pz_grid *out) {
    (void)eng;
    bt_pz_sq1_facelets(state, out);
}

bt_pz_engine bt_pz_sq1_engine(void) {
    BT_TRACE_ENTER("bt_pz_sq1_engine");
    bt_pz_engine eng = {
        .name = "sq1",
        .self = NULL,
        .move_size = sizeof(bt_pz_sq1_move),
        .state_size = sizeof(bt_pz_sq1_state),
        .parse_move = sq1_vt_parse_move,
        .parse_alg = sq1_vt_parse_alg,
        .format_alg = sq1_vt_format_alg,
        .invert_alg = sq1_vt_invert_alg,
        .apply_alg = sq1_vt_apply_alg,
        .solved_state = sq1_vt_solved_state,
        .is_solved = sq1_vt_is_solved,
        .facelets = sq1_vt_facelets,
    };
    BT_TRACE_EXIT("bt_pz_sq1_engine");
    return eng;
}
