#include "app/views/replay.h"

#include "app/theme.h"
#include "app/views/solve.h"
#include "app/window.h"
#include "common/bt_trace.h"

#include "rlgl.h"

#include <stddef.h>

static bt_view_replay_scene view_replay_scene;
static int view_replay_ready;

void bt_view_replay_scene_default(bt_view_replay_scene *scene) {
    BT_TRACE_ENTER("view.replay_scene_default");
    bt_replay_cfg_defaults(&scene->cfg);
    scene->orbit = bt_replay_default_orbit(&scene->cfg);
    bt_alg_init(&scene->moves);
    bt_replay_playback_init(&scene->playback, bt_cube_solved(), NULL, 0);
    BT_TRACE_EXIT("view.replay_scene_default");
}

void bt_view_replay_scene_session(bt_view_replay_scene *scene, bt_cube scramble,
                                  const bt_move *moves, size_t moves_len) {
    BT_TRACE_ENTER("view.replay_scene_session");
    bt_alg_free(&scene->moves);
    bt_alg_init(&scene->moves);
    for (size_t i = 0; i < moves_len; i++) {
        if (!bt_alg_push(&scene->moves, moves[i])) {
            BT_TRACE_FAIL("view.replay_scene_session", "moves push failed at %zu", i);
            break;
        }
    }
    bt_replay_playback_init(&scene->playback, scramble, scene->moves.items,
                            bt_alg_len(&scene->moves));
    BT_TRACE_EXIT("view.replay_scene_session");
}

static float *axis_slot(Vector3 *out, bt_axis axis) {
    if (axis == BT_AXIS_X) {
        return &out->x;
    }
    if (axis == BT_AXIS_Y) {
        return &out->y;
    }
    return &out->z;
}

Vector3 bt_view_replay_axis_vec(bt_axis axis) {
    BT_TRACE_ENTER("view.replay_axis_vec");
    Vector3 out = {0};
    *axis_slot(&out, axis) = 1.0F;
    BT_TRACE_EXIT("view.replay_axis_vec");
    return out;
}

static void ensure_scene(void) {
    if (!view_replay_ready) {
        bt_view_replay_scene_default(&view_replay_scene);
        view_replay_ready = 1;
    }
}

void bt_view_replay_enter(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("view.replay_enter");
    ensure_scene();
    bt_json_value *value = NULL;
    char error[BT_APP_ERROR_MAX];
    if (bt_app_load_config_value(ctx->paths.config_path, &value, error, sizeof error) == 0) {
        bt_replay_cfg_from(value, &view_replay_scene.cfg);
    } else {
        bt_replay_cfg_defaults(&view_replay_scene.cfg);
    }
    bt_json_free(value);
    view_replay_scene.orbit = bt_replay_default_orbit(&view_replay_scene.cfg);
    bt_alg_free(&view_replay_scene.moves);
    bt_alg_init(&view_replay_scene.moves);
    bt_view_solve_app_state *solve = bt_view_solve_app();
    if (solve != NULL && solve->session.has_memo && ctx->has_corpus) {
        bt_solve_outcome broken;
        bt_solve_outcome_init(&broken);
        bt_view_solve_executed_moves(&ctx->vendor_corpus, &solve->session.memo, &broken,
                                     &view_replay_scene.moves);
        bt_solve_outcome_free(&broken);
    }
    bt_cube start = solve != NULL ? solve->session.scramble : bt_cube_solved();
    bt_replay_playback_init(&view_replay_scene.playback, start, view_replay_scene.moves.items,
                            bt_alg_len(&view_replay_scene.moves));
    BT_TRACE_EXIT("view.replay_enter");
}

void bt_view_replay_app_free(void) {
    BT_TRACE_ENTER("view.replay_app_free");
    if (view_replay_ready) {
        bt_alg_free(&view_replay_scene.moves);
        view_replay_ready = 0;
    }
    BT_TRACE_EXIT("view.replay_app_free");
}

void bt_view_replay_frame(bt_app_ctx *ctx, bt_view_replay_scene *scene,
                          const bt_view_replay_input *input) {
    BT_TRACE_ENTER("view.replay_frame");
    if (input->space) {
        bt_replay_playback_toggle(&scene->playback);
    }
    if (input->r_key) {
        scene->orbit = bt_replay_default_orbit(&scene->cfg);
    }
    if (input->v_key) {
        bt_app_request_state(ctx, BT_APP_SOLVE);
    }
    if (input->drag_held && (input->dx != 0.0F || input->dy != 0.0F)) {
        scene->orbit = bt_replay_apply_drag_delta(scene->orbit, &scene->cfg, input->dx, input->dy);
    }
    if (input->wheel != 0.0F) {
        scene->orbit = bt_replay_apply_zoom(scene->orbit, &scene->cfg, input->wheel);
    }
    bt_replay_playback_tick(&scene->playback, input->dt, scene->cfg.secs_per_move);
    BT_TRACE_EXIT("view.replay_frame");
}

void bt_view_replay_update(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("view.replay_update");
    ensure_scene();
    bt_view_replay_input input = {0};
    input.space = IsKeyPressed(KEY_SPACE) ? 1 : 0;
    input.r_key = IsKeyPressed(KEY_R) ? 1 : 0;
    input.v_key = IsKeyPressed(KEY_V) ? 1 : 0;
    input.drag_held =
        (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ||
         IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
            ? 1
            : 0;
    Vector2 delta = GetMouseDelta();
    input.dx = delta.x;
    input.dy = delta.y;
    input.wheel = GetMouseWheelMove();
    input.dt = (double)GetFrameTime();
    bt_view_replay_frame(ctx, &view_replay_scene, &input);
    BT_TRACE_EXIT("view.replay_update");
}

Camera3D bt_view_replay_camera(const bt_view_replay_scene *scene) {
    BT_TRACE_ENTER("view.replay_camera");
    float offset[3];
    bt_replay_camera_offset(&scene->orbit, offset);
    Camera3D camera = {0};
    camera.position.x = offset[0];
    camera.position.y = offset[1];
    camera.position.z = offset[2];
    camera.target.x = 0.0F;
    camera.target.y = 0.0F;
    camera.target.z = 0.0F;
    camera.up.x = 0.0F;
    camera.up.y = 1.0F;
    camera.up.z = 0.0F;
    camera.fovy = BT_REPLAY_FOVY_DEG;
    camera.projection = CAMERA_PERSPECTIVE;
    BT_TRACE_EXIT("view.replay_camera");
    return camera;
}

static const float BT_VIEW_REPLAY_QUARTER_DEG = 90.0F;

typedef struct {
    bt_replay_i3 normal;
    bt_axis axis;
    int quarters;
} bt_view_replay_orient;

static const bt_view_replay_orient BT_VIEW_REPLAY_ORIENTS[] = {
    {{0, 1, 0}, BT_AXIS_X, -1},  {{0, -1, 0}, BT_AXIS_X, 1}, {{1, 0, 0}, BT_AXIS_Y, 1},
    {{-1, 0, 0}, BT_AXIS_Y, -1}, {{0, 0, -1}, BT_AXIS_X, 2},
};

static void orient_sticker(bt_replay_i3 normal) {
    for (size_t at = 0; at < sizeof BT_VIEW_REPLAY_ORIENTS / sizeof BT_VIEW_REPLAY_ORIENTS[0];
         at++) {
        const bt_view_replay_orient *orient = &BT_VIEW_REPLAY_ORIENTS[at];
        if (orient->normal.x == normal.x && orient->normal.y == normal.y &&
            orient->normal.z == normal.z) {
            Vector3 axis = bt_view_replay_axis_vec(orient->axis);
            rlRotatef((float)orient->quarters * BT_VIEW_REPLAY_QUARTER_DEG, axis.x, axis.y, axis.z);
            return;
        }
    }
}

static void draw_sticker(const bt_view_replay_scene *scene, const bt_replay_cell *cell) {
    size_t face = bt_replay_playback_sticker_face(&scene->playback, cell);
    Color color = bt_app_color(bt_theme_face_color(face));
    float half = BT_REPLAY_STICKER_SPAN / 2.0F;
    rlPushMatrix();
    orient_sticker(cell->normal);
    rlTranslatef(0.0F, 0.0F, BT_REPLAY_STICKER_LIFT);
    rlBegin(RL_QUADS);
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlVertex3f(-half, -half, 0.0F);
    rlVertex3f(half, -half, 0.0F);
    rlVertex3f(half, half, 0.0F);
    rlVertex3f(-half, half, 0.0F);
    rlEnd();
    rlPopMatrix();
}

static bt_theme_color body_color(void) {
    bt_theme_color color = {
        BT_VIEW_REPLAY_BODY_RED,
        BT_VIEW_REPLAY_BODY_GREEN,
        BT_VIEW_REPLAY_BODY_BLUE,
        1.0F,
    };
    return color;
}

static void draw_cubie(const bt_view_replay_scene *scene, bt_replay_i3 pos) {
    rlPushMatrix();
    rlTranslatef((float)pos.x, (float)pos.y, (float)pos.z);
    Vector3 origin = {0};
    DrawCube(origin, BT_REPLAY_CUBIE_SPAN, BT_REPLAY_CUBIE_SPAN, BT_REPLAY_CUBIE_SPAN,
             bt_app_color(body_color()));
    const bt_replay_cell *cells = bt_replay_cells();
    for (size_t i = 0; i < BT_REPLAY_CELL_COUNT; i++) {
        const bt_replay_cell *cell = &cells[i];
        if (cell->pos.x == pos.x && cell->pos.y == pos.y && cell->pos.z == pos.z) {
            draw_sticker(scene, cell);
        }
    }
    rlPopMatrix();
}

void bt_view_replay_draw(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("view.replay_draw");
    (void)ctx;
    ensure_scene();
    bt_view_replay_scene *scene = &view_replay_scene;
    ClearBackground(bt_app_color(bt_theme_background()));
    BeginMode3D(bt_view_replay_camera(scene));
    bt_replay_turn turn;
    int has_turn = bt_replay_playback_current_turn(&scene->playback, &turn) ? 1 : 0;
    float progress =
        bt_replay_playback_current_progress(&scene->playback, scene->cfg.secs_per_move);
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            for (int z = -1; z <= 1; z++) {
                bt_replay_i3 pos = {(int8_t)x, (int8_t)y, (int8_t)z};
                rlPushMatrix();
                if (has_turn && bt_replay_turn_rotates(&turn, pos)) {
                    Vector3 axis = bt_view_replay_axis_vec(turn.axis);
                    rlRotatef(bt_replay_turn_angle_deg(&turn, progress), axis.x, axis.y, axis.z);
                }
                draw_cubie(scene, pos);
                rlPopMatrix();
            }
        }
    }
    EndMode3D();
    BT_TRACE_EXIT("view.replay_draw");
}
