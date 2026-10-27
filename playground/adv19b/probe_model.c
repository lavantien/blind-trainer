#include "app/replay_scene.h"

#include "common/json.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void fail(const char *what, long detail) {
    fprintf(stderr, "PROBE-FAIL %s (%ld)\n", what, detail);
    failures++;
}

static int spec_corner(int row, int col) {
    if (row == 0 && col == 0) return 0;
    if (row == 0 && col == 2) return 1;
    if (row == 2 && col == 2) return 2;
    return 3;
}

static int spec_edge(int row, int col) {
    if (row == 0 && col == 1) return 0;
    if (row == 1 && col == 2) return 1;
    if (row == 2 && col == 1) return 2;
    return 3;
}

static int spec_slot(int face, int row, int col) {
    if (row == 1 && col == 1) return 48 + face;
    if (row % 2 == 0 && col % 2 == 0) return face * 4 + spec_corner(row, col);
    return 24 + face * 4 + spec_edge(row, col);
}

static void spec_pos(int face, int row, int col, int out[3]) {
    int r = row;
    int c = col;
    int table[6][3] = {
        {c - 1, 1, r - 1}, {-1, 1 - r, c - 1}, {c - 1, 1 - r, 1},
        {1, 1 - r, 1 - c},  {1 - c, 1 - r, -1}, {c - 1, -1, 1 - r},
    };
    for (int i = 0; i < 3; i++) {
        out[i] = table[face][i];
    }
}

static const int SPEC_NORMALS[6][3] = {
    {0, 1, 0}, {-1, 0, 0}, {0, 0, 1}, {1, 0, 0}, {0, 0, -1}, {0, -1, 0},
};

static void check_table(void) {
    const bt_replay_cell *cells = bt_replay_cells();
    for (int face = 0; face < 6; face++) {
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 3; col++) {
                int index = face * 9 + row * 3 + col;
                int pos[3];
                spec_pos(face, row, col, pos);
                const bt_replay_cell *cell = &cells[index];
                if (cell->slot != spec_slot(face, row, col)) {
                    fail("slot mismatch", index);
                }
                if (cell->face != face || cell->row != row || cell->col != col) {
                    fail("index metadata", index);
                }
                if (cell->pos.x != pos[0] || cell->pos.y != pos[1] || cell->pos.z != pos[2]) {
                    fail("pos mismatch", index);
                }
                if (cell->normal.x != SPEC_NORMALS[face][0] ||
                    cell->normal.y != SPEC_NORMALS[face][1] ||
                    cell->normal.z != SPEC_NORMALS[face][2]) {
                    fail("normal mismatch", index);
                }
            }
        }
    }
}

static void spec_rotate(const int v[3], int axis, int times, int out[3]) {
    int cur[3] = {v[0], v[1], v[2]};
    for (int t = 0; t < times; t++) {
        int nxt[3];
        if (axis == 0) {
            nxt[0] = cur[0];
            nxt[1] = -cur[2];
            nxt[2] = cur[1];
        } else if (axis == 1) {
            nxt[0] = cur[2];
            nxt[1] = cur[1];
            nxt[2] = -cur[0];
        } else {
            nxt[0] = -cur[1];
            nxt[1] = cur[0];
            nxt[2] = cur[2];
        }
        cur[0] = nxt[0];
        cur[1] = nxt[1];
        cur[2] = nxt[2];
    }
    out[0] = cur[0];
    out[1] = cur[1];
    out[2] = cur[2];
}

static void check_engine_oracle(void) {
    bt_move moves[72];
    size_t count = 0;
    for (uint8_t face = 0; face < 6; face++) {
        for (uint8_t quarter = 1; quarter <= 3; quarter++) {
            moves[count++] = bt_move_of_face(face, quarter);
            moves[count++] = bt_move_of_inner(face, quarter);
            moves[count++] = bt_move_of_wide(face, quarter);
        }
    }
    for (uint8_t slice = 0; slice < 3; slice++) {
        for (uint8_t quarter = 1; quarter <= 3; quarter++) {
            moves[count++] = bt_move_of_slice(slice, quarter);
        }
    }
    for (uint8_t axis = 0; axis < 3; axis++) {
        for (uint8_t quarter = 1; quarter <= 3; quarter++) {
            moves[count++] = bt_move_of_rotation(axis, quarter);
        }
    }
    const bt_replay_cell *cells = bt_replay_cells();
    for (size_t m = 0; m < count; m++) {
        bt_replay_turn turn = bt_replay_turn_of(moves[m]);
        bt_cube cube = bt_cube_solved();
        bt_cube_apply(&cube, moves[m]);
        for (size_t i = 0; i < 54; i++) {
            const bt_replay_cell *cell = &cells[i];
            bt_replay_i3 landed = cell->pos;
            bt_replay_i3 landed_normal = cell->normal;
            if (bt_replay_turn_rotates(&turn, cell->pos)) {
                int from[3] = {cell->pos.x, cell->pos.y, cell->pos.z};
                int from_normal[3] = {cell->normal.x, cell->normal.y, cell->normal.z};
                int to[3];
                int to_normal[3];
                spec_rotate(from, (int)turn.axis, turn.applied, to);
                spec_rotate(from_normal, (int)turn.axis, turn.applied, to_normal);
                landed.x = (int8_t)to[0];
                landed.y = (int8_t)to[1];
                landed.z = (int8_t)to[2];
                landed_normal.x = (int8_t)to_normal[0];
                landed_normal.y = (int8_t)to_normal[1];
                landed_normal.z = (int8_t)to_normal[2];
            }
            const bt_replay_cell *target = bt_replay_cell_at(landed, landed_normal);
            if (target == NULL) {
                fail("probe rotation missed face", (long)(m * 100 + i));
                continue;
            }
            if (target->slot != bt_cube_position_of(&cube, cell->slot)) {
                fail("engine oracle mismatch", (long)(m * 100 + i));
            }
        }
    }
    printf("engine oracle: %zu moves x 54 cells checked\n", count);
}

static void check_rotate_algebra(void) {
    bt_replay_i3 origin = {0, 0, 0};
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            for (int z = -1; z <= 1; z++) {
                bt_replay_i3 v = {(int8_t)x, (int8_t)y, (int8_t)z};
                for (int axis = 0; axis < 3; axis++) {
                    bt_replay_i3 four = bt_replay_rotate(v, (bt_axis)axis, 4);
                    bt_replay_i3 thrice = bt_replay_rotate(v, (bt_axis)axis, 3);
                    bt_replay_i3 back = bt_replay_rotate(thrice, (bt_axis)axis, 1);
                    if (four.x != v.x || four.y != v.y || four.z != v.z) {
                        fail("rotate 4 not identity", axis);
                    }
                    if (back.x != v.x || back.y != v.y || back.z != v.z) {
                        fail("rotate 3+1 not identity", axis);
                    }
                }
            }
        }
    }
    if (bt_replay_rotate(origin, BT_AXIS_Y, 2).x != 0) {
        fail("origin rotates", 0);
    }
}

static bt_json_value *parse(const char *text) {
    bt_json_value *root = NULL;
    bt_json_error error;
    if (bt_json_parse(text, strlen(text), &root, &error) != 0) {
        fail("probe json rejected", 0);
        return NULL;
    }
    return root;
}

static void check_cfg_semantics(void) {
    bt_replay_cfg defaults;
    bt_replay_cfg_defaults(&defaults);

    bt_json_value *mixed = parse(
        "{\"app\":{\"replay\":{\"yaw_deg\":30.0,\"distance\":\"nope\"}}}");
    bt_replay_cfg cfg;
    bt_replay_cfg_from(mixed, &cfg);
    bt_json_free(mixed);
    if (cfg.yaw_deg != defaults.yaw_deg || cfg.distance != defaults.distance) {
        fail("wrong-typed field must reset all to defaults", 0);
    }

    bt_json_value *integer_literal = parse("{\"app\":{\"replay\":{\"yaw_deg\":30}}}");
    bt_replay_cfg_from(integer_literal, &cfg);
    bt_json_free(integer_literal);
    if (cfg.yaw_deg != 30.0F) {
        fail("integer literal must parse as number", (long)cfg.yaw_deg);
    }

    bt_json_value *unknown = parse(
        "{\"app\":{\"replay\":{\"yaw_deg\":10.0,\"mystery\":true,\"extra\":[1,2]}}}");
    bt_replay_cfg_from(unknown, &cfg);
    bt_json_free(unknown);
    if (cfg.yaw_deg != 10.0F) {
        fail("unknown fields must be ignored", (long)cfg.yaw_deg);
    }

    bt_json_value *clamp3 = parse("{\"app\":{\"replay\":{\"pitch_clamp_deg\":[-70,0,70]}}}");
    bt_replay_cfg_from(clamp3, &cfg);
    bt_json_free(clamp3);
    if (cfg.pitch_clamp_deg[0] != defaults.pitch_clamp_deg[0] ||
        cfg.pitch_clamp_deg[1] != defaults.pitch_clamp_deg[1]) {
        fail("3-element clamp must reset all to defaults", 0);
    }

    bt_json_value *clamp_str = parse(
        "{\"app\":{\"replay\":{\"pitch_deg\":20.0,\"pitch_clamp_deg\":[-70,\"x\"]}}}");
    bt_replay_cfg_from(clamp_str, &cfg);
    bt_json_free(clamp_str);
    if (cfg.pitch_deg != defaults.pitch_deg) {
        fail("string clamp element must reset all to defaults", 0);
    }

    bt_json_value *null_section = parse("{\"app\":{\"replay\":null}}");
    bt_replay_cfg_from(null_section, &cfg);
    bt_json_free(null_section);
    if (cfg.distance != defaults.distance) {
        fail("null section must fall to defaults", 0);
    }

    if (bt_replay_wrap_yaw(-180.0F) != 180.0F) {
        fail("wrap_yaw(-180) must be 180", (long)bt_replay_wrap_yaw(-180.0F));
    }
    if (bt_replay_wrap_yaw(180.0F) != 180.0F) {
        fail("wrap_yaw(180) must stay 180", 0);
    }
}

static void check_camera_bounds(void) {
    struct {
        float yaw;
        float pitch;
        float dist;
    } cases[] = {
        {180.0F, 0.0F, 6.0F},  {-180.0F, 0.0F, 6.0F}, {0.0F, 80.0F, 6.0F},
        {0.0F, -80.0F, 6.0F},  {45.0F, 30.0F, 3.5F},  {225.0F, -30.0F, 12.0F},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        bt_replay_orbit orbit = {cases[i].yaw, cases[i].pitch, cases[i].dist};
        float got[3];
        bt_replay_camera_offset(&orbit, got);
        double yaw = (double)cases[i].yaw * 3.14159265358979323846 / 180.0;
        double pitch = (double)cases[i].pitch * 3.14159265358979323846 / 180.0;
        double want[3] = {
            (double)cases[i].dist * cos(pitch) * sin(yaw),
            (double)cases[i].dist * sin(pitch),
            (double)cases[i].dist * cos(pitch) * cos(yaw),
        };
        for (int k = 0; k < 3; k++) {
            if (fabs((double)got[k] - want[k]) > 1e-4) {
                fail("camera boundary", (long)(i * 10 + k));
            }
        }
        double radius = sqrt((double)got[0] * got[0] + (double)got[1] * got[1] +
                             (double)got[2] * got[2]);
        if (fabs(radius - (double)cases[i].dist) > 1e-4) {
            fail("camera off sphere", (long)i);
        }
    }
}

static void check_zero_secs_drain(void) {
    bt_move moves[3];
    moves[0] = bt_move_of_face(BT_FACE_R, 1);
    moves[1] = bt_move_of_face(BT_FACE_U, 2);
    moves[2] = bt_move_of_face(BT_FACE_F, 3);
    bt_replay_playback playback;
    bt_replay_playback_init(&playback, bt_cube_solved(), moves, 3);
    bt_replay_playback_toggle(&playback);
    bt_replay_playback_tick(&playback, 0.016, 0.0);
    if (playback.index != 3 || playback.playing) {
        fail("zero secs must drain all moves", (long)playback.index);
    }
}

int main(void) {
    check_table();
    check_engine_oracle();
    check_rotate_algebra();
    check_cfg_semantics();
    check_camera_bounds();
    check_zero_secs_drain();
    if (failures == 0) {
        printf("PROBE-OK\n");
        return 0;
    }
    printf("PROBE-FAILED %d\n", failures);
    return 1;
}
