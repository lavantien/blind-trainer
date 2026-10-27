#include "app/views/menu.h"

#include "common/bt_trace.h"

#include <stdio.h>
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

void bt_view_menu_warning_context(const bt_str_list *warnings, char out[BT_VIEW_MENU_TEXT_MAX]) {
    BT_TRACE_ENTER("app.view_menu_warning_context");
    size_t at = 0;
    out[0] = '\0';
    for (size_t i = 0; i < warnings->count; i++) {
        if (i > 0 && at + 3 < BT_VIEW_MENU_TEXT_MAX) {
            snprintf(out + at, BT_VIEW_MENU_TEXT_MAX - at, " | ");
            at += 3;
        }
        snprintf(out + at, BT_VIEW_MENU_TEXT_MAX - at, "%s", warnings->items[i]);
        at += strlen(out + at);
        if (at >= BT_VIEW_MENU_TEXT_MAX - 1) {
            break;
        }
    }
    BT_TRACE_EXIT("app.view_menu_warning_context");
}
