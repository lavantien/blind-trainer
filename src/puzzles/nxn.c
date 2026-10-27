#include "puzzles/nxn.h"

#include <stdlib.h>
#include <string.h>

#include "common/bt_harness.h"
#include "common/bt_trace.h"

enum {
    NXN_KIND_FACE = 0,
    NXN_KIND_ROTATION = 1,
    NXN_KIND_SLICE = 2,
};

static const int32_t NXN_NORMAL[6][3] = {
    {0, 1, 0}, {-1, 0, 0}, {0, 0, 1}, {1, 0, 0}, {0, 0, -1}, {0, -1, 0},
};
static const int32_t NXN_ROW[6][3] = {
    {0, 0, 1}, {0, -1, 0}, {0, -1, 0}, {0, -1, 0}, {0, -1, 0}, {0, 0, -1},
};
static const int32_t NXN_COL[6][3] = {
    {1, 0, 0}, {0, 0, 1}, {1, 0, 0}, {0, 0, -1}, {-1, 0, 0}, {1, 0, 0},
};
static const size_t NXN_AXIS[6] = {1, 0, 2, 0, 2, 1};
static const int32_t NXN_SIGN[6] = {1, -1, 1, 1, -1, -1};

static const char NXN_FACE_LETTER[6] = {'U', 'L', 'F', 'R', 'B', 'D'};

typedef struct {
    size_t face;
    char letter;
} nxn_face_letter;

static const nxn_face_letter NXN_ROTATION_LETTER[3] = {{3, 'x'}, {0, 'y'}, {2, 'z'}};
static const nxn_face_letter NXN_SLICE_LETTER[3] = {{1, 'M'}, {5, 'E'}, {2, 'S'}};

typedef struct {
    char letter;
    uint8_t face;
    uint8_t kind;
} nxn_notation;

static const nxn_notation NXN_NOTATION[12] = {
    {'U', 0, NXN_KIND_FACE},     {'D', 5, NXN_KIND_FACE},     {'L', 1, NXN_KIND_FACE},
    {'R', 3, NXN_KIND_FACE},     {'F', 2, NXN_KIND_FACE},     {'B', 4, NXN_KIND_FACE},
    {'x', 3, NXN_KIND_ROTATION}, {'y', 0, NXN_KIND_ROTATION}, {'z', 2, NXN_KIND_ROTATION},
    {'M', 1, NXN_KIND_SLICE},    {'E', 5, NXN_KIND_SLICE},    {'S', 2, NXN_KIND_SLICE},
};

static size_t nxn_index(size_t face, size_t row, size_t col) {
    return face * BT_PZ_NXN_STRIDE * BT_PZ_NXN_STRIDE + row * BT_PZ_NXN_STRIDE + col;
}

static int32_t nxn_dot(const int32_t a[3], const int32_t b[3]) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void nxn_rot90(const int32_t v[3], size_t axis, int32_t out[3]) {
    int32_t x = v[0];
    int32_t y = v[1];
    int32_t z = v[2];
    if (axis == 0) {
        out[0] = x;
        out[1] = -z;
        out[2] = y;
    } else if (axis == 1) {
        out[0] = z;
        out[1] = y;
        out[2] = -x;
    } else {
        out[0] = -y;
        out[1] = x;
        out[2] = z;
    }
}

static void nxn_sticker_center(size_t n, size_t face, size_t row, size_t col, int32_t out[3]) {
    int32_t h = (int32_t)n - 1;
    int32_t along_row = 2 * (int32_t)row - h;
    int32_t along_col = 2 * (int32_t)col - h;
    for (size_t i = 0; i < 3; i++) {
        out[i] =
            NXN_NORMAL[face][i] * h + NXN_ROW[face][i] * along_row + NXN_COL[face][i] * along_col;
    }
}

static size_t nxn_face_of_normal(const int32_t normal[3]) {
    if (normal[0] != 0) {
        return normal[0] > 0 ? 3 : 1;
    }
    if (normal[1] != 0) {
        return normal[1] > 0 ? 0 : 5;
    }
    return normal[2] > 0 ? 2 : 4;
}

static void nxn_cell_of(const int32_t center[3], size_t face, size_t n, size_t *row, size_t *col) {
    int32_t h = (int32_t)n - 1;
    int32_t along_row = nxn_dot(center, NXN_ROW[face]);
    int32_t along_col = nxn_dot(center, NXN_COL[face]);
    *row = (size_t)((along_row + h) / 2);
    *col = (size_t)((along_col + h) / 2);
}

static uint8_t nxn_middle(size_t n) {
    return (uint8_t)((n + 1) / 2);
}

static bool nxn_find_notation(char letter, uint8_t *face, uint8_t *kind) {
    for (size_t k = 0; k < 12; k++) {
        if (NXN_NOTATION[k].letter == letter) {
            *face = NXN_NOTATION[k].face;
            *kind = NXN_NOTATION[k].kind;
            return true;
        }
    }
    return false;
}

static bool nxn_lookup_letter(const nxn_face_letter table[3], uint8_t face, char *letter) {
    for (size_t k = 0; k < 3; k++) {
        if (table[k].face == (size_t)face) {
            *letter = table[k].letter;
            return true;
        }
    }
    return false;
}

bool bt_pz_nxn_init(bt_pz_nxn *nxn, size_t n) {
    BT_TRACE_ENTER("bt_pz_nxn_init");
    if (n < BT_PZ_NXN_MIN_N || n > BT_PZ_NXN_MAX_N) {
        BT_TRACE_FAIL("bt_pz_nxn_init", "n %zu out of range", n);
        BT_TRACE_EXIT("bt_pz_nxn_init");
        return false;
    }
    nxn->n = n;
    BT_TRACE_EXIT("bt_pz_nxn_init");
    return true;
}

size_t bt_pz_nxn_n(const bt_pz_nxn *nxn) {
    BT_TRACE_ENTER("bt_pz_nxn_n");
    size_t n = nxn->n;
    BT_TRACE_EXIT("bt_pz_nxn_n");
    return n;
}

bool bt_pz_nxn_move_eq(bt_pz_nxn_move a, bt_pz_nxn_move b) {
    BT_TRACE_ENTER("bt_pz_nxn_move_eq");
    bool eq = (a.face == b.face && a.depth_from == b.depth_from && a.depth_to == b.depth_to &&
               a.quarter == b.quarter) != 0;
    BT_TRACE_EXIT("bt_pz_nxn_move_eq");
    return eq;
}

bool bt_pz_nxn_state_eq(const bt_pz_nxn_state *a, const bt_pz_nxn_state *b) {
    BT_TRACE_ENTER("bt_pz_nxn_state_eq");
    if (a->n != b->n) {
        BT_TRACE_EXIT("bt_pz_nxn_state_eq");
        return false;
    }
    for (size_t face = 0; face < 6; face++) {
        for (size_t row = 0; row < a->n; row++) {
            for (size_t col = 0; col < a->n; col++) {
                if (a->facelet[nxn_index(face, row, col)] !=
                    b->facelet[nxn_index(face, row, col)]) {
                    BT_TRACE_EXIT("bt_pz_nxn_state_eq");
                    return false;
                }
            }
        }
    }
    BT_TRACE_EXIT("bt_pz_nxn_state_eq");
    return true;
}

static bool nxn_reject(bt_pz_error *err, const char *token, size_t len) {
    bt_pz_error_set(err, BT_PZ_ERR_PARSE, token, len);
    return false;
}

static bool nxn_parse_token(const bt_pz_nxn *nxn, const char *token, size_t len,
                            bt_pz_nxn_move *out, bt_pz_error *err) {
    size_t i = 0;
    uint8_t digit = 0;
    if (len > 0 && token[0] >= '0' && token[0] <= '9') {
        digit = (uint8_t)(token[0] - '0');
        if (digit < 2 || digit > 7) {
            return nxn_reject(err, token, len);
        }
        i = 1;
    }
    if (i >= len) {
        return nxn_reject(err, token, len);
    }
    char letter = token[i];
    i++;
    uint8_t face = 0;
    uint8_t kind = 0;
    bool found = nxn_find_notation(letter, &face, &kind);
    bool wide = false;
    if (!found && letter >= 'a' && letter <= 'z') {
        uint8_t upper_face = 0;
        uint8_t upper_kind = 0;
        if (nxn_find_notation((char)(letter - 'a' + 'A'), &upper_face, &upper_kind) &&
            upper_kind == NXN_KIND_FACE) {
            face = upper_face;
            kind = upper_kind;
            found = true;
            wide = true;
        }
    }
    if (!found) {
        return nxn_reject(err, token, len);
    }
    if (digit != 0 && kind != NXN_KIND_FACE) {
        return nxn_reject(err, token, len);
    }
    if (!wide && kind == NXN_KIND_FACE && i < len && token[i] == 'w') {
        wide = true;
        i++;
    }
    uint8_t halves = 0;
    uint8_t primes = 0;
    while (i < len) {
        if (token[i] == '2') {
            halves++;
        } else if (token[i] == '\'') {
            primes++;
        } else {
            return nxn_reject(err, token, len);
        }
        i++;
    }
    if (halves > 1 || primes > 1) {
        return nxn_reject(err, token, len);
    }
    uint8_t quarter = 1;
    if (halves == 1) {
        quarter = 2;
    } else if (primes == 1) {
        quarter = 3;
    }
    uint8_t middle = nxn_middle(nxn->n);
    uint8_t from = 0;
    uint8_t to = 0;
    if (kind == NXN_KIND_ROTATION) {
        from = 1;
        to = (uint8_t)nxn->n;
    } else if (kind == NXN_KIND_SLICE) {
        from = middle;
        to = middle;
    } else if (wide) {
        from = 1;
        to = digit != 0 ? digit : 2;
    } else {
        uint8_t depth = digit != 0 ? digit : 1;
        from = depth;
        to = depth;
    }
    if (from < 1 || from > to || to > (uint8_t)nxn->n) {
        return nxn_reject(err, token, len);
    }
    out->face = face;
    out->depth_from = from;
    out->depth_to = to;
    out->quarter = quarter;
    return true;
}

bool bt_pz_nxn_parse_move(const bt_pz_nxn *nxn, const char *token, size_t len, bt_pz_nxn_move *out,
                          bt_pz_error *err) {
    BT_TRACE_ENTER("bt_pz_nxn_parse_move");
    bool ok = nxn_parse_token(nxn, token, len, out, err);
    if (!ok) {
        BT_TRACE_FAIL("bt_pz_nxn_parse_move", "token rejected");
    }
    BT_TRACE_EXIT("bt_pz_nxn_parse_move");
    return ok;
}

bool bt_pz_nxn_parse_alg(const bt_pz_nxn *nxn, const char *text, size_t len, void *alg,
                         bt_pz_error *err) {
    BT_TRACE_ENTER("bt_pz_nxn_parse_alg");
    bt_pz_engine eng = bt_pz_nxn_engine(nxn);
    bool ok = bt_pz_alg_default_parse(&eng, text, len, alg, err);
    if (!ok) {
        BT_TRACE_FAIL("bt_pz_nxn_parse_alg", "alg rejected");
    }
    BT_TRACE_EXIT("bt_pz_nxn_parse_alg");
    return ok;
}

static void nxn_format_token(const bt_pz_nxn *nxn, bt_pz_nxn_move mv, bt_strbuf *out) {
    uint8_t from = mv.depth_from;
    uint8_t to = mv.depth_to;
    uint8_t middle = nxn_middle(nxn->n);
    char special = 0;
    if (from == 1 && to == (uint8_t)nxn->n) {
        nxn_lookup_letter(NXN_ROTATION_LETTER, mv.face, &special);
    } else if (from == middle && to == middle) {
        nxn_lookup_letter(NXN_SLICE_LETTER, mv.face, &special);
    }
    char face = NXN_FACE_LETTER[mv.face];
    if (special != 0) {
        bt_strbuf_appendf(out, "%c", special);
    } else if (from == 1 && to > 1) {
        if (to > 2) {
            bt_strbuf_appendf(out, "%u%cw", (unsigned)to, face);
        } else {
            bt_strbuf_appendf(out, "%cw", face);
        }
    } else if (from == to) {
        if (from > 1) {
            bt_strbuf_appendf(out, "%u%c", (unsigned)from, face);
        } else {
            bt_strbuf_appendf(out, "%c", face);
        }
    } else {
        BT_TRACE_FAIL("nxn_format_token", "unreachable format arm");
        bt_trace_dump();
        abort();
    }
    if (mv.quarter == 2) {
        bt_strbuf_append_str(out, "2");
    } else if (mv.quarter == 3) {
        bt_strbuf_append_str(out, "'");
    }
}

void bt_pz_nxn_format_alg(const bt_pz_nxn *nxn, const void *alg, bt_strbuf *out) {
    BT_TRACE_ENTER("bt_pz_nxn_format_alg");
    bt_pz_engine eng = bt_pz_nxn_engine(nxn);
    size_t len = bt_pz_alg_len(alg);
    for (size_t i = 0; i < len; i++) {
        if (i > 0) {
            bt_strbuf_append_str(out, " ");
        }
        const bt_pz_nxn_move *mv = bt_pz_alg_at(&eng, alg, i);
        nxn_format_token(nxn, *mv, out);
    }
    BT_TRACE_EXIT("bt_pz_nxn_format_alg");
}

void bt_pz_nxn_invert_alg(const bt_pz_nxn *nxn, const void *alg, void *out) {
    BT_TRACE_ENTER("bt_pz_nxn_invert_alg");
    bt_pz_engine eng = bt_pz_nxn_engine(nxn);
    bt_pz_alg *vec = out;
    vec->len = 0;
    size_t len = bt_pz_alg_len(alg);
    for (size_t i = 0; i < len; i++) {
        const bt_pz_nxn_move *src = bt_pz_alg_at(&eng, alg, len - 1 - i);
        bt_pz_nxn_move flipped = *src;
        flipped.quarter = (uint8_t)(4 - flipped.quarter);
        bt_pz_alg_push(&eng, out, &flipped);
    }
    BT_TRACE_EXIT("bt_pz_nxn_invert_alg");
}

static int32_t nxn_rem_euclid(int32_t value, int32_t modulus) {
    int32_t r = value % modulus;
    return r < 0 ? r + modulus : r;
}

static void nxn_apply_move(const bt_pz_nxn *nxn, bt_pz_nxn_state *state, bt_pz_nxn_move mv) {
    bt_pz_nxn_state src = *state;
    size_t face = mv.face;
    const int32_t *turned = NXN_NORMAL[face];
    size_t axis = NXN_AXIS[face];
    int32_t h = (int32_t)nxn->n - 1;
    int32_t u_lo = h - 2 * ((int32_t)mv.depth_to - 1);
    int32_t u_hi = h - 2 * ((int32_t)mv.depth_from - 1);
    size_t steps = (size_t)nxn_rem_euclid(-NXN_SIGN[face] * (int32_t)mv.quarter, 4);
    for (size_t f = 0; f < 6; f++) {
        for (size_t r = 0; r < nxn->n; r++) {
            for (size_t c = 0; c < nxn->n; c++) {
                int32_t center[3];
                nxn_sticker_center(nxn->n, f, r, c, center);
                int32_t normal[3] = {NXN_NORMAL[f][0], NXN_NORMAL[f][1], NXN_NORMAL[f][2]};
                int32_t u = nxn_dot(center, turned);
                size_t count = u >= u_lo && u <= u_hi ? steps : 0;
                for (size_t k = 0; k < count; k++) {
                    int32_t next_center[3];
                    int32_t next_normal[3];
                    nxn_rot90(center, axis, next_center);
                    nxn_rot90(normal, axis, next_normal);
                    memcpy(center, next_center, sizeof center);
                    memcpy(normal, next_normal, sizeof normal);
                }
                size_t dst = nxn_face_of_normal(normal);
                size_t dr = 0;
                size_t dc = 0;
                nxn_cell_of(center, dst, nxn->n, &dr, &dc);
                state->facelet[nxn_index(dst, dr, dc)] = src.facelet[nxn_index(f, r, c)];
            }
        }
    }
}

bool bt_pz_nxn_apply_alg(const bt_pz_nxn *nxn, bt_pz_nxn_state *state, const void *alg,
                         bt_pz_error *err) {
    BT_TRACE_ENTER("bt_pz_nxn_apply_alg");
    (void)err;
    bt_pz_engine eng = bt_pz_nxn_engine(nxn);
    size_t len = bt_pz_alg_len(alg);
    for (size_t i = 0; i < len; i++) {
        const bt_pz_nxn_move *mv = bt_pz_alg_at(&eng, alg, i);
        nxn_apply_move(nxn, state, *mv);
    }
    BT_TRACE_EXIT("bt_pz_nxn_apply_alg");
    return true;
}

void bt_pz_nxn_solved_state(const bt_pz_nxn *nxn, bt_pz_nxn_state *out) {
    BT_TRACE_ENTER("bt_pz_nxn_solved_state");
    const size_t face_cells = (size_t)BT_PZ_NXN_STRIDE * (size_t)BT_PZ_NXN_STRIDE;
    for (size_t face = 0; face < 6; face++) {
        for (size_t i = 0; i < face_cells; i++) {
            out->facelet[face * face_cells + i] = (uint8_t)face;
        }
    }
    out->n = nxn->n;
    BT_TRACE_EXIT("bt_pz_nxn_solved_state");
}

bool bt_pz_nxn_is_solved(const bt_pz_nxn *nxn, const bt_pz_nxn_state *state) {
    BT_TRACE_ENTER("bt_pz_nxn_is_solved");
    for (size_t face = 0; face < 6; face++) {
        for (size_t row = 0; row < nxn->n; row++) {
            for (size_t col = 0; col < nxn->n; col++) {
                if (state->facelet[nxn_index(face, row, col)] != (uint8_t)face) {
                    BT_TRACE_EXIT("bt_pz_nxn_is_solved");
                    return false;
                }
            }
        }
    }
    BT_TRACE_EXIT("bt_pz_nxn_is_solved");
    return true;
}

void bt_pz_nxn_facelets(const bt_pz_nxn *nxn, const bt_pz_nxn_state *state, bt_pz_grid *out) {
    BT_TRACE_ENTER("bt_pz_nxn_facelets");
    out->face_count = 6;
    for (size_t face = 0; face < 6; face++) {
        out->faces[face].row_count = nxn->n;
        for (size_t row = 0; row < nxn->n; row++) {
            out->faces[face].rows[row].cols = nxn->n;
            for (size_t col = 0; col < nxn->n; col++) {
                out->faces[face].rows[row].cells[col] = state->facelet[nxn_index(face, row, col)];
            }
        }
    }
    BT_TRACE_EXIT("bt_pz_nxn_facelets");
}

static const bt_pz_nxn *nxn_vt_self(const bt_pz_engine *eng) {
    const bt_pz_nxn *nxn = eng->self;
    if (nxn->n < BT_PZ_NXN_MIN_N || nxn->n > BT_PZ_NXN_MAX_N) {
        BT_TRACE_FAIL("bt_pz_nxn_engine", "n out of range: %zu", nxn->n);
        bt_harness_fail(__FILE__, __LINE__, "n out of range: %zu", nxn->n);
        bt_harness_exit_failure();
    }
    return nxn;
}

static bool nxn_vt_parse_move(const bt_pz_engine *eng, const char *token, size_t len,
                              void *out_move, bt_pz_error *err) {
    return bt_pz_nxn_parse_move(nxn_vt_self(eng), token, len, out_move, err);
}

static bool nxn_vt_parse_alg(const bt_pz_engine *eng, const char *text, size_t len, void *alg,
                             bt_pz_error *err) {
    return bt_pz_nxn_parse_alg(nxn_vt_self(eng), text, len, alg, err);
}

static void nxn_vt_format_alg(const bt_pz_engine *eng, const void *alg, bt_strbuf *out) {
    bt_pz_nxn_format_alg(nxn_vt_self(eng), alg, out);
}

static void nxn_vt_invert_alg(const bt_pz_engine *eng, const void *alg, void *out) {
    bt_pz_nxn_invert_alg(nxn_vt_self(eng), alg, out);
}

static bool nxn_vt_apply_alg(const bt_pz_engine *eng, void *state, const void *alg,
                             bt_pz_error *err) {
    return bt_pz_nxn_apply_alg(nxn_vt_self(eng), state, alg, err);
}

static void nxn_vt_solved_state(const bt_pz_engine *eng, void *out_state) {
    bt_pz_nxn_solved_state(nxn_vt_self(eng), out_state);
}

static bool nxn_vt_is_solved(const bt_pz_engine *eng, const void *state) {
    return bt_pz_nxn_is_solved(nxn_vt_self(eng), state);
}

static void nxn_vt_facelets(const bt_pz_engine *eng, const void *state, bt_pz_grid *out) {
    bt_pz_nxn_facelets(nxn_vt_self(eng), state, out);
}

bt_pz_engine bt_pz_nxn_engine(const bt_pz_nxn *nxn) {
    BT_TRACE_ENTER("bt_pz_nxn_engine");
    bt_pz_engine eng = {
        .name = "nxn",
        .self = nxn,
        .move_size = sizeof(bt_pz_nxn_move),
        .state_size = sizeof(bt_pz_nxn_state),
        .parse_move = nxn_vt_parse_move,
        .parse_alg = nxn_vt_parse_alg,
        .format_alg = nxn_vt_format_alg,
        .invert_alg = nxn_vt_invert_alg,
        .apply_alg = nxn_vt_apply_alg,
        .solved_state = nxn_vt_solved_state,
        .is_solved = nxn_vt_is_solved,
        .facelets = nxn_vt_facelets,
    };
    BT_TRACE_EXIT("bt_pz_nxn_engine");
    return eng;
}
