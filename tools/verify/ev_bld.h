#ifndef VERIFY_EV_BLD_H
#define VERIFY_EV_BLD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/alg.h"
#include "core/cube.h"
#include "data/seedpack/mod.h"
#include "verify/packs.h"

#define VERIFY_BLD_POSITION_COUNT 54
#define VERIFY_BLD_PIECE_BAND_END 48
#define VERIFY_BLD_MAX_PIECES 20
#define VERIFY_BLD_MAX_SLOT 4
#define VERIFY_BLD_NAME_MAX 8
#define VERIFY_BLD_KIND_MAX 24
#define VERIFY_BLD_TEXT_MAX 128
#define VERIFY_BLD_MAX_BAND 12

typedef struct {
    uint8_t pieces[VERIFY_BLD_MAX_PIECES][VERIFY_BLD_MAX_SLOT];
    size_t piece_lens[VERIFY_BLD_MAX_PIECES];
    uint8_t masks[VERIFY_BLD_MAX_PIECES];
    size_t piece_count;
    int by_name[256];
} verify_bld_layout;

typedef struct {
    size_t piece_at[VERIFY_BLD_MAX_PIECES];
    uint8_t disturbed[VERIFY_BLD_POSITION_COUNT];
    size_t disturbed_count;
} verify_bld_state_audit;

void verify_bld_map_wide_suffix(const char *text, char *out, size_t out_len);
void verify_bld_layout_probe(verify_bld_layout *layout);
void verify_bld_mask_name(uint8_t mask, char *out, size_t out_len);
int verify_bld_slot_index(const verify_bld_layout *layout, const char *name, char *err,
                          size_t err_len);
int verify_bld_slot(const verify_bld_layout *layout, const char *name, const uint8_t **out,
                    size_t *out_len, char *err, size_t err_len);
size_t verify_bld_corner_pieces(const verify_bld_layout *layout, size_t *out, size_t cap);
size_t verify_bld_edge_pieces(const verify_bld_layout *layout, size_t *out, size_t cap);
uint8_t verify_bld_mask_of(const verify_bld_layout *layout, size_t index);
const uint8_t *verify_bld_slot_of_index(const verify_bld_layout *layout, size_t index);
size_t verify_bld_slot_len_of(const verify_bld_layout *layout, size_t index);
void verify_bld_audit(const bt_cube *state, const verify_bld_layout *layout,
                      verify_bld_state_audit *out);
size_t verify_bld_displaced(const verify_bld_state_audit *audit, const size_t *band,
                            size_t band_count, size_t *out, size_t cap);
int verify_bld_transposed(const verify_bld_state_audit *audit, const verify_bld_layout *layout,
                          const char *first, const char *second, char *err, size_t err_len);
bool verify_bld_zero_permutation(const verify_bld_state_audit *audit,
                                 const verify_bld_layout *layout);
void verify_bld_case_cube(const bt_alg *alg, bt_cube *out);
int verify_bld_round_trip(const bt_alg *alg, char *err, size_t err_len);
int verify_bld_parse_pair(const char *spec, char sep, char out[2][VERIFY_BLD_NAME_MAX], char *err,
                          size_t err_len);
int verify_bld_parity_swaps(const char *swaps, char edge[2][VERIFY_BLD_NAME_MAX],
                            char corner[2][VERIFY_BLD_NAME_MAX], char *err, size_t err_len);
void verify_bld_verify_oldpoch(const bt_seed_rows *rows, verify_section_result *out);
void verify_bld_verify_m2(const bt_seed_rows *rows, verify_section_result *out);
void verify_bld_verify_3style(const bt_seed_rows *rows, verify_section_result *out);

#endif
