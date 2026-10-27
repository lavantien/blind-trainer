#include "puzzles/fto.h"

#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"

const char *const BT_FTO_FACE_NAMES[BT_FTO_FACE_COUNT] = {
    "U", "F", "R", "BR", "B", "BL", "L", "D",
};

static const int8_t FACE_SIGNS[8][3] = {
    {1, 1, 1},
    {1, 1, -1},
    {-1, 1, -1},
    {-1, 1, 1},
    {-1, -1, 1},
    {1, -1, 1},
    {1, -1, -1},
    {-1, -1, -1},
};

static const size_t EDGE_LOCALS[3][2] = {{0, 1}, {0, 2}, {1, 2}};

static void vertex(size_t face, size_t local, int8_t out[3]) {
    out[0] = 0;
    out[1] = 0;
    out[2] = 0;
    out[local] = FACE_SIGNS[face][local];
}

static size_t vertex_id(const int8_t point[3]) {
    size_t axis = 0;
    while (point[axis] == 0) {
        axis++;
    }
    return axis * 2 + (point[axis] < 0 ? 1 : 0);
}

static uint8_t face_mask(size_t face) {
    uint8_t mask = 0;
    for (size_t local = 0; local < 3; local++) {
        int8_t point[3];
        vertex(face, local, point);
        mask |= (uint8_t)(1u << vertex_id(point));
    }
    return mask;
}

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

static size_t corner_local(size_t face, size_t corner) {
    for (size_t local = 0; local < 3; local++) {
        int8_t point[3];
        vertex(face, local, point);
        if (vertex_id(point) == corner) {
            return local;
        }
    }
    BT_TRACE_FAIL("corner_local", "corner %zu not on face %zu", corner, face);
    bt_trace_dump();
    abort();
}

static size_t edge_local(size_t face, size_t edge[2]) {
    for (size_t offset = 0; offset < 3; offset++) {
        int8_t pa[3];
        int8_t pb[3];
        vertex(face, EDGE_LOCALS[offset][0], pa);
        vertex(face, EDGE_LOCALS[offset][1], pb);
        size_t want[2] = {vertex_id(pa), vertex_id(pb)};
        if ((want[0] == edge[0] && want[1] == edge[1]) ||
            (want[1] == edge[0] && want[0] == edge[1])) {
            return offset;
        }
    }
    BT_TRACE_FAIL("edge_local", "edge %zu %zu not on face %zu", edge[0], edge[1], face);
    bt_trace_dump();
    abort();
}

static size_t shared_vertices(size_t a, size_t b) {
    size_t count = 0;
    for (size_t axis = 0; axis < 3; axis++) {
        if (FACE_SIGNS[a][axis] == FACE_SIGNS[b][axis]) {
            count++;
        }
    }
    return count;
}

bt_fto bt_fto_new(void) {
    BT_TRACE_ENTER("bt_fto_new");
    bt_fto fto;
    for (size_t face = 0; face < 8; face++) {
        const int8_t *signs = FACE_SIGNS[face];
        int8_t anchor[3];
        vertex(face, 0, anchor);
        int8_t rotated[3];
        rotate_point(signs, 1, anchor, rotated);
        int8_t product[3];
        cross3(anchor, rotated, product);
        uint8_t quarter = dot3(product, signs) < 0 ? 1 : 2;
        size_t corner_map[3];
        for (size_t local = 0; local < 3; local++) {
            int8_t point[3];
            vertex(face, local, point);
            rotate_point(signs, quarter, point, rotated);
            corner_map[local] = corner_local(face, vertex_id(rotated));
        }
        size_t face_map[8];
        for (size_t other = 0; other < 8; other++) {
            uint8_t mask = 0;
            for (size_t local = 0; local < 3; local++) {
                int8_t point[3];
                vertex(other, local, point);
                rotate_point(signs, quarter, point, rotated);
                mask |= (uint8_t)(1u << vertex_id(rotated));
            }
            size_t candidate = 8;
            for (size_t scan = 0; scan < 8; scan++) {
                if (face_mask(scan) == mask) {
                    candidate = scan;
                    break;
                }
            }
            face_map[other] = candidate;
        }
        uint8_t table[BT_FTO_FACELET_COUNT];
        for (size_t i = 0; i < BT_FTO_FACELET_COUNT; i++) {
            table[i] = (uint8_t)i;
        }
        for (size_t local = 0; local < 3; local++) {
            size_t landing = corner_map[local];
            table[face * 9 + landing] = (uint8_t)(face * 9 + local);
            table[face * 9 + 6 + landing] = (uint8_t)(face * 9 + 6 + local);
        }
        for (size_t offset = 0; offset < 3; offset++) {
            size_t a = EDGE_LOCALS[offset][0];
            size_t b = EDGE_LOCALS[offset][1];
            int8_t pa[3];
            int8_t pb[3];
            vertex(face, a, pa);
            vertex(face, b, pb);
            rotate_point(signs, quarter, pa, rotated);
            size_t image[2] = {vertex_id(rotated), 0};
            int8_t rotated_b[3];
            rotate_point(signs, quarter, pb, rotated_b);
            image[1] = vertex_id(rotated_b);
            size_t landing = edge_local(face, image);
            table[face * 9 + 3 + landing] = (uint8_t)(face * 9 + 3 + offset);
        }
        for (size_t local = 0; local < 3; local++) {
            int8_t point[3];
            vertex(face, local, point);
            size_t home = vertex_id(point);
            rotate_point(signs, quarter, point, rotated);
            size_t away = vertex_id(rotated);
            for (size_t other = 0; other < 8; other++) {
                if (other == face || (face_mask(other) & (1u << home)) == 0) {
                    continue;
                }
                size_t target = face_map[other];
                size_t from = other * 9 + corner_local(other, home);
                size_t to = target * 9 + corner_local(target, away);
                table[to] = (uint8_t)from;
            }
        }
        for (size_t offset = 0; offset < 3; offset++) {
            size_t a = EDGE_LOCALS[offset][0];
            size_t b = EDGE_LOCALS[offset][1];
            int8_t pa[3];
            int8_t pb[3];
            vertex(face, a, pa);
            vertex(face, b, pb);
            size_t home[2] = {vertex_id(pa), vertex_id(pb)};
            rotate_point(signs, quarter, pa, rotated);
            int8_t rotated_b[3];
            rotate_point(signs, quarter, pb, rotated_b);
            size_t away[2] = {vertex_id(rotated), vertex_id(rotated_b)};
            size_t other = 8;
            for (size_t candidate = 0; candidate < 8; candidate++) {
                if (candidate == face || (face_mask(candidate) & (1u << home[0])) == 0 ||
                    (face_mask(candidate) & (1u << home[1])) == 0) {
                    continue;
                }
                other = candidate;
                break;
            }
            size_t target = face_map[other];
            size_t from = other * 9 + 3 + edge_local(other, home);
            size_t to = target * 9 + 3 + edge_local(target, away);
            table[to] = (uint8_t)from;
        }
        memcpy(fto.turns[face * 2], table, BT_FTO_FACELET_COUNT);
        for (size_t i = 0; i < BT_FTO_FACELET_COUNT; i++) {
            fto.turns[face * 2 + 1][i] = table[table[i]];
        }
    }
    BT_TRACE_EXIT("bt_fto_new");
    return fto;
}

void bt_fto_turn_table(const bt_fto *fto, uint8_t face, uint8_t quarter,
                       uint8_t out[BT_FTO_FACELET_COUNT]) {
    BT_TRACE_ENTER("bt_fto_turn_table");
    memcpy(out, fto->turns[(size_t)face * 2 + quarter - 1], BT_FTO_FACELET_COUNT);
    BT_TRACE_EXIT("bt_fto_turn_table");
}

void bt_fto_neighbors(const bt_fto *fto, uint8_t face, uint8_t out[3]) {
    BT_TRACE_ENTER("bt_fto_neighbors");
    (void)fto;
    size_t count = 0;
    for (uint8_t other = 0; other < 8; other++) {
        if (other != face && shared_vertices(face, other) == 2) {
            out[count] = other;
            count++;
        }
    }
    BT_TRACE_EXIT("bt_fto_neighbors");
}

uint8_t bt_fto_opposite(const bt_fto *fto, uint8_t face) {
    BT_TRACE_ENTER("bt_fto_opposite");
    (void)fto;
    uint8_t found = face;
    for (uint8_t other = 0; other < 8; other++) {
        if (shared_vertices(face, other) == 0) {
            found = other;
            break;
        }
    }
    BT_TRACE_EXIT("bt_fto_opposite");
    return found;
}

bool bt_fto_move_eq(bt_fto_move a, bt_fto_move b) {
    return a.face == b.face && a.quarter == b.quarter;
}

bool bt_fto_state_eq(const bt_fto_state *a, const bt_fto_state *b) {
    return memcmp(a->facelets, b->facelets, BT_FTO_FACELET_COUNT) == 0;
}

static bool token_rest_quarter(const char *rest, size_t rest_len, uint8_t *quarter) {
    if (rest_len == 0 || (rest_len == 2 && rest[0] == '2' && rest[1] == '\'')) {
        *quarter = 1;
        return true;
    }
    if (rest_len == 1 && (rest[0] == '\'' || rest[0] == '2')) {
        *quarter = 2;
        return true;
    }
    return false;
}

bool bt_fto_parse_move(const char *token, size_t len, bt_fto_move *out, bt_pz_error *err) {
    BT_TRACE_ENTER("bt_fto_parse_move");
    for (size_t face = 0; face < 8; face++) {
        size_t plen = strlen(BT_FTO_FACE_NAMES[face]);
        if (len < plen || strncmp(token, BT_FTO_FACE_NAMES[face], plen) != 0) {
            continue;
        }
        uint8_t quarter;
        if (!token_rest_quarter(token + plen, len - plen, &quarter)) {
            continue;
        }
        out->face = (uint8_t)face;
        out->quarter = quarter;
        BT_TRACE_EXIT("bt_fto_parse_move");
        return true;
    }
    bt_pz_error_set(err, BT_PZ_ERR_PARSE, token, len);
    BT_TRACE_FAIL("bt_fto_parse_move", "token rejected");
    BT_TRACE_EXIT("bt_fto_parse_move");
    return false;
}

bool bt_fto_parse_alg(const bt_fto *fto, const char *text, size_t len, void *alg,
                      bt_pz_error *err) {
    BT_TRACE_ENTER("bt_fto_parse_alg");
    bt_pz_engine eng = bt_fto_engine(fto);
    bool ok = bt_pz_alg_default_parse(&eng, text, len, alg, err);
    if (!ok) {
        BT_TRACE_FAIL("bt_fto_parse_alg", "alg rejected");
    }
    BT_TRACE_EXIT("bt_fto_parse_alg");
    return ok;
}

static void format_move(bt_fto_move mv, char out[5]) {
    const char *name = BT_FTO_FACE_NAMES[mv.face];
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

void bt_fto_format_alg(const bt_fto *fto, const void *alg, bt_strbuf *out) {
    BT_TRACE_ENTER("bt_fto_format_alg");
    (void)fto;
    bt_pz_engine eng = bt_fto_engine(fto);
    size_t len = bt_pz_alg_len(alg);
    for (size_t i = 0; i < len; i++) {
        if (i > 0) {
            bt_strbuf_append_str(out, " ");
        }
        bt_fto_move mv = *(const bt_fto_move *)bt_pz_alg_at(&eng, alg, i);
        char text[5];
        format_move(mv, text);
        bt_strbuf_append_str(out, text);
    }
    BT_TRACE_EXIT("bt_fto_format_alg");
}

void bt_fto_invert_alg(const bt_fto *fto, const void *alg, void *out) {
    BT_TRACE_ENTER("bt_fto_invert_alg");
    bt_pz_engine eng = bt_fto_engine(fto);
    bt_pz_alg *src = (bt_pz_alg *)alg;
    bt_pz_alg *dst = out;
    dst->len = 0;
    for (size_t i = src->len; i > 0; i--) {
        bt_fto_move mv = *(const bt_fto_move *)bt_pz_alg_at(&eng, src, i - 1);
        mv.quarter = (uint8_t)(3 - mv.quarter);
        bt_pz_alg_push(&eng, dst, &mv);
    }
    BT_TRACE_EXIT("bt_fto_invert_alg");
}

static const uint8_t *table_for(const bt_fto *fto, bt_fto_move mv, bt_pz_error *err) {
    if (mv.face >= 8 || mv.quarter == 0 || mv.quarter > 2) {
        char text[5];
        format_move(mv, text);
        bt_pz_error_set(err, BT_PZ_ERR_APPLY, text, strlen(text));
        return NULL;
    }
    return fto->turns[(size_t)mv.face * 2 + mv.quarter - 1];
}

bool bt_fto_apply_alg(const bt_fto *fto, bt_fto_state *state, const void *alg, bt_pz_error *err) {
    BT_TRACE_ENTER("bt_fto_apply_alg");
    bt_pz_engine eng = bt_fto_engine(fto);
    size_t len = bt_pz_alg_len(alg);
    for (size_t i = 0; i < len; i++) {
        bt_fto_move mv = *(const bt_fto_move *)bt_pz_alg_at(&eng, alg, i);
        const uint8_t *table = table_for(fto, mv, err);
        if (table == NULL) {
            BT_TRACE_FAIL("bt_fto_apply_alg", "move rejected");
            BT_TRACE_EXIT("bt_fto_apply_alg");
            return false;
        }
        uint8_t before[BT_FTO_FACELET_COUNT];
        memcpy(before, state->facelets, BT_FTO_FACELET_COUNT);
        for (size_t slot = 0; slot < BT_FTO_FACELET_COUNT; slot++) {
            state->facelets[slot] = before[table[slot]];
        }
    }
    BT_TRACE_EXIT("bt_fto_apply_alg");
    return true;
}

void bt_fto_solved_state(const bt_fto *fto, bt_fto_state *out) {
    BT_TRACE_ENTER("bt_fto_solved_state");
    (void)fto;
    for (size_t i = 0; i < BT_FTO_FACELET_COUNT; i++) {
        out->facelets[i] = (uint8_t)(i / 9);
    }
    BT_TRACE_EXIT("bt_fto_solved_state");
}

bool bt_fto_is_solved(const bt_fto *fto, const bt_fto_state *state) {
    BT_TRACE_ENTER("bt_fto_is_solved");
    (void)fto;
    bool solved = true;
    for (size_t i = 0; i < BT_FTO_FACELET_COUNT; i++) {
        solved = solved && state->facelets[i] == (uint8_t)(i / 9);
    }
    BT_TRACE_EXIT("bt_fto_is_solved");
    return solved;
}

void bt_fto_facelets(const bt_fto *fto, const bt_fto_state *state, bt_pz_grid *out) {
    BT_TRACE_ENTER("bt_fto_facelets");
    (void)fto;
    out->face_count = 8;
    static const size_t ROW_SLOTS[3][5] = {
        {0, 0, 0, 0, 0},
        {3, 6, 4, 0, 0},
        {1, 7, 5, 8, 2},
    };
    static const size_t ROW_WIDTHS[3] = {1, 3, 5};
    for (size_t face = 0; face < 8; face++) {
        bt_pz_face *view = &out->faces[face];
        view->row_count = 3;
        for (size_t row = 0; row < 3; row++) {
            view->rows[row].cols = ROW_WIDTHS[row];
            for (size_t col = 0; col < ROW_WIDTHS[row]; col++) {
                view->rows[row].cells[col] = state->facelets[face * 9 + ROW_SLOTS[row][col]];
            }
        }
    }
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
