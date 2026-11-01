#ifndef BT_APP_VIEWS_CASE_DRILL_VIEW_H
#define BT_APP_VIEWS_CASE_DRILL_VIEW_H

#include "app/views/algs/display.h"
#include "app/views/case_drill.h"
#include "app/views/state.h"

#include <stddef.h>

#define BT_CASE_VIEW_CELL_GAP_RATIO 0.12F
#define BT_CASE_VIEW_QUAD_MIN 8.0F
#define BT_CASE_VIEW_QUAD_MAX 56.0F
#define BT_CASE_VIEW_LINES_MAX 16

typedef struct {
    char text[BT_CASE_DRILL_TEXT_MAX];
    int danger;
    int fitted;
} bt_case_view_line;

void bt_case_view_layout_cells(const bt_netdisp_layout *layout, size_t *out_cols, size_t *out_rows);
float bt_case_view_quad_for(size_t cols, size_t rows, float region_w, float region_h);
size_t bt_case_view_lines(const bt_case_drill_session *session, int revealed,
                          bt_case_view_line out[BT_CASE_VIEW_LINES_MAX]);
float bt_case_view_text_band_height(const bt_theme *theme, size_t line_count);
void bt_case_view_net_extent(const bt_netdisp_layout *layout, float quad, float *out_w,
                             float *out_h);
void bt_case_view_net_place(const bt_netdisp_layout *layout, float region_x, float region_y,
                            float region_w, float region_h, float *out_x, float *out_y,
                            float *out_quad);

void bt_view_case_drill_draw(const bt_app_ctx *ctx, const void *font_regular, const void *font_bold,
                             float window_w, float window_h);
void bt_view_case_drill_row_draw(bt_app_ctx *ctx);

#endif
