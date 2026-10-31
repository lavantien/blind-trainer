#include "app/views/settings.h"

#include "app/boot.h"
#include "app/theme.h"
#include "app/views/frame.h"
#include "app/views/state.h"
#include "app/window.h"
#include "common/bt_trace.h"

#include "raylib.h"

#include <stdio.h>
#include <string.h>

void bt_view_settings_draft_from(const bt_settings *settings, bt_settings_draft *draft) {
    BT_TRACE_ENTER("app.view_settings_draft_from");
    draft->memo_cadence_ms = settings->memo_cadence_ms;
    draft->memo_mode = settings->memo_mode;
    draft->drill_reps = settings->drill_reps;
    BT_TRACE_EXIT("app.view_settings_draft_from");
}

void bt_view_settings_draft_apply(const bt_settings_draft *draft, bt_settings *out) {
    BT_TRACE_ENTER("app.view_settings_draft_apply");
    out->memo_cadence_ms = draft->memo_cadence_ms;
    out->memo_mode = draft->memo_mode;
    out->drill_reps = draft->drill_reps;
    BT_TRACE_EXIT("app.view_settings_draft_apply");
}

unsigned int bt_view_settings_clamp_cadence(long long value) {
    BT_TRACE_ENTER("app.view_settings_clamp_cadence");
    if (value < BT_SETTINGS_CADENCE_MIN) {
        value = BT_SETTINGS_CADENCE_MIN;
    }
    if (value > BT_SETTINGS_CADENCE_MAX) {
        value = BT_SETTINGS_CADENCE_MAX;
    }
    BT_TRACE_EXIT("app.view_settings_clamp_cadence");
    return (unsigned int)value;
}

unsigned int bt_view_settings_clamp_reps(long long value) {
    BT_TRACE_ENTER("app.view_settings_clamp_reps");
    if (value < BT_SETTINGS_REPS_MIN) {
        value = BT_SETTINGS_REPS_MIN;
    }
    if (value > BT_SETTINGS_REPS_MAX) {
        value = BT_SETTINGS_REPS_MAX;
    }
    BT_TRACE_EXIT("app.view_settings_clamp_reps");
    return (unsigned int)value;
}

static const float BT_SETTINGS_ROW_REM = 3.2F;
static const float BT_SETTINGS_CONTROL_REM = 14.0F;
static const float BT_SETTINGS_CONTROL_INSET_REM = 0.3F;
static const float BT_SETTINGS_ROW_GAP_SPACING = 2.0F;
static const float BT_SETTINGS_HINT_GAP_SPACING = 0.5F;

bt_view_settings_layout bt_view_settings_layout_for(float window_w, float window_h) {
    BT_TRACE_ENTER("app.view_settings_layout");
    const bt_theme *theme = bt_theme_table();
    bt_view_frame_metrics metrics = bt_view_frame_metrics_for(window_w, window_h);
    bt_view_settings_layout layout;
    float column_w = metrics.content_w;
    float column_half = column_w * 0.5F;
    float row_h = BT_SETTINGS_ROW_REM * theme->root_rem;
    float gap = theme->spacing_rem * BT_SETTINGS_ROW_GAP_SPACING * theme->root_rem;
    float block_h = row_h * (float)BT_SETTINGS_ROW_COUNT + gap * (float)(BT_SETTINGS_ROW_COUNT - 1);
    float column_x = metrics.content_x;
    float top_y = metrics.content_y + (metrics.content_h - block_h) / 2.0F;
    float control_w = BT_SETTINGS_CONTROL_REM * theme->root_rem;
    if (control_w > column_half) {
        control_w = column_half;
    }
    float inset = BT_SETTINGS_CONTROL_INSET_REM * theme->root_rem;
    if (row_h - inset - inset < theme->root_rem) {
        inset = (row_h - theme->root_rem) * 0.5F;
        if (inset < 0.0F) {
            inset = 0.0F;
        }
    }
    float control_h = row_h - inset - inset;
    float pad = theme->spacing_rem * theme->root_rem;
    float body = theme->font_body * theme->root_rem;
    float hint_gap = theme->spacing_rem * BT_SETTINGS_HINT_GAP_SPACING * theme->root_rem;
    float small = theme->font_small * theme->root_rem;
    for (int i = 0; i < BT_SETTINGS_ROW_COUNT; i++) {
        float row_y = top_y + (row_h + gap) * (float)i;
        layout.rows[i].x = column_x;
        layout.rows[i].y = row_y;
        layout.rows[i].width = column_w;
        layout.rows[i].height = row_h;
        layout.controls[i].x = column_x + column_w - control_w;
        layout.controls[i].y = row_y + inset;
        layout.controls[i].width = control_w;
        layout.controls[i].height = control_h;
        layout.hints[i].x = column_x + pad;
        layout.hints[i].y = row_y + pad + body + hint_gap;
        layout.hints[i].width = column_w - control_w - pad;
        layout.hints[i].height = small;
    }
    BT_TRACE_EXIT("app.view_settings_layout");
    return layout;
}

void bt_view_settings_labels(char out[BT_SETTINGS_ROW_COUNT][BT_SETTINGS_TEXT_MAX]) {
    BT_TRACE_ENTER("app.view_settings_labels");
    snprintf(out[0], BT_SETTINGS_TEXT_MAX, "%s", "memo cadence (ms)");
    snprintf(out[1], BT_SETTINGS_TEXT_MAX, "%s", "memo mode");
    snprintf(out[2], BT_SETTINGS_TEXT_MAX, "%s", "drill reps");
    BT_TRACE_EXIT("app.view_settings_labels");
}

void bt_view_settings_hints(char out[BT_SETTINGS_ROW_COUNT][BT_SETTINGS_TEXT_MAX]) {
    BT_TRACE_ENTER("app.view_settings_hints");
    snprintf(out[0], BT_SETTINGS_TEXT_MAX, "%s", "digits then enter");
    snprintf(out[1], BT_SETTINGS_TEXT_MAX, "%s", "press 1, 2, or 3");
    snprintf(out[2], BT_SETTINGS_TEXT_MAX, "%s", "digits then enter");
    BT_TRACE_EXIT("app.view_settings_hints");
}

static bt_settings_app_state g_settings_app;

void bt_view_settings_app_init(bt_settings_app_state *app) {
    BT_TRACE_ENTER("app.view_settings_app_init");
    memset(app, 0, sizeof *app);
    app->initialized = 1;
    BT_TRACE_EXIT("app.view_settings_app_init");
}

void bt_view_settings_app_free(bt_settings_app_state *app) {
    BT_TRACE_ENTER("app.view_settings_app_free");
    app->initialized = 0;
    app->changed = 0;
    BT_TRACE_EXIT("app.view_settings_app_free");
}

bt_settings_app_state *bt_view_settings_app(void) {
    if (!g_settings_app.initialized) {
        bt_view_settings_app_init(&g_settings_app);
    }
    return &g_settings_app;
}

void bt_view_settings_enter_state(bt_app_ctx *ctx, bt_settings_app_state *app) {
    BT_TRACE_ENTER("app.view_settings_enter_state");
    bt_view_settings_draft_from(&ctx->stores.settings, &app->draft);
    app->changed = 0;
    memset(&app->widgets, 0, sizeof app->widgets);
    BT_TRACE_EXIT("app.view_settings_enter_state");
}

void bt_view_settings_key(bt_app_ctx *ctx, bt_settings_app_state *app, int key) {
    BT_TRACE_ENTER("app.view_settings_key");
    bt_memo_mode mode;
    int index = key - '1';
    if (index < 0 || index >= BT_MEMO_MODE_COUNT || !bt_memo_mode_from_index(index, &mode)) {
        BT_TRACE_EXIT("app.view_settings_key");
        return;
    }
    if (app->widgets.mode_select.open) {
        app->widgets.mode_select.cursor = index;
        BT_TRACE_EXIT("app.view_settings_key");
        return;
    }
    if (ctx->input_focus) {
        BT_TRACE_EXIT("app.view_settings_key");
        return;
    }
    app->draft.memo_mode = mode;
    app->changed = 1;
    app->widgets.mode_select.open = 1;
    app->widgets.mode_select.cursor = index;
    BT_TRACE_EXIT("app.view_settings_key");
}

void bt_view_settings_keys(bt_app_ctx *ctx, bt_settings_app_state *app, const int *keys,
                           size_t count) {
    BT_TRACE_ENTER("app.view_settings_keys");
    for (size_t i = 0; i < count; i++) {
        bt_memo_mode mode;
        int index = keys[i] - '1';
        if (index < 0 || index >= BT_MEMO_MODE_COUNT || !bt_memo_mode_from_index(index, &mode)) {
            continue;
        }
        if (app->widgets.mode_select.open) {
            app->widgets.mode_select.cursor = index;
            BT_TRACE_EXIT("app.view_settings_keys");
            return;
        }
        if (ctx->input_focus) {
            BT_TRACE_EXIT("app.view_settings_keys");
            return;
        }
        app->draft.memo_mode = mode;
        app->changed = 1;
        app->widgets.mode_select.open = 1;
        app->widgets.mode_select.cursor = index;
        BT_TRACE_EXIT("app.view_settings_keys");
        return;
    }
    BT_TRACE_EXIT("app.view_settings_keys");
}

void bt_view_settings_update_state(bt_app_ctx *ctx, bt_settings_app_state *app) {
    BT_TRACE_ENTER("app.view_settings_update_state");
    if (!app->changed) {
        BT_TRACE_EXIT("app.view_settings_update_state");
        return;
    }
    bt_settings next;
    bt_view_settings_draft_apply(&app->draft, &next);
    if (next.memo_cadence_ms != ctx->stores.settings.memo_cadence_ms ||
        next.memo_mode != ctx->stores.settings.memo_mode ||
        next.drill_reps != ctx->stores.settings.drill_reps) {
        bt_app_apply_settings_change(ctx, &next);
    }
    app->changed = 0;
    BT_TRACE_EXIT("app.view_settings_update_state");
}

void bt_view_settings_enter(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.view_settings_enter");
    bt_view_settings_enter_state(ctx, bt_view_settings_app());
    BT_TRACE_EXIT("app.view_settings_enter");
}

void bt_view_settings_update(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.view_settings_update");
    bt_settings_app_state *app = bt_view_settings_app();
    int keys[BT_MEMO_MODE_COUNT] = {KEY_ONE, KEY_TWO, KEY_THREE};
    int pressed[BT_MEMO_MODE_COUNT];
    size_t count = 0;
    for (int i = 0; i < BT_MEMO_MODE_COUNT; i++) {
        if (IsKeyPressed(keys[i])) {
            pressed[count] = keys[i];
            count++;
        }
    }
    bt_view_settings_keys(ctx, app, pressed, count);
    bt_view_settings_update_state(ctx, app);
    BT_TRACE_EXIT("app.view_settings_update");
}

void bt_view_settings_draw(bt_settings_draft *draft, int *changed, int *focus,
                           bt_settings_widgets *widgets, const void *font_regular,
                           const void *font_bold, float window_w, float window_h,
                           const bt_ui_input *input) {
    BT_TRACE_ENTER("app.view_settings_draw");
    const bt_theme *theme = bt_theme_table();
    bt_view_settings_layout layout = bt_view_settings_layout_for(window_w, window_h);
    bt_view_frame_draw(font_bold, font_regular, window_w, window_h, "settings", "esc menu");
    char labels[BT_SETTINGS_ROW_COUNT][BT_SETTINGS_TEXT_MAX];
    char hints[BT_SETTINGS_ROW_COUNT][BT_SETTINGS_TEXT_MAX];
    bt_view_settings_labels(labels);
    bt_view_settings_hints(hints);
    float body = theme->font_body * theme->root_rem;
    float small = theme->font_small * theme->root_rem;
    float pad = theme->spacing_rem * theme->root_rem;
    for (int i = 0; i < BT_SETTINGS_ROW_COUNT; i++) {
        bt_app_draw_text(font_regular, labels[i], layout.rows[i].x + pad, layout.rows[i].y + pad,
                         body, bt_theme_text_primary());
        bt_app_draw_text(font_regular, hints[i], layout.hints[i].x, layout.hints[i].y, small,
                         bt_theme_text_muted());
    }
    Font regular = *(const Font *)font_regular;
    bt_ui_input field_input = *input;
    if (widgets->mode_select.open) {
        field_input.click = 0;
        field_input.down = 0;
        field_input.wheel = 0;
        field_input.escape = 0;
        field_input.enter = 0;
        field_input.backspace = 0;
        field_input.arrow_up = 0;
        field_input.arrow_down = 0;
        field_input.char_count = 0;
    }
    int edited = 0;
    long long cadence = bt_ui_number_field(
        &regular, &layout.controls[0], (long long)draft->memo_cadence_ms, BT_SETTINGS_CADENCE_MIN,
        BT_SETTINGS_CADENCE_MAX, &widgets->cadence, &field_input, &edited);
    if (edited) {
        unsigned int clamped = bt_view_settings_clamp_cadence(cadence);
        if (clamped != draft->memo_cadence_ms) {
            draft->memo_cadence_ms = clamped;
            *changed = 1;
        }
    }
    long long reps = bt_ui_number_field(&regular, &layout.controls[2], (long long)draft->drill_reps,
                                        BT_SETTINGS_REPS_MIN, BT_SETTINGS_REPS_MAX, &widgets->reps,
                                        &field_input, &edited);
    if (edited) {
        unsigned int clamped = bt_view_settings_clamp_reps(reps);
        if (clamped != draft->drill_reps) {
            draft->drill_reps = clamped;
            *changed = 1;
        }
    }
    char mode_rows[BT_MEMO_MODE_COUNT][BT_SETTINGS_TEXT_MAX];
    const char *mode_row_ptrs[BT_MEMO_MODE_COUNT];
    for (int i = 0; i < BT_MEMO_MODE_COUNT; i++) {
        snprintf(mode_rows[i], BT_SETTINGS_TEXT_MAX, "%d %s", i + 1, BT_MEMO_MODE_LABELS[i].label);
        mode_row_ptrs[i] = mode_rows[i];
    }
    int selected = bt_memo_mode_index(draft->memo_mode);
    if (selected < 0) {
        selected = 0;
    }
    int select_edited = 0;
    int picked = bt_ui_select(&regular, &layout.controls[1], mode_row_ptrs, BT_MEMO_MODE_COUNT,
                              selected, &widgets->mode_select, input, &select_edited, window_h);
    if (select_edited) {
        bt_memo_mode mode;
        if (bt_memo_mode_from_index(picked, &mode)) {
            draft->memo_mode = mode;
            *changed = 1;
        }
    }
    *focus = widgets->cadence.focused || widgets->reps.focused || widgets->mode_select.open;
    BT_TRACE_EXIT("app.view_settings_draw");
}

void bt_view_settings_row_draw(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.view_settings_row_draw");
    const bt_app_font_book *book = bt_app_fonts_current();
    if (book == NULL) {
        BT_TRACE_FAIL("app.view_settings_row_draw", "fonts not loaded");
        BT_TRACE_EXIT("app.view_settings_row_draw");
        return;
    }
    bt_settings_app_state *app = bt_view_settings_app();
    bt_ui_input fallback;
    bt_ui_input_clear(&fallback);
    const bt_ui_input *input = bt_app_frame_input();
    if (input == NULL) {
        input = &fallback;
    }
    int changed = 0;
    int focus = 0;
    bt_view_settings_draw(&app->draft, &changed, &focus, &app->widgets, book->regular, book->bold,
                          (float)GetScreenWidth(), (float)GetScreenHeight(), input);
    if (changed) {
        app->changed = 1;
    }
    ctx->input_focus = focus;
    BT_TRACE_EXIT("app.view_settings_row_draw");
}
