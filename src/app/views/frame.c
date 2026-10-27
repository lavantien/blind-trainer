#include "app/views/frame.h"

#include "app/window.h"
#include "common/bt_trace.h"

#include "raylib.h"

bt_view_frame_metrics bt_view_frame_metrics_for(float window_w, float window_h) {
    BT_TRACE_ENTER("app.view_frame_metrics");
    const bt_theme *theme = bt_theme_table();
    bt_view_frame_metrics metrics;
    metrics.pad = theme->panel_padding_rem * theme->root_rem;
    metrics.header_height = theme->header_height_rem * theme->root_rem;
    metrics.hairline = theme->hairline_rem * theme->root_rem;
    metrics.content_x = metrics.pad;
    metrics.content_y = metrics.pad + metrics.header_height;
    metrics.content_w = window_w - metrics.pad * 2.0F;
    metrics.content_h = window_h - metrics.pad * 2.0F - metrics.header_height;
    BT_TRACE_EXIT("app.view_frame_metrics");
    return metrics;
}

void bt_view_frame_draw(const void *font_bold, const void *font_regular, float window_w,
                        float window_h, const char *title, const char *context) {
    BT_TRACE_ENTER("app.view_frame_draw");
    bt_view_frame_metrics metrics = bt_view_frame_metrics_for(window_w, window_h);
    const bt_theme *theme = bt_theme_table();
    float title_size = theme->font_title * theme->root_rem;
    float context_size = theme->font_small * theme->root_rem;
    Font bold = *(const Font *)font_bold;
    Font regular = *(const Font *)font_regular;
    Vector2 title_box = MeasureTextEx(bold, title, title_size, 1.0F);
    Vector2 context_box = MeasureTextEx(regular, context, context_size, 1.0F);
    bt_app_draw_text(font_bold, title, metrics.pad,
                     metrics.pad + (metrics.header_height - title_box.y) / 2.0F, title_size,
                     bt_theme_text_primary());
    bt_app_draw_text(font_regular, context, window_w - metrics.pad - context_box.x,
                     metrics.pad + (metrics.header_height - context_box.y) / 2.0F, context_size,
                     bt_theme_text_muted());
    DrawRectangle(0, (int)(metrics.pad + metrics.header_height), (int)window_w,
                  (int)(metrics.hairline > 1.0F ? metrics.hairline : 1.0F),
                  bt_app_color(bt_theme_hairline()));
    BT_TRACE_EXIT("app.view_frame_draw");
}
