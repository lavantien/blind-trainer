#include "scheme/scramble.h"

#include "internal.h"

#include <string.h>

static void swap_u8(uint8_t *a, uint8_t *b) {
    uint8_t tmp = *a;
    *a = *b;
    *b = tmp;
}

void bt_scheme_fisher_yates(bt_rng *rng, uint8_t *perm, size_t n) {
    BT_TRACE_ENTER("scheme.fisher_yates");
    for (size_t i = n; i > 1; i--) {
        size_t j = (size_t)bt_rng_below(rng, (uint64_t)i);
        swap_u8(&perm[i - 1], &perm[j]);
    }
    BT_TRACE_EXIT("scheme.fisher_yates");
}

int8_t bt_scheme_perm_sign(const uint8_t *perm, size_t n) {
    BT_TRACE_ENTER("scheme.perm_sign");
    bool seen[24] = {false};
    int8_t sign = 1;
    for (size_t i = 0; i < n; i++) {
        if (seen[i]) {
            continue;
        }
        size_t len = 0;
        size_t cur = i;
        while (!seen[cur]) {
            seen[cur] = true;
            cur = perm[cur];
            len++;
        }
        if (len % 2 == 0) {
            sign = (int8_t)-sign;
        }
    }
    BT_TRACE_EXIT("scheme.perm_sign");
    return sign;
}

static void identity_occ(uint8_t occ[54]) {
    for (uint8_t i = 0; i < 54; i++) {
        occ[i] = i;
    }
}

void bt_scheme_build_occ(uint8_t occ[54], const uint8_t edges[12], const bool flips[12],
                         const uint8_t corners[8], const uint8_t twists[8]) {
    BT_TRACE_ENTER("scheme.build_occ");
    for (uint8_t slot = 0; slot < 12; slot++) {
        uint8_t positions[2];
        uint8_t occupants[2];
        bt_edge_stickers(slot, positions);
        bt_edge_stickers(edges[slot], occupants);
        if (flips[slot]) {
            occ[positions[0]] = occupants[1];
            occ[positions[1]] = occupants[0];
        } else {
            occ[positions[0]] = occupants[0];
            occ[positions[1]] = occupants[1];
        }
    }
    for (uint8_t slot = 0; slot < 8; slot++) {
        const uint8_t *slot_cyc = BT_SCHEME_CORNER_SUCC[slot];
        const uint8_t *occ_cyc = BT_SCHEME_CORNER_SUCC[corners[slot]];
        uint8_t w = twists[slot] % 3;
        for (uint8_t j = 0; j < 3; j++) {
            occ[slot_cyc[(j + w) % 3]] = occ_cyc[j];
        }
    }
    BT_TRACE_EXIT("scheme.build_occ");
}

void bt_scheme_random_occ(uint64_t seed, uint8_t occ[54]) {
    BT_TRACE_ENTER("scheme.random_occ");
    bt_rng rng;
    bt_rng_seed(&rng, seed);
    uint8_t edges[12];
    uint8_t corners[8];
    bool flips[12];
    uint8_t twists[8];
    uint8_t identity[54];
    identity_occ(identity);
    for (int attempt = 0; attempt < 8; attempt++) {
        for (uint8_t i = 0; i < 12; i++) {
            edges[i] = i;
        }
        for (uint8_t i = 0; i < 8; i++) {
            corners[i] = i;
        }
        bt_scheme_fisher_yates(&rng, edges, 12);
        bt_scheme_fisher_yates(&rng, corners, 8);
        for (uint8_t i = 0; i < 12; i++) {
            flips[i] = (bt_rng_next(&rng) & 1ULL) == 1ULL;
        }
        for (uint8_t i = 0; i < 8; i++) {
            twists[i] = (uint8_t)bt_rng_below(&rng, 3);
        }
        uint32_t sum = 0;
        for (uint8_t i = 0; i < 7; i++) {
            sum += twists[i];
        }
        twists[7] = (uint8_t)((3 - sum % 3) % 3);
        uint32_t flip_total = 0;
        for (uint8_t i = 0; i < 12; i++) {
            flip_total += flips[i] ? 1 : 0;
        }
        if (flip_total % 2 == 1) {
            flips[11] = !flips[11] != 0;
        }
        if (bt_scheme_perm_sign(edges, 12) != bt_scheme_perm_sign(corners, 8)) {
            swap_u8(&edges[0], &edges[1]);
        }
        identity_occ(occ);
        bt_scheme_build_occ(occ, edges, flips, corners, twists);
        if (memcmp(occ, identity, sizeof identity) != 0) {
            BT_TRACE_EXIT("scheme.random_occ");
            return;
        }
    }
    for (uint8_t i = 0; i < 12; i++) {
        edges[i] = i;
    }
    for (uint8_t i = 0; i < 8; i++) {
        corners[i] = i;
    }
    swap_u8(&edges[0], &edges[1]);
    swap_u8(&corners[0], &corners[1]);
    memset(flips, 0, sizeof flips);
    memset(twists, 0, sizeof twists);
    identity_occ(occ);
    bt_scheme_build_occ(occ, edges, flips, corners, twists);
    BT_TRACE_EXIT("scheme.random_occ");
}

uint32_t bt_scheme_twist_sum(const uint8_t occ[54]) {
    BT_TRACE_ENTER("scheme.twist_sum");
    uint32_t sum = 0;
    for (uint8_t slot = 0; slot < 8; slot++) {
        sum += bt_scheme_corner_twist_state(occ, slot);
    }
    BT_TRACE_EXIT("scheme.twist_sum");
    return sum;
}

uint32_t bt_scheme_flip_count(const uint8_t occ[54]) {
    BT_TRACE_ENTER("scheme.flip_count");
    uint32_t count = 0;
    for (uint8_t slot = 0; slot < 12; slot++) {
        if (bt_scheme_edge_flipped(occ, slot)) {
            count++;
        }
    }
    BT_TRACE_EXIT("scheme.flip_count");
    return count;
}

static bool slot_occupant(const uint8_t occ[54], bt_orbit orbit, uint8_t slot, uint8_t *out) {
    uint8_t head;
    if (orbit == BT_ORBIT_CORNER) {
        head = BT_SCHEME_CORNER_SUCC[slot][0];
    } else {
        uint8_t pair[2];
        bt_edge_stickers(slot, pair);
        head = pair[0];
    }
    bt_piece piece = bt_piece_of(occ[head]);
    if (piece.kind == BT_PIECE_CORNER) {
        if (orbit != BT_ORBIT_CORNER) {
            return false;
        }
    } else if (orbit != BT_ORBIT_EDGE) {
        return false;
    }
    *out = piece.index;
    return true;
}

bool bt_scheme_piece_perm(const uint8_t occ[54], bt_orbit orbit, uint8_t out[12]) {
    BT_TRACE_ENTER("scheme.piece_perm");
    uint8_t slots = orbit == BT_ORBIT_CORNER ? 8 : 12;
    for (uint8_t slot = 0; slot < slots; slot++) {
        if (!slot_occupant(occ, orbit, slot, &out[slot])) {
            BT_TRACE_EXIT("scheme.piece_perm");
            return false;
        }
    }
    BT_TRACE_EXIT("scheme.piece_perm");
    return true;
}

static uint8_t position_of_sticker(const uint8_t occ[54], uint8_t sticker) {
    for (uint8_t pos = 0; pos < 54; pos++) {
        if (occ[pos] == sticker) {
            return pos;
        }
    }
    return 255;
}

bool bt_scheme_occ_is_legal(const uint8_t occ[54]) {
    BT_TRACE_ENTER("scheme.occ_is_legal");
    bool seen[54] = {false};
    for (uint8_t i = 0; i < 54; i++) {
        if (occ[i] >= 54 || seen[occ[i]]) {
            BT_TRACE_EXIT("scheme.occ_is_legal");
            return false;
        }
        seen[occ[i]] = true;
    }
    for (uint8_t i = 48; i < 54; i++) {
        if (occ[i] != i) {
            BT_TRACE_EXIT("scheme.occ_is_legal");
            return false;
        }
    }
    for (uint8_t sticker = 0; sticker < 48; sticker++) {
        uint8_t pos = position_of_sticker(occ, sticker);
        if ((sticker < 24) != (pos < 24)) {
            BT_TRACE_EXIT("scheme.occ_is_legal");
            return false;
        }
    }
    for (uint8_t slot = 0; slot < 12; slot++) {
        uint8_t pair[2];
        bt_edge_stickers(slot, pair);
        if (occ[pair[1]] != bt_scheme_edge_mate(occ[pair[0]])) {
            BT_TRACE_EXIT("scheme.occ_is_legal");
            return false;
        }
    }
    for (uint8_t slot = 0; slot < 8; slot++) {
        const uint8_t *cyc = BT_SCHEME_CORNER_SUCC[slot];
        bt_piece piece = bt_piece_of(occ[cyc[0]]);
        if (piece.kind != BT_PIECE_CORNER) {
            BT_TRACE_EXIT("scheme.occ_is_legal");
            return false;
        }
        for (uint8_t i = 0; i < 3; i++) {
            bt_piece other = bt_piece_of(occ[cyc[i]]);
            if (other.kind != BT_PIECE_CORNER || other.index != piece.index) {
                BT_TRACE_EXIT("scheme.occ_is_legal");
                return false;
            }
        }
    }
    uint8_t edge_perm[12];
    uint8_t corner_perm[12];
    if (!bt_scheme_piece_perm(occ, BT_ORBIT_EDGE, edge_perm)) {
        BT_TRACE_EXIT("scheme.occ_is_legal");
        return false;
    }
    if (!bt_scheme_piece_perm(occ, BT_ORBIT_CORNER, corner_perm)) {
        BT_TRACE_EXIT("scheme.occ_is_legal");
        return false;
    }
    bool legal = (bt_scheme_twist_sum(occ) % 3 == 0 && bt_scheme_flip_count(occ) % 2 == 0 &&
                  bt_scheme_perm_sign(edge_perm, 12) == bt_scheme_perm_sign(corner_perm, 8)) != 0;
    BT_TRACE_EXIT("scheme.occ_is_legal");
    return legal;
}

void bt_scheme_random_state(uint64_t seed, bt_cube *out) {
    BT_TRACE_ENTER("scheme.random_state");
    uint8_t occ[54];
    bt_scheme_random_occ(seed, occ);
    *out = bt_cube_from_occ(occ);
    BT_TRACE_EXIT("scheme.random_state");
}
