#ifndef VERIFY_EV_NXN_H
#define VERIFY_EV_NXN_H

#include <stdbool.h>
#include <stddef.h>

#include "data/seedpack/mod.h"
#include "puzzles/nxn.h"
#include "verify/packs.h"

#define VERIFY_NXN_ROTATION_COUNT 24

typedef struct {
    void **items;
    size_t count;
    size_t cap;
} verify_nxn_rotations;

void verify_nxn_rotations_init(verify_nxn_rotations *rotations);
void verify_nxn_rotations_free(verify_nxn_rotations *rotations);
void verify_nxn_rotations_build(const bt_pz_nxn *engine, verify_nxn_rotations *out);
bool verify_nxn_d_layer_solved(const bt_pz_nxn *engine, const bt_pz_nxn_state *state);
int verify_nxn_tier1_nonempty(const bt_pz_nxn *engine, const char *alg_text, char *err,
                              size_t err_len);
verify_tier2 verify_nxn_tier2_cll(const bt_pz_nxn *engine, const verify_nxn_rotations *rotations,
                                  const char *alg_text);
void verify_nxn_verify_twoby2(const bt_seed_rows *rows, verify_section_result *out);

#endif
