#ifndef VERIFY_EV_CFOP_H
#define VERIFY_EV_CFOP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/alg.h"
#include "core/cube.h"
#include "data/seedpack/mod.h"
#include "verify/ev_bld.h"
#include "verify/packs.h"

typedef enum {
    VERIFY_CFOP_PLL,
    VERIFY_CFOP_OLL,
    VERIFY_CFOP_F2L,
} verify_cfop_stage;

size_t verify_cfop_u_band(const verify_bld_layout *layout, uint8_t *out, size_t cap);
bool verify_cfop_f2l_exact(const bt_cube *state, const uint8_t *band, size_t band_len);
int verify_cfop_rewrite_to_fixed(const char *alg_text, char *out, size_t out_len);
int verify_cfop_stage_of(const char *stage, verify_cfop_stage *out, char *err, size_t err_len);
verify_tier2 verify_cfop_stage_outcome(verify_cfop_stage stage, const bt_cube *state,
                                       const verify_bld_layout *layout, const char *front_json);
void verify_cfop_verify_pll(const bt_seed_rows *rows, verify_section_result *out);
void verify_cfop_verify_oll(const bt_seed_rows *rows, verify_section_result *out);
void verify_cfop_verify_f2l(const bt_seed_rows *rows, verify_section_result *out);

#endif
