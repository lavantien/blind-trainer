#include "app/views/replay.h"

#include "common.h"

#include "app/window.h"
#include "core/cube.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static bt_replay_i3 i3(int x, int y, int z) {
    bt_replay_i3 out = {(int8_t)x, (int8_t)y, (int8_t)z};
    return out;
}

static void check_scene_cfg_defaults(const bt_replay_cfg *cfg) {
    bt_replay_cfg defaults;
    bt_replay_cfg_defaults(&defaults);
    BT_CHECK_EQ(cfg->secs_per_move, defaults.secs_per_move);
    BT_CHECK_EQ(cfg->yaw_deg, defaults.yaw_deg);
    BT_CHECK_EQ(cfg->pitch_deg, defaults.pitch_deg);
    BT_CHECK_EQ(cfg->distance, defaults.distance);
    BT_CHECK_EQ(cfg->zoom_min, defaults.zoom_min);
    BT_CHECK_EQ(cfg->zoom_max, defaults.zoom_max);
    BT_CHECK_EQ(cfg->pitch_clamp_deg[0], defaults.pitch_clamp_deg[0]);
    BT_CHECK_EQ(cfg->pitch_clamp_deg[1], defaults.pitch_clamp_deg[1]);
}

BT_TEST(view_replay, every_cubie_carries_its_own_stickers) {
    const bt_replay_cell *cells = bt_replay_cells();
    int cubies = 0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            for (int z = -1; z <= 1; z++) {
                int stickers = 0;
                for (size_t i = 0; i < BT_REPLAY_CELL_COUNT; i++) {
                    if (cells[i].pos.x == x && cells[i].pos.y == y && cells[i].pos.z == z) {
                        stickers++;
                    }
                }
                int outward = (x == -1) + (x == 1) + (y == -1) + (y == 1) + (z == -1) + (z == 1);
                BT_CHECK_EQ(stickers, outward);
                cubies++;
            }
        }
    }
    BT_CHECK_EQ(cubies, 27);
    BT_CHECK_EQ(BT_REPLAY_CELL_COUNT, 54U);
}

BT_TEST(view_replay, turned_cell_is_identity_outside_the_layer) {
    bt_replay_turn turn = bt_replay_turn_of(bt_move_of_face(BT_FACE_R, 1));
    const bt_replay_cell *settled = bt_replay_cell_at(i3(-1, 0, 0), i3(-1, 0, 0));
    BT_CHECK(settled != NULL);
    const bt_replay_cell *turned = bt_replay_turned_cell(settled, &turn);
    BT_CHECK(turned != NULL);
    if (turned != NULL) {
        BT_CHECK_EQ(turned->slot, settled->slot);
    }
    const bt_replay_cell *moving = bt_replay_cell_at(i3(1, 1, 0), i3(1, 0, 0));
    BT_CHECK(moving != NULL);
    const bt_replay_cell *moved = bt_replay_turned_cell(moving, &turn);
    BT_CHECK(moved != NULL);
    if (moved != NULL && moving != NULL) {
        BT_CHECK(moved->slot != moving->slot);
    }
}

BT_TEST(view_replay, axis_vec_matches_the_engine_axes) {
    Vector3 x = bt_view_replay_axis_vec(BT_AXIS_X);
    BT_CHECK_EQ(x.x, 1.0F);
    BT_CHECK_EQ(x.y, 0.0F);
    BT_CHECK_EQ(x.z, 0.0F);
    Vector3 y = bt_view_replay_axis_vec(BT_AXIS_Y);
    BT_CHECK_EQ(y.x, 0.0F);
    BT_CHECK_EQ(y.y, 1.0F);
    BT_CHECK_EQ(y.z, 0.0F);
    Vector3 z = bt_view_replay_axis_vec(BT_AXIS_Z);
    BT_CHECK_EQ(z.x, 0.0F);
    BT_CHECK_EQ(z.y, 0.0F);
    BT_CHECK_EQ(z.z, 1.0F);
}

BT_TEST(view_replay, scene_default_holds_a_still_solved_cube) {
    bt_view_replay_scene scene;
    bt_view_replay_scene_default(&scene);
    bt_cube solved = bt_cube_solved();
    BT_CHECK(bt_cube_eq(&scene.playback.cube, &solved));
    BT_CHECK(bt_replay_playback_finished(&scene.playback));
    bt_replay_orbit orbit = bt_replay_default_orbit(&scene.cfg);
    BT_CHECK_EQ(scene.orbit.yaw_deg, orbit.yaw_deg);
    BT_CHECK_EQ(scene.orbit.pitch_deg, orbit.pitch_deg);
    BT_CHECK_EQ(scene.orbit.distance, orbit.distance);
    check_scene_cfg_defaults(&scene.cfg);
    Camera3D camera = bt_view_replay_camera(&scene);
    BT_CHECK_EQ(camera.fovy, 45.0F);
    BT_CHECK_EQ(camera.projection, CAMERA_PERSPECTIVE);
    BT_CHECK_EQ(camera.up.y, 1.0F);
    BT_CHECK_EQ(camera.target.x, 0.0F);
    BT_CHECK_EQ(camera.target.y, 0.0F);
    BT_CHECK_EQ(camera.target.z, 0.0F);
}

BT_TEST(view_replay, v_transition_frame_still_drags_and_ticks) {
    bt_alg alg;
    bt_alg_init(&alg);
    char err_token[64];
    BT_CHECK(bt_alg_parse("U", &alg, err_token, sizeof err_token));
    bt_view_replay_scene scene;
    bt_view_replay_scene_default(&scene);
    bt_view_replay_scene_session(&scene, bt_cube_solved(), alg.items, bt_alg_len(&alg));
    bt_replay_playback_toggle(&scene.playback);
    bt_app_ctx ctx;
    bt_app_ctx_init(&ctx);
    bt_view_replay_input input = {0};
    input.v_key = 1;
    input.drag_held = 1;
    input.dx = 10.0F;
    input.dt = 0.25;
    bt_view_replay_frame(&ctx, &scene, &input);
    BT_CHECK(ctx.has_pending);
    BT_CHECK(ctx.pending == BT_APP_SOLVE);
    BT_CHECK_F64_NEAR(scene.orbit.yaw_deg, 49.0, 1e-5);
    BT_CHECK_F64_NEAR(scene.playback.elapsed, 0.25, 1e-9);
    BT_CHECK_EQ(scene.playback.index, 0U);
    bt_alg_free(&alg);
}

BT_TEST(view_replay, played_scene_replays_the_collected_moves) {
    bt_alg alg;
    bt_alg_init(&alg);
    char err_token[64];
    BT_CHECK(bt_alg_parse("R U R' U'", &alg, err_token, sizeof err_token));
    bt_cube start = bt_cube_solved();
    bt_cube_apply_alg(&start, &alg);
    bt_alg inverse = bt_alg_inverse(&alg);
    bt_view_replay_scene scene;
    bt_view_replay_scene_default(&scene);
    bt_view_replay_scene_session(&scene, start, inverse.items, bt_alg_len(&inverse));
    bt_replay_playback_toggle(&scene.playback);
    for (int i = 0; i < 16; i++) {
        bt_replay_playback_tick(&scene.playback, 0.5, 0.5);
    }
    BT_CHECK(bt_cube_is_solved(&scene.playback.cube));
    bt_alg_free(&alg);
    bt_alg_free(&inverse);
}

BT_TEST(view_replay, draw_renders_the_cube_under_a_real_window) {
    BT_CHECK(bt_app_window_open(640, 480, "replay probe", NULL));
    bt_app_ctx ctx;
    bt_app_ctx_init(&ctx);
    bt_view_replay_enter(&ctx);
    for (int i = 0; i < 6; i++) {
        bt_app_window_frame_begin();
        bt_view_replay_draw(&ctx);
        bt_app_window_frame_end();
    }
    const char *dir = ta_temp_dir("replay", "shot");
    char shot[TA_PATH_MAX];
    snprintf(shot, sizeof shot, "%s/replay.png", dir);
    BT_CHECK(bt_app_window_capture(shot));
    Image image = LoadImage(shot);
    BT_CHECK(image.width == 640 && image.height == 480);
    Color background = bt_app_color(bt_theme_background());
    Color center = GetImageColor(image, 320, 240);
    BT_CHECK(center.r != background.r || center.g != background.g || center.b != background.b);
    unsigned char seen[4096] = {0};
    size_t distinct = 0;
    for (int y = 0; y < image.height; y += 16) {
        for (int x = 0; x < image.width; x += 16) {
            Color px = GetImageColor(image, x, y);
            unsigned int key = ((unsigned int)px.r >> 4U) << 8U | ((unsigned int)px.g >> 4U) << 4U |
                               (unsigned int)px.b >> 4U;
            if (!seen[key]) {
                seen[key] = 1;
                distinct++;
            }
        }
    }
    BT_CHECK(distinct >= 4);
    UnloadImage(image);
    bt_app_window_close();
}
