#ifndef BT_APP_VIEWS_FATAL_H
#define BT_APP_VIEWS_FATAL_H

#include "app/logic.h"

#include <stddef.h>

#define BT_FATAL_TEXT_MAX 256
#define BT_FATAL_LINES_MAX 32

size_t bt_view_fatal_lines(const bt_app_ctx *ctx, char out[][BT_FATAL_TEXT_MAX], size_t cap);
void bt_view_fatal_draw(const bt_app_ctx *ctx, const void *font_regular, const void *font_bold,
                        float window_w, float window_h);
void bt_view_fatal_row_draw(bt_app_ctx *ctx);

#endif
