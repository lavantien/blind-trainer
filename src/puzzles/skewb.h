#ifndef BT_PUZZLES_SKEWB_H
#define BT_PUZZLES_SKEWB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/strbuf.h"
#include "puzzles/geometry.h"

#define BT_SKEWB_FACELET_COUNT 54
#define BT_SKEWB_FACE_COUNT 6
#define BT_SKEWB_VERTEX_COUNT 4

extern const char *const BT_SKEWB_FACE_NAMES[BT_SKEWB_FACE_COUNT];
extern const char *const BT_SKEWB_VERTEX_NAMES[BT_SKEWB_VERTEX_COUNT];

typedef struct {
    uint8_t vertex;
    uint8_t quarter;
} bt_skewb_move;

typedef struct {
    uint8_t facelets[BT_SKEWB_FACELET_COUNT];
} bt_skewb_state;

typedef struct {
    uint8_t turns[BT_SKEWB_VERTEX_COUNT * 2][BT_SKEWB_FACELET_COUNT];
} bt_skewb;

bt_skewb bt_skewb_new(void);
void bt_skewb_turn_table(const bt_skewb *skewb, uint8_t vertex, uint8_t quarter,
                         uint8_t out[BT_SKEWB_FACELET_COUNT]);

bool bt_skewb_move_eq(bt_skewb_move a, bt_skewb_move b);
bool bt_skewb_state_eq(const bt_skewb_state *a, const bt_skewb_state *b);
bool bt_skewb_parse_move(const char *token, size_t len, bt_skewb_move *out, bt_pz_error *err);
bool bt_skewb_parse_alg(const bt_skewb *skewb, const char *text, size_t len, void *alg,
                        bt_pz_error *err);
void bt_skewb_format_alg(const bt_skewb *skewb, const void *alg, bt_strbuf *out);
void bt_skewb_invert_alg(const bt_skewb *skewb, const void *alg, void *out);
bool bt_skewb_apply_alg(const bt_skewb *skewb, bt_skewb_state *state, const void *alg,
                        bt_pz_error *err);
void bt_skewb_solved_state(const bt_skewb *skewb, bt_skewb_state *out);
bool bt_skewb_is_solved(const bt_skewb *skewb, const bt_skewb_state *state);
void bt_skewb_facelets(const bt_skewb *skewb, const bt_skewb_state *state, bt_pz_grid *out);
bt_pz_engine bt_skewb_engine(const bt_skewb *skewb);

#endif
