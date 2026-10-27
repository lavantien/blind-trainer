#include "app/views/menu.h"

#include "app/views/frame.h"
#include "app/views/state.h"
#include "app/window.h"

#include "raylib.h"

#include "common/bt_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void bt_view_menu_rows(bt_view_menu_row out[BT_VIEW_MENU_ROW_COUNT]) {
    BT_TRACE_ENTER("app.view_menu_rows");
    for (int i = 0; i < BT_VIEW_MENU_ROW_COUNT; i++) {
        out[i].number = i + 1;
        out[i].label = bt_app_state_label(BT_APP_MENU_KEYS[i].target);
        out[i].description = state_description(BT_APP_MENU_KEYS[i].target);
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

void bt_view_menu_draw(const bt_str_list *warnings, const void *font_regular, const void *font_bold,
                       float window_w, float window_h) {
    BT_TRACE_ENTER("app.view_menu_draw");
    const bt_theme *theme = bt_theme_table();
    bt_view_frame_metrics metrics = bt_view_frame_metrics_for(window_w, window_h);
    char *context = bt_view_menu_warning_context(warnings);
    bt_view_frame_draw(font_bold, font_regular, window_w, window_h, "menu",
                       context != NULL ? context : "");
    free(context);
    bt_view_menu_row rows[BT_VIEW_MENU_ROW_COUNT];
    bt_view_menu_rows(rows);
    float heading = theme->font_heading * theme->root_rem;
    float small = theme->font_small * theme->root_rem;
    float inner_gap = theme->spacing_rem * 0.5F * theme->root_rem;
    float row_gap = theme->spacing_rem * 2.0F * theme->root_rem;
    float row_height = heading + inner_gap + small;
    float total =
        row_height * (float)BT_VIEW_MENU_ROW_COUNT + row_gap * (float)(BT_VIEW_MENU_ROW_COUNT - 1);
    float y = metrics.content_y + (metrics.content_h - total) / 2.0F;
    for (int i = 0; i < BT_VIEW_MENU_ROW_COUNT; i++) {
        char number[8];
        snprintf(number, sizeof number, "%d", rows[i].number);
        Font regular = *(const Font *)font_regular;
        Vector2 number_box = MeasureTextEx(regular, number, heading, 1.0F);
        Vector2 space_box = MeasureTextEx(regular, " ", heading, 1.0F);
        float x = metrics.content_x;
        bt_app_draw_text(font_regular, number, x, y, heading, bt_theme_accent());
        x += number_box.x;
        bt_app_draw_text(font_regular, " ", x, y, heading, bt_theme_text_primary());
        x += space_box.x;
        bt_app_draw_text(font_regular, rows[i].label, x, y, heading, bt_theme_text_primary());
        bt_app_draw_text(font_regular, rows[i].description, metrics.content_x,
                         y + heading + inner_gap, small, bt_theme_text_muted());
        y += row_height + row_gap;
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
    bt_view_menu_draw(&ctx->boot_warnings, book->regular, book->bold, (float)GetScreenWidth(),
                      (float)GetScreenHeight());
    BT_TRACE_EXIT("app.view_menu_row_draw");
}
