#ifndef BT_PUZZLES_MEGA_H
#define BT_PUZZLES_MEGA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/strbuf.h"
#include "puzzles/geometry.h"

#define BT_MEGA_FACES 12
#define BT_MEGA_FACELETS 132
#define BT_MEGA_PER_FACE 11
#define BT_MEGA_RING 5

#define BT_MEGA_U 0
#define BT_MEGA_F 1
#define BT_MEGA_R 2
#define BT_MEGA_BR 3
#define BT_MEGA_BL 4
#define BT_MEGA_L 5
#define BT_MEGA_D 6
#define BT_MEGA_B 7
#define BT_MEGA_UR 8
#define BT_MEGA_DR 9
#define BT_MEGA_DL 10
#define BT_MEGA_UL 11

#define BT_MEGA_CENTER 0
#define BT_MEGA_CORNER_BASE 1
#define BT_MEGA_EDGE_BASE 6

extern const uint8_t BT_MEGA_NEIGHBOR_CW[BT_MEGA_FACES][BT_MEGA_RING];
extern const uint8_t BT_MEGA_Y_PERM[BT_MEGA_FACES];

typedef struct {
    uint8_t spin;
    uint8_t face;
    uint8_t steps;
} bt_mega_move;

typedef struct {
    uint8_t colors[BT_MEGA_FACELETS];
} bt_mega_state;

uint8_t bt_mega_corner_slot(size_t k);
uint8_t bt_mega_edge_slot(size_t k);
uint8_t bt_mega_color(const bt_mega_state *state, uint8_t face, uint8_t slot);
bool bt_mega_move_eq(bt_mega_move a, bt_mega_move b);
bool bt_mega_state_eq(const bt_mega_state *a, const bt_mega_state *b);
bool bt_mega_is_pinned_move(bt_mega_move mv);
bool bt_mega_parse_move(const char *token, size_t len, bt_mega_move *out, bt_pz_error *err);
bool bt_mega_parse_alg(const char *text, size_t len, void *alg, bt_pz_error *err);
void bt_mega_format_alg(const void *alg, bt_strbuf *out);
void bt_mega_invert_alg(const void *alg, void *out);
bool bt_mega_apply_alg(bt_mega_state *state, const void *alg, bt_pz_error *err);
void bt_mega_solved_state(bt_mega_state *out);
bool bt_mega_is_solved(const bt_mega_state *state);
void bt_mega_facelets(const bt_mega_state *state, bt_pz_grid *out);
bt_pz_engine bt_mega_engine(void);

#endif
