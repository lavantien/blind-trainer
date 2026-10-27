#include "app/views/case_drill.h"
#include "app/views/case_drill_view.h"
#include "app/views/net_view.h"
#include "app/views/solve.h"
#include "app/views/state.h"
#include "app/window.h"
#include "data/paths.h"

#include "common.h"

#include "raylib.h"

enum {
    DRAW_DWELL_FRAMES = 6,
};

static int open_window(bt_app_config *config) {
    config->window.width = 640;
    config->window.height = 480;
    config->render.capture_dwell_frames = DRAW_DWELL_FRAMES;
    snprintf(config->window.title, sizeof config->window.title, "%s", "l17 draw test");
    return bt_app_window_open(config->window.width, config->window.height, config->window.title,
                              &config->render);
}

static long long region_variance(const char *path, int x0, int y0, int x1, int y1) {
    Image image = LoadImage(path);
    if (image.width <= 0 || image.height <= 0) {
        UnloadImage(image);
        return -1;
    }
    Color *pixels = LoadImageColors(image);
    long long variance = 0;
    for (int y = y0; y < y1 && y < image.height; y++) {
        for (int x = x0 + 1; x < x1 && x < image.width; x++) {
            Color left = pixels[y * image.width + x - 1];
            Color right = pixels[y * image.width + x];
            variance += left.r != right.r || left.g != right.g || left.b != right.b;
        }
    }
    UnloadImageColors(pixels);
    UnloadImage(image);
    return variance;
}

static int draw_and_capture(void (*draw)(bt_app_ctx *), bt_app_ctx *ctx, const char *dir,
                            const char *shot) {
    bt_app_font_book book;
    char error[BT_APP_ERROR_MAX];
    char config_path[BT_PATH_MAX];
    bt_data_config_path(config_path);
    if (bt_app_fonts_load(config_path, NULL, NULL, 0, &book, error, sizeof error) != 0) {
        BT_ABORT("fonts: %s", error);
    }
    for (int i = 0; i < DRAW_DWELL_FRAMES; i++) {
        BeginDrawing();
        ClearBackground(BLACK);
        draw(ctx);
        EndDrawing();
    }
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/%s", dir, shot);
    TakeScreenshot(path);
    Image image = LoadImage(path);
    if (image.width <= 0 || image.height <= 0) {
        UnloadImage(image);
        return 0;
    }
    Color *pixels = LoadImageColors(image);
    long long variance = 0;
    for (int y = 0; y < image.height; y++) {
        for (int x = 1; x < image.width; x++) {
            Color left = pixels[y * image.width + x - 1];
            Color right = pixels[y * image.width + x];
            variance += left.r != right.r || left.g != right.g || left.b != right.b;
        }
    }
    UnloadImageColors(pixels);
    UnloadImage(image);
    bt_app_fonts_unload(&book);
    return variance > 0;
}

static void draw_solve(bt_app_ctx *ctx) {
    bt_views_net *pulse = &bt_app_views_shared()->net;
    bt_view_net_update(&ctx->net, pulse, 0.016F);
    bt_view_solve_row_draw(ctx);
}

static void draw_algs(bt_app_ctx *ctx) {
    bt_view_algs_row_draw(ctx);
}

static void draw_case_drill(bt_app_ctx *ctx) {
    bt_view_case_drill_row_draw(ctx);
}

BT_TEST(view_draw_b, solve_renders_pixels) {
    bt_app_config config;
    if (!open_window(&config)) {
        return;
    }
    bt_app_ctx ctx;
    bt_app_ctx_init(&ctx);
    ctx.state = BT_APP_SOLVE;
    bt_view_solve_app_state *app = bt_view_solve_app();
    bt_syllable_book book;
    bt_syllable_book_init(&book);
    bt_view_solve_rebuild(&app->session, 7, &book, NULL);
    ctx.solve_phase = BT_SOLVE_READY;
    char dir[BT_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", ta_temp_dir("view_draw_b", "solve"));
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/solve.png", dir);
    BT_CHECK(draw_and_capture(draw_solve, &ctx, dir, "solve.png"));
    BT_CHECK(region_variance(path, 60, 140, 230, 380) > 20);
    bt_app_ctx_free(&ctx);
    CloseWindow();
}

BT_TEST(view_draw_b, algs_renders_pixels) {
    bt_app_config config;
    if (!open_window(&config)) {
        return;
    }
    bt_app_ctx ctx;
    bt_app_ctx_init(&ctx);
    ctx.state = BT_APP_ALGS;
    bt_view_algs_enter(&ctx);
    char dir[BT_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", ta_temp_dir("view_draw_b", "algs"));
    BT_CHECK(draw_and_capture(draw_algs, &ctx, dir, "algs.png"));
    bt_app_ctx_free(&ctx);
    CloseWindow();
}

BT_TEST(view_draw_b, case_drill_renders_pixels) {
    bt_app_config config;
    if (!open_window(&config)) {
        return;
    }
    bt_app_ctx ctx;
    bt_app_ctx_init(&ctx);
    ctx.state = BT_APP_CASE_DRILL;
    bt_view_case_drill_app_state *app = bt_view_case_drill_app();
    bt_drill_case drill_case;
    memset(&drill_case, 0, sizeof drill_case);
    bt_case_ref_of(&drill_case.case_ref, "2x2", "cll", "cll", "cll.1");
    snprintf(drill_case.alg, BT_CASE_DRILL_TEXT_MAX, "%s", "R U R'");
    snprintf(drill_case.seed_alg, BT_CASE_DRILL_TEXT_MAX, "%s", "R U R'");
    bt_drill_case_queue queue;
    bt_drill_case_queue_init(&queue);
    bt_drill_case_queue_push(&queue, &drill_case);
    bt_case_drill_install(&app->session, &queue);
    bt_drill_case_queue_free(&queue);
    ctx.case_drill_phase = BT_CASE_DRILL_SHOW;
    char drill_dir[BT_PATH_MAX];
    snprintf(drill_dir, sizeof drill_dir, "%s", ta_temp_dir("view_draw_b", "case_drill"));
    BT_CHECK(draw_and_capture(draw_case_drill, &ctx, drill_dir, "case_drill.png"));
    bt_app_ctx_free(&ctx);
    CloseWindow();
}
