#include "app/views/algs/mod.h"
#include "app/views/case_drill.h"
#include "app/views/case_drill_view.h"
#include "app/views/drill_view.h"
#include "app/views/frame.h"
#include "app/views/net_view.h"
#include "app/views/solve.h"
#include "app/views/state.h"
#include "app/window.h"
#include "core/alg.h"
#include "core/cube.h"
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
    return bt_app_window_open_driven(config->window.width, config->window.height,
                                     config->window.title, &config->render);
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

BT_TEST(view_draw_b, solve_review_clamps_the_broken_block_and_anchors_the_hint) {
    bt_app_config config;
    bt_app_config_defaults(&config);
    config.window.width = 1280;
    config.window.height = 800;
    snprintf(config.window.title, sizeof config.window.title, "%s", "l49 review test");
    if (!bt_app_window_open_driven(config.window.width, config.window.height, config.window.title,
                                   NULL)) {
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
    bt_solve_view_parts parts;
    bt_view_solve_review_parts(&app->session, &parts);
    size_t broken_lines = 1;
    for (const char *at = parts.broken; *at != '\0'; at++) {
        if (*at == '\n') {
            broken_lines++;
        }
    }
    BT_CHECK(broken_lines >= 8);
    const bt_theme *theme = bt_theme_table();
    bt_view_frame_metrics metrics = bt_view_frame_metrics_for(1280.0F, 800.0F);
    bt_view_solve_text_column column = bt_view_solve_text_column_for(1280.0F, 800.0F);
    float gap = theme->spacing_rem * theme->root_rem;
    float focal = theme->font_focal * theme->root_rem;
    float title = theme->font_title * theme->root_rem;
    float heading = theme->font_heading * theme->root_rem;
    float small = theme->font_small * theme->root_rem;
    float bottom = metrics.content_y + metrics.content_h;
    float hint_y = bottom - small;
    float block_limit = hint_y - gap;
    float split_top = metrics.content_y + (focal + gap) * 2.0F + title + gap + heading + gap;
    size_t split_lines = 1;
    for (const char *at = parts.splits; *at != '\0'; at++) {
        if (*at == '\n') {
            split_lines++;
        }
    }
    if (parts.splits[0] == '\0') {
        split_lines = 0;
    }
    bt_view_solve_block_clamp split_clamp =
        bt_view_solve_block_clamp_for(split_top, block_limit, heading, split_lines);
    float split_rows = (float)split_clamp.visible + (split_clamp.hidden > 0 ? 1.0F : 0.0F);
    float broken_top = split_top + split_rows * heading + gap;
    bt_view_solve_block_clamp broken_clamp =
        bt_view_solve_block_clamp_for(broken_top, block_limit, small, broken_lines);
    BT_CHECK(broken_clamp.visible < broken_lines);
    char dir[BT_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", ta_temp_dir("view_draw_b", "review_shot"));
    BT_CHECK(draw_and_capture(draw_solve, &ctx, dir, "solve_review.png"));
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/solve_review.png", dir);
    int bands[DRAW_BANDS_MAX][2];
    size_t count =
        lit_row_bands(path, (int)column.x - 2, (int)(column.x + column.width) + 2,
                      (int)split_top - 2, config.window.height - 2, bands, DRAW_BANDS_MAX);
    BT_CHECK(count >= 4);
    if (count >= 4) {
        int last = (int)count - 1;
        int height = bands[last][1] - bands[last][0] + 1;
        int band_gap = bands[last][0] - bands[last - 1][1] - 1;
        BT_CHECK(height <= (int)small);
        BT_CHECK(band_gap >= (int)gap);
        BT_CHECK(bands[last][0] >= (int)hint_y - 2);
        BT_CHECK(bands[last][0] <= (int)(hint_y + small * 0.25F));
        BT_CHECK(bands[last][1] <= (int)(hint_y + small));
        BT_CHECK(bands[last - 1][1] < (int)block_limit);
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

enum {
    FIT_CARD_X = 40,
    FIT_CARD_W = 240,
    FIT_WINDOW_W = 640,
    FIT_WINDOW_H = 480,
};

static const bt_app_font_book *g_fitted_book;

static bt_corpus_row fitted_row(const char *alg, const char *commutator) {
    bt_corpus_row row;
    memset(&row, 0, sizeof row);
    row.solver = BT_SOLVER_CHERRY;
    row.sheet = BT_SHEET_EDGE;
    row.rowid = 1;
    row.position = _strdup("UF-UB-UR");
    char err_token[64];
    if (!bt_alg_parse(alg, &row.alg, err_token, sizeof err_token)) {
        BT_ABORT("fixture alg %s: %s", alg, err_token);
    }
    row.alg_text = _strdup(alg);
    row.commutator = _strdup(commutator);
    row.recon_link = _strdup("");
    row.youtube_link = _strdup("");
    return row;
}

static bt_corpus fitted_corpus(const char *alg, const char *commutator) {
    bt_corpus_row row = fitted_row(alg, commutator);
    bt_corpus_row *heap = malloc(sizeof row);
    memcpy(heap, &row, sizeof row);
    bt_corpus corpus;
    bt_corpus_from_rows(heap, 1, &corpus);
    return corpus;
}

static bt_stores fitted_stores(void) {
    bt_stores stores;
    memset(&stores, 0, sizeof stores);
    stores.settings.memo_cadence_ms = 800;
    stores.settings.memo_mode = BT_MEMO_SYLLABLES;
    stores.settings.drill_reps = 4;
    bt_srs_store_init(&stores.srs);
    bt_solve_log_init(&stores.solves);
    bt_user_algs_init(&stores.user_algs);
    bt_str_list_init(&stores.seeds.hidden);
    bt_str_list_init(&stores.seeds.warnings);
    return stores;
}

static bt_case fitted_edge_case(void) {
    bt_case case_id;
    case_id.orbit = BT_ORBIT_EDGE;
    case_id.t1 = bt_scheme_sticker_of(BT_ORBIT_EDGE, 'A');
    case_id.t2 = bt_scheme_sticker_of(BT_ORBIT_EDGE, 'B');
    return case_id;
}

static int fitted_case(void (*draw)(void), const char *tag) {
    bt_app_config config;
    if (!open_window(&config)) {
        return 0;
    }
    bt_app_font_book book;
    char error[BT_APP_ERROR_MAX];
    char config_path[BT_PATH_MAX];
    bt_data_config_path(config_path);
    if (bt_app_fonts_load(config_path, NULL, NULL, 0, &book, error, sizeof error) != 0) {
        CloseWindow();
        BT_ABORT("fonts: %s", error);
    }
    g_fitted_book = &book;
    for (int i = 0; i < DRAW_DWELL_FRAMES; i++) {
        BeginDrawing();
        ClearBackground(BLACK);
        draw();
        EndDrawing();
    }
    g_fitted_book = NULL;
    char dir[BT_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", ta_temp_dir("view_draw_b", tag));
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/%s.png", dir, tag);
    TakeScreenshot(path);
    Image image = LoadImage(path);
    if (image.width <= 0 || image.height <= 0) {
        UnloadImage(image);
        bt_app_fonts_unload(&book);
        CloseWindow();
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
    long long beyond = lit_pixels(path, FIT_CARD_X + FIT_CARD_W + 2, 0, FIT_WINDOW_W, FIT_WINDOW_H);
    bt_app_fonts_unload(&book);
    CloseWindow();
    return variance > 0 && beyond == 0;
}

static void draw_fitted_card_long_alg(void) {
    const bt_app_font_book *book = g_fitted_book;
    const char *alg = "R2 U' S R2 S' R2 U R2 S R2 U' S R2 S' R2 U R2";
    bt_corpus corpus = fitted_corpus(alg, "");
    bt_stores stores = fitted_stores();
    bt_drill_session session;
    bt_drill_session_init(&session);
    bt_view_install_pair(&session, &corpus, &stores, fitted_edge_case());
    bt_syllable_book syllables;
    bt_syllable_book_init(&syllables);
    bt_card_content content;
    bt_view_card_content(&session, &corpus, &syllables, &stores, 0, &content);
    bt_view_drill_card_draw(&content, book->regular, (float)FIT_CARD_X, (float)FIT_CARD_X,
                            (float)FIT_CARD_W);
    bt_syllable_book_free(&syllables);
    bt_drill_session_free(&session);
    bt_stores_free(&stores);
    bt_corpus_free(&corpus);
}

static void draw_fitted_card_wide_spans(void) {
    const bt_app_font_book *book = g_fitted_book;
    bt_corpus corpus = fitted_corpus("S R2 S' R2 U R2 U' R2", "[S,R2] + [U,R2]");
    bt_stores stores = fitted_stores();
    bt_drill_session session;
    bt_drill_session_init(&session);
    bt_view_install_pair(&session, &corpus, &stores, fitted_edge_case());
    bt_syllable_book syllables;
    bt_syllable_book_init(&syllables);
    bt_card_content content;
    bt_view_card_content(&session, &corpus, &syllables, &stores, 0, &content);
    bt_view_drill_card_draw(&content, book->regular, (float)FIT_CARD_X, (float)FIT_CARD_X,
                            (float)FIT_CARD_W);
    bt_syllable_book_free(&syllables);
    bt_drill_session_free(&session);
    bt_stores_free(&stores);
    bt_corpus_free(&corpus);
}

static void draw_fitted_reps_and_rate(void) {
    const bt_app_font_book *book = g_fitted_book;
    bt_corpus corpus = fitted_corpus("R2 U' S R2 S' R2 U R2", "");
    bt_stores stores = fitted_stores();
    bt_drill_session session;
    bt_drill_session_init(&session);
    bt_view_install_pair(&session, &corpus, &stores, fitted_edge_case());
    bt_view_drill_reps_draw(&session, book->regular, (float)FIT_CARD_X, (float)FIT_CARD_X,
                            (float)FIT_CARD_W);
    bt_view_drill_rate_draw(&session, book->regular, (float)FIT_CARD_X, 300.0F, (float)FIT_CARD_W);
    bt_drill_session_free(&session);
    bt_stores_free(&stores);
    bt_corpus_free(&corpus);
}

BT_TEST(view_draw_b, drill_card_fits_a_long_alg_inside_the_column) {
    BT_CHECK(fitted_case(draw_fitted_card_long_alg, "card_long_alg"));
}

BT_TEST(view_draw_b, drill_card_falls_back_to_one_fitted_line_for_wide_spans) {
    BT_CHECK(fitted_case(draw_fitted_card_wide_spans, "card_wide_spans"));
}

BT_TEST(view_draw_b, drill_reps_and_rate_fit_their_lines_inside_the_column) {
    BT_CHECK(fitted_case(draw_fitted_reps_and_rate, "reps_rate"));
}
