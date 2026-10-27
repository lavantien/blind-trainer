#include "scheme/classifier.h"

#include "internal.h"

#include <stdio.h>
#include <string.h>

bool bt_scheme_is_style_block(const uint8_t *block, size_t len) {
    BT_TRACE_ENTER("scheme.is_style_block");
    bool style = (len == 4 && !bt_scheme_same_piece(block[0], block[1]) &&
                  !bt_scheme_same_piece(block[0], block[2]) &&
                  !bt_scheme_same_piece(block[1], block[2])) != 0;
    BT_TRACE_EXIT("scheme.is_style_block");
    return style;
}

bool bt_scheme_is_flip_block(const uint8_t *block, size_t len) {
    BT_TRACE_ENTER("scheme.is_flip_block");
    bool flip = (len == 3 && bt_scheme_same_piece(block[0], block[1])) != 0;
    BT_TRACE_EXIT("scheme.is_flip_block");
    return flip;
}

bool bt_scheme_is_swap_block(const uint8_t *block, size_t len) {
    BT_TRACE_ENTER("scheme.is_swap_block");
    bool swap = (len == 3 && !bt_scheme_same_piece(block[0], block[1])) != 0;
    BT_TRACE_EXIT("scheme.is_swap_block");
    return swap;
}

bool bt_scheme_is_twist_block(const uint8_t *block, size_t len) {
    BT_TRACE_ENTER("scheme.is_twist_block");
    bool twist =
        (len == 4 && bt_scheme_same_piece(block[0], block[1]) &&
         bt_scheme_same_piece(block[0], block[2]) && bt_scheme_same_piece(block[1], block[2])) != 0;
    BT_TRACE_EXIT("scheme.is_twist_block");
    return twist;
}

bool bt_scheme_is_swap_twist_block(const uint8_t *block, size_t len) {
    BT_TRACE_ENTER("scheme.is_swap_twist_block");
    bool swap_twist = len == 7;
    for (size_t i = 0; i < 3 && swap_twist; i++) {
        if (!bt_scheme_same_piece(block[2 * i], block[0]) ||
            !bt_scheme_same_piece(block[2 * i + 1], block[1])) {
            swap_twist = false;
        }
    }
    if (swap_twist && bt_scheme_same_piece(block[0], block[1])) {
        swap_twist = false;
    }
    BT_TRACE_EXIT("scheme.is_swap_twist_block");
    return swap_twist;
}

bool bt_scheme_trace_is_solved(const bt_trace *trace) {
    BT_TRACE_ENTER("scheme.trace_is_solved");
    bool solved = trace->block_count == 0;
    BT_TRACE_EXIT("scheme.trace_is_solved");
    return solved;
}

size_t bt_scheme_trace_cases(const bt_trace *trace, bt_case out[BT_TRACE_BLOCKS_MAX]) {
    BT_TRACE_ENTER("scheme.trace_cases");
    size_t count = 0;
    for (size_t i = 0; i < trace->block_count; i++) {
        const uint8_t *block = trace->blocks[i];
        if (bt_scheme_is_style_block(block, trace->block_len[i])) {
            out[count].orbit = trace->orbit;
            out[count].t1 = block[2];
            out[count].t2 = block[1];
            count++;
        }
    }
    BT_TRACE_EXIT("scheme.trace_cases");
    return count;
}

static void require_cube_shaped(const uint8_t occ[54], const char *unit) {
    bt_cube probe;
    if (!bt_cube_try_from_occ(occ, &probe)) {
        BT_SCHEME_ABORT(unit, "state must be a permutation of 0..54 with fixed orbit bands");
    }
}

void bt_scheme_trace(const uint8_t occ[54], bt_orbit orbit, bt_trace *out) {
    BT_TRACE_ENTER("scheme.trace");
    require_cube_shaped(occ, "scheme.trace");
    const char *const *order =
        orbit == BT_ORBIT_EDGE ? BT_SCHEME_EDGE_FLOATING : BT_SCHEME_CORNER_FLOATING;
    size_t order_count = orbit == BT_ORBIT_EDGE ? 12 : 8;
    bool visited[48] = {false};
    memset(out, 0, sizeof *out);
    out->orbit = orbit;
    for (size_t p = 0; p < order_count; p++) {
        uint8_t head = bt_scheme_head_sticker(order[p]);
        bool skip = false;
        for (uint8_t s = bt_scheme_orbit_lo(orbit); s < bt_scheme_orbit_hi(orbit); s++) {
            if (bt_scheme_same_piece(s, head) && visited[s]) {
                skip = true;
                break;
            }
        }
        if (skip) {
            continue;
        }
        uint8_t block[BT_TRACE_BLOCK_MAX];
        size_t len = 1;
        block[0] = head;
        for (;;) {
            uint8_t target = occ[block[len - 1]];
            if (target == head) {
                break;
            }
            if (len >= BT_TRACE_BLOCK_MAX) {
                BT_SCHEME_ABORT("scheme.trace", "trace block budget exceeded at sticker %u",
                                block[len - 1]);
            }
            block[len++] = target;
        }
        if (len > 1) {
            if (len >= BT_TRACE_BLOCK_MAX) {
                BT_SCHEME_ABORT("scheme.trace", "trace block budget exceeded at sticker %u",
                                block[len - 1]);
            }
            block[len++] = head;
            for (size_t i = 0; i < len; i++) {
                visited[block[i]] = true;
            }
            if (out->block_count >= BT_TRACE_BLOCKS_MAX) {
                BT_SCHEME_ABORT("scheme.trace", "trace block count budget exceeded");
            }
            memcpy(out->blocks[out->block_count], block, len);
            out->block_len[out->block_count] = len;
            out->block_count++;
        }
    }
    BT_TRACE_EXIT("scheme.trace");
}

static void shape_sig(const bt_trace *trace, char out[BT_CLASS_TEXT_MAX]) {
    if (trace->block_count == 0) {
        snprintf(out, BT_CLASS_TEXT_MAX, "solved");
        return;
    }
    size_t at = 0;
    out[at++] = '[';
    for (size_t i = 0; i < trace->block_count; i++) {
        if (i > 0) {
            out[at++] = ',';
            out[at++] = ' ';
        }
        at += (size_t)snprintf(out + at, BT_CLASS_TEXT_MAX - at, "%zu", trace->block_len[i]);
    }
    out[at++] = ']';
    out[at] = '\0';
}

static void solo_style(const bt_trace *active, const bt_trace *other, bt_orbit orbit,
                       bt_classification *out) {
    if (!bt_scheme_trace_is_solved(other) || active->block_count != 1 ||
        !bt_scheme_is_style_block(active->blocks[0], active->block_len[0])) {
        return;
    }
    out->kind = BT_CLASS_THREE_STYLE;
    out->orbit = orbit;
    out->case_count = bt_scheme_trace_cases(active, out->cases);
}

static void classify_traces(const bt_trace *edge, const bt_trace *corner, bt_classification *out) {
    memset(out, 0, sizeof *out);
    if (bt_scheme_trace_is_solved(edge) && bt_scheme_trace_is_solved(corner)) {
        out->kind = BT_CLASS_SOLVED;
        return;
    }
    solo_style(edge, corner, BT_ORBIT_EDGE, out);
    if (out->kind == BT_CLASS_THREE_STYLE) {
        return;
    }
    solo_style(corner, edge, BT_ORBIT_CORNER, out);
    if (out->kind == BT_CLASS_THREE_STYLE) {
        return;
    }
    if (bt_scheme_trace_is_solved(corner) && edge->block_count == 2 &&
        bt_scheme_is_flip_block(edge->blocks[0], edge->block_len[0]) &&
        bt_scheme_is_flip_block(edge->blocks[1], edge->block_len[1])) {
        out->kind = BT_CLASS_FLIP_PAIR;
        out->first = edge->blocks[0][0];
        out->second = edge->blocks[1][0];
        return;
    }
    if (bt_scheme_trace_is_solved(edge) && corner->block_count == 2 &&
        bt_scheme_is_twist_block(corner->blocks[0], corner->block_len[0]) &&
        bt_scheme_is_twist_block(corner->blocks[1], corner->block_len[1])) {
        out->kind = BT_CLASS_TWIST_PAIR;
        out->first = corner->blocks[0][0];
        out->second = corner->blocks[1][0];
        return;
    }
    if (edge->block_count == 1 && bt_scheme_is_swap_block(edge->blocks[0], edge->block_len[0])) {
        if (corner->block_count == 1 &&
            bt_scheme_is_swap_block(corner->blocks[0], corner->block_len[0])) {
            out->kind = BT_CLASS_PARITY;
            out->e1 = edge->blocks[0][0];
            out->e2 = edge->blocks[0][1];
            out->c1 = corner->blocks[0][0];
            out->c2 = corner->blocks[0][1];
            return;
        }
        if (corner->block_count == 2) {
            const uint8_t *seven = NULL;
            const uint8_t *four = NULL;
            if (bt_scheme_is_swap_twist_block(corner->blocks[0], corner->block_len[0]) &&
                bt_scheme_is_twist_block(corner->blocks[1], corner->block_len[1])) {
                seven = corner->blocks[0];
                four = corner->blocks[1];
            } else if (bt_scheme_is_twist_block(corner->blocks[0], corner->block_len[0]) &&
                       bt_scheme_is_swap_twist_block(corner->blocks[1], corner->block_len[1])) {
                seven = corner->blocks[1];
                four = corner->blocks[0];
            }
            if (seven != NULL) {
                out->kind = BT_CLASS_LTCT;
                out->e1 = edge->blocks[0][0];
                out->e2 = edge->blocks[0][1];
                out->c1 = seven[0];
                out->c2 = seven[5];
                out->bracket = four[1];
                return;
            }
        }
    }
    out->kind = BT_CLASS_UNCLASSIFIED;
    char edge_sig[BT_CLASS_TEXT_MAX];
    char corner_sig[BT_CLASS_TEXT_MAX];
    shape_sig(edge, edge_sig);
    shape_sig(corner, corner_sig);
    snprintf(out->unclassified, BT_CLASS_TEXT_MAX, "edge:%s corner:%s", edge_sig, corner_sig);
}

static void defects_of(const uint8_t occ[54], bt_defects *out) {
    bt_cube cube = bt_cube_from_occ(occ);
    bt_state_summary summary = bt_summarize(&cube);
    for (uint8_t slot = 0; slot < 12; slot++) {
        bt_piece piece = {BT_PIECE_EDGE, summary.edges[slot].piece};
        out->edge_flips[slot].head = bt_head_sticker(piece);
        out->edge_flips[slot].flipped = summary.edges[slot].flipped;
    }
    for (uint8_t slot = 0; slot < 8; slot++) {
        bt_piece piece = {BT_PIECE_CORNER, summary.corners[slot].piece};
        out->corner_twists[slot].head = bt_head_sticker(piece);
        out->corner_twists[slot].twist = summary.corners[slot].twist;
    }
}

void bt_scheme_classify(const uint8_t occ[54], bt_report *out) {
    BT_TRACE_ENTER("scheme.classify");
    require_cube_shaped(occ, "scheme.classify");
    memset(out, 0, sizeof *out);
    bt_scheme_trace(occ, BT_ORBIT_EDGE, &out->edge);
    bt_scheme_trace(occ, BT_ORBIT_CORNER, &out->corner);
    classify_traces(&out->edge, &out->corner, &out->class);
    defects_of(occ, &out->defects);
    BT_TRACE_EXIT("scheme.classify");
}

void bt_scheme_classify_alg(const bt_alg *alg, bt_report *out) {
    BT_TRACE_ENTER("scheme.classify_alg");
    bt_cube cube = bt_cube_solved();
    bt_cube_apply_alg(&cube, alg);
    uint8_t occ[54];
    bt_scheme_state_array(&cube, occ);
    bt_scheme_classify(occ, out);
    out->has_qtm = true;
    out->qtm = bt_alg_qtm(alg);
    BT_TRACE_EXIT("scheme.classify_alg");
}

bool bt_scheme_report_position(const bt_report *report, char out[BT_CLASS_POSITION_MAX]) {
    BT_TRACE_ENTER("scheme.report_position");
    const bt_classification *class = &report->class;
    if (class->kind == BT_CLASS_SOLVED || class->kind == BT_CLASS_UNCLASSIFIED) {
        BT_TRACE_EXIT("scheme.report_position");
        return false;
    }
    if (class->kind == BT_CLASS_THREE_STYLE) {
        const bt_trace *trace = class->orbit == BT_ORBIT_EDGE ? &report->edge : &report->corner;
        size_t at = 0;
        for (size_t i = 0; i < trace->block_count; i++) {
            const uint8_t *block = trace->blocks[i];
            if (!bt_scheme_is_style_block(block, trace->block_len[i])) {
                continue;
            }
            if (at > 0) {
                out[at++] = ' ';
            }
            at += (size_t)snprintf(out + at, BT_CLASS_POSITION_MAX - at, "%s-%s-%s",
                                   bt_scheme_name_of(block[0]), bt_scheme_name_of(block[2]),
                                   bt_scheme_name_of(block[1]));
        }
        out[at] = '\0';
        BT_TRACE_EXIT("scheme.report_position");
        return true;
    }
    if (class->kind == BT_CLASS_FLIP_PAIR) {
        snprintf(out, BT_CLASS_POSITION_MAX, "%s & %s", bt_scheme_name_of(class->first),
                 bt_scheme_name_of(class->second));
        BT_TRACE_EXIT("scheme.report_position");
        return true;
    }
    if (class->kind == BT_CLASS_TWIST_PAIR) {
        int twist_first = (int)bt_scheme_defect_twist(&report->defects, class->first);
        int twist_second = (int)bt_scheme_defect_twist(&report->defects, class->second);
        twist_first = ((twist_first % 3) + 3) % 3;
        twist_second = ((twist_second % 3) + 3) % 3;
        bool first_is_plus = !(twist_second == 1 && twist_first != 1) != 0;
        uint8_t a = first_is_plus ? class->first : class->second;
        uint8_t b = first_is_plus ? class->second : class->first;
        snprintf(out, BT_CLASS_POSITION_MAX, "%s & %s", bt_scheme_name_of(a), bt_scheme_name_of(b));
        BT_TRACE_EXIT("scheme.report_position");
        return true;
    }
    if (class->kind == BT_CLASS_PARITY) {
        snprintf(out, BT_CLASS_POSITION_MAX, "%s-%s, %s-%s", bt_scheme_name_of(class->e1),
                 bt_scheme_name_of(class->e2), bt_scheme_name_of(class->c1),
                 bt_scheme_name_of(class->c2));
        BT_TRACE_EXIT("scheme.report_position");
        return true;
    }
    snprintf(out, BT_CLASS_POSITION_MAX, "%s-%s, %s-%s[%s]", bt_scheme_name_of(class->e1),
             bt_scheme_name_of(class->e2), bt_scheme_name_of(class->c1),
             bt_scheme_name_of(class->c2), bt_scheme_name_of(class->bracket));
    BT_TRACE_EXIT("scheme.report_position");
    return true;
}

bool bt_scheme_defect_flip(const bt_defects *defects, uint8_t piece) {
    BT_TRACE_ENTER("scheme.defect_flip");
    for (uint8_t i = 0; i < 12; i++) {
        if (defects->edge_flips[i].head == piece) {
            bool flipped = defects->edge_flips[i].flipped;
            BT_TRACE_EXIT("scheme.defect_flip");
            return flipped;
        }
    }
    BT_SCHEME_ABORT("scheme.defect_flip", "not an edge head: %u", piece);
    return false;
}

int8_t bt_scheme_defect_twist(const bt_defects *defects, uint8_t piece) {
    BT_TRACE_ENTER("scheme.defect_twist");
    for (uint8_t i = 0; i < 8; i++) {
        if (defects->corner_twists[i].head == piece) {
            int8_t twist = defects->corner_twists[i].twist;
            BT_TRACE_EXIT("scheme.defect_twist");
            return twist;
        }
    }
    BT_SCHEME_ABORT("scheme.defect_twist", "not a corner head: %u", piece);
    return 0;
}

size_t bt_scheme_flipped_pieces(const bt_defects *defects, uint8_t out[12]) {
    BT_TRACE_ENTER("scheme.flipped_pieces");
    size_t count = 0;
    for (uint8_t i = 0; i < 12; i++) {
        if (defects->edge_flips[i].flipped) {
            out[count++] = defects->edge_flips[i].head;
        }
    }
    BT_TRACE_EXIT("scheme.flipped_pieces");
    return count;
}

size_t bt_scheme_twisted_pieces(const bt_defects *defects, bt_corner_twist_note out[8]) {
    BT_TRACE_ENTER("scheme.twisted_pieces");
    size_t count = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (defects->corner_twists[i].twist != 0) {
            out[count++] = defects->corner_twists[i];
        }
    }
    BT_TRACE_EXIT("scheme.twisted_pieces");
    return count;
}

bool bt_scheme_classification_eq(const bt_classification *a, const bt_classification *b) {
    BT_TRACE_ENTER("scheme.classification_eq");
    if (a->kind != b->kind) {
        BT_TRACE_EXIT("scheme.classification_eq");
        return false;
    }
    bool equal = true;
    if (a->kind == BT_CLASS_THREE_STYLE) {
        equal = (a->orbit == b->orbit && a->case_count == b->case_count) != 0;
        for (size_t i = 0; i < a->case_count; i++) {
            if (a->cases[i].orbit != b->cases[i].orbit || a->cases[i].t1 != b->cases[i].t1 ||
                a->cases[i].t2 != b->cases[i].t2) {
                equal = false;
            }
        }
    } else if (a->kind == BT_CLASS_FLIP_PAIR || a->kind == BT_CLASS_TWIST_PAIR) {
        equal = (a->first == b->first && a->second == b->second) != 0;
    } else if (a->kind == BT_CLASS_PARITY || a->kind == BT_CLASS_LTCT) {
        equal = (a->e1 == b->e1 && a->e2 == b->e2 && a->c1 == b->c1 && a->c2 == b->c2 &&
                 (a->kind != BT_CLASS_LTCT || a->bracket == b->bracket)) != 0;
    } else if (a->kind == BT_CLASS_UNCLASSIFIED) {
        equal = strcmp(a->unclassified, b->unclassified) == 0;
    }
    if (!equal) {
        BT_TRACE_EXIT("scheme.classification_eq");
        return false;
    }
    BT_TRACE_EXIT("scheme.classification_eq");
    return true;
}

bool bt_scheme_trace_eq(const bt_trace *a, const bt_trace *b) {
    BT_TRACE_ENTER("scheme.trace_eq");
    if (a->orbit != b->orbit || a->block_count != b->block_count) {
        BT_TRACE_EXIT("scheme.trace_eq");
        return false;
    }
    for (size_t i = 0; i < a->block_count; i++) {
        if (a->block_len[i] != b->block_len[i] ||
            memcmp(a->blocks[i], b->blocks[i], a->block_len[i]) != 0) {
            BT_TRACE_EXIT("scheme.trace_eq");
            return false;
        }
    }
    BT_TRACE_EXIT("scheme.trace_eq");
    return true;
}
