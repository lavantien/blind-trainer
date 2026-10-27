#ifndef VERIFY_EV_SQ1_H
#define VERIFY_EV_SQ1_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "data/seedpack/mod.h"
#include "puzzles/sq1.h"
#include "verify/packs.h"

#define VERIFY_SQ1_NAME_MAX 32

typedef enum {
    VERIFY_SQ1_STAGE_CO,
    VERIFY_SQ1_STAGE_EO,
    VERIFY_SQ1_STAGE_CP,
    VERIFY_SQ1_STAGE_EP,
} verify_sq1_stage;

typedef struct {
    uint8_t id;
    uint8_t width;
} verify_sq1_span;

typedef struct {
    bt_pz_sq1_state state;
    uint8_t classes[2][BT_PZ_SQ1_UNITS];
    size_t class_lens[2];
    const char *front_json;
} verify_sq1_case;

typedef struct {
    char name[VERIFY_SQ1_NAME_MAX];
    uint8_t key[BT_PZ_SQ1_UNITS];
    size_t key_len;
} verify_sq1_family_key;

typedef struct {
    verify_sq1_family_key *keys;
    size_t key_count;
    char (*ambiguous)[VERIFY_SQ1_NAME_MAX];
    size_t ambiguous_count;
} verify_sq1_family_index;

int verify_sq1_start_case(const char *alg_text, verify_sq1_case *out, char *err, size_t err_len);
void verify_sq1_mirrored(const uint8_t *widths, size_t count, uint8_t *out);
void verify_sq1_famkey(const uint8_t *widths, size_t count, uint8_t *out);
int verify_sq1_shape_names(const char *front_json, char *top, char *bottom, size_t cap, char *err,
                           size_t err_len);
int verify_sq1_class_invariants(const verify_sq1_case *case_, char *err, size_t err_len);
void verify_sq1_resolve_family_keys(const verify_sq1_case *const *cases, size_t count,
                                    verify_sq1_family_index *out);
void verify_sq1_family_index_free(verify_sq1_family_index *index);
verify_tier2 verify_sq1_cubeshape_tier2(const verify_sq1_case *case_,
                                        const verify_sq1_family_index *index);
void verify_sq1_ring_spans(const uint8_t *ring, size_t count, verify_sq1_span *out,
                           size_t *out_count);
bool verify_sq1_corners_ordered(const bt_pz_sq1_state *state);
void verify_sq1_stage_predicates(const bt_pz_sq1_state *state, bool *corners, bool *edges,
                                 bool *placed);
verify_tier2 verify_sq1_stage_outcome(verify_sq1_stage stage, const verify_sq1_case *case_);
void verify_sq1_verify_cubeshape(const bt_seed_rows *rows, verify_section_result *out);
void verify_sq1_verify_co(const bt_seed_rows *rows, verify_section_result *out);
void verify_sq1_verify_eo(const bt_seed_rows *rows, verify_section_result *out);
void verify_sq1_verify_cp(const bt_seed_rows *rows, verify_section_result *out);
void verify_sq1_verify_ep(const bt_seed_rows *rows, verify_section_result *out);

#endif
