#ifndef BT_APP_VIEWS_MENU_H
#define BT_APP_VIEWS_MENU_H

#include "app/logic.h"
#include "app/states.h"

#include <stddef.h>

#define BT_VIEW_MENU_ROW_COUNT BT_APP_MENU_KEY_COUNT
#define BT_VIEW_MENU_TEXT_MAX 512
#define BT_VIEW_MENU_COLUMN_COUNT 2
#define BT_VIEW_MENU_DESC_MAX_LINES 2
#define BT_VIEW_MENU_WRAP_MAX 4

typedef struct {
    int number;
    const char *label;
    const char *description;
    char data[BT_VIEW_MENU_TEXT_MAX];
} bt_view_menu_row;

typedef struct {
    float col_x[BT_VIEW_MENU_COLUMN_COUNT];
    float col_w;
    float gutter;
    float row_h;
    float row_pitch;
    float first_row_y;
    float desc_dy;
    float data_dy;
} bt_view_menu_layout;

void bt_view_menu_rows(const bt_app_ctx *ctx, bt_view_menu_row out[BT_VIEW_MENU_ROW_COUNT]);
void bt_view_menu_title(const bt_view_menu_row *row, char out[BT_VIEW_MENU_TEXT_MAX]);
char *bt_view_menu_warning_context(const bt_str_list *warnings);
bt_view_menu_layout bt_view_menu_layout_for(float window_w, float window_h, int desc_lines);
size_t bt_view_menu_wrap_lines(const void *font, const char *text, float size, float max_w,
                               char out[][BT_VIEW_MENU_TEXT_MAX], size_t cap);

void bt_view_menu_draw(const bt_app_ctx *ctx, const bt_str_list *warnings, const void *font_regular,
                       const void *font_bold, float window_w, float window_h);

void bt_view_menu_row_draw(bt_app_ctx *ctx);

#endif
