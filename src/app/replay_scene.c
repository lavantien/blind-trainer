#include "app/replay_scene.h"

#include "common/bt_trace.h"

#include <math.h>

const bt_replay_i3 BT_REPLAY_FACE_NORMALS[6] = {
    {0, 1, 0},
    {-1, 0, 0},
    {0, 0, 1},
    {1, 0, 0},
    {0, 0, -1},
    {0, -1, 0},
};

static const uint8_t BT_REPLAY_FACE_AXIS[6] = {1, 0, 2, 0, 2, 1};
static const int8_t BT_REPLAY_FACE_SIGN[6] = {1, -1, 1, 1, -1, -1};
static const uint8_t BT_REPLAY_ROT_REF_FACE[3] = {3, 0, 2};

static bt_replay_cell cells_table[BT_REPLAY_CELL_COUNT];
static int cells_ready;

uint8_t bt_replay_corner_pos(uint8_t row, uint8_t col) {
    if (row == 0 && col == 0) {
        return 0;
    }
    if (row == 0 && col == 2) {
        return 1;
    }
    if (row == 2 && col == 2) {
        return 2;
    }
    if (row == 2 && col == 0) {
        return 3;
    }
    return 0;
}

uint8_t bt_replay_edge_pos(uint8_t row, uint8_t col) {
    if (row == 0 && col == 1) {
        return 0;
    }
    if (row == 1 && col == 2) {
        return 1;
    }
    if (row == 2 && col == 1) {
        return 2;
    }
    if (row == 1 && col == 0) {
        return 3;
    }
    return 0;
}

static uint8_t slot_of(uint8_t face, uint8_t row, uint8_t col) {
    if (row == 1 && col == 1) {
        return (uint8_t)(48 + face);
    }
    if (row % 2 == 0 && col % 2 == 0) {
        return (uint8_t)(face * 4 + bt_replay_corner_pos(row, col));
    }
    return (uint8_t)(24 + face * 4 + bt_replay_edge_pos(row, col));
}

static bt_replay_i3 pos_of(uint8_t face, uint8_t row, uint8_t col) {
    int8_t r = (int8_t)row;
    int8_t c = (int8_t)col;
    switch (face) {
    case 0:
        return (bt_replay_i3){(int8_t)(c - 1), 1, (int8_t)(r - 1)};
    case 1:
        return (bt_replay_i3){-1, (int8_t)(1 - r), (int8_t)(c - 1)};
    case 2:
        return (bt_replay_i3){(int8_t)(c - 1), (int8_t)(1 - r), 1};
    case 3:
        return (bt_replay_i3){1, (int8_t)(1 - r), (int8_t)(1 - c)};
    case 4:
        return (bt_replay_i3){(int8_t)(1 - c), (int8_t)(1 - r), -1};
    default:
        return (bt_replay_i3){(int8_t)(c - 1), -1, (int8_t)(1 - r)};
    }
}

static void build_cells(void) {
    size_t next = 0;
    for (uint8_t face = 0; face < 6; face++) {
        for (uint8_t row = 0; row < 3; row++) {
            for (uint8_t col = 0; col < 3; col++) {
                bt_replay_cell cell = {0};
                cell.slot = slot_of(face, row, col);
                cell.face = face;
                cell.row = row;
                cell.col = col;
                cell.pos = pos_of(face, row, col);
                cell.normal = BT_REPLAY_FACE_NORMALS[face];
                cells_table[next++] = cell;
            }
        }
    }
    cells_ready = 1;
}

const bt_replay_cell *bt_replay_cells(void) {
    BT_TRACE_ENTER("app.replay_cells");
    if (!cells_ready) {
        build_cells();
    }
    BT_TRACE_EXIT("app.replay_cells");
    return cells_table;
}

const bt_replay_cell *bt_replay_cell_at(bt_replay_i3 pos, bt_replay_i3 normal) {
    BT_TRACE_ENTER("app.replay_cell_at");
    if (!cells_ready) {
        build_cells();
    }
    for (size_t i = 0; i < BT_REPLAY_CELL_COUNT; i++) {
        const bt_replay_cell *cell = &cells_table[i];
        if (cell->pos.x == pos.x && cell->pos.y == pos.y && cell->pos.z == pos.z &&
            cell->normal.x == normal.x && cell->normal.y == normal.y && cell->normal.z == normal.z) {
            BT_TRACE_EXIT("app.replay_cell_at");
            return cell;
        }
    }
    BT_TRACE_FAIL("app.replay_cell_at", "no cell at %d,%d,%d normal %d,%d,%d", pos.x, pos.y, pos.z,
                  normal.x, normal.y, normal.z);
    BT_TRACE_EXIT("app.replay_cell_at");
    return NULL;
}

uint8_t bt_replay_home_face(uint8_t sticker) {
    BT_TRACE_ENTER("app.replay_home_face");
    uint8_t face;
    if (sticker < 24) {
        face = (uint8_t)(sticker / 4);
    } else if (sticker < 48) {
        face = (uint8_t)((sticker - 24) / 4);
    } else {
        face = (uint8_t)(sticker - 48);
    }
    BT_TRACE_EXIT("app.replay_home_face");
    return face;
}

static int8_t coord_along(bt_replay_i3 pos, bt_axis axis) {
    switch (axis) {
    case BT_AXIS_X:
        return pos.x;
    case BT_AXIS_Y:
        return pos.y;
    default:
        return pos.z;
    }
}

static uint8_t applied_quarters(uint8_t face, uint8_t quarter) {
    if (BT_REPLAY_FACE_SIGN[face] < 0) {
        return quarter;
    }
    return (uint8_t)(4 - quarter);
}

static bt_axis face_axis_of(uint8_t face) {
    return (bt_axis)BT_REPLAY_FACE_AXIS[face];
}

static void single_layer(uint8_t face, bool mask[3]) {
    bt_replay_i3 normal = BT_REPLAY_FACE_NORMALS[face];
    uint8_t axis = BT_REPLAY_FACE_AXIS[face];
    int8_t coord;
    switch (axis) {
    case 0:
        coord = normal.x;
        break;
    case 1:
        coord = normal.y;
        break;
    default:
        coord = normal.z;
        break;
    }
    mask[0] = false;
    mask[1] = false;
    mask[2] = false;
    mask[(size_t)(coord + 1)] = true;
}

static uint8_t slice_face(uint8_t slice) {
    switch (slice) {
    case BT_SLICE_M:
        return 1;
    case BT_SLICE_E:
        return 5;
    default:
        return 2;
    }
}

bt_replay_turn bt_replay_turn_of(bt_move mv) {
    BT_TRACE_ENTER("app.replay_turn_of");
    bt_replay_turn turn = {0};
    switch (mv.kind) {
    case BT_MOVE_FACE:
        turn.axis = face_axis_of(mv.face);
        turn.applied = applied_quarters(mv.face, mv.quarter);
        single_layer(mv.face, turn.mask);
        break;
    case BT_MOVE_INNER:
        turn.axis = face_axis_of(mv.face);
        turn.applied = applied_quarters(mv.face, mv.quarter);
        turn.mask[0] = false;
        turn.mask[1] = true;
        turn.mask[2] = false;
        break;
    case BT_MOVE_WIDE:
        turn.axis = face_axis_of(mv.face);
        turn.applied = applied_quarters(mv.face, mv.quarter);
        single_layer(mv.face, turn.mask);
        turn.mask[1] = true;
        break;
    case BT_MOVE_SLICE: {
        uint8_t face = slice_face(mv.slice);
        turn.axis = face_axis_of(face);
        turn.applied = applied_quarters(face, mv.quarter);
        turn.mask[0] = false;
        turn.mask[1] = true;
        turn.mask[2] = false;
        break;
    }
    default:
        turn.axis = (bt_axis)mv.axis;
        turn.applied = applied_quarters(BT_REPLAY_ROT_REF_FACE[mv.axis], mv.quarter);
        turn.mask[0] = true;
        turn.mask[1] = true;
        turn.mask[2] = true;
        break;
    }
    BT_TRACE_EXIT("app.replay_turn_of");
    return turn;
}

bool bt_replay_turn_rotates(const bt_replay_turn *turn, bt_replay_i3 pos) {
    BT_TRACE_ENTER("app.replay_turn_rotates");
    bool rotates = turn->mask[(size_t)(coord_along(pos, turn->axis) + 1)];
    BT_TRACE_EXIT("app.replay_turn_rotates");
    return rotates;
}

float bt_replay_turn_angle_deg(const bt_replay_turn *turn, float progress) {
    BT_TRACE_ENTER("app.replay_turn_angle_deg");
    float angle = (float)turn->applied * 90.0F * progress;
    BT_TRACE_EXIT("app.replay_turn_angle_deg");
    return angle;
}

static bt_replay_i3 rot_once(bt_replay_i3 v, uint8_t axis) {
    int8_t x = v.x;
    int8_t y = v.y;
    int8_t z = v.z;
    switch (axis) {
    case 0:
        return (bt_replay_i3){x, (int8_t)(-z), y};
    case 1:
        return (bt_replay_i3){z, y, (int8_t)(-x)};
    default:
        return (bt_replay_i3){(int8_t)(-y), x, z};
    }
}

bt_replay_i3 bt_replay_rotate(bt_replay_i3 v, bt_axis axis, uint8_t times) {
    BT_TRACE_ENTER("app.replay_rotate");
    bt_replay_i3 out = v;
    for (uint8_t i = 0; i < times; i++) {
        out = rot_once(out, (uint8_t)axis);
    }
    BT_TRACE_EXIT("app.replay_rotate");
    return out;
}

const bt_replay_cell *bt_replay_turned_cell(const bt_replay_cell *cell, const bt_replay_turn *turn) {
    BT_TRACE_ENTER("app.replay_turned_cell");
    const bt_replay_cell *out;
    if (!bt_replay_turn_rotates(turn, cell->pos)) {
        out = bt_replay_cell_at(cell->pos, cell->normal);
    } else {
        out = bt_replay_cell_at(bt_replay_rotate(cell->pos, turn->axis, turn->applied),
                                bt_replay_rotate(cell->normal, turn->axis, turn->applied));
    }
    if (out == NULL) {
        BT_TRACE_FAIL("app.replay_turned_cell", "turned cell slot %u missed its face", cell->slot);
    }
    BT_TRACE_EXIT("app.replay_turned_cell");
    return out;
}

void bt_replay_cfg_defaults(bt_replay_cfg *cfg) {
    BT_TRACE_ENTER("app.replay_cfg_defaults");
    cfg->secs_per_move = BT_REPLAY_DEFAULT_SECS_PER_MOVE;
    cfg->yaw_deg = BT_REPLAY_DEFAULT_YAW_DEG;
    cfg->pitch_deg = BT_REPLAY_DEFAULT_PITCH_DEG;
    cfg->distance = BT_REPLAY_DEFAULT_DISTANCE;
    cfg->zoom_min = BT_REPLAY_DEFAULT_ZOOM_MIN;
    cfg->zoom_max = BT_REPLAY_DEFAULT_ZOOM_MAX;
    cfg->pitch_clamp_deg[0] = BT_REPLAY_DEFAULT_PITCH_CLAMP_LO;
    cfg->pitch_clamp_deg[1] = BT_REPLAY_DEFAULT_PITCH_CLAMP_HI;
    BT_TRACE_EXIT("app.replay_cfg_defaults");
}

float bt_replay_wrap_yaw(float deg) {
    BT_TRACE_ENTER("app.replay_wrap_yaw");
    float out = fmodf(deg, 360.0F);
    if (out <= -180.0F) {
        out += 360.0F;
    }
    if (out > 180.0F) {
        out -= 360.0F;
    }
    BT_TRACE_EXIT("app.replay_wrap_yaw");
    return out;
}

static float clampf(float value, float lo, float hi) {
    if (value < lo) {
        return lo;
    }
    if (value > hi) {
        return hi;
    }
    return value;
}

bt_replay_cfg bt_replay_cfg_sanitized(bt_replay_cfg cfg) {
    BT_TRACE_ENTER("app.replay_cfg_sanitized");
    float lo = cfg.zoom_min;
    float hi = cfg.zoom_max;
    if (lo > hi) {
        float swap = lo;
        lo = hi;
        hi = swap;
    }
    float pitch_lo = cfg.pitch_clamp_deg[0];
    float pitch_hi = cfg.pitch_clamp_deg[1];
    if (pitch_lo > pitch_hi) {
        float swap = pitch_lo;
        pitch_lo = pitch_hi;
        pitch_hi = swap;
    }
    bt_replay_cfg out;
    out.secs_per_move = cfg.secs_per_move > 0.0 ? cfg.secs_per_move : 0.0;
    out.yaw_deg = bt_replay_wrap_yaw(cfg.yaw_deg);
    out.pitch_deg = clampf(cfg.pitch_deg, pitch_lo, pitch_hi);
    out.distance = clampf(cfg.distance, lo, hi);
    out.zoom_min = lo;
    out.zoom_max = hi;
    out.pitch_clamp_deg[0] = pitch_lo;
    out.pitch_clamp_deg[1] = pitch_hi;
    BT_TRACE_EXIT("app.replay_cfg_sanitized");
    return out;
}

static int number_field(const bt_json_value *section, const char *key, double *out) {
    const bt_json_value *field = bt_json_get(section, key);
    if (field == NULL) {
        return 0;
    }
    if (field->type != BT_JSON_NUMBER) {
        return -1;
    }
    *out = field->as.number;
    return 1;
}

void bt_replay_cfg_from(const bt_json_value *value, bt_replay_cfg *out) {
    BT_TRACE_ENTER("app.replay_cfg_from");
    bt_replay_cfg_defaults(out);
    const bt_json_value *app = NULL;
    if (value != NULL && value->type == BT_JSON_OBJECT) {
        app = bt_json_get(value, "app");
    }
    const bt_json_value *section = NULL;
    if (app != NULL && app->type == BT_JSON_OBJECT) {
        section = bt_json_get(app, "replay");
    }
    if (section == NULL) {
        BT_TRACE_EXIT("app.replay_cfg_from");
        *out = bt_replay_cfg_sanitized(*out);
        return;
    }
    if (section->type != BT_JSON_OBJECT) {
        BT_TRACE_FAIL("app.replay_cfg_from", "replay section malformed, defaults apply");
        BT_TRACE_EXIT("app.replay_cfg_from");
        *out = bt_replay_cfg_sanitized(*out);
        return;
    }
    int failed = 0;
    double number = 0;
    int present = number_field(section, "secs_per_move", &number);
    if (present < 0) {
        failed = 1;
    } else if (present) {
        out->secs_per_move = number;
    }
    present = number_field(section, "yaw_deg", &number);
    if (present < 0) {
        failed = 1;
    } else if (present) {
        out->yaw_deg = (float)number;
    }
    present = number_field(section, "pitch_deg", &number);
    if (present < 0) {
        failed = 1;
    } else if (present) {
        out->pitch_deg = (float)number;
    }
    present = number_field(section, "distance", &number);
    if (present < 0) {
        failed = 1;
    } else if (present) {
        out->distance = (float)number;
    }
    present = number_field(section, "zoom_min", &number);
    if (present < 0) {
        failed = 1;
    } else if (present) {
        out->zoom_min = (float)number;
    }
    present = number_field(section, "zoom_max", &number);
    if (present < 0) {
        failed = 1;
    } else if (present) {
        out->zoom_max = (float)number;
    }
    const bt_json_value *clamp = bt_json_get(section, "pitch_clamp_deg");
    if (clamp != NULL) {
        if (clamp->type != BT_JSON_ARRAY || clamp->as.array.count != 2 ||
            bt_json_at(clamp, 0) == NULL || bt_json_at(clamp, 0)->type != BT_JSON_NUMBER ||
            bt_json_at(clamp, 1) == NULL || bt_json_at(clamp, 1)->type != BT_JSON_NUMBER) {
            failed = 1;
        } else {
            out->pitch_clamp_deg[0] = (float)bt_json_at(clamp, 0)->as.number;
            out->pitch_clamp_deg[1] = (float)bt_json_at(clamp, 1)->as.number;
        }
    }
    if (failed) {
        BT_TRACE_FAIL("app.replay_cfg_from", "replay field malformed, defaults apply");
        bt_replay_cfg_defaults(out);
    }
    *out = bt_replay_cfg_sanitized(*out);
    BT_TRACE_EXIT("app.replay_cfg_from");
}

bt_replay_orbit bt_replay_default_orbit(const bt_replay_cfg *cfg) {
    BT_TRACE_ENTER("app.replay_default_orbit");
    bt_replay_orbit orbit = {bt_replay_wrap_yaw(cfg->yaw_deg), cfg->pitch_deg, cfg->distance};
    BT_TRACE_EXIT("app.replay_default_orbit");
    return orbit;
}

bt_replay_orbit bt_replay_apply_drag_delta(bt_replay_orbit orbit, const bt_replay_cfg *cfg,
                                           float dx_px, float dy_px) {
    BT_TRACE_ENTER("app.replay_apply_drag_delta");
    bt_replay_orbit out;
    out.yaw_deg = bt_replay_wrap_yaw(orbit.yaw_deg + dx_px * BT_REPLAY_DRAG_DEG_PER_PX);
    out.pitch_deg =
        clampf(orbit.pitch_deg + dy_px * BT_REPLAY_DRAG_DEG_PER_PX, cfg->pitch_clamp_deg[0],
               cfg->pitch_clamp_deg[1]);
    out.distance = clampf(orbit.distance, cfg->zoom_min, cfg->zoom_max);
    BT_TRACE_EXIT("app.replay_apply_drag_delta");
    return out;
}

bt_replay_orbit bt_replay_apply_zoom(bt_replay_orbit orbit, const bt_replay_cfg *cfg, float notches) {
    BT_TRACE_ENTER("app.replay_apply_zoom");
    bt_replay_orbit out;
    out.yaw_deg = orbit.yaw_deg;
    out.pitch_deg = orbit.pitch_deg;
    out.distance =
        clampf(orbit.distance - notches * BT_REPLAY_ZOOM_UNITS_PER_NOTCH, cfg->zoom_min, cfg->zoom_max);
    BT_TRACE_EXIT("app.replay_apply_zoom");
    return out;
}

void bt_replay_camera_offset(const bt_replay_orbit *orbit, float out[3]) {
    BT_TRACE_ENTER("app.replay_camera_offset");
    float yaw = orbit->yaw_deg * (float)(3.14159265358979323846 / 180.0);
    float pitch = orbit->pitch_deg * (float)(3.14159265358979323846 / 180.0);
    float sin_yaw = sinf(yaw);
    float cos_yaw = cosf(yaw);
    float sin_pitch = sinf(pitch);
    float cos_pitch = cosf(pitch);
    out[0] = orbit->distance * cos_pitch * sin_yaw;
    out[1] = orbit->distance * sin_pitch;
    out[2] = orbit->distance * cos_pitch * cos_yaw;
    BT_TRACE_EXIT("app.replay_camera_offset");
}

float bt_replay_turn_progress(double elapsed_secs, double secs_per_move) {
    BT_TRACE_ENTER("app.replay_turn_progress");
    if (secs_per_move <= 0.0) {
        BT_TRACE_EXIT("app.replay_turn_progress");
        return 1.0F;
    }
    double clamped = elapsed_secs / secs_per_move;
    if (clamped < 0.0) {
        clamped = 0.0;
    }
    if (clamped > 1.0) {
        clamped = 1.0;
    }
    float t = (float)clamped;
    float progress = t * t * (3.0F - 2.0F * t);
    BT_TRACE_EXIT("app.replay_turn_progress");
    return progress;
}

void bt_replay_playback_init(bt_replay_playback *playback, bt_cube start, const bt_move *moves,
                             size_t moves_len) {
    BT_TRACE_ENTER("app.replay_playback_init");
    playback->cube = start;
    playback->moves = moves;
    playback->moves_len = moves_len;
    playback->index = 0;
    playback->elapsed = 0.0;
    playback->playing = false;
    playback->settle_gen = 1;
    BT_TRACE_EXIT("app.replay_playback_init");
}

bool bt_replay_playback_finished(const bt_replay_playback *playback) {
    BT_TRACE_ENTER("app.replay_playback_finished");
    bool finished = playback->index >= playback->moves_len;
    BT_TRACE_EXIT("app.replay_playback_finished");
    return finished;
}

void bt_replay_playback_toggle(bt_replay_playback *playback) {
    BT_TRACE_ENTER("app.replay_playback_toggle");
    if (!bt_replay_playback_finished(playback)) {
        playback->playing = (bool)!playback->playing;
    }
    BT_TRACE_EXIT("app.replay_playback_toggle");
}

bool bt_replay_playback_current_turn(const bt_replay_playback *playback, bt_replay_turn *out) {
    BT_TRACE_ENTER("app.replay_playback_current_turn");
    if (bt_replay_playback_finished(playback)) {
        BT_TRACE_EXIT("app.replay_playback_current_turn");
        return false;
    }
    *out = bt_replay_turn_of(playback->moves[playback->index]);
    BT_TRACE_EXIT("app.replay_playback_current_turn");
    return true;
}

float bt_replay_playback_current_progress(const bt_replay_playback *playback, double secs_per_move) {
    BT_TRACE_ENTER("app.replay_playback_current_progress");
    float progress;
    if (bt_replay_playback_finished(playback)) {
        progress = 0.0F;
    } else {
        progress = bt_replay_turn_progress(playback->elapsed, secs_per_move);
    }
    BT_TRACE_EXIT("app.replay_playback_current_progress");
    return progress;
}

uint8_t bt_replay_playback_sticker_face(const bt_replay_playback *playback,
                                        const bt_replay_cell *cell) {
    BT_TRACE_ENTER("app.replay_playback_sticker_face");
    uint8_t face = bt_replay_home_face(bt_cube_sticker_at(&playback->cube, cell->slot));
    BT_TRACE_EXIT("app.replay_playback_sticker_face");
    return face;
}

void bt_replay_playback_tick(bt_replay_playback *playback, double dt, double secs_per_move) {
    BT_TRACE_ENTER("app.replay_playback_tick");
    if (!playback->playing) {
        BT_TRACE_EXIT("app.replay_playback_tick");
        return;
    }
    if (dt > 0.0) {
        playback->elapsed += dt;
    }
    double step = secs_per_move > 0.0 ? secs_per_move : 0.0;
    while (playback->elapsed >= secs_per_move && playback->index < playback->moves_len) {
        bt_cube_apply(&playback->cube, playback->moves[playback->index]);
        playback->index++;
        playback->elapsed -= step;
        playback->settle_gen++;
    }
    if (bt_replay_playback_finished(playback)) {
        playback->playing = false;
        playback->elapsed = 0.0;
    }
    BT_TRACE_EXIT("app.replay_playback_tick");
}
