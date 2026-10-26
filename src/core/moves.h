#ifndef BT_CORE_MOVES_H
#define BT_CORE_MOVES_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BT_SLICE_M = 0,
    BT_SLICE_E = 1,
    BT_SLICE_S = 2,
} bt_slice;

typedef enum {
    BT_AXIS_X = 0,
    BT_AXIS_Y = 1,
    BT_AXIS_Z = 2,
} bt_axis;

typedef enum {
    BT_MOVE_FACE = 0,
    BT_MOVE_WIDE = 1,
    BT_MOVE_SLICE = 2,
    BT_MOVE_INNER = 3,
    BT_MOVE_ROTATION = 4,
} bt_move_kind;

typedef struct {
    uint8_t kind;
    uint8_t face;
    uint8_t slice;
    uint8_t axis;
    uint8_t quarter;
} bt_move;

bt_move bt_move_of_face(uint8_t face, uint8_t quarter);
bt_move bt_move_of_wide(uint8_t face, uint8_t quarter);
bt_move bt_move_of_slice(uint8_t slice, uint8_t quarter);
bt_move bt_move_of_inner(uint8_t face, uint8_t quarter);
bt_move bt_move_of_rotation(uint8_t axis, uint8_t quarter);

uint8_t bt_move_quarter(bt_move mv);
bt_move bt_move_invert(bt_move mv);
bt_move bt_move_with_quarter(bt_move mv, uint8_t quarter);
bool bt_move_eq(bt_move a, bt_move b);

void bt_move_table(bt_move mv, uint8_t out[54]);

void bt_moves_geometry(size_t face, size_t r, size_t c, int32_t out[3]);
size_t bt_moves_face_of_normal(const int32_t n[3]);
void bt_moves_cell_of(size_t face, const int32_t p[3], size_t *r, size_t *c);

#endif
