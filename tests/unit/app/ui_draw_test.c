#include "app/config.h"
#include "app/theme.h"
#include "app/ui.h"
#include "app/window.h"

#include "common.h"

#include "raylib.h"

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
    return bt_app_window_open(config->window.width, config->window.height, config->window.title,
                              &config->render);
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

static void draw_button(const bt_app_font_book *book) {
    bt_ui_rect rect = {200.0F, 200.0F, 240.0F, 60.0F};
    bt_ui_button(book->bold, &rect, "start drill", &quiet_input);
}

static void draw_panel(const bt_app_font_book *book) {
    bt_ui_rect rect = {40.0F, 40.0F, 560.0F, 400.0F};
    bt_ui_panel(book->bold, &rect, "menu");
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
