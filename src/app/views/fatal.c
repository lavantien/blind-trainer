#include "app/views/fatal.h"

#include "app/views/frame.h"
#include "app/window.h"

#include "raylib.h"

#include "common/bt_trace.h"

#include <stdio.h>
#include <string.h>

size_t bt_view_fatal_lines(const bt_app_ctx *ctx, char out[][BT_FATAL_TEXT_MAX], size_t cap) {
    BT_TRACE_ENTER("app.view_fatal_lines");
    size_t count = 0;
    if (cap > 0) {
        snprintf(out[0], BT_FATAL_TEXT_MAX, "%s", ctx->fatal_reason);
        count = 1;
    }
    for (size_t i = 0; i < ctx->boot_warnings.count && count < cap; i++) {
        snprintf(out[count], BT_FATAL_TEXT_MAX, "%s", ctx->boot_warnings.items[i]);
        count++;
    }
    BT_TRACE_EXIT("app.view_fatal_lines");
    return count;
}

void bt_view_fatal_draw(const bt_app_ctx *ctx, const void *font_regular, const void *font_bold,
                        float window_w, float window_h) {
    BT_TRACE_ENTER("app.view_fatal_draw");
    const bt_theme *theme = bt_theme_table();
    bt_view_frame_draw(font_bold, font_regular, window_w, window_h, "fatal", "");
    char lines[BT_FATAL_LINES_MAX][BT_FATAL_TEXT_MAX];
    size_t count = bt_view_fatal_lines(ctx, lines, BT_FATAL_LINES_MAX);
    Font regular = *(const Font *)font_regular;
    float small = theme->font_small * theme->root_rem;
    float gap = theme->spacing_rem * theme->root_rem;
    float total = count > 0 ? (float)count * small + (float)(count - 1) * gap : 0.0F;
    bt_view_frame_metrics metrics = bt_view_frame_metrics_for(window_w, window_h);
    float y = metrics.content_y + (metrics.content_h - total) / 2.0F;
    for (size_t i = 0; i < count; i++) {
        Vector2 box = MeasureTextEx(regular, lines[i], small, 1.0F);
        bt_app_draw_text(font_regular, lines[i], metrics.content_x
                                                     + (metrics.content_w - box.x) / 2.0F,
                         y, small, bt_theme_text_muted());
        y += small + gap;
    }
    BT_TRACE_EXIT("app.view_fatal_draw");
}

void bt_view_fatal_row_draw(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.view_fatal_row_draw");
    const bt_app_font_book *book = bt_app_fonts_current();
    if (book == NULL) {
        BT_TRACE_FAIL("app.view_fatal_row_draw", "fonts not loaded");
        BT_TRACE_EXIT("app.view_fatal_row_draw");
        return;
    }
    bt_view_fatal_draw(ctx, book->regular, book->bold, (float)GetScreenWidth(),
                       (float)GetScreenHeight());
    BT_TRACE_EXIT("app.view_fatal_row_draw");
}
