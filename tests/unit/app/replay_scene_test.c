#include "app/replay_scene.h"

#include "common.h"

#include "core/cube.h"

#include <math.h>
#include <string.h>

static bt_replay_cfg replay_cfg_defaults(void) {
    bt_replay_cfg out;
    bt_replay_cfg_defaults(&out);
    return out;
}

static bt_replay_orbit default_orbit(void) {
    bt_replay_cfg config = replay_cfg_defaults();
    return bt_replay_default_orbit(&config);
}

static bt_replay_i3 i3(int x, int y, int z) {
    bt_replay_i3 out = {(int8_t)x, (int8_t)y, (int8_t)z};
    return out;
}

static int i3_eq(bt_replay_i3 a, bt_replay_i3 b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

static int mask_is(const bt_replay_turn *turn, bool a, bool b, bool c) {
    return turn->mask[0] == a && turn->mask[1] == b && turn->mask[2] == c;
}

static bt_json_value *fixture_json(const char *text) {
    bt_json_value *root = NULL;
    bt_json_error error;
    if (bt_json_parse(text, strlen(text), &root, &error) != 0) {
        BT_ABORT("fixture json rejected: %s", error.message);
    }
    return root;
}

static void check_cfg_eq(const bt_replay_cfg *a, const bt_replay_cfg *b) {
    BT_CHECK_EQ(a->secs_per_move, b->secs_per_move);
    BT_CHECK_EQ(a->yaw_deg, b->yaw_deg);
    BT_CHECK_EQ(a->pitch_deg, b->pitch_deg);
    BT_CHECK_EQ(a->distance, b->distance);
    BT_CHECK_EQ(a->zoom_min, b->zoom_min);
    BT_CHECK_EQ(a->zoom_max, b->zoom_max);
    BT_CHECK_EQ(a->pitch_clamp_deg[0], b->pitch_clamp_deg[0]);
    BT_CHECK_EQ(a->pitch_clamp_deg[1], b->pitch_clamp_deg[1]);
}

BT_TEST(replay, default_orbit_matches_config_defaults) {
    bt_replay_cfg config = replay_cfg_defaults();
    bt_replay_orbit orbit = bt_replay_default_orbit(&config);
    BT_CHECK_EQ(orbit.yaw_deg, 45.0F);
    BT_CHECK_EQ(orbit.pitch_deg, 30.0F);
    BT_CHECK_EQ(orbit.distance, 6.0F);
}

BT_TEST(replay, drag_clamps_pitch_at_both_bounds) {
    bt_replay_cfg config = replay_cfg_defaults();
    bt_replay_orbit base = default_orbit();
    bt_replay_orbit far_up = bt_replay_apply_drag_delta(base, &config, 0.0F, 5000.0F);
    BT_CHECK_EQ(far_up.pitch_deg, config.pitch_clamp_deg[1]);
    bt_replay_orbit far_down = bt_replay_apply_drag_delta(base, &config, 0.0F, -5000.0F);
    BT_CHECK_EQ(far_down.pitch_deg, config.pitch_clamp_deg[0]);
    bt_replay_orbit small = bt_replay_apply_drag_delta(base, &config, 0.0F, 10.0F);
    BT_CHECK_F64_NEAR(small.pitch_deg, 34.0, 1e-5);
}

BT_TEST(replay, drag_wraps_yaw_around) {
    bt_replay_cfg config = replay_cfg_defaults();
    bt_replay_orbit base = default_orbit();
    bt_replay_orbit spun = base;
    spun.yaw_deg = 170.0F;
    spun = bt_replay_apply_drag_delta(spun, &config, 100.0F, 0.0F);
    BT_CHECK_F64_NEAR(spun.yaw_deg, -150.0, 1e-5);
    bt_replay_orbit spun_back = base;
    spun_back.yaw_deg = -170.0F;
    spun_back = bt_replay_apply_drag_delta(spun_back, &config, -100.0F, 0.0F);
    BT_CHECK_F64_NEAR(spun_back.yaw_deg, 150.0, 1e-5);
    BT_CHECK_F64_NEAR(bt_replay_wrap_yaw(180.0F), 180.0, 1e-5);
    BT_CHECK_F64_NEAR(bt_replay_wrap_yaw(-180.0F), 180.0, 1e-5);
    BT_CHECK_F64_NEAR(bt_replay_wrap_yaw(0.0F), 0.0, 1e-5);
}

BT_TEST(replay, drag_keeps_yaw_free_and_distance_untouched) {
    bt_replay_cfg config = replay_cfg_defaults();
    bt_replay_orbit base = default_orbit();
    bt_replay_orbit moved = bt_replay_apply_drag_delta(base, &config, 900.0F, 0.0F);
    BT_CHECK_EQ(moved.distance, base.distance);
    BT_CHECK(moved.yaw_deg > -180.0F && moved.yaw_deg <= 180.0F);
    bt_replay_orbit centered = bt_replay_apply_drag_delta(base, &config, 3600.0F, 0.0F);
    BT_CHECK_F64_NEAR(centered.yaw_deg, 45.0, 1e-5);
}

BT_TEST(replay, zoom_clamps_at_both_bounds) {
    bt_replay_cfg config = replay_cfg_defaults();
    bt_replay_orbit base = default_orbit();
    bt_replay_orbit in_all_the_way = bt_replay_apply_zoom(base, &config, 100.0F);
    BT_CHECK_EQ(in_all_the_way.distance, config.zoom_min);
    bt_replay_orbit out_all_the_way = bt_replay_apply_zoom(base, &config, -100.0F);
    BT_CHECK_EQ(out_all_the_way.distance, config.zoom_max);
    bt_replay_orbit one_notch = bt_replay_apply_zoom(base, &config, 1.0F);
    BT_CHECK_F64_NEAR(one_notch.distance, 6.0 - BT_REPLAY_ZOOM_UNITS_PER_NOTCH, 1e-5);
    BT_CHECK_EQ(one_notch.yaw_deg, base.yaw_deg);
    BT_CHECK_EQ(one_notch.pitch_deg, base.pitch_deg);
}

BT_TEST(replay, camera_offset_places_the_eye_on_the_sphere) {
    bt_replay_orbit front_orbit = {0.0F, 0.0F, 6.0F};
    float front[3];
    bt_replay_camera_offset(&front_orbit, front);
    BT_CHECK_EQ(front[0], 0.0F);
    BT_CHECK_EQ(front[1], 0.0F);
    BT_CHECK_EQ(front[2], 6.0F);
    bt_replay_orbit right_orbit = {90.0F, 0.0F, 6.0F};
    float right[3];
    bt_replay_camera_offset(&right_orbit, right);
    BT_CHECK_F64_NEAR(right[0], 6.0, 1e-5);
    BT_CHECK(fabsf(right[1]) < 1e-5);
    BT_CHECK(fabsf(right[2]) < 1e-5);
    bt_replay_orbit above_orbit = {0.0F, 90.0F, 6.0F};
    float above[3];
    bt_replay_camera_offset(&above_orbit, above);
    BT_CHECK_F64_NEAR(above[1], 6.0, 1e-5);
    bt_replay_orbit base = default_orbit();
    float eye[3];
    bt_replay_camera_offset(&base, eye);
    float radius = sqrtf(eye[0] * eye[0] + eye[1] * eye[1] + eye[2] * eye[2]);
    BT_CHECK_F64_NEAR(radius, 6.0, 1e-4);
}

BT_TEST(replay, progress_is_eased_monotonic_and_bounded) {
    BT_CHECK_EQ(bt_replay_turn_progress(-1.0, 0.5), 0.0F);
    BT_CHECK_EQ(bt_replay_turn_progress(0.0, 0.5), 0.0F);
    BT_CHECK_EQ(bt_replay_turn_progress(0.5, 0.5), 1.0F);
    BT_CHECK_EQ(bt_replay_turn_progress(9.0, 0.5), 1.0F);
    BT_CHECK_EQ(bt_replay_turn_progress(1.0, 0.0), 1.0F);
    BT_CHECK_EQ(bt_replay_turn_progress(0.0, 0.0), 1.0F);
    float mid = bt_replay_turn_progress(0.25, 0.5);
    BT_CHECK_F64_NEAR(mid, 0.5, 1e-5);
    float last = 0.0F;
    for (int step = 0; step <= 40; step++) {
        float t = bt_replay_turn_progress((double)step * 0.02, 0.5);
        BT_CHECK(t >= last);
        last = t;
    }
    BT_CHECK_F64_NEAR(last, 1.0, 1e-6);
}

BT_TEST(replay, cells_cover_every_slot_once_and_match_engine_ids) {
    const bt_replay_cell *cells = bt_replay_cells();
    bool seen[BT_REPLAY_CELL_COUNT] = {false};
    for (size_t i = 0; i < BT_REPLAY_CELL_COUNT; i++) {
        const bt_replay_cell *cell = &cells[i];
        size_t slot = cell->slot;
        BT_CHECK(!seen[slot]);
        seen[slot] = true;
        if (cell->row == 1 && cell->col == 1) {
            BT_CHECK_EQ(cell->slot, bt_center_id((bt_face)cell->face));
        } else if (cell->row % 2 == 0 && cell->col % 2 == 0) {
            BT_CHECK_EQ(cell->slot,
                        bt_corner_id((bt_face)cell->face, bt_replay_corner_pos(cell->row, cell->col)));
        } else {
            BT_CHECK_EQ(cell->slot,
                        bt_edge_id((bt_face)cell->face, bt_replay_edge_pos(cell->row, cell->col)));
        }
    }
    for (size_t slot = 0; slot < BT_REPLAY_CELL_COUNT; slot++) {
        BT_CHECK(seen[slot]);
    }
    int keys[BT_REPLAY_CELL_COUNT];
    for (size_t i = 0; i < BT_REPLAY_CELL_COUNT; i++) {
        const bt_replay_cell *cell = &cells[i];
        keys[i] = (cell->pos.x + 1) * 9 + (cell->pos.y + 1) * 3 + (cell->pos.z + 1);
    }
    for (size_t i = 1; i < BT_REPLAY_CELL_COUNT; i++) {
        for (size_t j = i; j > 0 && keys[j - 1] > keys[j]; j--) {
            int swap = keys[j - 1];
            keys[j - 1] = keys[j];
            keys[j] = swap;
        }
    }
    size_t unique = 1;
    for (size_t i = 1; i < BT_REPLAY_CELL_COUNT; i++) {
        if (keys[i] != keys[i - 1]) {
            unique++;
        }
    }
    BT_CHECK_EQ(unique, 26U);
    bool origin = false;
    for (size_t i = 0; i < BT_REPLAY_CELL_COUNT; i++) {
        if (keys[i] == 13) {
            origin = true;
        }
    }
    BT_CHECK(!origin);
}

BT_TEST(replay, face_order_matches_the_theme_face_table) {
    const char *names[6] = {"U", "L", "F", "R", "B", "D"};
    for (int face = 0; face < 6; face++) {
        bt_replay_i3 normal = BT_REPLAY_FACE_NORMALS[face];
        const char *spelled;
        if (i3_eq(normal, i3(0, 1, 0))) {
            spelled = "U";
        } else if (i3_eq(normal, i3(-1, 0, 0))) {
            spelled = "L";
        } else if (i3_eq(normal, i3(0, 0, 1))) {
            spelled = "F";
        } else if (i3_eq(normal, i3(1, 0, 0))) {
            spelled = "R";
        } else if (i3_eq(normal, i3(0, 0, -1))) {
            spelled = "B";
        } else if (i3_eq(normal, i3(0, -1, 0))) {
            spelled = "D";
        } else {
            BT_ABORT("face %d normal off axis", face);
        }
        BT_CHECK(strcmp(spelled, names[face]) == 0);
    }
}

BT_TEST(replay, solved_cube_paints_every_cell_with_its_own_face) {
    bt_cube cube = bt_cube_solved();
    const bt_replay_cell *cells = bt_replay_cells();
    for (size_t i = 0; i < BT_REPLAY_CELL_COUNT; i++) {
        const bt_replay_cell *cell = &cells[i];
        BT_CHECK_EQ(bt_replay_home_face(bt_cube_sticker_at(&cube, cell->slot)), cell->face);
    }
    BT_CHECK_EQ(bt_replay_home_face(0), 0);
    BT_CHECK_EQ(bt_replay_home_face(23), 5);
    BT_CHECK_EQ(bt_replay_home_face(24), 0);
    BT_CHECK_EQ(bt_replay_home_face(47), 5);
    BT_CHECK_EQ(bt_replay_home_face(48), 0);
    BT_CHECK_EQ(bt_replay_home_face(53), 5);
}

static size_t all_moves(bt_move *out, size_t cap) {
    size_t count = 0;
    for (uint8_t face = 0; face < 6; face++) {
        for (uint8_t quarter = 1; quarter <= 3; quarter++) {
            out[count++] = bt_move_of_face(face, quarter);
            out[count++] = bt_move_of_inner(face, quarter);
            out[count++] = bt_move_of_wide(face, quarter);
        }
    }
    for (uint8_t slice = 0; slice < 3; slice++) {
        for (uint8_t quarter = 1; quarter <= 3; quarter++) {
            out[count++] = bt_move_of_slice(slice, quarter);
        }
    }
    for (uint8_t axis = 0; axis < 3; axis++) {
        for (uint8_t quarter = 1; quarter <= 3; quarter++) {
            out[count++] = bt_move_of_rotation(axis, quarter);
        }
    }
    BT_CHECK(count <= cap);
    return count;
}

BT_TEST(replay, every_move_family_turns_exactly_like_the_engine_permutes) {
    bt_move moves[72];
    size_t count = all_moves(moves, 72);
    const bt_replay_cell *cells = bt_replay_cells();
    for (size_t m = 0; m < count; m++) {
        bt_replay_turn turn = bt_replay_turn_of(moves[m]);
        bt_cube cube = bt_cube_solved();
        bt_cube_apply(&cube, moves[m]);
        for (size_t i = 0; i < BT_REPLAY_CELL_COUNT; i++) {
            const bt_replay_cell *cell = &cells[i];
            const bt_replay_cell *target = bt_replay_turned_cell(cell, &turn);
            BT_CHECK(target != NULL);
            if (target == NULL) {
                continue;
            }
            BT_CHECK_EQ(target->slot, bt_cube_position_of(&cube, cell->slot));
        }
    }
}

BT_TEST(replay, turn_of_picks_the_physical_layer_for_each_family) {
    bt_replay_turn r = bt_replay_turn_of(bt_move_of_face(BT_FACE_R, 1));
    BT_CHECK_EQ(r.axis, BT_AXIS_X);
    BT_CHECK(mask_is(&r, false, false, true));
    BT_CHECK(bt_replay_turn_rotates(&r, i3(1, 0, 0)));
    BT_CHECK(!bt_replay_turn_rotates(&r, i3(0, 0, 0)));
    BT_CHECK(!bt_replay_turn_rotates(&r, i3(-1, 0, 0)));
    bt_replay_turn u = bt_replay_turn_of(bt_move_of_face(BT_FACE_U, 1));
    BT_CHECK_EQ(u.axis, BT_AXIS_Y);
    BT_CHECK(mask_is(&u, false, false, true));
    BT_CHECK(bt_replay_turn_rotates(&u, i3(0, 1, 0)));
    BT_CHECK(!bt_replay_turn_rotates(&u, i3(0, 0, 0)));
    bt_replay_turn inner_r = bt_replay_turn_of(bt_move_of_inner(BT_FACE_R, 1));
    BT_CHECK(mask_is(&inner_r, false, true, false));
    BT_CHECK(bt_replay_turn_rotates(&inner_r, i3(0, 0, 0)));
    BT_CHECK(!bt_replay_turn_rotates(&inner_r, i3(1, 0, 0)));
    bt_replay_turn wide_r = bt_replay_turn_of(bt_move_of_wide(BT_FACE_R, 1));
    BT_CHECK(mask_is(&wide_r, false, true, true));
    bt_replay_turn m = bt_replay_turn_of(bt_move_of_slice(BT_SLICE_M, 1));
    BT_CHECK_EQ(m.axis, BT_AXIS_X);
    BT_CHECK(mask_is(&m, false, true, false));
    bt_replay_turn x = bt_replay_turn_of(bt_move_of_rotation(BT_AXIS_X, 1));
    BT_CHECK_EQ(x.axis, BT_AXIS_X);
    BT_CHECK(mask_is(&x, true, true, true));
    BT_CHECK(bt_replay_turn_rotates(&x, i3(-1, -1, -1)));
}

BT_TEST(replay, angle_scales_with_applied_quarters_and_progress) {
    bt_replay_turn half = {BT_AXIS_Y, 2, {true, true, true}};
    BT_CHECK_F64_NEAR(bt_replay_turn_angle_deg(&half, 1.0F), 180.0, 1e-5);
    BT_CHECK_F64_NEAR(bt_replay_turn_angle_deg(&half, 0.5F), 90.0, 1e-5);
    bt_replay_turn prime = {BT_AXIS_Y, 3, {true, true, true}};
    BT_CHECK_F64_NEAR(bt_replay_turn_angle_deg(&prime, 1.0F), 270.0, 1e-5);
    BT_CHECK_F64_NEAR(bt_replay_turn_angle_deg(&prime, 0.0F), 0.0, 1e-6);
}

BT_TEST(replay, playback_settles_each_move_and_freezes_at_the_end) {
    bt_alg alg;
    bt_alg_init(&alg);
    char err_token[64];
    BT_CHECK(bt_alg_parse("R U r' 2F M2 x S'", &alg, err_token, sizeof err_token));
    bt_cube start = bt_cube_solved();
    bt_cube_apply_alg(&start, &alg);
    bt_alg inverse = bt_alg_inverse(&alg);
    bt_replay_playback playback;
    bt_replay_playback_init(&playback, start, inverse.items, bt_alg_len(&inverse));
    BT_CHECK(!bt_replay_playback_finished(&playback));
    bt_replay_turn current;
    BT_CHECK(bt_replay_playback_current_turn(&playback, &current));
    BT_CHECK_EQ(current.axis, BT_AXIS_Z);
    bt_replay_playback_toggle(&playback);
    BT_CHECK(playback.playing);
    unsigned long long start_gen = playback.settle_gen;
    bt_replay_playback_tick(&playback, 0.4, 0.5);
    BT_CHECK_EQ(playback.index, 0U);
    BT_CHECK_F64_NEAR(bt_replay_playback_current_progress(&playback, 0.5),
                      bt_replay_turn_progress(0.4, 0.5), 1e-6);
    bt_replay_playback_tick(&playback, 0.2, 0.5);
    BT_CHECK_EQ(playback.index, 1U);
    BT_CHECK_EQ(playback.settle_gen, start_gen + 1);
    for (int i = 0; i < 60; i++) {
        bt_replay_playback_tick(&playback, 0.5, 0.5);
    }
    BT_CHECK(bt_replay_playback_finished(&playback));
    BT_CHECK(!playback.playing);
    BT_CHECK(bt_cube_is_solved(&playback.cube));
    BT_CHECK(!bt_replay_playback_current_turn(&playback, &current));
    bt_replay_playback_toggle(&playback);
    BT_CHECK(!playback.playing);
    bt_alg_free(&alg);
    bt_alg_free(&inverse);
}

BT_TEST(replay, playback_catches_up_when_a_frame_spans_several_moves) {
    bt_alg alg;
    bt_alg_init(&alg);
    char err_token[64];
    BT_CHECK(bt_alg_parse("R U F2", &alg, err_token, sizeof err_token));
    bt_replay_playback playback;
    bt_replay_playback_init(&playback, bt_cube_solved(), alg.items, bt_alg_len(&alg));
    bt_replay_playback_toggle(&playback);
    bt_replay_playback_tick(&playback, 1.6, 0.5);
    BT_CHECK_EQ(playback.index, 3U);
    BT_CHECK(bt_replay_playback_finished(&playback));
    bt_cube reference = bt_cube_solved();
    bt_cube_apply_alg(&reference, &alg);
    BT_CHECK(bt_cube_eq(&playback.cube, &reference));
    bt_alg_free(&alg);
}

BT_TEST(replay, paused_playback_holds_mid_turn_state) {
    bt_alg alg;
    bt_alg_init(&alg);
    char err_token[64];
    BT_CHECK(bt_alg_parse("U", &alg, err_token, sizeof err_token));
    bt_replay_playback playback;
    bt_replay_playback_init(&playback, bt_cube_solved(), alg.items, bt_alg_len(&alg));
    bt_replay_playback_toggle(&playback);
    bt_replay_playback_tick(&playback, 0.25, 0.5);
    bt_replay_playback_toggle(&playback);
    BT_CHECK(!playback.playing);
    bt_cube before = playback.cube;
    bt_replay_playback_tick(&playback, 1.0, 0.5);
    BT_CHECK(bt_cube_eq(&playback.cube, &before));
    BT_CHECK_EQ(playback.index, 0U);
    BT_CHECK_F64_NEAR(bt_replay_playback_current_progress(&playback, 0.5), 0.5, 1e-5);
    bt_alg_free(&alg);
}

BT_TEST(replay, zero_move_replay_shows_a_still_cube) {
    bt_replay_playback playback;
    bt_replay_playback_init(&playback, bt_cube_solved(), NULL, 0);
    BT_CHECK(bt_replay_playback_finished(&playback));
    bt_replay_turn current;
    BT_CHECK(!bt_replay_playback_current_turn(&playback, &current));
    bt_replay_playback_toggle(&playback);
    BT_CHECK(!playback.playing);
    bt_replay_playback_tick(&playback, 1.0, 0.5);
    BT_CHECK(bt_cube_is_solved(&playback.cube));
}

BT_TEST(replay, cfg_sanitizes_reversed_and_out_of_range_values) {
    bt_replay_cfg raw = {
        -1.0, 405.0F, 90.0F, 99.0F, 12.0F, 3.5F, {80.0F, -80.0F},
    };
    bt_replay_cfg clean = bt_replay_cfg_sanitized(raw);
    BT_CHECK_EQ(clean.secs_per_move, 0.0);
    BT_CHECK_F64_NEAR(clean.yaw_deg, 45.0, 1e-5);
    BT_CHECK_EQ(clean.pitch_deg, 80.0F);
    BT_CHECK_EQ(clean.distance, 12.0F);
    BT_CHECK_EQ(clean.zoom_min, 3.5F);
    BT_CHECK_EQ(clean.zoom_max, 12.0F);
    BT_CHECK_EQ(clean.pitch_clamp_deg[0], -80.0F);
    BT_CHECK_EQ(clean.pitch_clamp_deg[1], 80.0F);
}

BT_TEST(replay, cfg_reads_the_hub_section_and_falls_back_to_defaults) {
    bt_json_value *hub = fixture_json(
        "{\"app\":{\"replay\":{"
        "\"secs_per_move\":0.25,"
        "\"yaw_deg\":30.0,"
        "\"pitch_deg\":20.0,"
        "\"distance\":8.0,"
        "\"zoom_min\":4.0,"
        "\"zoom_max\":10.0,"
        "\"pitch_clamp_deg\":[-70.0,70.0]"
        "}}}");
    bt_replay_cfg parsed;
    bt_replay_cfg_from(hub, &parsed);
    bt_json_free(hub);
    BT_CHECK_EQ(parsed.secs_per_move, 0.25);
    BT_CHECK_EQ(parsed.yaw_deg, 30.0F);
    BT_CHECK_EQ(parsed.pitch_deg, 20.0F);
    BT_CHECK_EQ(parsed.distance, 8.0F);
    BT_CHECK_EQ(parsed.zoom_min, 4.0F);
    BT_CHECK_EQ(parsed.zoom_max, 10.0F);
    BT_CHECK_EQ(parsed.pitch_clamp_deg[0], -70.0F);
    BT_CHECK_EQ(parsed.pitch_clamp_deg[1], 70.0F);

    bt_json_value *empty = fixture_json("{}");
    bt_replay_cfg fallback;
    bt_replay_cfg_from(empty, &fallback);
    bt_json_free(empty);
    bt_replay_cfg defaults = replay_cfg_defaults();
    check_cfg_eq(&fallback, &defaults);

    bt_json_value *broken = fixture_json("{\"app\":{\"replay\":\"broken\"}}");
    bt_replay_cfg from_broken;
    bt_replay_cfg_from(broken, &from_broken);
    bt_json_free(broken);
    check_cfg_eq(&from_broken, &defaults);

    bt_json_value *partial = fixture_json("{\"app\":{\"replay\":{\"distance\":7.0}}}");
    bt_replay_cfg from_partial;
    bt_replay_cfg_from(partial, &from_partial);
    bt_json_free(partial);
    BT_CHECK_EQ(from_partial.distance, 7.0F);
    BT_CHECK_EQ(from_partial.secs_per_move, defaults.secs_per_move);
}
