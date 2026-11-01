#include "app/views/drill_view.h"

#include "app/theme.h"
#include "app/ui.h"
#include "app/views/frame.h"
#include "app/views/net_view.h"
#include "app/window.h"
#include "common/bt_trace.h"

#include "raylib.h"

bt_view_drill_layout bt_view_drill_layout_for(float content_x, float content_y, float content_w,
                                              float content_h) {
    BT_TRACE_ENTER("app.view_drill_layout");
    const bt_theme *theme = bt_theme_table();
    bt_view_drill_layout layout;
    float gap = theme->spacing_rem * theme->root_rem;
    layout.net_x = content_x;
    layout.net_y = content_y;
    layout.net_w = content_w * theme->drill_net_width_pct;
    layout.net_h = content_h;
    layout.card_x = content_x + layout.net_w + gap;
    layout.card_y = content_y;
    layout.card_w = content_w - layout.net_w - gap;
    layout.card_h = content_h;
    BT_TRACE_EXIT("app.view_drill_layout");
    return layout;
}

bt_net_geometry bt_view_drill_net_geometry(float window_w, float window_h) {
    BT_TRACE_ENTER("app.view_drill_net_geometry");
    const bt_theme *theme = bt_theme_table();
    float frame = 2.0F * theme->root_rem * theme->panel_padding_rem;
    float width = (window_w - frame) * theme->drill_net_width_pct;
    float chrome = theme->root_rem * (theme->header_height_rem + 2.0F * theme->panel_padding_rem);
    bt_net_geometry geo = bt_net_for_region(
        width, (window_h - chrome < theme->root_rem) ? theme->root_rem : window_h - chrome);
    BT_TRACE_EXIT("app.view_drill_net_geometry");
    return geo;
}

void bt_view_drill_net_draw(const void *font_bold, const bt_views_net *pulse, float region_x,
                            float region_y, float region_w, float region_h) {
    BT_TRACE_ENTER("app.view_drill_net_draw");
    bt_net_geometry geo = bt_net_for_region(region_w, region_h);
    float x = region_x;
    float y = region_y;
    bt_view_net_center(&geo, region_x, region_y, region_w, region_h, &x, &y);
    bt_view_net_draw(font_bold, &geo, pulse->ink, x, y);
    BT_TRACE_EXIT("app.view_drill_net_draw");
}

static const float BT_VIEW_DRILL_TEXT_SPACING = 1.0F;

float bt_view_drill_alg_size_for(float measured_w, float max_w) {
    BT_TRACE_ENTER("app.view_drill_alg_size_for");
    const bt_theme *theme = bt_theme_table();
    float base = theme->font_alg * theme->root_rem;
    float floor_size = theme->font_small * theme->root_rem;
    float size = base;
    if (max_w <= 0.0F) {
        size = floor_size;
    } else if (measured_w > max_w && measured_w > 0.0F) {
        size = base * max_w / measured_w;
        if (size < floor_size) {
            size = floor_size;
        }
    }
    BT_TRACE_EXIT("app.view_drill_alg_size_for");
    return size;
}

static void draw_span_line(const void *font_regular, const bt_card_content *content, float x,
                           float y, float max_w) {
    Font regular = *(const Font *)font_regular;
    const bt_theme *theme = bt_theme_table();
    float size = theme->font_alg * theme->root_rem;
    float total = 0.0F;
    for (size_t i = 0; i < content->span_count; i++) {
        total += MeasureTextEx(regular, content->spans[i].text, size, BT_VIEW_DRILL_TEXT_SPACING).x;
    }
    if (total > max_w) {
        char joined[BT_CARD_SPANS_MAX * BT_CARD_TEXT_MAX];
        bt_view_span_text(content, joined, sizeof joined);
        bt_ui_draw_fitted(font_regular, joined, x, y, size, bt_theme_text_primary(), max_w);
        return;
    }
    float cursor = x;
    for (size_t i = 0; i < content->span_count; i++) {
        const bt_card_span *span = &content->spans[i];
        float span_size = span->size_rem * theme->root_rem;
        bt_app_draw_text(font_regular, span->text, cursor, y, span_size, span->color);
        Vector2 box = MeasureTextEx(regular, span->text, span_size, BT_VIEW_DRILL_TEXT_SPACING);
        cursor += box.x;
    }
}

void bt_view_drill_card_draw(const bt_card_content *content, const void *font_regular, float x,
                             float y, float max_w) {
    BT_TRACE_ENTER("app.view_drill_card_draw");
    const bt_theme *theme = bt_theme_table();
    Font regular = *(const Font *)font_regular;
    float gap = theme->spacing_rem * theme->root_rem;
    float cursor = y;
    for (size_t i = 0; i < content->row_count; i++) {
        float size_rem;
        bt_theme_color color;
        bt_row_style_metrics(content->rows[i].style, &size_rem, &color);
        float size = size_rem * theme->root_rem;
        float drawn = size;
        if (content->rows[i].style == BT_ROW_ALG && content->has_mechanics) {
            draw_span_line(font_regular, content, x, cursor, max_w);
        } else {
            if (content->rows[i].style == BT_ROW_ALG) {
                float measured =
                    MeasureTextEx(regular, content->rows[i].text, size, BT_VIEW_DRILL_TEXT_SPACING)
                        .x;
                drawn = bt_view_drill_alg_size_for(measured, max_w);
            }
            bt_ui_draw_fitted(font_regular, content->rows[i].text, x, cursor, drawn, color, max_w);
        }
        cursor += drawn + gap;
    }
    BT_TRACE_EXIT("app.view_drill_card_draw");
}

void bt_view_drill_reps_draw(const bt_drill_session *session, const void *font_regular, float x,
                             float y, float max_w) {
    BT_TRACE_ENTER("app.view_drill_reps_draw");
    const bt_theme *theme = bt_theme_table();
    Font regular = *(const Font *)font_regular;
    float gap = theme->spacing_rem * theme->root_rem;
    const bt_drill_side *side = bt_drill_session_current_side(session);
    char letters[3] = "";
    char alg[BT_CARD_TEXT_MAX] = "";
    if (side != NULL) {
        bt_drill_side_letters(side, letters);
        snprintf(alg, sizeof alg, "%s", side->alg_text);
    }
    char count_line[BT_CARD_TEXT_MAX];
    snprintf(count_line, sizeof count_line, "rep %u of %u", session->reps_done,
             session->reps_target);
    char timer[BT_CARD_TEXT_MAX];
    snprintf(timer, sizeof timer, "%us", (unsigned int)session->reps_clock);
    const char *rows[5] = {letters, alg, count_line, timer, "space each rep"};
    float sizes[5] = {
        theme->font_focal * theme->root_rem,   theme->font_alg * theme->root_rem,
        theme->font_heading * theme->root_rem, theme->font_heading * theme->root_rem,
        theme->font_small * theme->root_rem,
    };
    bt_theme_color colors[5] = {
        bt_theme_text_primary(), bt_theme_teal(),       bt_theme_text_primary(),
        bt_theme_text_muted(),   bt_theme_text_muted(),
    };
    float cursor = y;
    for (int i = 0; i < 5; i++) {
        float size = sizes[i];
        if (i == 1) {
            float measured = MeasureTextEx(regular, rows[i], size, BT_VIEW_DRILL_TEXT_SPACING).x;
            size = bt_view_drill_alg_size_for(measured, max_w);
        }
        bt_ui_draw_fitted(font_regular, rows[i], x, cursor, size, colors[i], max_w);
        cursor += size + gap;
    }
    BT_TRACE_EXIT("app.view_drill_reps_draw");
}

void bt_view_drill_rate_draw(const bt_drill_session *session, const void *font_regular, float x,
                             float y, float max_w) {
    BT_TRACE_ENTER("app.view_drill_rate_draw");
    const bt_theme *theme = bt_theme_table();
    float gap = theme->spacing_rem * theme->root_rem;
    char lines[3][BT_CARD_TEXT_MAX];
    size_t count = 0;
    bt_view_rate_lines(session, lines, 3, &count);
    float sizes[3] = {
        theme->font_focal * theme->root_rem,
        theme->font_heading * theme->root_rem,
        theme->font_small * theme->root_rem,
    };
    bt_theme_color colors[3] = {
        bt_theme_text_primary(),
        bt_theme_text_primary(),
        bt_theme_text_muted(),
    };
    float cursor = y;
    for (size_t i = 0; i < count && i < 3; i++) {
        bt_ui_draw_fitted(font_regular, lines[i], x, cursor, sizes[i], colors[i], max_w);
        cursor += sizes[i] + gap;
    }
    BT_TRACE_EXIT("app.view_drill_rate_draw");
}

void bt_view_drill_row_draw(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.view_drill_row_draw");
    const bt_app_font_book *book = bt_app_fonts_current();
    if (book == NULL) {
        BT_TRACE_FAIL("app.view_drill_row_draw", "fonts not loaded");
        BT_TRACE_EXIT("app.view_drill_row_draw");
        return;
    }
    if (!ctx->has_corpus || !ctx->has_stores) {
        BT_TRACE_FAIL("app.view_drill_row_draw", "corpus %d stores %d", ctx->has_corpus,
                      ctx->has_stores);
        BT_TRACE_EXIT("app.view_drill_row_draw");
        return;
    }
    bt_drill_app_state *app = bt_view_drill_app();
    bt_app_views *views = bt_app_views_shared();
    float window_w = (float)GetScreenWidth();
    float window_h = (float)GetScreenHeight();
    bt_view_frame_draw(book->bold, book->regular, window_w, window_h, "drill",
                       "space reps, 1..4 rate, esc menu");
    bt_view_frame_metrics metrics = bt_view_frame_metrics_for(window_w, window_h);
    bt_view_drill_layout layout = bt_view_drill_layout_for(metrics.content_x, metrics.content_y,
                                                           metrics.content_w, metrics.content_h);
    bt_view_drill_net_draw(book->bold, &views->net, layout.net_x, layout.net_y, layout.net_w,
                           layout.net_h);
    if (ctx->drill_phase == BT_DRILL_CARD) {
        bt_card_content content;
        bt_view_card_content(&app->session, &ctx->vendor_corpus, bt_views_book(ctx), &ctx->stores,
                             bt_views_now_day(), &content);
        bt_view_drill_card_draw(&content, book->regular, layout.card_x, layout.card_y,
                                layout.card_w);
    } else if (ctx->drill_phase == BT_DRILL_REPS) {
        bt_view_drill_reps_draw(&app->session, book->regular, layout.card_x, layout.card_y,
                                layout.card_w);
    } else {
        bt_view_drill_rate_draw(&app->session, book->regular, layout.card_x, layout.card_y,
                                layout.card_w);
    }
    BT_TRACE_EXIT("app.view_drill_row_draw");
}
