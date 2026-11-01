#include "app/views/case_drill_view.h"

#include "app/ui.h"
#include "app/views/algs/display.h"
#include "app/views/frame.h"
#include "app/window.h"

#include "raylib.h"

#include "common/bt_trace.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static size_t band_row_count(const bt_netdisp_layout *layout, size_t band) {
    size_t rows = 0;
    for (size_t slot = 0; slot < layout->band_slot_count[band]; slot++) {
        if (layout->bands[band][slot].kind == BT_NETDISP_BAND_FACE &&
            layout->bands[band][slot].face.row_count > rows) {
            rows = layout->bands[band][slot].face.row_count;
        }
    }
    return rows;
}

void bt_case_view_layout_cells(const bt_netdisp_layout *layout, size_t *out_cols,
                               size_t *out_rows) {
    BT_TRACE_ENTER("app.case_view_layout_cells");
    size_t cols = 0;
    size_t rows = 0;
    for (size_t band = 0; band < layout->band_count; band++) {
        size_t band_cols = 0;
        for (size_t slot = 0; slot < layout->band_slot_count[band]; slot++) {
            if (layout->bands[band][slot].kind == BT_NETDISP_BAND_FACE) {
                band_cols += layout->bands[band][slot].face.width;
            } else {
                band_cols += layout->bands[band][slot].spacer;
            }
        }
        if (band_cols > cols) {
            cols = band_cols;
        }
        rows += band_row_count(layout, band);
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

static void push_line(bt_case_view_line out[BT_CASE_VIEW_LINES_MAX], size_t *count,
                      const char *text, int danger, int fitted) {
    if (*count >= BT_CASE_VIEW_LINES_MAX) {
        return;
    }
    snprintf(out[*count].text, BT_CASE_DRILL_TEXT_MAX, "%s", text);
    out[*count].danger = danger;
    out[*count].fitted = fitted;
    (*count)++;
}

size_t bt_case_view_lines(const bt_case_drill_session *session, int revealed,
                          bt_case_view_line out[BT_CASE_VIEW_LINES_MAX]) {
    BT_TRACE_ENTER("app.case_view_lines");
    size_t count = 0;
    if (!session->has_current) {
        push_line(out, &count, "no case", 0, 0);
        BT_TRACE_EXIT("app.case_view_lines");
        return count;
    }
    char progress[BT_CASE_DRILL_TEXT_MAX];
    snprintf(progress, sizeof progress, "case %zu of %zu", session->done + 1, session->total);
    push_line(out, &count, session->current.case_ref.case_key, 0, 0);
    bt_case_drill_case_site_line(&session->current, out[count].text);
    out[count].danger = 0;
    out[count].fitted = 0;
    count++;
    push_line(out, &count, progress, 0, 0);
    if (revealed) {
        push_line(out, &count, session->current.alg, 0, 1);
        push_line(out, &count, "1 again 2 hard 3 good 4 easy", 0, 0);
    } else {
        push_line(out, &count, "space to reveal", 0, 0);
    }
    if (session->has_save_error) {
        push_line(out, &count, session->save_error, 1, 1);
    }
    if (session->has_grid_error) {
        push_line(out, &count, session->grid_error, 1, 1);
    }
    BT_TRACE_EXIT("app.case_view_lines");
    return count;
}

static float title_line_advance(const bt_theme *theme) {
    return theme->font_title * theme->root_rem + theme->spacing_rem * theme->root_rem;
}

static float body_line_advance(const bt_theme *theme) {
    return theme->font_small * theme->root_rem + theme->spacing_rem * theme->root_rem * 0.5F;
}

float bt_case_view_text_band_height(const bt_theme *theme, size_t line_count) {
    BT_TRACE_ENTER("app.case_view_text_band_height");
    float height = 0.0F;
    if (line_count > 0) {
        height = title_line_advance(theme) + (float)(line_count - 1) * body_line_advance(theme);
    }
    BT_TRACE_EXIT("app.case_view_text_band_height");
    return height;
}

static float face_plate_width(size_t cells, float quad, float gap) {
    return (float)cells * quad + (float)(cells > 0 ? cells - 1 : 0) * gap;
}

static float case_view_gap(float quad) {
    float gap = quad * BT_CASE_VIEW_CELL_GAP_RATIO;
    return gap < 1.5F ? 1.5F : gap;
}

static float slot_width(const bt_netdisp_band_slot *slot, float quad, float gap, float stroke) {
    if (slot->kind == BT_NETDISP_BAND_FACE) {
        return face_plate_width(slot->face.width, quad, gap) + stroke * 2.0F;
    }
    return face_plate_width(slot->spacer, quad, gap);
}

static float band_width(const bt_netdisp_layout *layout, size_t band, float quad, float gap,
                        float stroke) {
    float width = 0.0F;
    for (size_t slot = 0; slot < layout->band_slot_count[band]; slot++) {
        width += slot_width(&layout->bands[band][slot], quad, gap, stroke);
    }
    size_t slots = layout->band_slot_count[band];
    return width + (float)(slots > 0 ? slots - 1 : 0) * gap;
}

static float band_height(const bt_netdisp_layout *layout, size_t band, float quad, float gap,
                         float stroke) {
    size_t rows = band_row_count(layout, band);
    if (rows == 0) {
        return 0.0F;
    }
    return face_plate_width(rows, quad, gap) + stroke * 2.0F;
}

void bt_case_view_net_extent(const bt_netdisp_layout *layout, float quad, float *out_w,
                             float *out_h) {
    BT_TRACE_ENTER("app.case_view_net_extent");
    float gap = case_view_gap(quad);
    float stroke = bt_net_cell_stroke(quad);
    float width = 0.0F;
    float height = 0.0F;
    for (size_t band = 0; band < layout->band_count; band++) {
        float drawn = band_width(layout, band, quad, gap, stroke);
        if (drawn > width) {
            width = drawn;
        }
        height += band_height(layout, band, quad, gap, stroke);
    }
    height += (float)(layout->band_count > 0 ? layout->band_count - 1 : 0) * gap;
    *out_w = width;
    *out_h = height;
    BT_TRACE_EXIT("app.case_view_net_extent");
}

void bt_case_view_net_place(const bt_netdisp_layout *layout, float region_x, float region_y,
                            float region_w, float region_h, float *out_x, float *out_y,
                            float *out_quad) {
    BT_TRACE_ENTER("app.case_view_net_place");
    size_t cols = 0;
    size_t rows = 0;
    bt_case_view_layout_cells(layout, &cols, &rows);
    float quad = bt_case_view_quad_for(cols, rows, region_w, region_h);
    float w = 0.0F;
    float h = 0.0F;
    while (quad > BT_CASE_VIEW_QUAD_MIN) {
        bt_case_view_net_extent(layout, quad, &w, &h);
        if (w <= region_w && h <= region_h) {
            break;
        }
        quad = floorf(quad - 1.0F);
    }
    bt_case_view_net_extent(layout, quad, &w, &h);
    *out_quad = quad;
    *out_x = region_x + (region_w - w) / 2.0F;
    *out_y = region_y + (region_h - h) / 2.0F;
    BT_TRACE_EXIT("app.case_view_net_place");
}

static void draw_net(const bt_netdisp_layout *layout, float quad, float x, float y) {
    float gap = case_view_gap(quad);
    float stroke = bt_net_cell_stroke(quad);
    for (size_t band = 0; band < layout->band_count; band++) {
        float slot_x = x;
        for (size_t slot = 0; slot < layout->band_slot_count[band]; slot++) {
            const bt_netdisp_band_slot *at = &layout->bands[band][slot];
            if (at->kind == BT_NETDISP_BAND_SPACER) {
                slot_x += slot_width(at, quad, gap, stroke) + gap;
                continue;
            }
            const bt_netdisp_face *face = &at->face;
            float plate_w = face_plate_width(face->width, quad, gap) + stroke * 2.0F;
            float plate_h = face_plate_width(face->row_count, quad, gap) + stroke * 2.0F;
            DrawRectangleLinesEx((Rectangle){slot_x, y, plate_w, plate_h}, stroke,
                                 bt_app_color(bt_theme_text_primary()));
            float row_y = y + stroke;
            for (size_t row = 0; row < face->row_count; row++) {
                float cell_x = slot_x + stroke;
                for (size_t col = 0; col < face->row_lengths[row]; col++) {
                    if (face->rows[row][col].kind == BT_NETDISP_SLOT_CELL) {
                        bt_theme_color color = bt_netdisp_cell_color(&face->rows[row][col].cell);
                        DrawRectangleRounded((Rectangle){cell_x, row_y, quad, quad}, 0.15F, 4,
                                             bt_app_color(color));
                    }
                    cell_x += quad + gap;
                }
                row_y += quad + gap;
            }
            slot_x += plate_w + gap;
        }
        y += band_height(layout, band, quad, gap, stroke) + gap;
    }
}

static void draw_centered_line(const void *font, const char *text, float content_x, float content_w,
                               float y, float size, bt_theme_color color) {
    Font face = *(const Font *)font;
    Vector2 box = MeasureTextEx(face, text, size, 1.0F);
    bt_app_draw_text(font, text, content_x + (content_w - box.x) / 2.0F, y, size, color);
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
        float size =
            i == 0 ? theme->font_title * theme->root_rem : theme->font_small * theme->root_rem;
        bt_theme_color color = bt_theme_text_muted();
        if (lines[i].danger) {
            color = bt_theme_danger();
        } else if (i == 0) {
            color = bt_theme_text_primary();
        }
        const void *font = i == 0 ? font_bold : font_regular;
        if (lines[i].fitted) {
            float fitted_w = bt_ui_fitted_width(font, lines[i].text, size, metrics.content_w);
            bt_ui_draw_fitted(font, lines[i].text,
                              metrics.content_x + (metrics.content_w - fitted_w) / 2.0F, y, size,
                              color, metrics.content_w);
        } else {
            draw_centered_line(font, lines[i].text, metrics.content_x, metrics.content_w, y, size,
                               color);
        }
        y += i == 0 ? title_line_advance(theme) : body_line_advance(theme);
    }
    if (session->has_grid) {
        bt_netdisp_layout layout;
        bt_netdisp_layout_of(&session->grid, &layout);
        float band = bt_case_view_text_band_height(theme, count);
        float x = 0.0F;
        float net_y = 0.0F;
        float quad = 0.0F;
        bt_case_view_net_place(&layout, metrics.content_x, metrics.content_y + band,
                               metrics.content_w, metrics.content_h - band, &x, &net_y, &quad);
        draw_net(&layout, quad, x, net_y);
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
