#ifndef BT_APP_WINDOW_H
#define BT_APP_WINDOW_H

#include "app/logic.h"
#include "app/theme.h"
#include "app/ui.h"

#include <stddef.h>

#include "raylib.h"

typedef struct {
    const void *regular;
    const void *bold;
    int glyph_count;
} bt_app_font_book;

int bt_app_window_open(unsigned int width, unsigned int height, const char *title,
                       const bt_app_render_cfg *render);
void bt_app_window_close(void);
int bt_app_window_ready(void);
int bt_app_window_capture(const char *path);
void bt_app_window_frame_begin(void);
void bt_app_window_frame_end(void);
void bt_app_poll_keys(bt_app_ctx *ctx);
void bt_app_poll_input(bt_ui_input *input);
Color bt_app_color(bt_theme_color color);
void bt_app_draw_text(const void *font, const char *text, float x, float y, float size,
                      bt_theme_color color);
int bt_app_fonts_load(const char *config_path, const bt_corpus *corpus, const char *const *words,
                      size_t word_count, bt_app_font_book *out, char *error, size_t error_len);
void bt_app_fonts_unload(bt_app_font_book *book);
const bt_app_font_book *bt_app_fonts_current(void);
void bt_app_fonts_dir_from(const char *configured, char out[BT_PATH_MAX]);

#endif
