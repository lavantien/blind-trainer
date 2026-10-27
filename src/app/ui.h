#ifndef BT_APP_UI_H
#define BT_APP_UI_H

#include <stddef.h>
#include <stdint.h>

#define BT_UI_TEXT_MAX 256
#define BT_UI_INPUT_CHARS 16

struct Font;

typedef struct {
    float x;
    float y;
    float width;
    float height;
} bt_ui_rect;

typedef struct {
    float x;
    float y;
    int click;
    int down;
    int wheel;
    int escape;
    int enter;
    int backspace;
    int chars[BT_UI_INPUT_CHARS];
    int char_count;
} bt_ui_input;

typedef struct {
    int clicked;
    int hovered;
    int pressed;
} bt_ui_button_state;

typedef struct {
    int first;
    int selected;
} bt_ui_list_window;

void bt_ui_input_clear(bt_ui_input *input);
int bt_ui_contains(const bt_ui_rect *rect, float x, float y);
bt_ui_button_state bt_ui_button_hit(const bt_ui_rect *rect, const bt_ui_input *input);
int bt_ui_list_scroll(bt_ui_list_window *window, int row_count, int visible_rows, int wheel);
void bt_ui_list_show(bt_ui_list_window *window, int row_count, int visible_rows);
int bt_ui_field_edit(char *buffer, size_t cap, size_t *cursor, const bt_ui_input *input,
                     int *committed, int *canceled);
int64_t bt_ui_number_clamp(int64_t value, int64_t low, int64_t high);

bt_ui_button_state bt_ui_button(const struct Font *font, const bt_ui_rect *rect, const char *label,
                                const bt_ui_input *input);
bt_ui_rect bt_ui_panel(const struct Font *font, const bt_ui_rect *rect, const char *title);
bt_ui_list_window bt_ui_list(const struct Font *font, const bt_ui_rect *rect,
                             const char *const *rows, int row_count, int visible_rows,
                             bt_ui_list_window window, const bt_ui_input *input);
int bt_ui_text_field(const struct Font *font, const bt_ui_rect *rect, char *buffer, size_t cap,
                     size_t *cursor, int focused, const bt_ui_input *input, int *committed);
int64_t bt_ui_number_field(const struct Font *font, const bt_ui_rect *rect, int64_t value,
                           int64_t low, int64_t high, const bt_ui_input *input, int *edited);

#endif
