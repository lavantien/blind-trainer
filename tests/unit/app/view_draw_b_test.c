#include "app/views/case_drill.h"
#include "app/views/case_drill_view.h"
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

static int draw_and_capture(void (*draw)(bt_app_ctx *), bt_app_ctx *ctx, const char *shot) {
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
    snprintf(path, sizeof path, "%s\\%s", ta_temp_dir("view_draw_b", shot), shot);
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
    BT_CHECK(draw_and_capture(draw_solve, &ctx, "solve.png"));
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
    BT_CHECK(draw_and_capture(draw_algs, &ctx, "algs.png"));
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
    bt_drill_case queue[1];
    memset(queue, 0, sizeof queue);
    bt_case_ref_of(&queue[0].case_ref, "2x2", "cll", "cll", "cll.1");
    snprintf(queue[0].alg, BT_CASE_DRILL_TEXT_MAX, "%s", "R U R'");
    snprintf(queue[0].seed_alg, BT_CASE_DRILL_TEXT_MAX, "%s", "R U R'");
    bt_case_drill_install(&app->session, queue, 1);
    ctx.case_drill_phase = BT_CASE_DRILL_SHOW;
    BT_CHECK(draw_and_capture(draw_case_drill, &ctx, "case_drill.png"));
    bt_app_ctx_free(&ctx);
    CloseWindow();
}
