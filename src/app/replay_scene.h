#ifndef BT_APP_REPLAY_SCENE_H
#define BT_APP_REPLAY_SCENE_H

#include "common/json.h"
#include "core/cube.h"
#include "core/moves.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BT_REPLAY_DEFAULT_SECS_PER_MOVE 0.5
#define BT_REPLAY_DEFAULT_YAW_DEG 45.0F
#define BT_REPLAY_DEFAULT_PITCH_DEG 30.0F
#define BT_REPLAY_DEFAULT_DISTANCE 6.0F
#define BT_REPLAY_DEFAULT_ZOOM_MIN 3.5F
#define BT_REPLAY_DEFAULT_ZOOM_MAX 12.0F
#define BT_REPLAY_DEFAULT_PITCH_CLAMP_LO -80.0F
#define BT_REPLAY_DEFAULT_PITCH_CLAMP_HI 80.0F
#define BT_REPLAY_DRAG_DEG_PER_PX 0.4F
#define BT_REPLAY_ZOOM_UNITS_PER_NOTCH 0.75F
#define BT_REPLAY_CUBIE_SPAN 0.94F
#define BT_REPLAY_STICKER_SPAN 0.82F
#define BT_REPLAY_STICKER_LIFT 0.475F

#define BT_REPLAY_CELL_COUNT 54

typedef struct {
    int8_t x;
    int8_t y;
    int8_t z;
} bt_replay_i3;

typedef struct {
    uint8_t slot;
    uint8_t face;
    uint8_t row;
    uint8_t col;
    bt_replay_i3 pos;
    bt_replay_i3 normal;
} bt_replay_cell;

extern const bt_replay_i3 BT_REPLAY_FACE_NORMALS[6];

const bt_replay_cell *bt_replay_cells(void);
const bt_replay_cell *bt_replay_cell_at(bt_replay_i3 pos, bt_replay_i3 normal);
uint8_t bt_replay_home_face(uint8_t sticker);
uint8_t bt_replay_corner_pos(uint8_t row, uint8_t col);
uint8_t bt_replay_edge_pos(uint8_t row, uint8_t col);

typedef struct {
    bt_axis axis;
    uint8_t applied;
    bool mask[3];
} bt_replay_turn;

bool bt_replay_turn_rotates(const bt_replay_turn *turn, bt_replay_i3 pos);
float bt_replay_turn_angle_deg(const bt_replay_turn *turn, float progress);
bt_replay_turn bt_replay_turn_of(bt_move mv);
bt_replay_i3 bt_replay_rotate(bt_replay_i3 v, bt_axis axis, uint8_t times);
const bt_replay_cell *bt_replay_turned_cell(const bt_replay_cell *cell, const bt_replay_turn *turn);

typedef struct {
    double secs_per_move;
    float yaw_deg;
    float pitch_deg;
    float distance;
    float zoom_min;
    float zoom_max;
    float pitch_clamp_deg[2];
} bt_replay_cfg;

void bt_replay_cfg_defaults(bt_replay_cfg *cfg);
bt_replay_cfg bt_replay_cfg_sanitized(bt_replay_cfg cfg);
void bt_replay_cfg_from(const bt_json_value *value, bt_replay_cfg *out);

typedef struct {
    float yaw_deg;
    float pitch_deg;
    float distance;
} bt_replay_orbit;

bt_replay_orbit bt_replay_default_orbit(const bt_replay_cfg *cfg);
float bt_replay_wrap_yaw(float deg);
bt_replay_orbit bt_replay_apply_drag_delta(bt_replay_orbit orbit, const bt_replay_cfg *cfg,
                                           float dx_px, float dy_px);
bt_replay_orbit bt_replay_apply_zoom(bt_replay_orbit orbit, const bt_replay_cfg *cfg,
                                     float notches);
void bt_replay_camera_offset(const bt_replay_orbit *orbit, float out[3]);
float bt_replay_turn_progress(double elapsed_secs, double secs_per_move);

typedef struct {
    bt_cube cube;
    const bt_move *moves;
    size_t moves_len;
    size_t index;
    double elapsed;
    bool playing;
    uint64_t settle_gen;
} bt_replay_playback;

void bt_replay_playback_init(bt_replay_playback *playback, bt_cube start, const bt_move *moves,
                             size_t moves_len);
bool bt_replay_playback_finished(const bt_replay_playback *playback);
void bt_replay_playback_toggle(bt_replay_playback *playback);
bool bt_replay_playback_current_turn(const bt_replay_playback *playback, bt_replay_turn *out);
float bt_replay_playback_current_progress(const bt_replay_playback *playback, double secs_per_move);
uint8_t bt_replay_playback_sticker_face(const bt_replay_playback *playback,
                                        const bt_replay_cell *cell);
void bt_replay_playback_tick(bt_replay_playback *playback, double dt, double secs_per_move);

#endif
