#include "scheme/memo.h"

#include "internal.h"
#include "scheme/scramble.h"

#include <string.h>

const uint8_t BT_SCHEME_CORNER_SUCC[8][3] = {
    {1, 13, 16},  {0, 17, 4},  {2, 9, 12},   {3, 5, 8},
    {22, 19, 14}, {23, 7, 18}, {21, 15, 10}, {20, 11, 6},
};

static uint8_t piece_index(uint8_t sticker) {
    bt_piece piece = bt_piece_of(sticker);
    return piece.index;
}

static uint8_t head_of(bt_orbit orbit, uint8_t piece) {
    bt_piece keyed = {orbit == BT_ORBIT_CORNER ? BT_PIECE_CORNER : BT_PIECE_EDGE, piece};
    return bt_head_sticker(keyed);
}

static void stickers_of(bt_orbit orbit, uint8_t piece, uint8_t *out, size_t *count) {
    if (orbit == BT_ORBIT_CORNER) {
        bt_corner_stickers(piece, out);
        *count = 3;
    } else {
        bt_edge_stickers(piece, out);
        *count = 2;
    }
}

static void floating_pieces(bt_orbit orbit, uint8_t out[12], size_t *count) {
    uint8_t heads[12];
    if (orbit == BT_ORBIT_CORNER) {
        bt_scheme_corner_floating_heads(heads);
        *count = 8;
    } else {
        bt_scheme_edge_floating_heads(heads);
        *count = 12;
    }
    for (size_t i = 0; i < *count; i++) {
        out[i] = piece_index(heads[i]);
    }
}

static bool solved_piece(const uint8_t occ[54], bt_orbit orbit, uint8_t piece) {
    uint8_t homes[3];
    size_t count;
    stickers_of(orbit, piece, homes, &count);
    for (size_t i = 0; i < count; i++) {
        if (occ[homes[i]] != homes[i]) {
            return false;
        }
    }
    return true;
}

static bool in_place(const uint8_t occ[54], bt_orbit orbit, uint8_t piece) {
    uint8_t homes[3];
    size_t count;
    stickers_of(orbit, piece, homes, &count);
    uint8_t occupants[3];
    uint8_t expected[3];
    for (size_t i = 0; i < count; i++) {
        occupants[i] = occ[homes[i]];
        expected[i] = homes[i];
    }
    for (size_t i = 1; i < count; i++) {
        for (size_t j = i; j > 0 && occupants[j - 1] > occupants[j]; j--) {
            uint8_t tmp = occupants[j - 1];
            occupants[j - 1] = occupants[j];
            occupants[j] = tmp;
        }
    }
    for (size_t i = 1; i < count; i++) {
        for (size_t j = i; j > 0 && expected[j - 1] > expected[j]; j--) {
            uint8_t tmp = expected[j - 1];
            expected[j - 1] = expected[j];
            expected[j] = tmp;
        }
    }
    for (size_t i = 0; i < count; i++) {
        if (occupants[i] != expected[i]) {
            return false;
        }
    }
    return true;
}

void bt_scheme_state_array(const bt_cube *cube, uint8_t out[54]) {
    BT_TRACE_ENTER("scheme.state_array");
    for (uint8_t i = 0; i < 54; i++) {
        out[i] = bt_cube_sticker_at(cube, i);
    }
    BT_TRACE_EXIT("scheme.state_array");
}

uint8_t bt_scheme_corner_succ(uint8_t pos) {
    BT_TRACE_ENTER("scheme.corner_succ");
    bt_piece piece = bt_piece_of(pos);
    if (piece.kind != BT_PIECE_CORNER) {
        BT_SCHEME_ABORT("scheme.corner_succ", "corner_succ on non corner sticker %u", pos);
    }
    const uint8_t *cyc = BT_SCHEME_CORNER_SUCC[piece.index];
    uint8_t at = 3;
    for (uint8_t i = 0; i < 3; i++) {
        if (cyc[i] == pos) {
            at = i;
        }
    }
    uint8_t next = cyc[(at + 1) % 3];
    BT_TRACE_EXIT("scheme.corner_succ");
    return next;
}

uint8_t bt_scheme_edge_mate(uint8_t pos) {
    BT_TRACE_ENTER("scheme.edge_mate");
    bt_piece piece = bt_piece_of(pos);
    if (piece.kind != BT_PIECE_EDGE) {
        BT_SCHEME_ABORT("scheme.edge_mate", "edge_mate on non edge sticker %u", pos);
    }
    uint8_t pair[2];
    bt_edge_stickers(piece.index, pair);
    uint8_t mate = pair[0] == pos ? pair[1] : pair[0];
    BT_TRACE_EXIT("scheme.edge_mate");
    return mate;
}

bool bt_scheme_edge_flipped(const uint8_t occ[54], uint8_t slot) {
    BT_TRACE_ENTER("scheme.edge_flipped");
    uint8_t pair[2];
    bt_edge_stickers(slot, pair);
    uint8_t occupant = piece_index(occ[pair[0]]);
    bool flipped = occ[pair[0]] != head_of(BT_ORBIT_EDGE, occupant);
    BT_TRACE_EXIT("scheme.edge_flipped");
    return flipped;
}

uint8_t bt_scheme_corner_twist_state(const uint8_t occ[54], uint8_t slot) {
    BT_TRACE_ENTER("scheme.corner_twist_state");
    const uint8_t *cyc = BT_SCHEME_CORNER_SUCC[slot];
    uint8_t ohead = head_of(BT_ORBIT_CORNER, piece_index(occ[cyc[0]]));
    uint8_t at = 3;
    for (uint8_t i = 0; i < 3; i++) {
        if (occ[cyc[i]] == ohead) {
            at = i;
        }
    }
    if (at == 3) {
        BT_SCHEME_ABORT("scheme.corner_twist_state", "twist head %u missing in slot %u", ohead,
                        slot);
    }
    BT_TRACE_EXIT("scheme.corner_twist_state");
    return at;
}

static void pair_cycles(bt_orbit orbit, uint8_t h, uint8_t t1, uint8_t t2, bt_scheme_cycles *out) {
    if (orbit == BT_ORBIT_EDGE) {
        out->count = 2;
        out->items[0][0] = h;
        out->items[0][1] = t1;
        out->items[0][2] = t2;
        out->items[1][0] = bt_scheme_edge_mate(h);
        out->items[1][1] = bt_scheme_edge_mate(t1);
        out->items[1][2] = bt_scheme_edge_mate(t2);
    } else {
        out->count = 3;
        uint8_t sh = h;
        uint8_t s1 = t1;
        uint8_t s2 = t2;
        for (uint8_t i = 0; i < 3; i++) {
            out->items[i][0] = sh;
            out->items[i][1] = s1;
            out->items[i][2] = s2;
            sh = bt_scheme_corner_succ(sh);
            s1 = bt_scheme_corner_succ(s1);
            s2 = bt_scheme_corner_succ(s2);
        }
    }
}

void bt_scheme_ideal_cycle(const bt_case *case_id, bt_scheme_cycles *out) {
    BT_TRACE_ENTER("scheme.ideal_cycle");
    uint8_t buffer = case_id->orbit == BT_ORBIT_CORNER ? BT_SCHEME_CORNER_BUFFER_STICKER
                                                       : BT_SCHEME_EDGE_BUFFER_STICKER;
    pair_cycles(case_id->orbit, buffer, case_id->t1, case_id->t2, out);
    BT_TRACE_EXIT("scheme.ideal_cycle");
}

void bt_scheme_ideal_cycle_at(bt_orbit orbit, uint8_t buffer, uint8_t t1, uint8_t t2,
                              bt_scheme_cycles *out) {
    BT_TRACE_ENTER("scheme.ideal_cycle_at");
    pair_cycles(orbit, buffer, t1, t2, out);
    BT_TRACE_EXIT("scheme.ideal_cycle_at");
}

void bt_scheme_apply_sticker_cycles(uint8_t occ[54], const bt_scheme_cycles *cycles) {
    BT_TRACE_ENTER("scheme.apply_sticker_cycles");
    for (uint8_t i = 0; i < cycles->count; i++) {
        uint8_t a = cycles->items[i][0];
        uint8_t b = cycles->items[i][1];
        uint8_t c = cycles->items[i][2];
        uint8_t sa = occ[a];
        uint8_t sb = occ[b];
        uint8_t sc = occ[c];
        occ[b] = sa;
        occ[c] = sb;
        occ[a] = sc;
    }
    BT_TRACE_EXIT("scheme.apply_sticker_cycles");
}

static void apply_swap(uint8_t occ[54], uint8_t a, uint8_t b) {
    uint8_t tmp = occ[a];
    occ[a] = occ[b];
    occ[b] = tmp;
}

void bt_scheme_apply_aligned_swap(uint8_t occ[54], bt_orbit orbit, uint8_t h1, uint8_t h2) {
    BT_TRACE_ENTER("scheme.apply_aligned_swap");
    if (orbit == BT_ORBIT_EDGE) {
        apply_swap(occ, h1, h2);
        apply_swap(occ, bt_scheme_edge_mate(h1), bt_scheme_edge_mate(h2));
    } else {
        uint8_t a = h1;
        uint8_t b = h2;
        for (uint8_t i = 0; i < 3; i++) {
            apply_swap(occ, a, b);
            a = bt_scheme_corner_succ(a);
            b = bt_scheme_corner_succ(b);
        }
    }
    BT_TRACE_EXIT("scheme.apply_aligned_swap");
}

static void apply_flip(uint8_t occ[54], uint8_t head) {
    apply_swap(occ, head, bt_scheme_edge_mate(head));
}

static void apply_twist(uint8_t occ[54], uint8_t head, uint8_t quarter) {
    const uint8_t *cyc = BT_SCHEME_CORNER_SUCC[piece_index(head)];
    uint8_t s0 = occ[cyc[0]];
    uint8_t s1 = occ[cyc[1]];
    uint8_t s2 = occ[cyc[2]];
    if (quarter == 1) {
        occ[cyc[1]] = s0;
        occ[cyc[2]] = s1;
        occ[cyc[0]] = s2;
    } else {
        occ[cyc[0]] = s1;
        occ[cyc[1]] = s2;
        occ[cyc[2]] = s0;
    }
}

void bt_scheme_apply_residue(uint8_t occ[54], const bt_residue *residue) {
    BT_TRACE_ENTER("scheme.apply_residue");
    if (residue->has_parity) {
        bt_scheme_apply_aligned_swap(occ, BT_ORBIT_EDGE, residue->parity.edge[0],
                                     residue->parity.edge[1]);
        bt_scheme_apply_aligned_swap(occ, BT_ORBIT_CORNER, residue->parity.corner[0],
                                     residue->parity.corner[1]);
    }
    for (uint8_t i = 0; i < residue->flip_count; i++) {
        apply_flip(occ, residue->flips[i]);
    }
    for (uint8_t i = 0; i < residue->twist_count; i++) {
        apply_twist(occ, residue->twists[i].corner, residue->twists[i].quarter);
    }
    BT_TRACE_EXIT("scheme.apply_residue");
}

void bt_scheme_apply_memo(uint8_t occ[54], const bt_memo *memo) {
    BT_TRACE_ENTER("scheme.apply_memo");
    for (size_t i = 0; i < memo->edge_pair_count; i++) {
        bt_scheme_cycles cycles;
        pair_cycles(memo->edge_pairs[i].orbit, memo->edge_buffers[i], memo->edge_pairs[i].t1,
                    memo->edge_pairs[i].t2, &cycles);
        bt_scheme_apply_sticker_cycles(occ, &cycles);
    }
    for (size_t i = 0; i < memo->corner_pair_count; i++) {
        bt_scheme_cycles cycles;
        pair_cycles(memo->corner_pairs[i].orbit, memo->corner_buffers[i], memo->corner_pairs[i].t1,
                    memo->corner_pairs[i].t2, &cycles);
        bt_scheme_apply_sticker_cycles(occ, &cycles);
    }
    bt_scheme_apply_residue(occ, &memo->residue);
    BT_TRACE_EXIT("scheme.apply_memo");
}

static void assert_case_legal(const bt_case *case_id) {
    uint8_t buffer = case_id->orbit == BT_ORBIT_CORNER ? BT_SCHEME_CORNER_BUFFER_STICKER
                                                       : BT_SCHEME_EDGE_BUFFER_STICKER;
    if (case_id->t1 < bt_scheme_orbit_lo(case_id->orbit) ||
        case_id->t1 >= bt_scheme_orbit_hi(case_id->orbit)) {
        BT_SCHEME_ABORT("memo.assert_case_legal", "t1 outside orbit: orbit %d t1 %u t2 %u",
                        (int)case_id->orbit, case_id->t1, case_id->t2);
    }
    if (case_id->t2 < bt_scheme_orbit_lo(case_id->orbit) ||
        case_id->t2 >= bt_scheme_orbit_hi(case_id->orbit)) {
        BT_SCHEME_ABORT("memo.assert_case_legal", "t2 outside orbit: orbit %d t1 %u t2 %u",
                        (int)case_id->orbit, case_id->t1, case_id->t2);
    }
    if (bt_scheme_same_piece(case_id->t1, buffer)) {
        BT_SCHEME_ABORT("memo.assert_case_legal", "t1 on buffer piece: orbit %d t1 %u t2 %u",
                        (int)case_id->orbit, case_id->t1, case_id->t2);
    }
    if (bt_scheme_same_piece(case_id->t2, buffer)) {
        BT_SCHEME_ABORT("memo.assert_case_legal", "t2 on buffer piece: orbit %d t1 %u t2 %u",
                        (int)case_id->orbit, case_id->t1, case_id->t2);
    }
    if (bt_scheme_same_piece(case_id->t1, case_id->t2)) {
        BT_SCHEME_ABORT("memo.assert_case_legal", "t2 on t1 piece: orbit %d t1 %u t2 %u",
                        (int)case_id->orbit, case_id->t1, case_id->t2);
    }
}

static bool scan_piece(bt_orbit_walk *walk, bool main_scan, uint8_t *found) {
    for (size_t i = 0; i < walk->piece_count; i++) {
        uint8_t p = walk->pieces[i];
        if (walk->reserved[p] || solved_piece(walk->occ, walk->orbit, p)) {
            continue;
        }
        if (in_place(walk->occ, walk->orbit, p)) {
            walk->reserved[p] = true;
            continue;
        }
        if (!main_scan) {
            uint8_t homes[3];
            size_t count;
            stickers_of(walk->orbit, p, homes, &count);
            bool touched = false;
            for (size_t j = 0; j < count; j++) {
                if (walk->visited[homes[j]]) {
                    touched = true;
                }
            }
            if (touched) {
                continue;
            }
        }
        *found = p;
        return true;
    }
    return false;
}

void bt_scheme_walk_init(bt_orbit_walk *walk, bt_orbit orbit, uint8_t occ[54]) {
    BT_TRACE_ENTER("scheme.walk_init");
    memset(walk, 0, sizeof *walk);
    walk->orbit = orbit;
    walk->occ = occ;
    floating_pieces(orbit, walk->pieces, &walk->piece_count);
    BT_TRACE_EXIT("scheme.walk_init");
}

void bt_scheme_walk_run(bt_orbit_walk *walk) {
    BT_TRACE_ENTER("scheme.walk_run");
    uint32_t budget = 0;
    uint32_t spins = 0;
    for (;;) {
        spins++;
        if (spins > 200) {
            BT_SCHEME_ABORT("memo.walk", "memo walk spin budget exceeded");
        }
        uint8_t buffer;
        if (!scan_piece(walk, true, &buffer)) {
            walk->pending_count = 0;
            break;
        }
        uint8_t h = head_of(walk->orbit, buffer);
        if (walk->pending_count > 0 && walk->reserved[piece_index(walk->pending[0])]) {
            walk->pending_count = 0;
        }
        uint8_t pos;
        if (walk->pending_count == 0) {
            walk->visited[h] = true;
            pos = h;
        } else {
            uint8_t q;
            if (scan_piece(walk, false, &q)) {
                uint8_t qh = head_of(walk->orbit, q);
                walk->visited[qh] = true;
                pos = qh;
            } else {
                uint8_t t = walk->pending[0];
                walk->has_parity = true;
                walk->parity.edge[0] = h;
                walk->parity.edge[1] = head_of(walk->orbit, piece_index(t));
                break;
            }
        }
        for (;;) {
            uint8_t s = walk->occ[pos];
            if (bt_scheme_same_piece(s, h)) {
                break;
            }
            walk->pending[walk->pending_count++] = s;
            walk->letters[walk->letter_count++] = bt_scheme_letter_of(s);
            walk->visited[s] = true;
            budget++;
            if (budget > BT_MEMO_MAX_LETTERS) {
                BT_SCHEME_ABORT("memo.walk", "memo walk letter budget exceeded");
            }
            if (walk->pending_count == 2) {
                bt_case case_id = {walk->orbit, walk->pending[0], walk->pending[1]};
                assert_case_legal(&case_id);
                walk->pairs[walk->pair_count++] = case_id;
                walk->buffers[walk->buffer_count++] = h;
                bt_scheme_cycles cycles;
                pair_cycles(walk->orbit, h, case_id.t1, case_id.t2, &cycles);
                bt_scheme_apply_sticker_cycles(walk->occ, &cycles);
                walk->pending_count = 0;
                pos = h;
            } else {
                pos = s;
            }
        }
    }
    BT_TRACE_EXIT("scheme.walk_run");
}

void bt_scheme_memo_occ(const uint8_t occ[54], bt_memo *out) {
    BT_TRACE_ENTER("scheme.memo_occ");
    uint8_t work[54];
    memcpy(work, occ, sizeof work);
    bt_orbit_walk edge;
    bt_orbit_walk corner;
    bt_scheme_walk_init(&edge, BT_ORBIT_EDGE, work);
    bt_scheme_walk_run(&edge);
    bt_scheme_walk_init(&corner, BT_ORBIT_CORNER, work);
    bt_scheme_walk_run(&corner);
    if (edge.has_parity != corner.has_parity) {
        BT_SCHEME_ABORT("scheme.memo_occ", "parity must hit both orbits: edge %d corner %d",
                        (int)edge.has_parity, (int)corner.has_parity);
    }
    memset(out, 0, sizeof *out);
    out->edge_letter_count = edge.letter_count;
    memcpy(out->edge_letters, edge.letters, edge.letter_count);
    out->corner_letter_count = corner.letter_count;
    memcpy(out->corner_letters, corner.letters, corner.letter_count);
    out->edge_pair_count = edge.pair_count;
    memcpy(out->edge_pairs, edge.pairs, edge.pair_count * sizeof(bt_case));
    out->corner_pair_count = corner.pair_count;
    memcpy(out->corner_pairs, corner.pairs, corner.pair_count * sizeof(bt_case));
    out->edge_buffer_count = edge.buffer_count;
    memcpy(out->edge_buffers, edge.buffers, edge.buffer_count);
    out->corner_buffer_count = corner.buffer_count;
    memcpy(out->corner_buffers, corner.buffers, corner.buffer_count);
    out->residue.has_parity = edge.has_parity;
    if (edge.has_parity) {
        bt_scheme_apply_aligned_swap(work, BT_ORBIT_EDGE, edge.parity.edge[0], edge.parity.edge[1]);
        bt_scheme_apply_aligned_swap(work, BT_ORBIT_CORNER, corner.parity.edge[0],
                                     corner.parity.edge[1]);
        out->residue.parity = edge.parity;
        out->residue.parity.corner[0] = corner.parity.edge[0];
        out->residue.parity.corner[1] = corner.parity.edge[1];
    }
    for (uint8_t slot = 0; slot < 12; slot++) {
        if (in_place(work, BT_ORBIT_EDGE, slot) && !solved_piece(work, BT_ORBIT_EDGE, slot)) {
            out->residue.flips[out->residue.flip_count++] = head_of(BT_ORBIT_EDGE, slot);
        }
    }
    for (uint8_t slot = 0; slot < 8; slot++) {
        if (in_place(work, BT_ORBIT_CORNER, slot) && !solved_piece(work, BT_ORBIT_CORNER, slot)) {
            uint8_t quarter = bt_scheme_corner_twist_state(work, slot);
            out->residue.twists[out->residue.twist_count].corner = head_of(BT_ORBIT_CORNER, slot);
            out->residue.twists[out->residue.twist_count].quarter = quarter == 1 ? 2 : 1;
            out->residue.twist_count++;
        }
    }
    BT_TRACE_EXIT("scheme.memo_occ");
}

void bt_scheme_memo_from_array(const uint8_t occ[54], bt_memo *out) {
    BT_TRACE_ENTER("scheme.memo_from_array");
    if (!bt_scheme_occ_is_legal(occ)) {
        BT_SCHEME_ABORT("scheme.memo_from_array", "memo requires a legal state");
    }
    bt_scheme_memo_occ(occ, out);
    BT_TRACE_EXIT("scheme.memo_from_array");
}

void bt_scheme_memo(const bt_cube *cube, bt_memo *out) {
    BT_TRACE_ENTER("scheme.memo");
    uint8_t occ[54];
    bt_scheme_state_array(cube, occ);
    bt_scheme_memo_from_array(occ, out);
    BT_TRACE_EXIT("scheme.memo");
}

bt_residue_class bt_scheme_residue_class(const bt_residue *residue) {
    BT_TRACE_ENTER("scheme.residue_class");
    bt_residue_class out = {BT_RESIDUE_COMPOUND, 0, 0, 0};
    if (!residue->has_parity) {
        if (residue->twist_count == 0) {
            if (residue->flip_count == 0) {
                out.kind = BT_RESIDUE_NONE;
            } else if (residue->flip_count == 2) {
                out.kind = BT_RESIDUE_FLIP_PAIR;
                out.a = residue->flips[0];
                out.b = residue->flips[1];
            }
        } else if (residue->flip_count == 0) {
            if (residue->twist_count == 2 &&
                residue->twists[0].quarter + residue->twists[1].quarter == 3) {
                out.kind = BT_RESIDUE_TWIST_PAIR;
                out.a = residue->twists[0].corner;
                out.b = residue->twists[1].corner;
            } else if (residue->twist_count == 3 &&
                       residue->twists[0].quarter == residue->twists[1].quarter &&
                       residue->twists[1].quarter == residue->twists[2].quarter) {
                out.kind = BT_RESIDUE_TWIST_TRIPLE;
                out.a = residue->twists[0].corner;
                out.b = residue->twists[1].corner;
                out.c = residue->twists[2].corner;
            }
        }
        BT_TRACE_EXIT("scheme.residue_class");
        return out;
    }
    if (residue->flip_count == 0 && residue->twist_count == 0) {
        out.kind = BT_RESIDUE_PARITY;
        BT_TRACE_EXIT("scheme.residue_class");
        return out;
    }
    if (residue->flip_count == 0) {
        uint8_t c1 = residue->parity.corner[0];
        uint8_t c2 = residue->parity.corner[1];
        bool outside = false;
        for (uint8_t i = 0; i < residue->twist_count; i++) {
            if (!bt_scheme_same_piece(residue->twists[i].corner, c1) &&
                !bt_scheme_same_piece(residue->twists[i].corner, c2)) {
                outside = true;
            }
        }
        out.kind = outside ? BT_RESIDUE_LTCT : BT_RESIDUE_PARITY;
        BT_TRACE_EXIT("scheme.residue_class");
        return out;
    }
    BT_TRACE_EXIT("scheme.residue_class");
    return out;
}

bool bt_scheme_residue_eq(const bt_residue *a, const bt_residue *b) {
    BT_TRACE_ENTER("scheme.residue_eq");
    if (a->has_parity != b->has_parity || a->flip_count != b->flip_count ||
        a->twist_count != b->twist_count || memcmp(a->flips, b->flips, a->flip_count) != 0 ||
        memcmp(a->twists, b->twists, a->twist_count * sizeof(bt_twist)) != 0) {
        BT_TRACE_EXIT("scheme.residue_eq");
        return false;
    }
    if (a->has_parity && memcmp(&a->parity, &b->parity, sizeof(bt_parity)) != 0) {
        BT_TRACE_EXIT("scheme.residue_eq");
        return false;
    }
    BT_TRACE_EXIT("scheme.residue_eq");
    return true;
}

bool bt_scheme_memo_eq(const bt_memo *a, const bt_memo *b) {
    BT_TRACE_ENTER("scheme.memo_eq");
    bool equal =
        (a->edge_letter_count == b->edge_letter_count &&
         memcmp(a->edge_letters, b->edge_letters, a->edge_letter_count) == 0 &&
         a->corner_letter_count == b->corner_letter_count &&
         memcmp(a->corner_letters, b->corner_letters, a->corner_letter_count) == 0 &&
         a->edge_pair_count == b->edge_pair_count &&
         memcmp(a->edge_pairs, b->edge_pairs, a->edge_pair_count * sizeof(bt_case)) == 0 &&
         a->corner_pair_count == b->corner_pair_count &&
         memcmp(a->corner_pairs, b->corner_pairs, a->corner_pair_count * sizeof(bt_case)) == 0 &&
         a->edge_buffer_count == b->edge_buffer_count &&
         memcmp(a->edge_buffers, b->edge_buffers, a->edge_buffer_count) == 0 &&
         a->corner_buffer_count == b->corner_buffer_count &&
         memcmp(a->corner_buffers, b->corner_buffers, a->corner_buffer_count) == 0 &&
         bt_scheme_residue_eq(&a->residue, &b->residue)) != 0;
    BT_TRACE_EXIT("scheme.memo_eq");
    return equal;
}

bool bt_scheme_residue_class_eq(bt_residue_class a, bt_residue_class b) {
    BT_TRACE_ENTER("scheme.residue_class_eq");
    bool equal = (a.kind == b.kind && a.a == b.a && a.b == b.b && a.c == b.c) != 0;
    BT_TRACE_EXIT("scheme.residue_class_eq");
    return equal;
}
