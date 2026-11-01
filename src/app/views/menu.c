#include "app/views/menu.h"

#include "app/ui.h"
#include "app/views/frame.h"
#include "app/views/state.h"
#include "app/window.h"
#include "data/srs.h"

#include "raylib.h"

#include "common/bt_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t due_card_count(const bt_app_ctx *ctx) {
    long long now_day = bt_views_now_day();
    size_t due = 0;
    for (size_t i = 0; i < ctx->stores.srs.count; i++) {
        if (bt_card_is_due(&ctx->stores.srs.items[i].card, now_day)) {
            due++;
        }
    }
    return due;
}

static void menu_data_line(const bt_app_ctx *ctx, bt_app_state target, char *out, size_t cap) {
    const bt_settings *settings = &ctx->stores.settings;
    switch (target) {
    case BT_APP_DRILL:
        snprintf(out, cap, "%zu pairs due", due_card_count(ctx));
        break;
    case BT_APP_MEMO:
        snprintf(out, cap, "cadence %u ms, mode %s", settings->memo_cadence_ms,
                 bt_memo_mode_label(settings->memo_mode));
        break;
    case BT_APP_SOLVE:
        snprintf(out, cap, "%zu solves logged", ctx->stores.solves.count);
        break;
    case BT_APP_ALGS:
        if (ctx->has_corpus) {
            snprintf(out, cap, "%zu vendor rows", ctx->vendor_corpus.count);
        } else {
            snprintf(out, cap, "%s", "vendor corpus not loaded");
        }
        break;
    case BT_APP_STATS:
        snprintf(out, cap, "%zu solves, %zu due cards", ctx->stores.solves.count,
                 due_card_count(ctx));
        break;
    case BT_APP_SETTINGS:
        snprintf(out, cap, "cadence %u ms, reps %u, mode %s", settings->memo_cadence_ms,
                 settings->drill_reps, bt_memo_mode_label(settings->memo_mode));
        break;
    default:
        out[0] = '\0';
        break;
    }
}

static const char *state_description(bt_app_state state) {
    switch (state) {
    case BT_APP_DRILL:
        return "physical 3-style pair drilling with spaced repetition";
    case BT_APP_MEMO:
        return "streaming memo on seeded scrambles";
    case BT_APP_SOLVE:
        return "the timed full cycle";
    case BT_APP_REPLAY:
        return "orbit the 3d solve replay";
    case BT_APP_ALGS:
        return "browse 818 cases and the event repos";
    case BT_APP_STATS:
        return "solve history and review load";
    case BT_APP_SETTINGS:
        return "cadence, memo mode, reps";
    default:
        return "";
    }
}

void bt_view_menu_rows(const bt_app_ctx *ctx, bt_view_menu_row out[BT_VIEW_MENU_ROW_COUNT]) {
    BT_TRACE_ENTER("app.view_menu_rows");
    for (int i = 0; i < BT_VIEW_MENU_ROW_COUNT; i++) {
        out[i].number = i + 1;
        out[i].label = bt_app_state_label(BT_APP_MENU_KEYS[i].target);
        out[i].description = state_description(BT_APP_MENU_KEYS[i].target);
        menu_data_line(ctx, BT_APP_MENU_KEYS[i].target, out[i].data, BT_VIEW_MENU_TEXT_MAX);
    }
    BT_TRACE_EXIT("app.view_menu_rows");
}

void bt_view_menu_title(const bt_view_menu_row *row, char out[BT_VIEW_MENU_TEXT_MAX]) {
    BT_TRACE_ENTER("app.view_menu_title");
    snprintf(out, BT_VIEW_MENU_TEXT_MAX, "%d %s", row->number, row->label);
    BT_TRACE_EXIT("app.view_menu_title");
}

char *bt_view_menu_warning_context(const bt_str_list *warnings) {
    BT_TRACE_ENTER("app.view_menu_warning_context");
    size_t cap = 64;
    char *joined = malloc(cap);
    if (joined == NULL) {
        BT_TRACE_FAIL("app.view_menu_warning_context", "out of memory");
        BT_TRACE_EXIT("app.view_menu_warning_context");
        return NULL;
    }
    size_t at = 0;
    joined[0] = 0;
    for (size_t i = 0; i < warnings->count; i++) {
        const char *sep = i > 0 ? " | " : "";
        size_t need = at + strlen(sep) + strlen(warnings->items[i]) + 1;
        if (need > cap) {
            char *grown = realloc(joined, need);
            if (grown == NULL) {
                free(joined);
                BT_TRACE_FAIL("app.view_menu_warning_context", "out of memory");
                BT_TRACE_EXIT("app.view_menu_warning_context");
                return NULL;
            }
            joined = grown;
            cap = need;
        }
        at += (size_t)snprintf(joined + at, need - at, "%s%s", sep, warnings->items[i]);
    }
    BT_TRACE_EXIT("app.view_menu_warning_context");
    return joined;
}

static const float BT_MENU_TEXT_SPACING = 1.0F;

bt_view_menu_layout bt_view_menu_layout_for(float window_w, float window_h, int desc_lines) {
    BT_TRACE_ENTER("app.view_menu_layout_for");
    const bt_theme *theme = bt_theme_table();
    bt_view_frame_metrics metrics = bt_view_frame_metrics_for(window_w, window_h);
    int lines = desc_lines < 1 ? 1 : desc_lines;
    float heading = theme->font_heading * theme->root_rem;
    float small = theme->font_small * theme->root_rem;
    float micro = theme->font_micro * theme->root_rem;
    float inner_gap = theme->spacing_rem * 0.5F * theme->root_rem;
    float row_gap = theme->spacing_rem * 2.0F * theme->root_rem;
    int rows_per_column = BT_VIEW_MENU_ROW_COUNT / BT_VIEW_MENU_COLUMN_COUNT;
    bt_view_menu_layout layout;
    layout.gutter = theme->spacing_rem * theme->root_rem;
    layout.col_w = (metrics.content_w - layout.gutter) / (float)BT_VIEW_MENU_COLUMN_COUNT;
    layout.col_x[0] = metrics.content_x;
    layout.col_x[1] = metrics.content_x + layout.col_w + layout.gutter;
    layout.row_h = heading + inner_gap + small * (float)lines + inner_gap + micro;
    layout.row_pitch = layout.row_h + row_gap;
    layout.desc_dy = heading + inner_gap;
    layout.data_dy = layout.desc_dy + small * (float)lines + inner_gap;
    float block_h = (float)rows_per_column * layout.row_h + (float)(rows_per_column - 1) * row_gap;
    layout.first_row_y = metrics.content_y + (metrics.content_h - block_h) / 2.0F;
    BT_TRACE_EXIT("app.view_menu_layout_for");
    return layout;
}

size_t bt_view_menu_wrap_lines(const void *font, const char *text, float size, float max_w,
                               char out[][BT_VIEW_MENU_TEXT_MAX], size_t cap) {
    BT_TRACE_ENTER("app.view_menu_wrap_lines");
    Font regular = *(const Font *)font;
    size_t count = 0;
    const char *at = text;
    while (*at != '\0' && count < cap) {
        const char *end = at;
        const char *last_fit = NULL;
        while (*end != '\0') {
            const char *space = strchr(end, ' ');
            const char *stop = space != NULL ? space : end + strlen(end);
            char piece[BT_VIEW_MENU_TEXT_MAX];
            size_t len = (size_t)(stop - at);
            if (len >= sizeof piece) {
                len = sizeof piece - 1;
            }
            memcpy(piece, at, len);
            piece[len] = '\0';
            Vector2 box = MeasureTextEx(regular, piece, size, BT_MENU_TEXT_SPACING);
            if (box.x > max_w) {
                break;
            }
            last_fit = stop;
            if (space == NULL) {
                break;
            }
            end = space + 1;
        }
        if (last_fit == NULL) {
            last_fit = at + strlen(at);
        }
        size_t len = (size_t)(last_fit - at);
        if (len >= BT_VIEW_MENU_TEXT_MAX) {
            len = BT_VIEW_MENU_TEXT_MAX - 1;
        }
        memcpy(out[count], at, len);
        out[count][len] = '\0';
        count++;
        at = *last_fit == ' ' ? last_fit + 1 : last_fit;
        if (*last_fit == '\0') {
            break;
        }
    }
    BT_TRACE_EXIT("app.view_menu_wrap_lines");
    return count;
}

void bt_view_menu_draw(const bt_app_ctx *ctx, const bt_str_list *warnings, const void *font_regular,
                       const void *font_bold, float window_w, float window_h) {
    BT_TRACE_ENTER("app.view_menu_draw");
    const bt_theme *theme = bt_theme_table();
    char *context = bt_view_menu_warning_context(warnings);
    bt_view_frame_draw(font_bold, font_regular, window_w, window_h, "menu",
                       context != NULL ? context : "");
    free(context);
    bt_view_menu_row rows[BT_VIEW_MENU_ROW_COUNT];
    bt_view_menu_rows(ctx, rows);
    float heading = theme->font_heading * theme->root_rem;
    float small = theme->font_small * theme->root_rem;
    float micro = theme->font_micro * theme->root_rem;
    bt_view_menu_layout layout =
        bt_view_menu_layout_for(window_w, window_h, BT_VIEW_MENU_DESC_MAX_LINES);
    int rows_per_column = BT_VIEW_MENU_ROW_COUNT / BT_VIEW_MENU_COLUMN_COUNT;
    Font regular = *(const Font *)font_regular;
    for (int i = 0; i < BT_VIEW_MENU_ROW_COUNT; i++) {
        int column = i / rows_per_column;
        int slot = i % rows_per_column;
        float x = layout.col_x[column];
        float y = layout.first_row_y + layout.row_pitch * (float)slot;
        char number[8];
        snprintf(number, sizeof number, "%d", rows[i].number);
        Vector2 number_box = MeasureTextEx(regular, number, heading, BT_MENU_TEXT_SPACING);
        Vector2 space_box = MeasureTextEx(regular, " ", heading, BT_MENU_TEXT_SPACING);
        bt_app_draw_text(font_regular, number, x, y, heading, bt_theme_accent());
        bt_app_draw_text(font_regular, " ", x + number_box.x, y, heading, bt_theme_text_primary());
        float label_x = x + number_box.x + space_box.x;
        bt_ui_draw_fitted(font_regular, rows[i].label, label_x, y, heading, bt_theme_text_primary(),
                          x + layout.col_w - label_x);
        char wrapped[BT_VIEW_MENU_WRAP_MAX][BT_VIEW_MENU_TEXT_MAX];
        size_t line_count = bt_view_menu_wrap_lines(font_regular, rows[i].description, small,
                                                    layout.col_w, wrapped, BT_VIEW_MENU_WRAP_MAX);
        if (line_count > (size_t)BT_VIEW_MENU_DESC_MAX_LINES) {
            line_count = (size_t)BT_VIEW_MENU_DESC_MAX_LINES;
        }
        for (size_t line = 0; line < line_count; line++) {
            bt_ui_draw_fitted(font_regular, wrapped[line], x,
                              y + layout.desc_dy + small * (float)line, small,
                              bt_theme_text_muted(), layout.col_w);
        }
        bt_ui_draw_fitted(font_regular, rows[i].data, x, y + layout.data_dy, micro,
                          bt_theme_text_muted(), layout.col_w);
    }
    BT_TRACE_EXIT("app.view_menu_draw");
}

void bt_view_menu_row_draw(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.view_menu_row_draw");
    const bt_app_font_book *book = bt_app_fonts_current();
    if (book == NULL) {
        BT_TRACE_FAIL("app.view_menu_row_draw", "fonts not loaded");
        BT_TRACE_EXIT("app.view_menu_row_draw");
        return;
    }
    bt_view_menu_draw(ctx, &ctx->boot_warnings, book->regular, book->bold, (float)GetScreenWidth(),
                      (float)GetScreenHeight());
    BT_TRACE_EXIT("app.view_menu_row_draw");
}
