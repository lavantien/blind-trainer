#ifndef VERIFY_EV_PYRA_H
#define VERIFY_EV_PYRA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "data/seedpack/mod.h"
#include "puzzles/pyra.h"
#include "verify/packs.h"

#define VERIFY_PYRA_U_FRAME_WORDS 4

uint8_t verify_pyra_flat(const bt_pz_grid *grid, size_t face, size_t local);
int verify_pyra_l4e_disturbance(const bt_pyra_state *state, char *err, size_t err_len);
int verify_pyra_case_state(const char *alg_text, bt_pyra_state *state, char *err, size_t err_len);
void verify_pyra_verify_l4e(const bt_seed_rows *rows, verify_section_result *out);

#endif
