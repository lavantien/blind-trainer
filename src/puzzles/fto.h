#ifndef BT_PUZZLES_FTO_H
#define BT_PUZZLES_FTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/strbuf.h"
#include "puzzles/geometry.h"

#define BT_FTO_FACELET_COUNT 72
#define BT_FTO_FACE_COUNT 8

extern const char *const BT_FTO_FACE_NAMES[BT_FTO_FACE_COUNT];

typedef struct {
    uint8_t face;
    uint8_t quarter;
} bt_fto_move;

typedef struct {
    uint8_t facelets[BT_FTO_FACELET_COUNT];
} bt_fto_state;

typedef struct {
    uint8_t turns[BT_FTO_FACE_COUNT * 2][BT_FTO_FACELET_COUNT];
} bt_fto;

bt_fto bt_fto_new(void);
void bt_fto_turn_table(const bt_fto *fto, uint8_t face, uint8_t quarter,
                       uint8_t out[BT_FTO_FACELET_COUNT]);
void bt_fto_neighbors(const bt_fto *fto, uint8_t face, uint8_t out[3]);
uint8_t bt_fto_opposite(const bt_fto *fto, uint8_t face);

bool bt_fto_move_eq(bt_fto_move a, bt_fto_move b);
bool bt_fto_state_eq(const bt_fto_state *a, const bt_fto_state *b);
bool bt_fto_parse_move(const char *token, size_t len, bt_fto_move *out, bt_pz_error *err);
bool bt_fto_parse_alg(const bt_fto *fto, const char *text, size_t len, void *alg, bt_pz_error *err);
void bt_fto_format_alg(const bt_fto *fto, const void *alg, bt_strbuf *out);
void bt_fto_invert_alg(const bt_fto *fto, const void *alg, void *out);
bool bt_fto_apply_alg(const bt_fto *fto, bt_fto_state *state, const void *alg, bt_pz_error *err);
void bt_fto_solved_state(const bt_fto *fto, bt_fto_state *out);
bool bt_fto_is_solved(const bt_fto *fto, const bt_fto_state *state);
void bt_fto_facelets(const bt_fto *fto, const bt_fto_state *state, bt_pz_grid *out);
bt_pz_engine bt_fto_engine(const bt_fto *fto);

#endif
