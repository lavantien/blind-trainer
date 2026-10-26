#include "common/rng.h"

#include "common/bt_trace.h"

#define BT_RNG_GOLDEN 0x9E3779B97F4A7C15ULL
#define BT_RNG_MIX_A 0xBF58476D1CE4E5B9ULL
#define BT_RNG_MIX_B 0x94D049BB133111EBULL

void bt_rng_seed(bt_rng *rng, uint64_t seed) {
    BT_TRACE_ENTER("rng.seed");
    rng->state = seed;
    BT_TRACE_EXIT("rng.seed");
}

uint64_t bt_rng_next(bt_rng *rng) {
    BT_TRACE_ENTER("rng.next");
    rng->state += BT_RNG_GOLDEN;
    uint64_t z = rng->state;
    z = (z ^ (z >> 30U)) * BT_RNG_MIX_A;
    z = (z ^ (z >> 27U)) * BT_RNG_MIX_B;
    BT_TRACE_EXIT("rng.next");
    return z ^ (z >> 31U);
}

uint64_t bt_rng_below(bt_rng *rng, uint64_t bound) {
    BT_TRACE_ENTER("rng.below");
    unsigned __int128 product = (unsigned __int128)(bt_rng_next(rng) >> 32U);
    product *= bound;
    BT_TRACE_EXIT("rng.below");
    return (uint64_t)(product >> 32U);
}
