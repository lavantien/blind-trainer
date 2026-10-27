#ifndef BT_PUZZLES_NXN_H
#define BT_PUZZLES_NXN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/strbuf.h"
#include "puzzles/geometry.h"

#define BT_PZ_NXN_MIN_N 2
#define BT_PZ_NXN_MAX_N 7
#define BT_PZ_NXN_STRIDE BT_PZ_NXN_MAX_N
#define BT_PZ_NXN_FACELET_LEN (6 * BT_PZ_NXN_STRIDE * BT_PZ_NXN_STRIDE)

typedef struct {
    uint8_t face;
    uint8_t depth_from;
    uint8_t depth_to;
    uint8_t quarter;
} bt_pz_nxn_move;

typedef struct {
    size_t n;
    uint8_t facelet[BT_PZ_NXN_FACELET_LEN];
} bt_pz_nxn_state;

typedef struct {
    size_t n;
} bt_pz_nxn;

bool bt_pz_nxn_init(bt_pz_nxn *nxn, size_t n);
size_t bt_pz_nxn_n(const bt_pz_nxn *nxn);
bool bt_pz_nxn_move_eq(bt_pz_nxn_move a, bt_pz_nxn_move b);
bool bt_pz_nxn_state_eq(const bt_pz_nxn_state *a, const bt_pz_nxn_state *b);
bool bt_pz_nxn_parse_move(const bt_pz_nxn *nxn, const char *token, size_t len, bt_pz_nxn_move *out,
                          bt_pz_error *err);
bool bt_pz_nxn_parse_alg(const bt_pz_nxn *nxn, const char *text, size_t len, void *alg,
                         bt_pz_error *err);
void bt_pz_nxn_format_alg(const bt_pz_nxn *nxn, const void *alg, bt_strbuf *out);
void bt_pz_nxn_invert_alg(const bt_pz_nxn *nxn, const void *alg, void *out);
bool bt_pz_nxn_apply_alg(const bt_pz_nxn *nxn, bt_pz_nxn_state *state, const void *alg,
                         bt_pz_error *err);
void bt_pz_nxn_solved_state(const bt_pz_nxn *nxn, bt_pz_nxn_state *out);
bool bt_pz_nxn_is_solved(const bt_pz_nxn *nxn, const bt_pz_nxn_state *state);
void bt_pz_nxn_facelets(const bt_pz_nxn *nxn, const bt_pz_nxn_state *state, bt_pz_grid *out);
bt_pz_engine bt_pz_nxn_engine(const bt_pz_nxn *nxn);

#endif
