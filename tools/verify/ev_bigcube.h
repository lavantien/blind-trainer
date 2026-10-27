#ifndef VERIFY_EV_BIGCUBE_H
#define VERIFY_EV_BIGCUBE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "data/seedpack/mod.h"
#include "puzzles/nxn.h"
#include "verify/packs.h"

#define VERIFY_BIGCUBE_MAX_SIZES 8

typedef struct {
    uint8_t center_color;
    bool centers_uniform;
    bool corners_solved;
    bool midges_solved;
    size_t disturbed_slots;
} verify_bigcube_face_audit;

void verify_bigcube_audit_face(const bt_pz_grid *grid, size_t face, size_t n,
                               verify_bigcube_face_audit *out);
int verify_bigcube_reduction_state(const bt_pz_nxn *engine, const bt_pz_nxn_state *state, char *err,
                                   size_t err_len);
int verify_bigcube_tagged_size(const char *tag, size_t *out, char *err, size_t err_len);
size_t verify_bigcube_sizes_of(const bt_nspec *n, size_t *out, size_t cap);
int verify_bigcube_row_sizes(const bt_pack_entry *entry, const char *tag, size_t *out, size_t cap,
                             char *err, size_t err_len);
int verify_bigcube_tag_of(const char *front_json, char *out, size_t out_len);
void verify_bigcube_verify_parity(const bt_pack_entry *entry, const bt_seed_rows *rows,
                                  verify_section_result *out);

#endif
