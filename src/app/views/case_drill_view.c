#include "app/views/case_drill_view.h"

#include "app/views/algs/display.h"
#include "app/views/frame.h"
#include "app/window.h"

#include "raylib.h"

#include "common/bt_trace.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void bt_case_view_layout_cells(const bt_netdisp_layout *layout, size_t *out_cols,
                               size_t *out_rows) {
    BT_TRACE_ENTER("app.case_view_layout_cells");
    size_t cols = 0;
    size_t rows = 0;
    for (size_t band = 0; band < layout->band_count; band++) {
        size_t band_cols = 0;
        size_t band_rows = 0;
        for (size_t slot = 0; slot < layout->band_slot_count[band]; slot++) {
            if (layout->bands[band][slot].kind == BT_NETDISP_BAND_FACE) {
                const bt_netdisp_face *face = &layout->bands[band][slot].face;
                band_cols += face->width;
                if (face->row_count > band_rows) {
                    band_rows = face->row_count;
                }
            } else {
                band_cols += layout->bands[band][slot].spacer;
            }
        }
        if (band_cols > cols) {
            cols = band_cols;
        }
        rows += band_rows;
    }
    *out_cols = cols;
    *out_rows = rows;
    BT_TRACE_EXIT("app.case_view_layout_cells");
}

float bt_case_view_quad_for(size_t cols, size_t rows, float region_w, float region_h) {
    BT_TRACE_ENTER("app.case_view_quad_for");
    float span_cols = (float)(cols > 0 ? cols : 1) * (1.0F + BT_CASE_VIEW_CELL_GAP_RATIO) -
                      BT_CASE_VIEW_CELL_GAP_RATIO;
    float span_rows = (float)(rows > 0 ? rows : 1) * (1.0F + BT_CASE_VIEW_CELL_GAP_RATIO) -
                      BT_CASE_VIEW_CELL_GAP_RATIO;
    float quad = region_w / span_cols;
    if (region_h / span_rows < quad) {
        quad = region_h / span_rows;
    }
    quad = floorf(quad);
    if (quad < BT_CASE_VIEW_QUAD_MIN) {
        quad = BT_CASE_VIEW_QUAD_MIN;
    }
    if (quad > BT_CASE_VIEW_QUAD_MAX) {
        quad = BT_CASE_VIEW_QUAD_MAX;
    }
    BT_TRACE_EXIT("app.case_view_quad_for");
    return quad;
}

size_t bt_case_view_lines(const bt_case_drill_session *session, int revealed,
                          bt_case_view_line out[BT_CASE_VIEW_LINES_MAX]) {
    BT_TRACE_ENTER("app.case_view_lines");
    size_t count = 0;
    if (!session->has_current) {
        snprintf(out[count].text, BT_CASE_DRILL_TEXT_MAX, "%s", "no case");
        out[count].danger = 0;
        count++;
        BT_TRACE_EXIT("app.case_view_lines");
        return count;
    }
    snprintf(out[count].text, BT_CASE_DRILL_TEXT_MAX, "%s", session->current.case_ref.case_key);
    out[count].danger = 0;
    count++;
    bt_case_drill_case_site_line(&session->current, out[count].text);
    out[count].danger = 0;
    count++;
    snprintf(out[count].text, BT_CASE_DRILL_TEXT_MAX, "case %zu of %zu", session->done + 1,
             session->total);
    out[count].danger = 0;
    count++;
    if (revealed) {
        snprintf(out[count].text, BT_CASE_DRILL_TEXT_MAX, "%s", session->current.alg);
        out[count].danger = 0;
        count++;
        snprintf(out[count].text, BT_CASE_DRILL_TEXT_MAX, "%s", "1 again 2 hard 3 good 4 easy");
        out[count].danger = 0;
        count++;
    } else {
        snprintf(out[count].text, BT_CASE_DRILL_TEXT_MAX, "%s", "space to reveal");
        out[count].danger = 0;
        count++;
    }
    if (session->has_save_error) {
        snprintf(out[count].text, BT_CASE_DRILL_TEXT_MAX, "%s", session->save_error);
        out[count].danger = 1;
        count++;
    }
    if (session->has_grid_error && count < BT_CASE_VIEW_LINES_MAX) {
        snprintf(out[count].text, BT_CASE_DRILL_TEXT_MAX, "%s", session->grid_error);
        out[count].danger = 1;
        count++;
    }
    BT_TRACE_EXIT("app.case_view_lines");
    return count;
}

static float face_plate_width(size_t cells, float quad, float gap) {
    return (float)cells * quad + (float)(cells > 0 ? cells - 1 : 0) * gap;
}

static void draw_net(const bt_netdisp_layout *layout, float quad, float x, float y) {
    float gap = quad * BT_CASE_VIEW_CELL_GAP_RATIO;
    if (gap < 1.5F) {
        gap = 1.5F;
    }
    for (size_t band = 0; band < layout->band_count; band++) {
        float slot_x = x;
        for (size_t slot = 0; slot < layout->band_slot_count[band]; slot++) {
            if (layout->bands[band][slot].kind == BT_NETDISP_BAND_SPACER) {
                slot_x += face_plate_width(layout->bands[band][slot].spacer, quad, gap) + gap;
                continue;
            }
            const bt_netdisp_face *face = &layout->bands[band][slot].face;
            float plate_x = slot_x;
            float plate_y = y;
            float stroke = bt_net_cell_stroke(quad);
            DrawRectangleLinesEx(
                (Rectangle){
                    plate_x,
                    plate_y,
                    face_plate_width(face->width, quad, gap) + stroke * 2.0F,
                    face_plate_width(face->row_count, quad, gap) + stroke * 2.0F,
                },
                stroke, bt_app_color(bt_theme_text_primary()));
            float row_y = plate_y + stroke;
            for (size_t row = 0; row < face->row_count; row++) {
                float cell_x = plate_x + stroke;
                for (size_t at = 0; at < face->row_lengths[row]; at++) {
                    if (face->rows[row][at].kind == BT_NETDISP_SLOT_CELL) {
                        bt_theme_color color = bt_netdisp_cell_color(&face->rows[row][at].cell);
                        DrawRectangleRounded((Rectangle){cell_x, row_y, quad, quad}, 0.15F, 4,
                                             bt_app_color(color));
                    }
                    cell_x += quad + gap;
                }
                row_y += quad + gap;
            }
            slot_x += face_plate_width(face->width, quad, gap) + stroke * 2.0F + gap;
        }
        y += face_plate_width(layout->bands[band][0].kind == BT_NETDISP_BAND_FACE
                                  ? layout->bands[band][0].face.row_count
                                  : 0,
                              quad, gap) +
             gap;
    }
}

void bt_view_case_drill_draw(const bt_app_ctx *ctx, const void *font_regular, const void *font_bold,
                             float window_w, float window_h) {
    BT_TRACE_ENTER("app.view_case_drill_draw");
    const bt_theme *theme = bt_theme_table();
    bt_view_frame_metrics metrics = bt_view_frame_metrics_for(window_w, window_h);
    bt_view_frame_draw(font_bold, font_regular, window_w, window_h, "case drill",
                       "space reveal, 1..4 grade, esc menu");
    bt_view_case_drill_app_state *app = bt_view_case_drill_app();
    const bt_case_drill_session *session = &app->session;
    bt_case_view_line lines[BT_CASE_VIEW_LINES_MAX];
    size_t count = bt_case_view_lines(session, ctx->case_drill_phase == BT_CASE_DRILL_GRADE, lines);
    float y = metrics.content_y;
    for (size_t i = 0; i < count; i++) {
        bt_theme_color color = lines[i].danger ? bt_theme_danger() : bt_theme_text_muted();
        if (i == 0) {
            bt_app_draw_text(font_bold, lines[i].text, metrics.content_x, y,
                             theme->font_title * theme->root_rem, bt_theme_text_primary());
            y += theme->font_title * theme->root_rem + theme->spacing_rem * theme->root_rem;
        } else {
            bt_app_draw_text(font_regular, lines[i].text, metrics.content_x, y,
                             theme->font_small * theme->root_rem, color);
            y += theme->font_small * theme->root_rem + theme->spacing_rem * theme->root_rem * 0.5F;
        }
    }
    if (session->has_grid) {
        bt_netdisp_layout layout;
        bt_netdisp_layout_of(&session->grid, &layout);
        size_t cols = 0;
        size_t rows = 0;
        bt_case_view_layout_cells(&layout, &cols, &rows);
        float quad = bt_case_view_quad_for(cols, rows, window_w * BT_CASE_VIEW_REGION_W_PCT,
                                           window_h * BT_CASE_VIEW_REGION_H_PCT);
        draw_net(&layout, quad, metrics.content_x, metrics.content_y + metrics.content_h * 0.3F);
    }
    BT_TRACE_EXIT("app.view_case_drill_draw");
}

void bt_view_case_drill_row_draw(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.view_case_drill_row_draw");
    const bt_app_font_book *book = bt_app_fonts_current();
    if (book == NULL) {
        BT_TRACE_FAIL("app.view_case_drill_row_draw", "fonts not loaded");
        BT_TRACE_EXIT("app.view_case_drill_row_draw");
        return;
    }
    bt_view_case_drill_draw(ctx, book->regular, book->bold, (float)GetScreenWidth(),
                            (float)GetScreenHeight());
    BT_TRACE_EXIT("app.view_case_drill_row_draw");
}
