#include "app/views/algs/mod.h"

#include "app/open_url.h"
#include "app/views/algs/browse.h"
#include "app/window.h"
#include "common/bt_trace.h"

#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>

static const float BT_ALGS_DETAIL_SPLIT = 0.4F;
static const float BT_ALGS_LINK_UNDERLINE_PX = 2.0F;

size_t bt_view_algs_detail_visible(const float *line_rems, size_t count, float avail_px) {
    BT_TRACE_ENTER("app.algs_detail_visible");
    const bt_theme *theme = bt_theme_table();
    float pitch = theme->root_rem * BT_ALGS_LINE_FACTOR;
    float tail = theme->font_micro * pitch;
    float used = 0.0F;
    size_t visible = 0;
    while (visible < count) {
        float lines = used + line_rems[visible] * pitch;
        float next = visible + 1 < count ? lines + tail : lines;
        if (next > avail_px) {
            break;
        }
        used = lines;
        visible++;
    }
    BT_TRACE_EXIT("app.algs_detail_visible");
    return visible;
}

void bt_view_algs_style_metrics(bt_algs_style style, float *size, bt_theme_color *color) {
    BT_TRACE_ENTER("app.algs_style_metrics");
    const bt_theme *theme = bt_theme_table();
    float font = theme->font_small;
    bt_theme_color paint = bt_theme_text_muted();
    switch (style) {
    case BT_ALGS_STYLE_FOCAL:
        font = theme->font_title;
        paint = bt_theme_text_primary();
        break;
    case BT_ALGS_STYLE_MUTED:
    case BT_ALGS_STYLE_BODY:
        font = theme->font_body;
        paint = bt_theme_text_muted();
        break;
    case BT_ALGS_STYLE_HEADING:
        font = theme->font_heading;
        paint = bt_theme_text_primary();
        break;
    case BT_ALGS_STYLE_ALG:
        font = theme->font_alg;
        paint = bt_theme_text_primary();
        break;
    case BT_ALGS_STYLE_SMALL:
        font = theme->font_small;
        paint = bt_theme_text_muted();
        break;
    case BT_ALGS_STYLE_MICRO:
        font = theme->font_micro;
        paint = bt_theme_text_muted();
        break;
    }
    *size = font;
    if (color != NULL) {
        *color = paint;
    }
    BT_TRACE_EXIT("app.algs_style_metrics");
}

bt_view_algs_column bt_view_algs_detail_column(const bt_view_frame_metrics *metrics) {
    BT_TRACE_ENTER("app.algs_detail_column");
    bt_view_algs_column column;
    column.x = metrics->content_x + metrics->content_w * BT_ALGS_DETAIL_SPLIT;
    column.width = metrics->content_w * (1.0F - BT_ALGS_DETAIL_SPLIT);
    column.center = column.x + column.width * 0.5F;
    BT_TRACE_EXIT("app.algs_detail_column");
    return column;
}

bt_ui_rect bt_view_algs_query_band(const bt_view_frame_metrics *metrics) {
    BT_TRACE_ENTER("app.algs_query_band");
    const bt_theme *theme = bt_theme_table();
    bt_ui_rect band;
    band.x = metrics->content_x;
    band.y = metrics->content_y + metrics->content_h - theme->root_rem * BT_ALGS_QUERY_BAND_REM;
    band.width = metrics->content_w * BT_ALGS_DETAIL_SPLIT;
    band.height = theme->root_rem * BT_ALGS_QUERY_BAND_REM;
    BT_TRACE_EXIT("app.algs_query_band");
    return band;
}

static float pack_span_total(const bt_view_algs_detail_line *line, bt_view_algs_measure measure,
                             void *user) {
    float total = 0.0F;
    for (size_t at = 0; at < line->span_count; at++) {
        total += measure(line->spans[at].text, user);
    }
    return total;
}

size_t bt_view_algs_pack_spans(bt_view_algs_detail_line *lines, size_t count, float max_w,
                               bt_view_algs_measure measure, void *user) {
    BT_TRACE_ENTER("app.algs_pack_spans");
    size_t out = 0;
    size_t at = 0;
    while (at < count) {
        if (lines[at].span_count == 0) {
            lines[out++] = lines[at++];
            continue;
        }
        size_t head = at;
        float used = pack_span_total(&lines[head], measure, user);
        size_t next = at + 1;
        while (next < count && lines[next].span_count > 0) {
            float join = measure(BT_ALGS_UNIT_JOIN, user);
            float width = pack_span_total(&lines[next], measure, user);
            if (used + join + width > max_w ||
                lines[head].span_count + 1 + lines[next].span_count > BT_ALGS_SPANS_MAX) {
                break;
            }
            snprintf(lines[head].spans[lines[head].span_count].text, BT_ALGS_TEXT_MAX, "%s",
                     BT_ALGS_UNIT_JOIN);
            lines[head].spans[lines[head].span_count].color = bt_theme_text_muted();
            lines[head].span_count++;
            for (size_t span = 0; span < lines[next].span_count; span++) {
                lines[head].spans[lines[head].span_count++] = lines[next].spans[span];
            }
            used += join + width;
            next++;
        }
        lines[out++] = lines[head];
        at = next;
    }
    BT_TRACE_EXIT("app.algs_pack_spans");
    return out;
}

float bt_view_algs_centered_line_x(float center, float line_w) {
    BT_TRACE_ENTER("app.algs_centered_line_x");
    float x = center - line_w * 0.5F;
    BT_TRACE_EXIT("app.algs_centered_line_x");
    return x;
}

bt_ui_rect bt_view_algs_link_rect(float center, float y, float line_w, float line_h) {
    BT_TRACE_ENTER("app.algs_link_rect");
    bt_ui_rect rect;
    rect.x = bt_view_algs_centered_line_x(center, line_w);
    rect.y = y;
    rect.width = line_w;
    rect.height = line_h;
    BT_TRACE_EXIT("app.algs_link_rect");
    return rect;
}

const char *bt_view_algs_selected_recon(const bt_corpus *corpus,
                                        const bt_view_algs_browser *browser) {
    BT_TRACE_ENTER("app.algs_selected_recon");
    const char *recon = "";
    if (browser->filtered_count > 0 && browser->selected < browser->filtered_count) {
        const bt_corpus_row **rows = NULL;
        size_t count =
            bt_view_algs_case_rows_dyn(corpus, &browser->filtered[browser->selected], &rows);
        if (count > 0) {
            recon = rows[0]->recon_link;
        }
        free(rows);
    }
    BT_TRACE_EXIT("app.algs_selected_recon");
    return recon;
}

void bt_view_algs_link_record(bt_view_algs_app_state *app, float center, float y, float line_w,
                              float line_h, const char *url) {
    BT_TRACE_ENTER("app.algs_link_record");
    app->link_rect = bt_view_algs_link_rect(center, y, line_w, line_h);
    snprintf(app->link_url, sizeof app->link_url, "%s", url);
    app->link_ready = 1;
    BT_TRACE_EXIT("app.algs_link_record");
}

void bt_view_algs_link_draw(bt_view_algs_app_state *app, float center, float y, float text_w,
                            float text_h, float line_h, const char *url) {
    BT_TRACE_ENTER("app.algs_link_draw");
    float x = bt_view_algs_centered_line_x(center, text_w);
    DrawRectangle((int)x, (int)(y + text_h), (int)text_w, (int)BT_ALGS_LINK_UNDERLINE_PX,
                  bt_app_color(bt_theme_accent()));
    bt_view_algs_link_record(app, center, y, text_w, line_h, url);
    BT_TRACE_EXIT("app.algs_link_draw");
}

int bt_view_algs_link_click(bt_view_algs_app_state *app, const bt_ui_input *input) {
    BT_TRACE_ENTER("app.algs_link_click");
    if (!app->link_ready || input == NULL || !input->click ||
        !bt_ui_contains(&app->link_rect, input->x, input->y)) {
        BT_TRACE_EXIT("app.algs_link_click");
        return 0;
    }
    bt_app_open_url(app->link_url);
    BT_TRACE_EXIT("app.algs_link_click");
    return 1;
}

void bt_view_algs_open_recon(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.algs_open_recon");
    bt_view_algs_app_state *app = bt_view_algs_app();
    if (!bt_repo_browser_letters_focused(bt_view_algs_repo())) {
        BT_TRACE_EXIT("app.algs_open_recon");
        return;
    }
    const char *recon = bt_view_algs_selected_recon(&ctx->vendor_corpus, &app->browser);
    if (bt_app_open_url(recon) != 0) {
        BT_TRACE_FAIL("app.algs_open_recon", "cannot open recon %s", recon);
    }
    BT_TRACE_EXIT("app.algs_open_recon");
}
