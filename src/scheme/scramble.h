#ifndef BT_SCHEME_SCRAMBLE_H
#define BT_SCHEME_SCRAMBLE_H

#include "common/rng.h"
#include "core_deps.h"
#include "scheme/memo.h"
#include "scheme/scheme.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void bt_scheme_fisher_yates(bt_rng *rng, uint8_t *perm, size_t n);
int8_t bt_scheme_perm_sign(const uint8_t *perm, size_t n);
void bt_scheme_build_occ(uint8_t occ[54], const uint8_t edges[12], const bool flips[12],
                         const uint8_t corners[8], const uint8_t twists[8]);
void bt_scheme_random_occ(uint64_t seed, uint8_t occ[54]);
uint32_t bt_scheme_twist_sum(const uint8_t occ[54]);
uint32_t bt_scheme_flip_count(const uint8_t occ[54]);
bool bt_scheme_piece_perm(const uint8_t occ[54], bt_orbit orbit, uint8_t out[12]);
bool bt_scheme_occ_is_legal(const uint8_t occ[54]);
void bt_scheme_random_state(uint64_t seed, bt_cube *out);

#endif
