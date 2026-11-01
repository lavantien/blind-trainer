#include "app/views/frame.h"
#include "app/views/menu.h"
#include "app/window.h"

#include "common.h"

#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static bt_str_list list_of(const char *const *items, size_t count) {
    bt_str_list list;
    bt_str_list_init(&list);
    for (size_t i = 0; i < count; i++) {
        bt_str_list_push(&list, items[i]);
    }
    return list;
}

static void fixture_menu_ctx(bt_app_ctx *ctx) {
    bt_app_ctx_init(ctx);
    ctx->has_stores = 1;
    memset(&ctx->stores, 0, sizeof ctx->stores);
    ctx->stores.settings.memo_cadence_ms = 800;
    ctx->stores.settings.memo_mode = BT_MEMO_PER_ORBIT;
    ctx->stores.settings.drill_reps = 6;
    bt_srs_store_init(&ctx->stores.srs);
    bt_solve_log_init(&ctx->stores.solves);
    bt_user_algs_init(&ctx->stores.user_algs);
    bt_str_list_init(&ctx->stores.seeds.hidden);
    bt_str_list_init(&ctx->stores.seeds.warnings);
}

BT_TEST(view_menu, menu_rows_list_every_key_with_its_description) {
    bt_app_ctx ctx;
    fixture_menu_ctx(&ctx);
    bt_view_menu_row rows[BT_VIEW_MENU_ROW_COUNT];
    bt_view_menu_rows(&ctx, rows);
    const char *titles[BT_VIEW_MENU_ROW_COUNT] = {
        "1 drill", "2 memo training", "3 solve", "4 algs", "5 stats", "6 settings",
    };
    for (int i = 0; i < BT_VIEW_MENU_ROW_COUNT; i++) {
        char title[BT_VIEW_MENU_TEXT_MAX];
        bt_view_menu_title(&rows[i], title);
        BT_CHECK(strcmp(title, titles[i]) == 0);
    }
    BT_CHECK(strcmp(rows[0].description, "physical 3-style pair drilling with spaced repetition") ==
             0);
    BT_CHECK(strcmp(rows[1].description, "streaming memo on seeded scrambles") == 0);
    BT_CHECK(strcmp(rows[2].description, "the timed full cycle") == 0);
    BT_CHECK(strcmp(rows[3].description, "browse 818 cases and the event repos") == 0);
    BT_CHECK(strcmp(rows[4].description, "solve history and review load") == 0);
    BT_CHECK(strcmp(rows[5].description, "cadence, memo mode, reps") == 0);
    bt_app_ctx_free(&ctx);
}

BT_TEST(view_menu, menu_rows_carry_context_data_lines) {
    bt_app_ctx ctx;
    fixture_menu_ctx(&ctx);
    bt_solve_record record;
    record.started_ms = 1;
    record.memo_ms = 20000;
    record.exec_ms = 10000;
    record.success = 1;
    bt_solve_log_push(&ctx.stores.solves, record);
    bt_view_menu_row rows[BT_VIEW_MENU_ROW_COUNT];
    bt_view_menu_rows(&ctx, rows);
    BT_CHECK(strcmp(rows[0].data, "0 pairs due") == 0);
    BT_CHECK(strcmp(rows[1].data, "cadence 800 ms, mode per-orbit") == 0);
    BT_CHECK(strcmp(rows[2].data, "1 solves logged") == 0);
    BT_CHECK(strcmp(rows[3].data, "vendor corpus not loaded") == 0);
    BT_CHECK(strcmp(rows[4].data, "1 solves, 0 due cards") == 0);
    BT_CHECK(strcmp(rows[5].data, "cadence 800 ms, reps 6, mode per-orbit") == 0);
    bt_app_ctx_free(&ctx);
}

BT_TEST(view_menu, warning_context_joins_long_warning_lists_in_full) {
    char big[300];
    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = 0;
    const char *items[3] = {big, big, big};
    bt_str_list list = list_of(items, 3);
    char *context = bt_view_menu_warning_context(&list);
    size_t wanted = (sizeof big - 1) * 3 + strlen(" | ") * 2;
    BT_CHECK(strlen(context) == wanted);
    free(context);
    bt_str_list_free(&list);
}

BT_TEST(view_menu, warning_context_joins_boot_warnings) {
    bt_str_list empty;
    bt_str_list_init(&empty);
    char *context = bt_view_menu_warning_context(&empty);
    BT_CHECK(context != NULL && strcmp(context, "") == 0);
    free(context);
    bt_str_list_free(&empty);

    const char *one_items[1] = {"vendor data missing"};
    bt_str_list one = list_of(one_items, 1);
    char *one_text = bt_view_menu_warning_context(&one);
    BT_CHECK(one_text != NULL && strcmp(one_text, "vendor data missing") == 0);
    free(one_text);
    bt_str_list_free(&one);

    const char *two_items[2] = {"a", "b"};
    bt_str_list two = list_of(two_items, 2);
    char *two_text = bt_view_menu_warning_context(&two);
    BT_CHECK(two_text != NULL && strcmp(two_text, "a | b") == 0);
    free(two_text);
    bt_str_list_free(&two);
}

enum {
    MENU_DEFAULT_WINDOW_W = 1280,
    MENU_DEFAULT_WINDOW_H = 800,
    MENU_PROBE_WINDOW_W = 640,
    MENU_PROBE_WINDOW_H = 480,
};

static const float MENU_FLOAT_TOL = 0.01F;

static void pin_default_root(void) {
    bt_theme_set_root_for((float)MENU_DEFAULT_WINDOW_H);
}

BT_TEST(view_menu, menu_layout_tiles_two_columns_across_the_content_area) {
    pin_default_root();
    const bt_theme *theme = bt_theme_table();
    bt_view_frame_metrics metrics =
        bt_view_frame_metrics_for((float)MENU_DEFAULT_WINDOW_W, (float)MENU_DEFAULT_WINDOW_H);
    bt_view_menu_layout layout = bt_view_menu_layout_for(
        (float)MENU_DEFAULT_WINDOW_W, (float)MENU_DEFAULT_WINDOW_H, BT_VIEW_MENU_DESC_MAX_LINES);
    float gutter = theme->spacing_rem * theme->root_rem;
    float col_w = (metrics.content_w - gutter) / 2.0F;
    BT_CHECK(fabsf(layout.gutter - gutter) < MENU_FLOAT_TOL);
    BT_CHECK(fabsf(layout.col_w - col_w) < MENU_FLOAT_TOL);
    BT_CHECK(fabsf(layout.col_x[0] - metrics.content_x) < MENU_FLOAT_TOL);
    BT_CHECK(fabsf(layout.col_x[1] - (metrics.content_x + col_w + gutter)) < MENU_FLOAT_TOL);
    float tiled = (layout.col_x[1] + layout.col_w) - layout.col_x[0];
    BT_CHECK(fabsf(tiled - metrics.content_w) < MENU_FLOAT_TOL);
    BT_CHECK(layout.col_x[0] >= metrics.content_x - MENU_FLOAT_TOL);
    BT_CHECK(layout.col_x[1] + layout.col_w <=
             metrics.content_x + metrics.content_w + MENU_FLOAT_TOL);
}

BT_TEST(view_menu, menu_layout_pitches_rows_and_centers_the_block) {
    pin_default_root();
    const bt_theme *theme = bt_theme_table();
    bt_view_frame_metrics metrics =
        bt_view_frame_metrics_for((float)MENU_DEFAULT_WINDOW_W, (float)MENU_DEFAULT_WINDOW_H);
    int rows_per_column = BT_VIEW_MENU_ROW_COUNT / BT_VIEW_MENU_COLUMN_COUNT;
    for (int desc_lines = 1; desc_lines <= BT_VIEW_MENU_DESC_MAX_LINES; desc_lines++) {
        bt_view_menu_layout layout = bt_view_menu_layout_for(
            (float)MENU_DEFAULT_WINDOW_W, (float)MENU_DEFAULT_WINDOW_H, desc_lines);
        float heading = theme->font_heading * theme->root_rem;
        float small = theme->font_small * theme->root_rem;
        float micro = theme->font_micro * theme->root_rem;
        float inner_gap = theme->spacing_rem * 0.5F * theme->root_rem;
        float row_gap = theme->spacing_rem * 2.0F * theme->root_rem;
        float row_h = heading + inner_gap + small * (float)desc_lines + inner_gap + micro;
        float block_h = (float)rows_per_column * row_h + (float)(rows_per_column - 1) * row_gap;
        BT_CHECK(fabsf(layout.row_h - row_h) < MENU_FLOAT_TOL);
        BT_CHECK(fabsf(layout.row_pitch - (row_h + row_gap)) < MENU_FLOAT_TOL);
        BT_CHECK(block_h <= metrics.content_h + MENU_FLOAT_TOL);
        float top_room = layout.first_row_y - metrics.content_y;
        float bottom_room =
            (metrics.content_y + metrics.content_h) - (layout.first_row_y + block_h);
        BT_CHECK(fabsf(top_room - bottom_room) < MENU_FLOAT_TOL);
        BT_CHECK(top_room >= -MENU_FLOAT_TOL);
    }
}

BT_TEST(view_menu, menu_layout_rooms_the_data_line_inside_each_row) {
    pin_default_root();
    const bt_theme *theme = bt_theme_table();
    bt_view_menu_layout layout = bt_view_menu_layout_for(
        (float)MENU_DEFAULT_WINDOW_W, (float)MENU_DEFAULT_WINDOW_H, BT_VIEW_MENU_DESC_MAX_LINES);
    float heading = theme->font_heading * theme->root_rem;
    float small = theme->font_small * theme->root_rem;
    float micro = theme->font_micro * theme->root_rem;
    float inner_gap = theme->spacing_rem * 0.5F * theme->root_rem;
    BT_CHECK(fabsf(layout.desc_dy - (heading + inner_gap)) < MENU_FLOAT_TOL);
    BT_CHECK(fabsf(layout.data_dy -
                   (heading + inner_gap + small * (float)BT_VIEW_MENU_DESC_MAX_LINES + inner_gap)) <
             MENU_FLOAT_TOL);
    BT_CHECK(layout.data_dy + micro <= layout.row_h + MENU_FLOAT_TOL);
}

BT_TEST(view_menu, menu_layout_fits_across_window_sizes) {
    const float sizes[][2] = {
        {1024.0F, 640.0F},
        {1280.0F, 800.0F},
        {1600.0F, 900.0F},
        {1920.0F, 1080.0F},
    };
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        float window_w = sizes[i][0];
        float window_h = sizes[i][1];
        bt_theme_set_root_for(window_h);
        bt_view_frame_metrics metrics = bt_view_frame_metrics_for(window_w, window_h);
        bt_view_menu_layout layout =
            bt_view_menu_layout_for(window_w, window_h, BT_VIEW_MENU_DESC_MAX_LINES);
        int rows_per_column = BT_VIEW_MENU_ROW_COUNT / BT_VIEW_MENU_COLUMN_COUNT;
        float block_h = (float)rows_per_column * layout.row_h +
                        (float)(rows_per_column - 1) * (layout.row_pitch - layout.row_h);
        BT_CHECK(block_h <= metrics.content_h + MENU_FLOAT_TOL);
        float tiled = (layout.col_x[1] + layout.col_w) - layout.col_x[0];
        BT_CHECK(fabsf(tiled - metrics.content_w) < MENU_FLOAT_TOL);
    }
    pin_default_root();
}

BT_TEST(view_menu, menu_layout_keeps_the_block_inside_short_windows) {
    const float short_heights[] = {400.0F, 450.0F};
    for (size_t i = 0; i < sizeof short_heights / sizeof short_heights[0]; i++) {
        float window_h = short_heights[i];
        bt_theme_set_root_for(window_h);
        bt_view_frame_metrics metrics =
            bt_view_frame_metrics_for((float)MENU_DEFAULT_WINDOW_W, window_h);
        bt_view_menu_layout layout = bt_view_menu_layout_for((float)MENU_DEFAULT_WINDOW_W, window_h,
                                                             BT_VIEW_MENU_DESC_MAX_LINES);
        int rows_per_column = BT_VIEW_MENU_ROW_COUNT / BT_VIEW_MENU_COLUMN_COUNT;
        float block_h = (float)rows_per_column * layout.row_h +
                        (float)(rows_per_column - 1) * (layout.row_pitch - layout.row_h);
        BT_CHECK(layout.first_row_y >= metrics.content_y - MENU_FLOAT_TOL);
        BT_CHECK(layout.first_row_y + block_h <=
                 metrics.content_y + metrics.content_h + MENU_FLOAT_TOL);
        BT_CHECK(layout.desc_lines >= 1 && layout.desc_lines <= BT_VIEW_MENU_DESC_MAX_LINES);
    }
    pin_default_root();
}

static int open_menu_probe_window(void) {
    return bt_app_window_open_driven(MENU_PROBE_WINDOW_W, MENU_PROBE_WINDOW_H, "menu wrap test",
                                     NULL);
}

static int with_menu_fonts(int (*run)(const bt_app_font_book *)) {
    if (!open_menu_probe_window()) {
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

static float default_column_width(void) {
    bt_view_menu_layout layout = bt_view_menu_layout_for(
        (float)MENU_DEFAULT_WINDOW_W, (float)MENU_DEFAULT_WINDOW_H, BT_VIEW_MENU_DESC_MAX_LINES);
    return layout.col_w;
}

static size_t wrap_at_default_column(const void *font, const char *text, float size,
                                     char lines[][BT_VIEW_MENU_TEXT_MAX], size_t cap) {
    return bt_view_menu_wrap_lines(font, text, size, default_column_width(), lines, cap);
}

static int lines_join_back(const char *text, char lines[][BT_VIEW_MENU_TEXT_MAX], size_t count) {
    char joined[BT_VIEW_MENU_TEXT_MAX];
    joined[0] = '\0';
    for (size_t i = 0; i < count; i++) {
        if (i > 0) {
            strncat(joined, " ", sizeof joined - strlen(joined) - 1);
        }
        strncat(joined, lines[i], sizeof joined - strlen(joined) - 1);
    }
    return strcmp(joined, text) == 0;
}

static int wrap_descriptions_fit(const bt_app_font_book *book) {
    bt_app_ctx ctx;
    fixture_menu_ctx(&ctx);
    bt_view_menu_row rows[BT_VIEW_MENU_ROW_COUNT];
    bt_view_menu_rows(&ctx, rows);
    pin_default_root();
    float small = bt_theme_table()->font_small * bt_theme_table()->root_rem;
    Font regular = *(const Font *)book->regular;
    int ok = 1;
    for (int i = 0; i < BT_VIEW_MENU_ROW_COUNT; i++) {
        char lines[BT_VIEW_MENU_WRAP_MAX][BT_VIEW_MENU_TEXT_MAX];
        size_t count = wrap_at_default_column(book->regular, rows[i].description, small, lines,
                                              BT_VIEW_MENU_WRAP_MAX);
        if (count < 1 || count > (size_t)BT_VIEW_MENU_DESC_MAX_LINES) {
            ok = 0;
        }
        for (size_t j = 0; j < count; j++) {
            if (MeasureTextEx(regular, lines[j], small, 1.0F).x > default_column_width()) {
                ok = 0;
            }
        }
        if (!lines_join_back(rows[i].description, lines, count)) {
            ok = 0;
        }
    }
    bt_app_ctx_free(&ctx);
    return ok;
}

BT_TEST(view_menu, menu_wrap_fits_every_description_inside_its_column) {
    BT_CHECK(with_menu_fonts(wrap_descriptions_fit));
}

static int wrap_edge_cases_hold(const bt_app_font_book *book) {
    pin_default_root();
    float small = bt_theme_table()->font_small * bt_theme_table()->root_rem;
    char lines[BT_VIEW_MENU_WRAP_MAX][BT_VIEW_MENU_TEXT_MAX];
    size_t count = wrap_at_default_column(book->regular, "", small, lines, BT_VIEW_MENU_WRAP_MAX);
    if (count != 0) {
        return 0;
    }
    count = wrap_at_default_column(book->regular, "cadence, memo mode, reps", small, lines,
                                   BT_VIEW_MENU_WRAP_MAX);
    if (count != 1 || strcmp(lines[0], "cadence, memo mode, reps") != 0) {
        return 0;
    }
    count = wrap_at_default_column(
        book->regular, "physical 3-style pair drilling with spaced repetition", small, lines, 1);
    if (count != 1 || strlen(lines[0]) == 0) {
        return 0;
    }
    char word[BT_VIEW_MENU_TEXT_MAX];
    memset(word, 'a', sizeof word - 1);
    word[sizeof word - 1] = '\0';
    count = wrap_at_default_column(book->regular, word, small, lines, BT_VIEW_MENU_WRAP_MAX);
    if (count != 1 || strcmp(lines[0], word) != 0) {
        return 0;
    }
    return 1;
}

BT_TEST(view_menu, menu_wrap_edge_cases_hold_single_words_and_caps) {
    BT_CHECK(with_menu_fonts(wrap_edge_cases_hold));
}
