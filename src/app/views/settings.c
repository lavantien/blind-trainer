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

void bt_view_settings_lines(const bt_settings_draft *draft,
                            char out[BT_SETTINGS_LINE_COUNT][BT_SETTINGS_TEXT_MAX]) {
    BT_TRACE_ENTER("app.view_settings_lines");
    snprintf(out[0], BT_SETTINGS_TEXT_MAX, "memo cadence %u ms", draft->memo_cadence_ms);
    snprintf(out[1], BT_SETTINGS_TEXT_MAX, "memo mode %s", bt_memo_mode_label(draft->memo_mode));
    snprintf(out[2], BT_SETTINGS_TEXT_MAX, "drill reps %u", draft->drill_reps);
    snprintf(out[3], BT_SETTINGS_TEXT_MAX, "%s",
             "1 syllables  2 images  3 per-orbit  edits save "
             "immediately");
    BT_TRACE_EXIT("app.view_settings_lines");
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
    BT_TRACE_EXIT("app.view_settings_enter_state");
}

void bt_view_settings_key(bt_app_ctx *ctx, bt_settings_app_state *app, int key) {
    BT_TRACE_ENTER("app.view_settings_key");
    if (ctx->input_focus) {
        BT_TRACE_EXIT("app.view_settings_key");
        return;
    }
    bt_memo_mode mode;
    int index = key - '1';
    if (index < 0 || index >= BT_MEMO_MODE_COUNT || !bt_memo_mode_from_index(index, &mode)) {
        BT_TRACE_EXIT("app.view_settings_key");
        return;
    }
    app->draft.memo_mode = mode;
    app->changed = 1;
    BT_TRACE_EXIT("app.view_settings_key");
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
    for (int i = 0; i < BT_MEMO_MODE_COUNT; i++) {
        if (IsKeyPressed(keys[i])) {
            bt_view_settings_key(ctx, app, keys[i]);
        }
    }
    bt_view_settings_update_state(ctx, app);
    BT_TRACE_EXIT("app.view_settings_update");
}

void bt_view_settings_draw(bt_settings_draft *draft, int *changed, const void *font_regular,
                           const void *font_bold, float window_w, float window_h,
                           const bt_ui_input *input) {
    BT_TRACE_ENTER("app.view_settings_draw");
    const bt_theme *theme = bt_theme_table();
    bt_view_frame_metrics metrics = bt_view_frame_metrics_for(window_w, window_h);
    bt_view_frame_draw(font_bold, font_regular, window_w, window_h, "settings", "esc menu");
    char lines[BT_SETTINGS_LINE_COUNT][BT_SETTINGS_TEXT_MAX];
    bt_view_settings_lines(draft, lines);
    float body = theme->font_body * theme->root_rem;
    float line_gap = theme->spacing_rem * 1.0F * theme->root_rem;
    float total_h =
        body * (float)BT_SETTINGS_LINE_COUNT + line_gap * (float)(BT_SETTINGS_LINE_COUNT - 1);
    float y = metrics.content_y + (metrics.content_h - total_h) / 2.0F;
    for (size_t i = 0; i < BT_SETTINGS_LINE_COUNT; i++) {
        bt_app_draw_text(font_regular, lines[i], metrics.content_x, y, body,
                         bt_theme_text_primary());
        y += body + line_gap;
    }
    Font regular = *(const Font *)font_regular;
    float field_h = body * 1.4F;
    float field_w = 160.0F;
    float field_y = metrics.content_y + metrics.content_h - 2.0F * field_h - line_gap;
    bt_ui_rect cadence_rect = {
        metrics.content_x + metrics.content_w - 2.0F * field_w - line_gap,
        field_y,
        field_w,
        field_h,
    };
    bt_ui_rect reps_rect = {cadence_rect.x + field_w + line_gap, field_y, field_w, field_h};
    bt_ui_rect select_rect = {
        cadence_rect.x,
        field_y + field_h + line_gap,
        metrics.content_w - (cadence_rect.x - metrics.content_x),
        field_h,
    };
    int edited = 0;
    long long cadence =
        bt_ui_number_field(&regular, &cadence_rect, (long long)draft->memo_cadence_ms,
                           BT_SETTINGS_CADENCE_MIN, BT_SETTINGS_CADENCE_MAX, input, &edited);
    if (edited) {
        unsigned int clamped = bt_view_settings_clamp_cadence(cadence);
        if (clamped != draft->memo_cadence_ms) {
            draft->memo_cadence_ms = clamped;
            *changed = 1;
        }
    }
    long long reps = bt_ui_number_field(&regular, &reps_rect, (long long)draft->drill_reps,
                                        BT_SETTINGS_REPS_MIN, BT_SETTINGS_REPS_MAX, input, &edited);
    if (edited) {
        unsigned int clamped = bt_view_settings_clamp_reps(reps);
        if (clamped != draft->drill_reps) {
            draft->drill_reps = clamped;
            *changed = 1;
        }
    }
    const char *mode_rows[BT_MEMO_MODE_COUNT];
    for (int i = 0; i < BT_MEMO_MODE_COUNT; i++) {
        mode_rows[i] = BT_MEMO_MODE_LABELS[i].label;
    }
    bt_ui_list_window window;
    memset(&window, 0, sizeof window);
    window.selected = bt_memo_mode_index(draft->memo_mode);
    bt_ui_list_window picked = bt_ui_list(&regular, &select_rect, mode_rows, BT_MEMO_MODE_COUNT,
                                          BT_MEMO_MODE_COUNT, window, input);
    if (picked.selected != window.selected) {
        bt_memo_mode mode;
        if (bt_memo_mode_from_index(picked.selected, &mode)) {
            draft->memo_mode = mode;
            *changed = 1;
        }
    }
    BT_TRACE_EXIT("app.view_settings_draw");
}

void bt_view_settings_row_draw(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.view_settings_row_draw");
    (void)ctx;
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
    bt_view_settings_draw(&app->draft, &changed, book->regular, book->bold, (float)GetScreenWidth(),
                          (float)GetScreenHeight(), input);
    if (changed) {
        app->changed = 1;
    }
    BT_TRACE_EXIT("app.view_settings_row_draw");
}
