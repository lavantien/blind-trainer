#include <stdio.h>

#include "core/alg.h"
#include "core/cube.h"
#include "core/moves.h"

int main(void) {
    const uint8_t axes[3] = {BT_AXIS_X, BT_AXIS_Y, BT_AXIS_Z};
    const uint8_t faces[3] = {BT_FACE_R, BT_FACE_U, BT_FACE_F};
    const uint8_t slices[3] = {BT_SLICE_M, BT_SLICE_E, BT_SLICE_S};
    const uint8_t backs[3] = {BT_FACE_L, BT_FACE_D, BT_FACE_B};
    const char *names[3] = {"x", "y", "z"};
    for (size_t i = 0; i < 3; i++) {
        for (uint8_t q = 1; q <= 3; q++) {
            uint8_t inv = (uint8_t)(4 - q);
            bt_cube composed = bt_cube_solved();
            bt_cube_apply(&composed, bt_move_of_face(faces[i], q));
            bt_cube_apply(&composed, bt_move_of_slice(slices[i], inv));
            bt_cube_apply(&composed, bt_move_of_face(backs[i], inv));
            bt_cube rotation = bt_cube_solved();
            bt_cube_apply(&rotation, bt_move_of_rotation(axes[i], q));
            int diff = 0;
            for (size_t k = 0; k < 54; k++) {
                if (composed.occ[k] != rotation.occ[k]) {
                    diff++;
                }
            }
            printf("%s q=%u diff=%d\n", names[i], q, diff);
        }
    }
    return 0;
}
