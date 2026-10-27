#ifndef BT_PUZZLES_PYRA_H
#define BT_PUZZLES_PYRA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/strbuf.h"
#include "puzzles/geometry.h"

#define BT_PYRA_FACELET_COUNT 36

typedef enum {
    BT_PYRA_VERTEX_U = 0,
    BT_PYRA_VERTEX_L = 1,
    BT_PYRA_VERTEX_R = 2,
    BT_PYRA_VERTEX_B = 3,
} bt_pyra_vertex;

typedef enum {
    BT_PYRA_LAYERS_TWIN = 0,
    BT_PYRA_LAYERS_TIP = 1,
} bt_pyra_layers;

typedef enum {
    BT_PYRA_SENSE_CLOCK = 0,
    BT_PYRA_SENSE_COUNTER = 1,
} bt_pyra_sense;

typedef struct {
    uint8_t vertex;
    uint8_t layers;
    uint8_t sense;
} bt_pyra_move;

typedef struct {
    uint8_t facelets[BT_PYRA_FACELET_COUNT];
} bt_pyra_state;

bool bt_pyra_move_eq(bt_pyra_move a, bt_pyra_move b);
bool bt_pyra_state_eq(const bt_pyra_state *a, const bt_pyra_state *b);
bool bt_pyra_parse_move(const char *token, size_t len, bt_pyra_move *out, bt_pz_error *err);
bool bt_pyra_parse_alg(const char *text, size_t len, void *alg, bt_pz_error *err);
void bt_pyra_format_alg(const void *alg, bt_strbuf *out);
void bt_pyra_invert_alg(const void *alg, void *out);
bool bt_pyra_apply_alg(bt_pyra_state *state, const void *alg, bt_pz_error *err);
void bt_pyra_solved_state(bt_pyra_state *out);
bool bt_pyra_is_solved(const bt_pyra_state *state);
void bt_pyra_facelets(const bt_pyra_state *state, bt_pz_grid *out);
bt_pz_engine bt_pyra_engine(void);

#endif
