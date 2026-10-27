#ifndef BT_APP_VIEWS_REPLAY_H
#define BT_APP_VIEWS_REPLAY_H

#include "app/logic.h"
#include "app/replay_scene.h"
#include "core/alg.h"

#include "raylib.h"

#define BT_REPLAY_FOVY_DEG 45.0F
#define BT_VIEW_REPLAY_BODY_RED 0.04F
#define BT_VIEW_REPLAY_BODY_GREEN 0.04F
#define BT_VIEW_REPLAY_BODY_BLUE 0.05F

typedef struct {
    bt_replay_cfg cfg;
    bt_replay_orbit orbit;
    bt_replay_playback playback;
    bt_alg moves;
} bt_view_replay_scene;

void bt_view_replay_scene_default(bt_view_replay_scene *scene);
void bt_view_replay_scene_session(bt_view_replay_scene *scene, bt_cube scramble,
                                  const bt_move *moves, size_t moves_len);
Vector3 bt_view_replay_axis_vec(bt_axis axis);
Camera3D bt_view_replay_camera(const bt_view_replay_scene *scene);

void bt_view_replay_enter(bt_app_ctx *ctx);
void bt_view_replay_update(bt_app_ctx *ctx);
void bt_view_replay_draw(bt_app_ctx *ctx);
void bt_view_replay_app_free(void);

#endif
