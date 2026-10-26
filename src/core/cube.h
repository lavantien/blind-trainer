#ifndef BT_CORE_CUBE_H
#define BT_CORE_CUBE_H

#include <stdbool.h>
#include <stdint.h>

#include "core/alg.h"
#include "core/moves.h"

typedef enum {
    BT_FACE_U = 0,
    BT_FACE_L = 1,
    BT_FACE_F = 2,
    BT_FACE_R = 3,
    BT_FACE_B = 4,
    BT_FACE_D = 5,
} bt_face;

#define BT_FACE_COUNT 6

typedef struct {
    uint8_t occ[54];
} bt_cube;

uint8_t bt_corner_id(bt_face face, uint8_t pos);
uint8_t bt_edge_id(bt_face face, uint8_t pos);
uint8_t bt_center_id(bt_face face);

bt_cube bt_cube_solved(void);
bool bt_cube_try_from_occ(const uint8_t occ[54], bt_cube *out);
bt_cube bt_cube_from_occ(const uint8_t occ[54]);
void bt_cube_apply(bt_cube *cube, bt_move mv);
void bt_cube_apply_alg(bt_cube *cube, const bt_alg *alg);
uint8_t bt_cube_sticker_at(const bt_cube *cube, uint8_t pos);
uint8_t bt_cube_position_of(const bt_cube *cube, uint8_t sticker);
bt_cube bt_cube_inverse_state(const bt_cube *cube);
bool bt_cube_is_solved(const bt_cube *cube);
bool bt_cube_centers_fixed(const bt_cube *cube);
bool bt_cube_eq(const bt_cube *a, const bt_cube *b);

#endif
