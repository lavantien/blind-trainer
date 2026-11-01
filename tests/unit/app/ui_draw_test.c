#include "app/config.h"
#include "app/theme.h"
#include "app/ui.h"
#include "app/window.h"

#include "common.h"

#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    DRAW_DWELL_FRAMES = 6,
};

static int open_config_window(bt_app_config *config) {
    config->window.width = 640;
    config->window.height = 480;
    snprintf(config->window.title, sizeof config->window.title, "%s", "ui draw test");
    return bt_app_window_open_driven(config->window.width, config->window.height,
                                     config->window.title, &config->render);
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

static int draw_and_capture(const bt_app_font_book *book, void (*draw)(const bt_app_font_book *)) {
    bt_app_config config;
    if (!open_config_window(&config)) {
        return 0;
    }
    for (unsigned frame = 0; frame < DRAW_DWELL_FRAMES; frame++) {
        bt_app_window_frame_begin();
        ClearBackground(bt_app_color(bt_theme_background()));
        draw(book);
        bt_app_window_frame_end();
    }
    char shot[BT_PATH_MAX];
    snprintf(shot, sizeof shot, "%s/app-ui-draw-%d.png", getenv("TEMP"), _getpid());
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

static bt_ui_input quiet_input;

static const float FIT_DRAW_SIZE = 37.0F;
static const float FIT_DRAW_X = 40.0F;
static const float FIT_DRAW_Y = 40.0F;
static const float FIT_ROOMY_MAX_W = 400.0F;
static const float FIT_TIGHT_MAX_W = 180.0F;
static const char FIT_SHORT_TEXT[] = "fit me";
static const char FIT_LONG_TEXT[] =
    "press space to fold, 1..4 to rate, esc for the menu, tab flips the pair side";
static const float PANEL_LONG_X = 40.0F;
static const float PANEL_LONG_W = 300.0F;
static const float PANEL_LONG_H = 260.0F;
static const char PANEL_LONG_TITLE[] =
    "configure the trainer before the first blind solve of the day";
static const float SELECT_X = 40.0F;
static const float SELECT_Y = 40.0F;
static const float SELECT_W = 300.0F;
static const float SELECT_H = 44.0F;
static const char SELECT_LONG_ROW[] = "pick the letter pair scheme that matches your memory order";

static int color_distance(Color a, Color b) {
    return abs(a.r - b.r) + abs(a.g - b.g) + abs(a.b - b.b) + abs(a.a - b.a);
}

static int scan_ink_extent(const char *path, int y0, int y1, int x0, int x1, Color surface,
                           Color ink, int *leftmost, int *rightmost) {
    Image image = LoadImage(path);
    if (image.width <= 0 || image.height <= 0) {
        UnloadImage(image);
        return 0;
    }
    Color *pixels = LoadImageColors(image);
    int found = 0;
    int left = image.width;
    int right = -1;
    for (int y = y0; y <= y1 && y < image.height; y++) {
        for (int x = x0; x <= x1 && x < image.width; x++) {
            Color pixel = pixels[y * image.width + x];
            if (color_distance(pixel, ink) < color_distance(pixel, surface)) {
                found = 1;
                if (x < left) {
                    left = x;
                }
                if (x > right) {
                    right = x;
                }
            }
        }
    }
    UnloadImageColors(pixels);
    UnloadImage(image);
    *leftmost = left;
    *rightmost = right;
    return found;
}

static int scan_case_checked(void (*draw)(const bt_app_font_book *), int y0, int y1, int x0, int x1,
                             Color surface, Color ink,
                             void (*verify)(const bt_app_font_book *, int, int)) {
    bt_app_config config;
    if (!open_config_window(&config)) {
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
    for (unsigned frame = 0; frame < DRAW_DWELL_FRAMES; frame++) {
        bt_app_window_frame_begin();
        ClearBackground(bt_app_color(bt_theme_background()));
        draw(&book);
        bt_app_window_frame_end();
    }
    char shot[BT_PATH_MAX];
    snprintf(shot, sizeof shot, "%s/app-ui-scan-%d.png", getenv("TEMP"), _getpid());
    remove(shot);
    int left = -1;
    int right = -1;
    int ok = bt_app_window_capture(shot) &&
             scan_ink_extent(shot, y0, y1, x0, x1, surface, ink, &left, &right);
    remove(shot);
    if (ok) {
        verify(&book, left, right);
    }
    bt_app_fonts_unload(&book);
    bt_app_window_close();
    return ok;
}

static void draw_fitted_roomy(const bt_app_font_book *book) {
    bt_ui_draw_fitted(book->regular, FIT_SHORT_TEXT, FIT_DRAW_X, FIT_DRAW_Y, FIT_DRAW_SIZE,
                      bt_theme_text_primary(), FIT_ROOMY_MAX_W);
}

static void draw_fitted_tight(const bt_app_font_book *book) {
    bt_ui_draw_fitted(book->regular, FIT_LONG_TEXT, FIT_DRAW_X, FIT_DRAW_Y, FIT_DRAW_SIZE,
                      bt_theme_text_primary(), FIT_TIGHT_MAX_W);
}

static bt_ui_select_state scan_select_state;

static void draw_select_closed_long_row(const bt_app_font_book *book) {
    const char *rows[2] = {SELECT_LONG_ROW, "short"};
    bt_ui_rect rect = {SELECT_X, SELECT_Y, SELECT_W, SELECT_H};
    int edited = 0;
    scan_select_state.open = 0;
    scan_select_state.cursor = 0;
    bt_ui_select(book->regular, &rect, rows, 2, 0, &scan_select_state, &quiet_input, &edited,
                 480.0F);
}

static void draw_select_open_long_row(const bt_app_font_book *book) {
    const char *rows[2] = {SELECT_LONG_ROW, "short"};
    bt_ui_rect rect = {SELECT_X, SELECT_Y, SELECT_W, SELECT_H};
    int edited = 0;
    scan_select_state.open = 1;
    scan_select_state.cursor = 0;
    bt_ui_select(book->regular, &rect, rows, 2, 0, &scan_select_state, &quiet_input, &edited,
                 480.0F);
}

static void draw_button(const bt_app_font_book *book) {
    bt_ui_rect rect = {200.0F, 200.0F, 240.0F, 60.0F};
    bt_ui_button(book->bold, &rect, "start drill", &quiet_input);
}

static void draw_panel(const bt_app_font_book *book) {
    bt_ui_rect rect = {40.0F, 40.0F, 560.0F, 400.0F};
    bt_ui_panel(book->bold, &rect, "menu");
}

static void draw_panel_long_title(const bt_app_font_book *book) {
    bt_ui_rect rect = {PANEL_LONG_X, PANEL_LONG_X, PANEL_LONG_W, PANEL_LONG_H};
    bt_ui_panel(book->bold, &rect, PANEL_LONG_TITLE);
}

static void draw_list(const bt_app_font_book *book) {
    const char *rows[3] = {"drill", "memo", "solve"};
    bt_ui_rect rect = {40.0F, 40.0F, 560.0F, 300.0F};
    bt_ui_list_window window = {0, 1};
    bt_ui_list(book->regular, &rect, rows, 3, 6, window, &quiet_input);
}

static void draw_text_field(const bt_app_font_book *book) {
    bt_ui_rect rect = {40.0F, 200.0F, 560.0F, 60.0F};
    char buffer[BT_UI_TEXT_MAX] = "query";
    size_t cursor = 5;
    int committed = 0;
    bt_ui_text_field(book->regular, &rect, buffer, sizeof buffer, &cursor, 1, &quiet_input,
                     &committed);
}

static void draw_number_field(const bt_app_font_book *book) {
    bt_ui_rect rect = {240.0F, 200.0F, 160.0F, 60.0F};
    int edited = 0;
    bt_ui_number_state state;
    memset(&state, 0, sizeof state);
    bt_ui_number_field(book->regular, &rect, 800, 1, 10000, &state, &quiet_input, &edited);
}

static int with_fonts(int (*run)(const bt_app_font_book *)) {
    bt_app_config config;
    if (!open_config_window(&config)) {
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

static int run_button(const bt_app_font_book *book) {
    return draw_and_capture(book, draw_button);
}

static int run_panel(const bt_app_font_book *book) {
    return draw_and_capture(book, draw_panel);
}

static int run_list(const bt_app_font_book *book) {
    return draw_and_capture(book, draw_list);
}

static int run_text_field(const bt_app_font_book *book) {
    return draw_and_capture(book, draw_text_field);
}

static int run_number_field(const bt_app_font_book *book) {
    return draw_and_capture(book, draw_number_field);
}

static int run_fitted_width(const bt_app_font_book *book) {
    Font regular = *(const Font *)book->regular;
    float full_long = MeasureTextEx(regular, FIT_LONG_TEXT, FIT_DRAW_SIZE, 1.0F).x;
    float dots = MeasureTextEx(regular, "...", FIT_DRAW_SIZE, 1.0F).x;
    float full_short = MeasureTextEx(regular, FIT_SHORT_TEXT, FIT_DRAW_SIZE, 1.0F).x;
    float tight = bt_ui_fitted_width(book->regular, FIT_LONG_TEXT, FIT_DRAW_SIZE, FIT_TIGHT_MAX_W);
    float roomy = bt_ui_fitted_width(book->regular, FIT_SHORT_TEXT, FIT_DRAW_SIZE, FIT_ROOMY_MAX_W);
    return full_long > FIT_TIGHT_MAX_W && tight <= FIT_TIGHT_MAX_W && tight >= dots - 0.01F &&
           fabsf(roomy - full_short) < 0.01F;
}

BT_TEST(ui_draw, button_renders_pixels) {
    bt_ui_input_clear(&quiet_input);
    BT_CHECK(with_fonts(run_button));
}

BT_TEST(ui_draw, panel_renders_pixels) {
    bt_ui_input_clear(&quiet_input);
    BT_CHECK(with_fonts(run_panel));
}

BT_TEST(ui_draw, list_renders_pixels) {
    bt_ui_input_clear(&quiet_input);
    BT_CHECK(with_fonts(run_list));
}

BT_TEST(ui_draw, text_field_renders_pixels) {
    bt_ui_input_clear(&quiet_input);
    BT_CHECK(with_fonts(run_text_field));
}

BT_TEST(ui_draw, number_field_renders_pixels) {
    bt_ui_input_clear(&quiet_input);
    BT_CHECK(with_fonts(run_number_field));
}

BT_TEST(ui_draw, fitted_width_returns_the_full_measure_when_it_fits) {
    BT_CHECK(with_fonts(run_fitted_width));
}

static void fitted_roomy_verify(const bt_app_font_book *book, int left, int right) {
    Font probe = *(const Font *)book->regular;
    float full = MeasureTextEx(probe, FIT_SHORT_TEXT, FIT_DRAW_SIZE, 1.0F).x;
    BT_CHECK(full < FIT_ROOMY_MAX_W);
    BT_CHECK(left >= (int)FIT_DRAW_X - 2);
    BT_CHECK(right <= (int)(FIT_DRAW_X + full) + 2);
    BT_CHECK(right >= (int)(FIT_DRAW_X + full) - 2);
}

BT_TEST(ui_draw, fitted_draw_leaves_short_text_unchanged) {
    bt_ui_input_clear(&quiet_input);
    BT_CHECK(scan_case_checked(draw_fitted_roomy, (int)FIT_DRAW_Y - 4,
                               (int)(FIT_DRAW_Y + FIT_DRAW_SIZE * 1.6F), 0, 639,
                               bt_app_color(bt_theme_background()),
                               bt_app_color(bt_theme_text_primary()), fitted_roomy_verify));
}

static void fitted_tight_verify(const bt_app_font_book *book, int left, int right) {
    Font probe = *(const Font *)book->regular;
    float full = MeasureTextEx(probe, FIT_LONG_TEXT, FIT_DRAW_SIZE, 1.0F).x;
    BT_CHECK(full > FIT_TIGHT_MAX_W);
    BT_CHECK(left >= (int)FIT_DRAW_X - 2);
    BT_CHECK(right <= (int)(FIT_DRAW_X + FIT_TIGHT_MAX_W) + 1);
    BT_CHECK(right < (int)(FIT_DRAW_X + full) - 2);
}

BT_TEST(ui_draw, fitted_draw_ellipsizes_long_text_inside_max_w) {
    bt_ui_input_clear(&quiet_input);
    BT_CHECK(scan_case_checked(draw_fitted_tight, (int)FIT_DRAW_Y - 4,
                               (int)(FIT_DRAW_Y + FIT_DRAW_SIZE * 1.6F), 0, 639,
                               bt_app_color(bt_theme_background()),
                               bt_app_color(bt_theme_text_primary()), fitted_tight_verify));
}

static void panel_long_title_verify(const bt_app_font_book *book, int left, int right) {
    const bt_theme *theme = bt_theme_table();
    float pad = theme->panel_padding_rem * theme->root_rem;
    float title_size = theme->font_title * theme->root_rem;
    Font probe = *(const Font *)book->bold;
    float full = MeasureTextEx(probe, PANEL_LONG_TITLE, title_size, 1.0F).x;
    BT_CHECK(full > PANEL_LONG_W - pad * 2.0F);
    BT_CHECK(left >= (int)(PANEL_LONG_X + pad) - 2);
    BT_CHECK(right < (int)(PANEL_LONG_X + PANEL_LONG_W));
}

BT_TEST(ui_draw, panel_title_stays_inside_the_panel) {
    bt_ui_input_clear(&quiet_input);
    const bt_theme *theme = bt_theme_table();
    float pad = theme->panel_padding_rem * theme->root_rem;
    float title_size = theme->font_title * theme->root_rem;
    BT_CHECK(scan_case_checked(draw_panel_long_title, (int)(PANEL_LONG_X + pad),
                               (int)(PANEL_LONG_X + pad + title_size * 1.3F), 0, 639,
                               bt_app_color(bt_theme_panel()),
                               bt_app_color(bt_theme_text_primary()), panel_long_title_verify));
}

static void select_row_verify(const bt_app_font_book *book, int left, int right) {
    const bt_theme *theme = bt_theme_table();
    float row_size = theme->font_body * theme->root_rem;
    float indent = theme->spacing_rem * theme->root_rem;
    Font probe = *(const Font *)book->regular;
    float full = MeasureTextEx(probe, SELECT_LONG_ROW, row_size, 1.0F).x;
    BT_CHECK(full > SELECT_W);
    BT_CHECK(left >= (int)(SELECT_X + indent) - 2);
    BT_CHECK(right < (int)(SELECT_X + SELECT_W));
}

BT_TEST(ui_draw, select_closed_row_stays_inside_the_widget) {
    bt_ui_input_clear(&quiet_input);
    BT_CHECK(scan_case_checked(
        draw_select_closed_long_row, (int)SELECT_Y, (int)(SELECT_Y + SELECT_H) - 1, 0, 639,
        bt_app_color(bt_theme_panel()), bt_app_color(bt_theme_text_primary()), select_row_verify));
}

BT_TEST(ui_draw, select_open_rows_stay_inside_their_slots) {
    bt_ui_input_clear(&quiet_input);
    int first_slot_y = (int)(SELECT_Y + SELECT_H);
    int last_slot_bottom = (int)(SELECT_Y + SELECT_H * 3.0F) - 1;
    BT_CHECK(scan_case_checked(draw_select_open_long_row, first_slot_y, last_slot_bottom, 0, 639,
                               bt_app_color(bt_theme_panel()),
                               bt_app_color(bt_theme_text_primary()), select_row_verify));
}
