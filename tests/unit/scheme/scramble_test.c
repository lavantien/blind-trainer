#include "scheme/memo.h"
#include "scheme/scheme.h"
#include "scheme/scramble.h"

#include "common/bt_harness.h"
#include "common/rng.h"

#include <stdlib.h>
#include <string.h>

static void identity(uint8_t occ[54]) {
    for (uint8_t i = 0; i < 54; i++) {
        occ[i] = i;
    }
}

static int compare_bytes(const void *a, const void *b) {
    return (int)*(const uint8_t *)a - (int)*(const uint8_t *)b;
}

BT_TEST(scramble, splitmix64_reference_vectors) {
    bt_rng rng;
    bt_rng_seed(&rng, 0);
    BT_CHECK_EQ(bt_rng_next(&rng), 16294208416658607535ULL);
    BT_CHECK_EQ(bt_rng_next(&rng), 7960286522194355700ULL);
    BT_CHECK_EQ(bt_rng_next(&rng), 487617019471545679ULL);
    BT_CHECK_EQ(bt_rng_next(&rng), 17909611376780542444ULL);
    bt_rng other;
    bt_rng_seed(&other, 0x0123456789ABCDEFULL);
    BT_CHECK_EQ(bt_rng_next(&other), 1547611027431991965ULL);
    BT_CHECK_EQ(bt_rng_next(&other), 15380727978956804243ULL);
}

BT_TEST(scramble, below_stays_in_range_and_spreads) {
    bt_rng rng;
    bt_rng_seed(&rng, 42);
    unsigned counts[5] = {0, 0, 0, 0, 0};
    for (int i = 0; i < 5000; i++) {
        uint64_t value = bt_rng_below(&rng, 5);
        BT_CHECK(value < 5);
        counts[value]++;
    }
    for (int i = 0; i < 5; i++) {
        BT_CHECK(counts[i] > 800);
        BT_CHECK(counts[i] < 1200);
    }
    bt_rng one;
    bt_rng_seed(&one, 7);
    BT_CHECK_EQ(bt_rng_below(&one, 1), 0ULL);
}

BT_TEST(scramble, fisher_yates_keeps_permutation_property) {
    bt_rng rng;
    bt_rng_seed(&rng, 123);
    uint8_t perm[12];
    for (uint8_t i = 0; i < 12; i++) {
        perm[i] = i;
    }
    bt_scheme_fisher_yates(&rng, perm, 12);
    uint8_t sorted[12];
    memcpy(sorted, perm, sizeof perm);
    qsort(sorted, 12, 1, compare_bytes);
    for (uint8_t i = 0; i < 12; i++) {
        BT_CHECK_EQ(sorted[i], i);
    }
    bool identity_flag = true;
    for (uint8_t i = 0; i < 12; i++) {
        if (perm[i] != i) {
            identity_flag = false;
        }
    }
    BT_CHECK(!identity_flag);
}

BT_TEST(scramble, random_occ_is_legal_and_unsolved_over_thousands) {
    uint8_t occ[54];
    uint8_t want[54];
    for (uint64_t seed = 0; seed < 3000; seed++) {
        bt_scheme_random_occ(seed, occ);
        BT_CHECK(bt_scheme_occ_is_legal(occ));
        identity(want);
        BT_CHECK(memcmp(occ, want, sizeof want) != 0);
        BT_CHECK_EQ(bt_scheme_twist_sum(occ) % 3, 0ULL);
        BT_CHECK_EQ(bt_scheme_flip_count(occ) % 2, 0ULL);
    }
}

BT_TEST(scramble, random_occ_is_deterministic_and_spreads) {
    uint8_t first[54];
    uint8_t second[54];
    for (uint64_t seed = 0; seed < 200; seed++) {
        bt_scheme_random_occ(seed, first);
        bt_scheme_random_occ(seed, second);
        BT_CHECK(memcmp(first, second, sizeof first) == 0);
    }
    static uint8_t seen[1000][54];
    size_t distinct = 0;
    for (uint64_t seed = 0; seed < 1000; seed++) {
        bt_scheme_random_occ(seed, seen[distinct]);
        bool duplicate = false;
        for (size_t i = 0; i < distinct; i++) {
            if (memcmp(seen[i], seen[distinct], 54) == 0) {
                duplicate = true;
            }
        }
        if (!duplicate) {
            distinct++;
        }
    }
    BT_CHECK_EQ(distinct, (size_t)1000);
}

BT_TEST(scramble, perm_sign_matches_transposition_parity) {
    uint8_t identity_perm[12];
    for (uint8_t i = 0; i < 12; i++) {
        identity_perm[i] = i;
    }
    BT_CHECK_EQ(bt_scheme_perm_sign(identity_perm, 12), 1);
    uint8_t tmp = identity_perm[0];
    identity_perm[0] = identity_perm[1];
    identity_perm[1] = tmp;
    BT_CHECK_EQ(bt_scheme_perm_sign(identity_perm, 12), -1);
    tmp = identity_perm[2];
    identity_perm[2] = identity_perm[3];
    identity_perm[3] = tmp;
    BT_CHECK_EQ(bt_scheme_perm_sign(identity_perm, 12), 1);
    uint8_t cycle[12] = {1, 2, 3, 0, 4, 5, 6, 7, 8, 9, 10, 11};
    BT_CHECK_EQ(bt_scheme_perm_sign(cycle, 12), -1);
}

BT_TEST(scramble, perm_sign_on_odd_length_permutations) {
    uint8_t first[3] = {0, 1, 2};
    uint8_t second[3] = {1, 2, 0};
    uint8_t third[3] = {1, 0, 2};
    BT_CHECK_EQ(bt_scheme_perm_sign(first, 3), 1);
    BT_CHECK_EQ(bt_scheme_perm_sign(second, 3), 1);
    BT_CHECK_EQ(bt_scheme_perm_sign(third, 3), -1);
}

BT_TEST(scramble, build_occ_rotates_twisted_corner_successor_cycles) {
    uint8_t occ[54];
    identity(occ);
    uint8_t edges[12];
    uint8_t corners[8];
    bool flips[12] = {false};
    uint8_t twists[8] = {0};
    for (uint8_t i = 0; i < 12; i++) {
        edges[i] = i;
    }
    for (uint8_t i = 0; i < 8; i++) {
        corners[i] = i;
    }
    twists[0] = 1;
    bt_scheme_build_occ(occ, edges, flips, corners, twists);
    BT_CHECK_EQ(occ[1], 16);
    BT_CHECK_EQ(occ[13], 1);
    BT_CHECK_EQ(occ[16], 13);
    for (int i = 0; i < 54; i++) {
        if (i != 1 && i != 13 && i != 16) {
            BT_CHECK_EQ(occ[i], (uint8_t)i);
        }
    }
}

BT_TEST(scramble, random_occ_flips_follow_the_generator_stream) {
    bt_rng rng;
    bt_rng_seed(&rng, 0);
    for (size_t i = 12; i > 1; i--) {
        bt_rng_below(&rng, i);
    }
    for (size_t i = 8; i > 1; i--) {
        bt_rng_below(&rng, i);
    }
    bool flips[12];
    for (int i = 0; i < 12; i++) {
        flips[i] = (bt_rng_next(&rng) & 1ULL) == 1ULL;
    }
    uint32_t flip_total = 0;
    for (int i = 0; i < 12; i++) {
        flip_total += flips[i] ? 1 : 0;
    }
    if (flip_total % 2 == 1) {
        flips[11] = !flips[11] != 0;
    }
    uint8_t occ[54];
    bt_scheme_random_occ(0, occ);
    for (uint8_t slot = 0; slot < 12; slot++) {
        BT_CHECK_EQ(bt_scheme_edge_flipped(occ, slot), flips[slot]);
    }
}

BT_TEST(scramble, random_occ_first_attempt_draw_sequence_is_pinned) {
    bt_rng rng;
    bt_rng_seed(&rng, 0);
    uint8_t edges[12];
    uint8_t corners[8];
    for (uint8_t i = 0; i < 12; i++) {
        edges[i] = i;
    }
    for (uint8_t i = 0; i < 8; i++) {
        corners[i] = i;
    }
    bt_scheme_fisher_yates(&rng, edges, 12);
    bt_scheme_fisher_yates(&rng, corners, 8);
    bool flips[12];
    for (int i = 0; i < 12; i++) {
        flips[i] = (bt_rng_next(&rng) & 1ULL) == 1ULL;
    }
    uint8_t twists[8] = {0};
    for (uint8_t i = 0; i < 7; i++) {
        twists[i] = (uint8_t)bt_rng_below(&rng, 3);
    }
    uint32_t twist_sum = 0;
    for (uint8_t i = 0; i < 7; i++) {
        twist_sum += twists[i];
    }
    twists[7] = (uint8_t)((3 - twist_sum % 3) % 3);
    uint32_t flip_total = 0;
    for (int i = 0; i < 12; i++) {
        flip_total += flips[i] ? 1 : 0;
    }
    if (flip_total % 2 == 1) {
        flips[11] = !flips[11] != 0;
    }
    if (bt_scheme_perm_sign(edges, 12) != bt_scheme_perm_sign(corners, 8)) {
        uint8_t tmp = edges[0];
        edges[0] = edges[1];
        edges[1] = tmp;
    }
    uint8_t replayed[54];
    identity(replayed);
    bt_scheme_build_occ(replayed, edges, flips, corners, twists);
    uint8_t occ[54];
    bt_scheme_random_occ(0, occ);
    BT_CHECK(memcmp(replayed, occ, sizeof replayed) == 0);
    BT_CHECK_EQ(bt_scheme_twist_sum(replayed) % 3, 0ULL);
    BT_CHECK_EQ(bt_scheme_flip_count(replayed) % 2, 0ULL);
}

BT_TEST(scramble, identity_and_single_transposition_legality) {
    uint8_t occ[54];
    identity(occ);
    BT_CHECK(bt_scheme_occ_is_legal(occ));
    uint8_t tmp = occ[26];
    occ[26] = occ[32];
    occ[32] = tmp;
    BT_CHECK(!bt_scheme_occ_is_legal(occ));
}
