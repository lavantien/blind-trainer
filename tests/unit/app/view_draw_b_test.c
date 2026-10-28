#include "app/views/algs/mod.h"
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
    DRAW_TEXT_LIT_FLOOR = 400,
    DRAW_BANDS_MAX = 16,
};

static int open_window(bt_app_config *config) {
    config->window.width = 640;
    config->window.height = 480;
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

static long long lit_pixels(const char *path, int x0, int y0, int x1, int y1) {
    Image image = LoadImage(path);
    if (image.width <= 0 || image.height <= 0) {
        UnloadImage(image);
        return -1;
    }
    Color *pixels = LoadImageColors(image);
    long long lit = 0;
    for (int y = y0; y < y1 && y < image.height; y++) {
        for (int x = x0; x < x1 && x < image.width; x++) {
            Color pixel = pixels[y * image.width + x];
            lit += pixel.r != 0 || pixel.g != 0 || pixel.b != 0;
        }
    }
    UnloadImageColors(pixels);
    UnloadImage(image);
    return lit;
}

static size_t lit_row_bands(const char *path, int x0, int x1, int y0, int y1, int out[][2],
                            size_t max) {
    Image image = LoadImage(path);
    if (image.width <= 0 || image.height <= 0) {
        UnloadImage(image);
        return 0;
    }
    Color *pixels = LoadImageColors(image);
    size_t bands = 0;
    int run_start = -1;
    for (int y = y0; y < y1 && y < image.height; y++) {
        int lit = 0;
        for (int x = x0; x < x1 && x < image.width && !lit; x++) {
            Color pixel = pixels[y * image.width + x];
            lit = pixel.r != 0 || pixel.g != 0 || pixel.b != 0;
        }
        if (lit && run_start < 0) {
            run_start = y;
        }
        if (!lit && run_start >= 0 && bands < max) {
            out[bands][0] = run_start;
            out[bands][1] = y - 1;
            bands++;
            run_start = -1;
        }
    }
    if (run_start >= 0 && bands < max) {
        out[bands][0] = run_start;
        out[bands][1] = (y1 < image.height ? y1 : image.height) - 1;
        bands++;
    }
    UnloadImageColors(pixels);
    UnloadImage(image);
    return bands;
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

static bt_corpus_row pair_row(void) {
    bt_corpus_row row;
    memset(&row, 0, sizeof row);
    row.solver = BT_SOLVER_CHERRY;
    row.sheet = BT_SHEET_EDGE;
    row.rowid = 1;
    row.position = _strdup("UF-UB-UR");
    char err_token[64];
    if (!bt_alg_parse("R2 U' S R2 S' R2 U R2", &row.alg, err_token, sizeof err_token)) {
        BT_ABORT("fixture alg: %s", err_token);
    }
    row.alg_text = _strdup("R2 U' S R2 S' R2 U R2");
    row.commutator = _strdup("R2 U':[S,R2]");
    row.recon_link = _strdup("");
    row.youtube_link = _strdup("https://example.test/recon");
    return row;
}

static void seed_algs_ctx(bt_app_ctx *ctx) {
    ctx->state = BT_APP_ALGS;
    ctx->has_stores = 1;
    memset(&ctx->stores, 0, sizeof ctx->stores);
    bt_srs_store_init(&ctx->stores.srs);
    bt_solve_log_init(&ctx->stores.solves);
    bt_user_algs_init(&ctx->stores.user_algs);
    bt_str_list_init(&ctx->stores.seeds.hidden);
    bt_str_list_init(&ctx->stores.seeds.warnings);
    snprintf(ctx->paths.user_root, sizeof ctx->paths.user_root, "%s",
             ta_temp_dir("view_draw_b", "algs"));
    bt_corpus_row row = pair_row();
    bt_corpus_row *heap = malloc(sizeof row);
    memcpy(heap, &row, sizeof row);
    bt_corpus_from_rows(heap, 1, &ctx->vendor_corpus);
    ctx->has_corpus = 1;
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
    BT_CHECK(lit_pixels(path, 310, 95, 630, 460) >= DRAW_TEXT_LIT_FLOOR);
    bt_app_ctx_free(&ctx);
    CloseWindow();
}

BT_TEST(view_draw_b, solve_review_hint_draws_clear_of_the_splits_block) {
    bt_app_config config;
    bt_app_config_defaults(&config);
    config.window.width = 1280;
    config.window.height = 800;
    snprintf(config.window.title, sizeof config.window.title, "%s", "l49 review test");
    if (!bt_app_window_open(config.window.width, config.window.height, config.window.title, NULL)) {
        return;
    }
    bt_app_ctx ctx;
    bt_app_ctx_init(&ctx);
    ctx.state = BT_APP_SOLVE;
    ctx.has_stores = 1;
    memset(&ctx.stores, 0, sizeof ctx.stores);
    ctx.stores.settings.memo_cadence_ms = 800;
    bt_srs_store_init(&ctx.stores.srs);
    bt_solve_log_init(&ctx.stores.solves);
    bt_user_algs_init(&ctx.stores.user_algs);
    bt_str_list_init(&ctx.stores.seeds.hidden);
    bt_str_list_init(&ctx.stores.seeds.warnings);
    snprintf(ctx.paths.user_root, sizeof ctx.paths.user_root, "%s",
             ta_temp_dir("view_draw_b", "solve_review"));
    bt_view_solve_app_state *app = bt_view_solve_app();
    bt_syllable_book book;
    bt_syllable_book_init(&book);
    bt_view_solve_rebuild(&app->session, 7, &book, NULL);
    ctx.solve_phase = BT_SOLVE_READY;
    bt_view_solve_key(&ctx, app, KEY_SPACE);
    bt_view_solve_key(&ctx, app, KEY_SPACE);
    bt_view_solve_key(&ctx, app, KEY_SPACE);
    BT_CHECK(ctx.solve_phase == BT_SOLVE_REVIEW);
    char dir[BT_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", ta_temp_dir("view_draw_b", "review_shot"));
    BT_CHECK(draw_and_capture(draw_solve, &ctx, dir, "solve_review.png"));
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/solve_review.png", dir);
    int bands[DRAW_BANDS_MAX][2];
    size_t count = lit_row_bands(path, 590, 1260, 92, 780, bands, DRAW_BANDS_MAX);
    BT_CHECK(count >= 3);
    if (count >= 2) {
        int last = (int)count - 1;
        int height = bands[last][1] - bands[last][0] + 1;
        int gap = bands[last][0] - bands[last - 1][1] - 1;
        BT_CHECK(height <= 20);
        BT_CHECK(gap >= 20);
    }
    bt_syllable_book_free(&book);
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
    seed_algs_ctx(&ctx);
    bt_view_algs_enter(&ctx);
    char dir[BT_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", ta_temp_dir("view_draw_b", "algs"));
    BT_CHECK(draw_and_capture(draw_algs, &ctx, dir, "algs.png"));
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/algs.png", dir);
    BT_CHECK(lit_pixels(path, 25, 95, 250, 460) >= DRAW_TEXT_LIT_FLOOR);
    BT_CHECK(lit_pixels(path, 265, 95, 630, 460) >= DRAW_TEXT_LIT_FLOOR);
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
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/case_drill.png", drill_dir);
    BT_CHECK(lit_pixels(path, 25, 92, 430, 200) >= DRAW_TEXT_LIT_FLOOR);
    bt_app_ctx_free(&ctx);
    CloseWindow();
}
