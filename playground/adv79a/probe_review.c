#include "app/boot.h"
#include "app/logic.h"
#include "app/views/solve.h"
#include "app/views/state.h"
#include "app/window.h"
#include "core/alg.h"
#include "core/cube.h"
#include "data/paths.h"

#include "raylib.h"
#include "rlgl.h"

#include <direct.h>
#include <stdio.h>
#include <string.h>

static void seed_ctx(bt_app_ctx *ctx, const char *user_root) {
    bt_app_ctx_init(ctx);
    ctx->state = BT_APP_SOLVE;
    ctx->has_stores = 1;
    memset(&ctx->stores, 0, sizeof ctx->stores);
    ctx->stores.settings.memo_cadence_ms = 800;
    bt_srs_store_init(&ctx->stores.srs);
    bt_solve_log_init(&ctx->stores.solves);
    bt_user_algs_init(&ctx->stores.user_algs);
    bt_str_list_init(&ctx->stores.seeds.hidden);
    bt_str_list_init(&ctx->stores.seeds.warnings);
    snprintf(ctx->paths.user_root, sizeof ctx->paths.user_root, "%s", user_root);
}

static int drive_case(const char *shot_path, const char *user_root, uint64_t seed,
                      const bt_cube *override) {
    bt_app_ctx ctx;
    seed_ctx(&ctx, user_root);
    bt_view_solve_app_state *app = bt_view_solve_app();
    bt_syllable_book book;
    bt_syllable_book_init(&book);
    bt_view_solve_rebuild(&app->session, seed, &book, override);
    ctx.solve_phase = BT_SOLVE_READY;
    bt_view_solve_key(&ctx, app, KEY_SPACE);
    bt_view_solve_key(&ctx, app, KEY_SPACE);
    bt_view_solve_key(&ctx, app, KEY_SPACE);
    if (ctx.solve_phase != BT_SOLVE_REVIEW) {
        printf("seed %llu did not reach review\n", (unsigned long long)seed);
        bt_syllable_book_free(&book);
        bt_app_ctx_free(&ctx);
        return 1;
    }
    bt_solve_view_parts parts;
    bt_view_solve_review_parts(&app->session, &parts);
    size_t broken = 0;
    const char *at = parts.broken;
    while (*at != '\0') {
        if (*at == '\n') {
            broken++;
        }
        at++;
    }
    if (parts.broken[0] != '\0') {
        broken++;
    }
    printf("seed %llu broken lines %zu result %s\n", (unsigned long long)seed, broken,
           parts.result);
    bt_app_font_book fonts;
    char error[BT_APP_ERROR_MAX];
    char config_path[BT_PATH_MAX];
    bt_data_config_path(config_path);
    if (bt_app_fonts_load(config_path, NULL, NULL, 0, &fonts, error, sizeof error) != 0) {
        printf("fonts: %s\n", error);
        bt_syllable_book_free(&book);
        bt_app_ctx_free(&ctx);
        return 1;
    }
    for (int frame = 0; frame < 6; frame++) {
        bt_app_window_frame_begin();
        ClearBackground(bt_app_color(bt_theme_background()));
        bt_view_solve_draw(&ctx, fonts.regular, fonts.bold, 1280.0F, 800.0F);
        if (frame == 5) {
            rlDrawRenderBatchActive();
            TakeScreenshot(shot_path);
        }
        bt_app_window_frame_end();
    }
    printf("shot %s exists %d\n", shot_path, FileExists(shot_path) ? 1 : 0);
    bt_app_fonts_unload(&fonts);
    bt_syllable_book_free(&book);
    bt_app_ctx_free(&ctx);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("usage: probe_review.exe <out dir> <user root>\n");
        return 2;
    }
    _mkdir(argv[1]);
    _mkdir(argv[2]);
    if (!bt_app_window_open(1280, 800, "adv79a review probe", NULL)) {
        printf("window open failed\n");
        return 1;
    }
    char shot_old[BT_PATH_MAX];
    snprintf(shot_old, sizeof shot_old, "%s/review_seed7_null.png", argv[1]);
    char shot_new[BT_PATH_MAX];
    snprintf(shot_new, sizeof shot_new, "%s/review_seed4_r.png", argv[1]);
    char user_old[BT_PATH_MAX];
    snprintf(user_old, sizeof user_old, "%s/seed7", argv[2]);
    char user_new[BT_PATH_MAX];
    snprintf(user_new, sizeof user_new, "%s/seed4", argv[2]);
    int code = drive_case(shot_old, user_old, 7, NULL);
    bt_cube scramble = bt_cube_solved();
    bt_alg scramble_alg;
    bt_alg_init(&scramble_alg);
    char scramble_err[64];
    if (!bt_alg_parse("R", &scramble_alg, scramble_err, sizeof scramble_err)) {
        printf("scramble: %s\n", scramble_err);
        bt_alg_free(&scramble_alg);
        bt_app_window_close();
        return 1;
    }
    bt_cube_apply_alg(&scramble, &scramble_alg);
    bt_alg_free(&scramble_alg);
    code = drive_case(shot_new, user_new, 4, &scramble) || code;
    bt_app_window_close();
    return code;
}
