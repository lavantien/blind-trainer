#ifndef BT_RNG_H
#define BT_RNG_H

#include <stdint.h>

typedef struct {
    uint64_t state;
} bt_rng;

void bt_rng_seed(bt_rng *rng, uint64_t seed);
uint64_t bt_rng_next(bt_rng *rng);
uint64_t bt_rng_below(bt_rng *rng, uint64_t bound);

#endif
