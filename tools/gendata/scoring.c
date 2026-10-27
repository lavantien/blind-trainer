#include "gendata/scoring.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "core/cube.h"

enum { GD_LANE_SLICE_BASE = 6 };

static int face_lane(uint8_t face) {
    return (int)face;
}

static int slice_lane(uint8_t slice) {
    return GD_LANE_SLICE_BASE + (int)slice;
}

static uint8_t slice_follows(uint8_t slice) {
    switch (slice) {
    case BT_SLICE_M:
        return BT_FACE_L;
    case BT_SLICE_E:
        return BT_FACE_D;
    default:
        return BT_FACE_F;
    }
}

static bool finger_face_of(bt_move mv, uint8_t *face) {
    switch ((bt_move_kind)mv.kind) {
    case BT_MOVE_ROTATION:
        return false;
    case BT_MOVE_SLICE:
        *face = slice_follows(mv.slice);
        return true;
    default:
        *face = mv.face;
        return true;
    }
}

static bool preferred_lane_of(bt_move mv, bt_orbit orbit, int *lane) {
    switch ((bt_move_kind)mv.kind) {
    case BT_MOVE_ROTATION:
        return false;
    case BT_MOVE_SLICE:
        *lane = orbit == BT_ORBIT_EDGE ? slice_lane(mv.slice) : face_lane(slice_follows(mv.slice));
        return true;
    default:
        *lane = face_lane(mv.face);
        return true;
    }
}

static int side_of(uint8_t face) {
    switch (face) {
    case BT_FACE_U:
    case BT_FACE_R:
        return 0;
    case BT_FACE_L:
    case BT_FACE_D:
        return 1;
    default:
        return -1;
    }
}

bool bt_gd_count_regrips(const bt_move *moves, size_t count, uint32_t *out) {
    bool displaced[2] = {false, false};
    size_t last = 0;
    bool has_prev = false;
    uint8_t prev = 0;
    uint32_t regrips = 0;
    for (size_t i = 0; i < count; i++) {
        uint8_t face = 0;
        if (!finger_face_of(moves[i], &face)) {
            return false;
        }
        if (has_prev && prev == face) {
            continue;
        }
        prev = face;
        has_prev = true;
        int side = side_of(face);
        size_t hand = side >= 0 ? (size_t)side : last;
        if (side >= 0) {
            last = hand;
        }
        if (displaced[hand]) {
            regrips++;
        }
        displaced[hand] = side < 0;
    }
    *out = regrips;
    return true;
}

bool bt_gd_score(const bt_move *moves, size_t count, bt_orbit orbit, const bt_gd_scoring_cfg *cfg,
                 bt_gd_score_parts *out) {
    const bool *preferred = orbit == BT_ORBIT_EDGE ? cfg->preferred_edge : cfg->preferred;
    uint32_t qtm = 0;
    float subset = 0.0F;
    float finger = 0.0F;
    for (size_t i = 0; i < count; i++) {
        uint8_t quarter = bt_move_quarter(moves[i]);
        qtm += quarter == 2 ? 2 : 1;
        int lane = 0;
        if (!preferred_lane_of(moves[i], orbit, &lane)) {
            return false;
        }
        if (!preferred[lane]) {
            subset += cfg->penalty_other;
        }
        uint8_t face = 0;
        if (!finger_face_of(moves[i], &face)) {
            return false;
        }
        size_t dir = quarter == 3 ? 1 : 0;
        finger += cfg->finger_cost[face][dir];
    }
    uint32_t regrips = 0;
    if (!bt_gd_count_regrips(moves, count, &regrips)) {
        return false;
    }
    float total = cfg->weight_len * (float)qtm;
    total = total + cfg->weight_subset * subset;
    total = total + cfg->weight_regrip * (float)regrips;
    total = total + cfg->weight_finger * finger;
    if (out != NULL) {
        *out = (bt_gd_score_parts){
            .qtm = qtm,
            .subset = subset,
            .regrips = regrips,
            .finger = finger,
            .total = total,
        };
    }
    return true;
}

static uint32_t f32_total_key(float value) {
    uint32_t bits = 0;
    memcpy(&bits, &value, sizeof bits);
    if ((bits >> 31U) == 0) {
        return bits | 0x80000000U;
    }
    return ~bits;
}

static int f32_total_cmp(float left, float right) {
    uint32_t a = f32_total_key(left);
    uint32_t b = f32_total_key(right);
    if (a < b) {
        return -1;
    }
    if (a > b) {
        return 1;
    }
    return 0;
}

static float total_of(const bt_move *moves, size_t count, bt_orbit orbit,
                      const bt_gd_scoring_cfg *cfg) {
    bt_gd_score_parts parts = {0};
    if (bt_gd_score(moves, count, orbit, cfg, &parts)) {
        return parts.total;
    }
    return INFINITY;
}

int bt_gd_rank_cmp(const bt_move *left, size_t left_len, const bt_move *right, size_t right_len,
                   bt_orbit orbit, const bt_gd_scoring_cfg *cfg) {
    float left_total = total_of(left, left_len, orbit, cfg);
    float right_total = total_of(right, right_len, orbit, cfg);
    int by_total = f32_total_cmp(left_total, right_total);
    if (by_total != 0) {
        return by_total;
    }
    if (left_len < right_len) {
        return -1;
    }
    if (left_len > right_len) {
        return 1;
    }
    return 0;
}

static int parse_lanes(const bt_json_value *value, const char *key, bool out[9], char *err,
                       size_t err_len) {
    const char *lanes_message =
        "synth.scoring.%s entries must be single lanes from U D L R F B M E S";
    const char *empty_message = "synth.scoring.%s must list at least one lane";
    if (value == NULL || value->type != BT_JSON_ARRAY) {
        snprintf(err, err_len, lanes_message, key);
        return -1;
    }
    if (value->as.array.count == 0) {
        snprintf(err, err_len, empty_message, key);
        return -1;
    }
    for (size_t i = 0; i < 9; i++) {
        out[i] = false;
    }
    for (size_t i = 0; i < value->as.array.count; i++) {
        const bt_json_value *entry = bt_json_at(value, i);
        if (entry == NULL || entry->type != BT_JSON_STRING || strlen(entry->as.string) != 1) {
            snprintf(err, err_len, lanes_message, key);
            return -1;
        }
        char letter = entry->as.string[0];
        int lane = -1;
        if (letter == 'U') {
            lane = BT_FACE_U;
        } else if (letter == 'L') {
            lane = BT_FACE_L;
        } else if (letter == 'F') {
            lane = BT_FACE_F;
        } else if (letter == 'R') {
            lane = BT_FACE_R;
        } else if (letter == 'B') {
            lane = BT_FACE_B;
        } else if (letter == 'D') {
            lane = BT_FACE_D;
        } else if (letter == 'M') {
            lane = GD_LANE_SLICE_BASE;
        } else if (letter == 'E') {
            lane = GD_LANE_SLICE_BASE + 1;
        } else if (letter == 'S') {
            lane = GD_LANE_SLICE_BASE + 2;
        }
        if (lane < 0) {
            snprintf(err, err_len, lanes_message, key);
            return -1;
        }
        out[lane] = true;
    }
    return 0;
}

static int parse_number(const bt_json_value *value, const char *key, bool floor_zero, float *out,
                        char *err, size_t err_len) {
    const char *suffix = floor_zero ? " >= 0" : "";
    if (value == NULL || value->type != BT_JSON_NUMBER) {
        snprintf(err, err_len, "synth.scoring.%s must be a finite number%s", key, suffix);
        return -1;
    }
    double raw = value->is_integer ? (double)value->integer : value->as.number;
    float parsed = (float)raw;
    if (!isfinite(parsed) || (floor_zero && parsed < 0.0F)) {
        snprintf(err, err_len, "synth.scoring.%s must be a finite number%s", key, suffix);
        return -1;
    }
    *out = parsed;
    return 0;
}

static const bt_json_value *required_field(const bt_json_value *section, const char *key, char *err,
                                           size_t err_len) {
    const bt_json_value *node = bt_json_get(section, key);
    if (node == NULL || node->type == BT_JSON_NULL) {
        snprintf(err, err_len, "config section missing: synth.scoring.%s", key);
        return NULL;
    }
    return node;
}

static int parse_finger_cost(const bt_json_value *value, const char *key, float out[6][2],
                             char *err, size_t err_len) {
    const char *lanes_message = "synth.scoring.%s must hold exactly the lanes U D L R F B";
    if (value == NULL || value->type != BT_JSON_OBJECT) {
        snprintf(err, err_len, lanes_message, key);
        return -1;
    }
    static const uint8_t faces[6] = {
        BT_FACE_U, BT_FACE_L, BT_FACE_F, BT_FACE_R, BT_FACE_B, BT_FACE_D,
    };
    static const char letters[6] = {'U', 'L', 'F', 'R', 'B', 'D'};
    for (size_t f = 0; f < 6; f++) {
        char name[2] = {letters[f], '\0'};
        char entry_message[96];
        snprintf(entry_message, sizeof entry_message,
                 "synth.scoring.%s.%c must be [cw, ccw] finite numbers", key, letters[f]);
        const bt_json_value *pair = bt_json_get(value, name);
        if (pair == NULL || pair->type != BT_JSON_ARRAY || pair->as.array.count != 2) {
            snprintf(err, err_len, "%s", entry_message);
            return -1;
        }
        for (size_t dir = 0; dir < 2; dir++) {
            const bt_json_value *half = bt_json_at(pair, dir);
            if (half == NULL || half->type != BT_JSON_NUMBER) {
                snprintf(err, err_len, "%s", entry_message);
                return -1;
            }
            double raw = half->is_integer ? (double)half->integer : half->as.number;
            float parsed = (float)raw;
            if (!isfinite(parsed)) {
                snprintf(err, err_len, "%s", entry_message);
                return -1;
            }
            out[faces[f]][dir] = parsed;
        }
    }
    if (value->as.object.count != 6) {
        snprintf(err, err_len, lanes_message, key);
        return -1;
    }
    return 0;
}

static int load_lanes(const bt_json_value *section, const char *key, bool out[9], char *err,
                      size_t err_len) {
    const bt_json_value *field = required_field(section, key, err, err_len);
    if (field == NULL) {
        return -1;
    }
    return parse_lanes(field, key, out, err, err_len);
}

static int load_number(const bt_json_value *section, const char *key, bool floor_zero, float *out,
                       char *err, size_t err_len) {
    const bt_json_value *field = required_field(section, key, err, err_len);
    if (field == NULL) {
        return -1;
    }
    return parse_number(field, key, floor_zero, out, err, err_len);
}

static int load_finger_cost(const bt_json_value *section, const char *key, float out[6][2],
                            char *err, size_t err_len) {
    const bt_json_value *field = required_field(section, key, err, err_len);
    if (field == NULL) {
        return -1;
    }
    return parse_finger_cost(field, key, out, err, err_len);
}

int bt_gd_scoring_from_config(const bt_json_value *config, bt_gd_scoring_cfg *out, char *err,
                              size_t err_len) {
    BT_TRACE_ENTER("gd.scoring.from_config");
    const bt_json_value *synth = config != NULL ? bt_json_get(config, "synth") : NULL;
    const bt_json_value *section = synth != NULL ? bt_json_get(synth, "scoring") : NULL;
    if (section == NULL || section->type == BT_JSON_NULL) {
        snprintf(err, err_len, "%s", "config section missing: synth.scoring");
        BT_TRACE_FAIL("gd.scoring.from_config", err);
        return -1;
    }
    if (load_lanes(section, "preferred", out->preferred, err, err_len) != 0 ||
        load_lanes(section, "preferred_edge", out->preferred_edge, err, err_len) != 0 ||
        load_number(section, "penalty_other", false, &out->penalty_other, err, err_len) != 0 ||
        load_number(section, "weight_len", true, &out->weight_len, err, err_len) != 0 ||
        load_number(section, "weight_subset", true, &out->weight_subset, err, err_len) != 0 ||
        load_number(section, "weight_regrip", true, &out->weight_regrip, err, err_len) != 0 ||
        load_number(section, "weight_finger", true, &out->weight_finger, err, err_len) != 0 ||
        load_finger_cost(section, "finger_cost", out->finger_cost, err, err_len) != 0) {
        BT_TRACE_FAIL("gd.scoring.from_config", err);
        return -1;
    }
    BT_TRACE_EXIT("gd.scoring.from_config");
    return 0;
}
