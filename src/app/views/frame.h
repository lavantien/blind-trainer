#ifndef BT_APP_VIEWS_FRAME_H
#define BT_APP_VIEWS_FRAME_H

#include "app/theme.h"

#include <stddef.h>

typedef struct {
    float pad;
    float header_height;
    float hairline;
    float content_x;
    float content_y;
    float content_w;
    float content_h;
} bt_view_frame_metrics;

bt_view_frame_metrics bt_view_frame_metrics_for(float window_w, float window_h);
void bt_view_frame_draw(const void *font_bold, const void *font_regular, float window_w,
                        float window_h, const char *title, const char *context);

#endif
