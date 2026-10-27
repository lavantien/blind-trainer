#ifndef BT_APP_VIEWS_MENU_H
#define BT_APP_VIEWS_MENU_H

#include "app/logic.h"
#include "app/states.h"

#include <stddef.h>

#define BT_VIEW_MENU_ROW_COUNT BT_APP_MENU_KEY_COUNT
#define BT_VIEW_MENU_TEXT_MAX 512

typedef struct {
    int number;
    const char *label;
    const char *description;
} bt_view_menu_row;

void bt_view_menu_rows(bt_view_menu_row out[BT_VIEW_MENU_ROW_COUNT]);
void bt_view_menu_title(const bt_view_menu_row *row, char out[BT_VIEW_MENU_TEXT_MAX]);
void bt_view_menu_warning_context(const bt_str_list *warnings, char out[BT_VIEW_MENU_TEXT_MAX]);

void bt_view_menu_draw(const bt_str_list *warnings, const void *font_regular, const void *font_bold,
                       float window_w, float window_h);

#endif
