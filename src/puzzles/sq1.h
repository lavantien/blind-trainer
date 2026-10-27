#ifndef BT_PUZZLES_SQ1_H
#define BT_PUZZLES_SQ1_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/strbuf.h"
#include "puzzles/geometry.h"

#define BT_PZ_SQ1_UNITS 12
#define BT_PZ_SQ1_LAYERS 2
#define BT_PZ_SQ1_PIECES_PER_LAYER 8
#define BT_PZ_SQ1_UNIT_TOTAL (BT_PZ_SQ1_LAYERS * BT_PZ_SQ1_UNITS)
#define BT_PZ_SQ1_TWISTS (BT_PZ_SQ1_UNITS * BT_PZ_SQ1_UNITS - 1)
#define BT_PZ_SQ1_TURN_MIN (-5)
#define BT_PZ_SQ1_TURN_MAX 6

typedef struct {
    int32_t top;
    int32_t bottom;
    bool slash;
} bt_pz_sq1_move;

typedef struct {
    uint8_t units[BT_PZ_SQ1_UNIT_TOTAL];
    bool slice_flipped;
} bt_pz_sq1_state;

extern const uint8_t BT_PZ_SQ1_SQUARE_SHAPE[BT_PZ_SQ1_PIECES_PER_LAYER];

bt_pz_sq1_move bt_pz_sq1_move_twist(int32_t top, int32_t bottom);
bt_pz_sq1_move bt_pz_sq1_move_slash(void);
bool bt_pz_sq1_move_eq(bt_pz_sq1_move a, bt_pz_sq1_move b);
bool bt_pz_sq1_state_eq(const bt_pz_sq1_state *a, const bt_pz_sq1_state *b);
void bt_pz_sq1_solved_state(bt_pz_sq1_state *out);
bool bt_pz_sq1_can_slash(const bt_pz_sq1_state *state);
bool bt_pz_sq1_apply(bt_pz_sq1_state *state, bt_pz_sq1_move mv, bt_pz_error *err);
bool bt_pz_sq1_is_solved(const bt_pz_sq1_state *state);
void bt_pz_sq1_format_move(bt_pz_sq1_move mv, bt_strbuf *out);
size_t bt_pz_sq1_canonical_shape(const bt_pz_sq1_state *state, size_t layer, uint8_t *out,
                                 size_t cap);
bool bt_pz_sq1_parse_move(const char *token, size_t len, bt_pz_sq1_move *out, bt_pz_error *err);
bool bt_pz_sq1_parse_alg(const char *text, size_t len, void *alg, bt_pz_error *err);
void bt_pz_sq1_format_alg(const void *alg, bt_strbuf *out);
void bt_pz_sq1_invert_alg(const void *alg, void *out);
bool bt_pz_sq1_apply_alg(bt_pz_sq1_state *state, const void *alg, bt_pz_error *err);
void bt_pz_sq1_facelets(const bt_pz_sq1_state *state, bt_pz_grid *out);
bt_pz_engine bt_pz_sq1_engine(void);

#endif
