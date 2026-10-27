#include "app/views/drill.h"
#include "app/views/drill_card.h"
#include "app/views/drill_view.h"
#include "app/views/frame.h"
#include "app/views/memo.h"
#include "app/views/menu.h"
#include "app/views/net_view.h"
#include "app/window.h"

#include "common.h"

#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    DRAW_DWELL_FRAMES = 6,
    PROBE_W = 640,
    PROBE_H = 480,
};

static bt_corpus_row heap_row(const char *position, const char *alg, const char *commutator) {
    bt_corpus_row row;
    memset(&row, 0, sizeof row);
    row.solver = BT_SOLVER_CHERRY;
    row.sheet = BT_SHEET_EDGE;
    row.rowid = 1;
    row.position = _strdup(position);
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

static bt_corpus single_pair_corpus(void) {
    bt_corpus_row rows[1] = {heap_row("UF-UB-UR", "R2 U' S R2 S' R2 U R2", "R2 U':[S,R2]")};
    bt_corpus_row *heap = malloc(sizeof rows);
    memcpy(heap, rows, sizeof rows);
    bt_corpus corpus;
    bt_corpus_from_rows(heap, 1, &corpus);
    return corpus;
}

static bt_case edge_ab(void) {
    bt_case case_id;
    case_id.orbit = BT_ORBIT_EDGE;
    case_id.t1 = bt_scheme_sticker_of(BT_ORBIT_EDGE, 'A');
    case_id.t2 = bt_scheme_sticker_of(BT_ORBIT_EDGE, 'B');
    return case_id;
}

static bt_stores probe_stores(void) {
    bt_stores stores;
    memset(&stores, 0, sizeof stores);
    stores.settings.memo_cadence_ms = 800;
    stores.settings.memo_mode = BT_MEMO_PER_ORBIT;
    stores.settings.drill_reps = 4;
    bt_srs_store_init(&stores.srs);
    bt_solve_log_init(&stores.solves);
    bt_user_algs_init(&stores.user_algs);
    bt_str_list_init(&stores.seeds.hidden);
    bt_str_list_init(&stores.seeds.warnings);
    return stores;
}

BT_TEST(view_draw, frame_metrics_pin_the_header_and_content_rect) {
    bt_view_frame_metrics metrics = bt_view_frame_metrics_for(1280.0F, 800.0F);
    BT_CHECK(metrics.pad == 20.0F);
    BT_CHECK(metrics.header_height == 70.0F);
    BT_CHECK(metrics.hairline == 1.0F);
    BT_CHECK(metrics.content_x == 20.0F);
    BT_CHECK(metrics.content_y == 90.0F);
    BT_CHECK(metrics.content_w == 1240.0F);
    BT_CHECK(metrics.content_h == 690.0F);
}

BT_TEST(view_draw, drill_net_geometry_fills_the_left_region) {
    bt_net_geometry geo = bt_view_drill_net_geometry(1280.0F, 800.0F);
    BT_CHECK(geo.quad == 37.0F);
    BT_CHECK(bt_net_width(&geo) <= 545.6F);
    BT_CHECK(bt_net_height(&geo) <= 690.0F);
}

BT_TEST(view_draw, drill_layout_splits_the_content_region) {
    bt_view_drill_layout layout = bt_view_drill_layout_for(20.0F, 90.0F, 1240.0F, 690.0F);
    BT_CHECK(fabsf(layout.net_w - 545.6F) < 1e-4F);
    BT_CHECK(fabsf(layout.card_x - 575.6F) < 1e-4F);
    BT_CHECK(fabsf(layout.card_w - 684.4F) < 1e-4F);
    BT_CHECK(layout.net_x == 20.0F && layout.net_y == 90.0F);
    BT_CHECK(layout.card_h == 690.0F);
}

static int open_config_window(void) {
    return bt_app_window_open(PROBE_W, PROBE_H, "view draw test", NULL);
}

static int shot_has_variance(const char *path) {
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
    return variance > 0;
}

static const bt_app_font_book *g_book;

static int draw_and_capture(void (*draw)(void)) {
    if (!open_config_window()) {
        return 0;
    }
    for (unsigned frame = 0; frame < DRAW_DWELL_FRAMES; frame++) {
        bt_app_window_frame_begin();
        ClearBackground(bt_app_color(bt_theme_background()));
        draw();
        bt_app_window_frame_end();
    }
    char shot[BT_PATH_MAX];
    snprintf(shot, sizeof shot, "%s/app-view-draw-%d.png", getenv("TEMP"), _getpid());
    remove(shot);
    if (!bt_app_window_capture(shot)) {
        bt_app_window_close();
        return 0;
    }
    int ok = shot_has_variance(shot);
    remove(shot);
    bt_app_window_close();
    return ok;
}

static int with_fonts(int (*run)(const bt_app_font_book *)) {
    if (!open_config_window()) {
        return 0;
    }
    bt_app_font_book book;
    char error[BT_APP_ERROR_MAX];
    char path[BT_PATH_MAX];
    bt_data_config_path(path);
    if (bt_app_fonts_load(path, NULL, NULL, 0, &book, error, sizeof error) != 0) {
        bt_app_window_close();
        BT_ABORT("fonts: %s", error);
    }
    int ok = run(&book);
    bt_app_fonts_unload(&book);
    bt_app_window_close();
    return ok;
}

static void draw_menu(void) {
    const bt_app_font_book *book = g_book;
    bt_str_list warnings;
    bt_str_list_init(&warnings);
    bt_view_menu_draw(&warnings, book->regular, book->bold, PROBE_W, PROBE_H);
}

static void draw_drill_card(void) {
    const bt_app_font_book *book = g_book;
    bt_corpus corpus = single_pair_corpus();
    bt_stores stores = probe_stores();
    bt_drill_session session;
    bt_drill_session_init(&session);
    bt_view_install_pair(&session, &corpus, &stores, edge_ab());
    bt_syllable_book syllables;
    bt_syllable_book_init(&syllables);
    bt_card_content content;
    bt_view_card_content(&session, &corpus, &syllables, &stores, 0, &content);
    bt_view_frame_draw(book->bold, book->regular, PROBE_W, PROBE_H, "drill",
                       "space reps, 1..4 rate, esc menu");
    bt_view_drill_card_draw(&content, book->regular, 60.0F, 60.0F, 360.0F);
    bt_syllable_book_free(&syllables);
    bt_drill_session_free(&session);
    bt_stores_free(&stores);
    bt_corpus_free(&corpus);
}

static void draw_drill_reps(void) {
    const bt_app_font_book *book = g_book;
    bt_corpus corpus = single_pair_corpus();
    bt_stores stores = probe_stores();
    bt_drill_session session;
    bt_drill_session_init(&session);
    bt_view_install_pair(&session, &corpus, &stores, edge_ab());
    bt_view_drill_reps_draw(&session, book->regular, 60.0F, 60.0F, 360.0F);
    session.reps_done = 2;
    session.splits[0].is_a = 1;
    session.splits[0].secs = 1.0F;
    session.splits[1].is_a = 0;
    session.splits[1].secs = 2.0F;
    session.split_count = 2;
    bt_view_drill_rate_draw(&session, book->regular, 60.0F, 300.0F, 160.0F);
    bt_drill_session_free(&session);
    bt_stores_free(&stores);
    bt_corpus_free(&corpus);
}

static void draw_net(void) {
    const bt_app_font_book *book = g_book;
    bt_app_net_state net;
    bt_app_net_state_default(&net);
    bt_app_views *views = bt_app_views_shared();
    bt_view_net_update(&net, &views->net, 0.016F);
    bt_view_drill_net_draw(book->bold, &views->net, 40.0F, 40.0F, 560.0F, 400.0F);
    bt_app_views_dispose_shared();
}

static void draw_memo(void) {
    const bt_app_font_book *book = g_book;
    bt_memo_rows rows;
    snprintf(rows.focal, sizeof rows.focal, "%s", "tả");
    snprintf(rows.recent, sizeof rows.recent, "%s", "AB CD");
    snprintf(rows.status, sizeof rows.status, "%s", "stream 3/20  cadence 800 ms");
    bt_view_memo_draw(&rows, book->regular, book->bold, PROBE_W, PROBE_H);
}

static int run_menu(const bt_app_font_book *book) {
    g_book = book;
    return draw_and_capture(draw_menu);
}

static int run_drill_card(const bt_app_font_book *book) {
    g_book = book;
    return draw_and_capture(draw_drill_card);
}

static int run_drill_reps(const bt_app_font_book *book) {
    g_book = book;
    return draw_and_capture(draw_drill_reps);
}

static int run_net(const bt_app_font_book *book) {
    g_book = book;
    return draw_and_capture(draw_net);
}

static int run_memo(const bt_app_font_book *book) {
    g_book = book;
    return draw_and_capture(draw_memo);
}

BT_TEST(view_draw, menu_rows_render_pixels) {
    BT_CHECK(with_fonts(run_menu));
}

BT_TEST(view_draw, drill_card_renders_pixels) {
    BT_CHECK(with_fonts(run_drill_card));
}

BT_TEST(view_draw, drill_reps_and_rate_render_pixels) {
    BT_CHECK(with_fonts(run_drill_reps));
}

BT_TEST(view_draw, net_view_renders_pixels) {
    BT_CHECK(with_fonts(run_net));
}

BT_TEST(view_draw, memo_rows_render_pixels) {
    BT_CHECK(with_fonts(run_memo));
}

static void fill_row_ctx(bt_app_ctx *ctx) {
    bt_app_ctx_init(ctx);
    ctx->state = BT_APP_DRILL;
    ctx->has_corpus = 1;
    bt_corpus_row rows[1] = {heap_row("UF-UB-UR", "R2 U' S R2 S' R2 U R2", "R2 U':[S,R2]")};
    bt_corpus_row *heap = malloc(sizeof rows);
    memcpy(heap, rows, sizeof rows);
    bt_corpus_from_rows(heap, 1, &ctx->vendor_corpus);
    ctx->has_stores = 1;
    memset(&ctx->stores, 0, sizeof ctx->stores);
    ctx->stores.settings.memo_cadence_ms = 800;
    ctx->stores.settings.memo_mode = BT_MEMO_PER_ORBIT;
    ctx->stores.settings.drill_reps = 4;
    bt_srs_store_init(&ctx->stores.srs);
    bt_solve_log_init(&ctx->stores.solves);
    bt_user_algs_init(&ctx->stores.user_algs);
    bt_str_list_init(&ctx->stores.seeds.hidden);
    bt_str_list_init(&ctx->stores.seeds.warnings);
    bt_fsrs_config_defaults(&ctx->fsrs);
}

static bt_app_ctx g_ctx;

static void draw_drill_row(void) {
    bt_view_drill_row_draw(&g_ctx);
}

static void draw_memo_row(void) {
    g_ctx.state = BT_APP_MEMO;
    bt_view_memo_row_draw(&g_ctx);
    g_ctx.state = BT_APP_DRILL;
}

static void draw_menu_row(void) {
    g_ctx.state = BT_APP_MENU;
    bt_view_menu_row_draw(&g_ctx);
    g_ctx.state = BT_APP_DRILL;
}

static int run_drill_row(const bt_app_font_book *book) {
    g_book = book;
    fill_row_ctx(&g_ctx);
    bt_drill_app_state *app = bt_view_drill_app();
    bt_view_drill_enter_state(&g_ctx, app);
    bt_app_views *views = bt_app_views_shared();
    bt_view_net_update(&g_ctx.net, &views->net, 0.016F);
    int ok = draw_and_capture(draw_drill_row);
    if (ok) {
        bt_view_drill_key(&g_ctx, app, 32);
        bt_view_net_update(&g_ctx.net, &views->net, 0.016F);
        ok = draw_and_capture(draw_drill_row);
    }
    if (ok) {
        for (int rep = 0; rep < 4; rep++) {
            bt_view_drill_key(&g_ctx, app, 32);
        }
        bt_view_net_update(&g_ctx.net, &views->net, 0.016F);
        ok = draw_and_capture(draw_drill_row);
    }
    bt_app_ctx_free(&g_ctx);
    bt_app_views_dispose_shared();
    return ok;
}

static int run_memo_row(const bt_app_font_book *book) {
    g_book = book;
    fill_row_ctx(&g_ctx);
    bt_memo_app_state *app = bt_view_memo_app();
    bt_view_memo_enter_state(&g_ctx, app);
    g_ctx.memo_phase = BT_MEMO_STREAM;
    int ok = draw_and_capture(draw_memo_row);
    bt_app_ctx_free(&g_ctx);
    bt_app_views_dispose_shared();
    return ok;
}

static int run_menu_row(const bt_app_font_book *book) {
    g_book = book;
    fill_row_ctx(&g_ctx);
    bt_str_list_push(&g_ctx.boot_warnings, "vendor data missing");
    int ok = draw_and_capture(draw_menu_row);
    bt_app_ctx_free(&g_ctx);
    bt_app_views_dispose_shared();
    return ok;
}

static int run_fonts_current(const bt_app_font_book *book) {
    const bt_app_font_book *current = bt_app_fonts_current();
    return current != NULL && current->regular == book->regular && current->bold == book->bold;
}

BT_TEST(view_draw, drill_row_draw_renders_all_three_phases) {
    BT_CHECK(bt_app_fonts_current() == NULL);
    BT_CHECK(with_fonts(run_drill_row));
}

BT_TEST(view_draw, memo_row_draw_renders) {
    BT_CHECK(with_fonts(run_memo_row));
}

BT_TEST(view_draw, menu_row_draw_renders) {
    BT_CHECK(with_fonts(run_menu_row));
}

BT_TEST(view_draw, fonts_current_tracks_the_loaded_book) {
    BT_CHECK(with_fonts(run_fonts_current));
    BT_CHECK(bt_app_fonts_current() == NULL);
}

static int colors_equal(bt_theme_color a, bt_theme_color b) {
    return a.red == b.red && a.green == b.green && a.blue == b.blue && a.alpha == b.alpha;
}

static int colors_near(bt_theme_color a, bt_theme_color b) {
    return fabsf(a.red - b.red) < 1e-5F && fabsf(a.green - b.green) < 1e-5F &&
           fabsf(a.blue - b.blue) < 1e-5F && fabsf(a.alpha - b.alpha) < 1e-5F;
}

BT_TEST(view_draw, net_update_puts_highlight_ink_on_target_cells) {
    bt_app_ctx ctx;
    fill_row_ctx(&ctx);
    bt_drill_app_state *app = bt_view_drill_app();
    bt_view_drill_app_init(app);
    app->seed = 20261012;
    bt_view_drill_enter_state(&ctx, app);
    bt_app_views *views = bt_app_views_shared();
    memset(&views->net, 0, sizeof views->net);
    bt_view_net_update(&ctx.net, &views->net, 0.5F);
    float wave = 0.6F + 0.4F * sinf(0.5F * 6.283185307F * 2.0F);
    unsigned char target = ctx.net.targets[0];
    size_t target_grid = bt_net_grid_index(target);
    unsigned char occupant = bt_cube_sticker_at(&ctx.net.cube, target);
    bt_theme_color expected = bt_theme_mix(bt_theme_face_color((size_t)bt_net_face_of(occupant)),
                                           bt_theme_interchange(), wave);
    BT_CHECK(colors_near(views->net.ink[target_grid].background, expected));
    unsigned char buffer = ctx.net.buffer_sticker;
    size_t buffer_grid = bt_net_grid_index(buffer);
    unsigned char buffer_occupant = bt_cube_sticker_at(&ctx.net.cube, buffer);
    bt_theme_color buffer_expected = bt_theme_mix(
        bt_theme_face_color((size_t)bt_net_face_of(buffer_occupant)), bt_theme_teal(), wave);
    BT_CHECK(colors_near(views->net.ink[buffer_grid].background, buffer_expected));
    BT_CHECK(colors_equal(views->net.ink[4].background, bt_theme_face_color(0)));
    bt_app_views_dispose_shared();
    bt_app_ctx_free(&ctx);
}
