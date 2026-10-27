#ifndef VERIFY_EV_MEGA_H
#define VERIFY_EV_MEGA_H

#include <stdbool.h>
#include <stddef.h>

#include "data/seedpack/mod.h"
#include "puzzles/mega.h"
#include "verify/packs.h"

typedef enum {
    VERIFY_MEGA_STAGE_EO,
    VERIFY_MEGA_STAGE_CO,
    VERIFY_MEGA_STAGE_CP,
    VERIFY_MEGA_STAGE_EP,
} verify_mega_ll_stage;

size_t verify_mega_ring(void);
bool verify_mega_edges_oriented(const bt_mega_state *state);
bool verify_mega_corners_oriented(const bt_mega_state *state);
bool verify_mega_corners_placed(const bt_mega_state *state);
int verify_mega_stage_of(const char *tag, verify_mega_ll_stage *out, char *err, size_t err_len);
int verify_mega_tag_of(const char *front_json, char *out, size_t out_len);
int verify_mega_case_state(const char *alg_text, bt_mega_state *state, char *err, size_t err_len);
verify_tier2 verify_mega_stage_outcome(verify_mega_ll_stage stage, const bt_mega_state *state);
void verify_mega_verify_4lll(const bt_seed_rows *rows, verify_section_result *out);

#endif
