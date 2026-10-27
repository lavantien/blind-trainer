#ifndef BT_APP_VIEWS_SETTINGS_H
#define BT_APP_VIEWS_SETTINGS_H

#include "app/logic.h"
#include "app/persist.h"
#include "app/states.h"
#include "app/ui.h"

#include <stddef.h>

#define BT_SETTINGS_TEXT_MAX 128
#define BT_SETTINGS_LINE_COUNT 4
#define BT_SETTINGS_CADENCE_MIN 1
#define BT_SETTINGS_CADENCE_MAX 10000
#define BT_SETTINGS_REPS_MIN 1
#define BT_SETTINGS_REPS_MAX 100

typedef struct {
    unsigned int memo_cadence_ms;
    bt_memo_mode memo_mode;
    unsigned int drill_reps;
} bt_settings_draft;

typedef struct {
    bt_ui_number_state cadence;
    bt_ui_number_state reps;
    bt_ui_list_window mode_list;
} bt_settings_widgets;

void bt_view_settings_draft_from(const bt_settings *settings, bt_settings_draft *draft);
void bt_view_settings_draft_apply(const bt_settings_draft *draft, bt_settings *out);
unsigned int bt_view_settings_clamp_cadence(long long value);
unsigned int bt_view_settings_clamp_reps(long long value);
void bt_view_settings_lines(const bt_settings_draft *draft,
                            char out[BT_SETTINGS_LINE_COUNT][BT_SETTINGS_TEXT_MAX]);

typedef struct {
    int initialized;
    bt_settings_draft draft;
    int changed;
    bt_settings_widgets widgets;
} bt_settings_app_state;

void bt_view_settings_app_init(bt_settings_app_state *app);
void bt_view_settings_app_free(bt_settings_app_state *app);
bt_settings_app_state *bt_view_settings_app(void);
void bt_view_settings_enter_state(bt_app_ctx *ctx, bt_settings_app_state *app);
void bt_view_settings_key(bt_app_ctx *ctx, bt_settings_app_state *app, int key);
void bt_view_settings_keys(bt_app_ctx *ctx, bt_settings_app_state *app, const int *keys,
                           size_t count);
void bt_view_settings_update_state(bt_app_ctx *ctx, bt_settings_app_state *app);
void bt_view_settings_enter(bt_app_ctx *ctx);
void bt_view_settings_update(bt_app_ctx *ctx);

void bt_view_settings_draw(bt_settings_draft *draft, int *changed, int *focus,
                          bt_settings_widgets *widgets, const void *font_regular,
                          const void *font_bold, float window_w, float window_h,
                          const bt_ui_input *input);
void bt_view_settings_row_draw(bt_app_ctx *ctx);

#endif
