#include "app/boot.h"
#include "app/views/frame.h"
#include "app/views/settings.h"
#include "app/views/state.h"
#include "common.h"

#include <stdio.h>
#include <string.h>

static bt_settings fixture_settings(void) {
    bt_settings settings;
    settings.memo_cadence_ms = 800;
    settings.memo_mode = BT_MEMO_SYLLABLES;
    settings.drill_reps = 4;
    return settings;
}

static void fixture_settings_ctx(const char *tag, bt_app_ctx *ctx) {
    bt_app_ctx_init(ctx);
    ctx->state = BT_APP_SETTINGS;
    bt_fsrs_config_defaults(&ctx->fsrs);
    snprintf(ctx->paths.user_root, sizeof ctx->paths.user_root, "%s",
             ta_temp_dir("view_settings", tag));
    char error[BT_APP_ERROR_MAX];
    if (bt_stores_load(ctx->paths.user_root, fixture_settings(), &ctx->fsrs, &ctx->stores, error,
                       sizeof error) != 0) {
        BT_ABORT("fixture stores: %s", error);
    }
    ctx->has_stores = 1;
}

static int rect_inside(const bt_ui_rect *inner, const bt_ui_rect *outer) {
    return inner->x >= outer->x && inner->y >= outer->y &&
           inner->x + inner->width <= outer->x + outer->width + 0.01F &&
           inner->y + inner->height <= outer->y + outer->height + 0.01F;
}

static int near(float value, float expected) {
    float diff = value - expected;
    if (diff < 0.0F) {
        diff = -diff;
    }
    return diff < 0.02F;
}

BT_TEST(view_settings, draft_round_trips_settings) {
    bt_settings settings;
    settings.memo_cadence_ms = 650;
    settings.memo_mode = BT_MEMO_IMAGES;
    settings.drill_reps = 8;
    bt_settings_draft draft;
    bt_view_settings_draft_from(&settings, &draft);
    bt_settings round;
    bt_view_settings_draft_apply(&draft, &round);
    BT_CHECK(round.memo_cadence_ms == 650);
    BT_CHECK(round.memo_mode == BT_MEMO_IMAGES);
    BT_CHECK(round.drill_reps == 8);
}

BT_TEST(view_settings, clamps_keep_values_usable) {
    BT_CHECK(bt_view_settings_clamp_cadence(0) == 1);
    BT_CHECK(bt_view_settings_clamp_cadence(500) == 500);
    BT_CHECK(bt_view_settings_clamp_cadence(100000) == 10000);
    BT_CHECK(bt_view_settings_clamp_reps(-3) == 1);
    BT_CHECK(bt_view_settings_clamp_reps(999) == 100);
}

BT_TEST(view_settings, mode_rows_cover_every_mode_in_label_order) {
    for (int row = 0; row < BT_MEMO_MODE_COUNT; row++) {
        bt_memo_mode mode;
        BT_CHECK(bt_memo_mode_from_index(row, &mode) == 1);
        BT_CHECK(bt_memo_mode_index(mode) == row);
        BT_CHECK(mode == BT_MEMO_MODE_LABELS[row].mode);
        BT_CHECK(strcmp(bt_memo_mode_label(mode), BT_MEMO_MODE_LABELS[row].label) == 0);
    }
    bt_memo_mode past;
    BT_CHECK(bt_memo_mode_from_index(BT_MEMO_MODE_COUNT, &past) == 0);
    BT_CHECK(bt_memo_mode_parse("per-orbit", &past) == 1 && past == BT_MEMO_PER_ORBIT);
}

BT_TEST(view_settings, labels_name_every_row_without_the_value) {
    char labels[BT_SETTINGS_ROW_COUNT][BT_SETTINGS_TEXT_MAX];
    bt_view_settings_labels(labels);
    BT_CHECK(strcmp(labels[0], "memo cadence (ms)") == 0);
    BT_CHECK(strcmp(labels[1], "memo mode") == 0);
    BT_CHECK(strcmp(labels[2], "drill reps") == 0);
}

BT_TEST(view_settings, every_row_carries_a_hint_and_the_mode_hint_names_the_keys) {
    char hints[BT_SETTINGS_ROW_COUNT][BT_SETTINGS_TEXT_MAX];
    bt_view_settings_hints(hints);
    for (int i = 0; i < BT_SETTINGS_ROW_COUNT; i++) {
        BT_CHECK(hints[i][0] != '\0');
    }
    BT_CHECK(strstr(hints[1], "1") != NULL);
    BT_CHECK(strstr(hints[1], "2") != NULL);
    BT_CHECK(strstr(hints[1], "3") != NULL);
}

BT_TEST(view_settings, layout_keeps_controls_inside_their_rows_right_aligned) {
    bt_view_settings_layout layout = bt_view_settings_layout_for(1280.0F, 800.0F);
    for (int i = 0; i < BT_SETTINGS_ROW_COUNT; i++) {
        BT_CHECK(layout.rows[i].width > 0.0F);
        BT_CHECK(layout.rows[i].height > 0.0F);
        BT_CHECK(rect_inside(&layout.controls[i], &layout.rows[i]));
        BT_CHECK(layout.controls[i].x + layout.controls[i].width ==
                 layout.rows[i].x + layout.rows[i].width);
        BT_CHECK(layout.controls[i].x > layout.rows[i].x);
    }
}

BT_TEST(view_settings, layout_rows_stack_evenly_in_a_centered_column) {
    bt_view_settings_layout layout = bt_view_settings_layout_for(1280.0F, 800.0F);
    bt_view_frame_metrics metrics = bt_view_frame_metrics_for(1280.0F, 800.0F);
    for (int i = 0; i < BT_SETTINGS_ROW_COUNT; i++) {
        BT_CHECK(layout.rows[i].height == layout.rows[0].height);
        BT_CHECK(layout.rows[i].width == layout.rows[0].width);
        BT_CHECK(rect_inside(&layout.rows[i], &(bt_ui_rect){metrics.content_x, metrics.content_y,
                                                            metrics.content_w, metrics.content_h}));
        if (i > 0) {
            BT_CHECK(layout.rows[i].y > layout.rows[i - 1].y + layout.rows[i - 1].height);
            BT_CHECK(layout.rows[i].x == layout.rows[i - 1].x);
        }
    }
    float left_gap = layout.rows[0].x - metrics.content_x;
    float right_gap =
        metrics.content_x + metrics.content_w - (layout.rows[0].x + layout.rows[0].width);
    BT_CHECK(left_gap == right_gap);
}

BT_TEST(view_settings, layout_hints_sit_under_the_label_and_left_of_the_control) {
    bt_view_settings_layout layout = bt_view_settings_layout_for(1280.0F, 800.0F);
    for (int i = 0; i < BT_SETTINGS_ROW_COUNT; i++) {
        BT_CHECK(rect_inside(&layout.hints[i], &layout.rows[i]));
        BT_CHECK(layout.hints[i].y > layout.rows[i].y);
        BT_CHECK(layout.hints[i].x + layout.hints[i].width <= layout.controls[i].x + 0.01F);
    }
}

BT_TEST(view_settings, layout_fits_the_small_probe_window_too) {
    bt_view_settings_layout layout = bt_view_settings_layout_for(640.0F, 480.0F);
    bt_view_frame_metrics metrics = bt_view_frame_metrics_for(640.0F, 480.0F);
    for (int i = 0; i < BT_SETTINGS_ROW_COUNT; i++) {
        BT_CHECK(rect_inside(&layout.rows[i], &(bt_ui_rect){metrics.content_x, metrics.content_y,
                                                            metrics.content_w, metrics.content_h}));
        BT_CHECK(rect_inside(&layout.controls[i], &layout.rows[i]));
    }
}

BT_TEST(view_settings, layout_pins_absolute_row_bands_and_control_edges) {
    bt_view_settings_layout layout = bt_view_settings_layout_for(1280.0F, 800.0F);
    float row_tops[BT_SETTINGS_ROW_COUNT] = {284.5F, 393.7F, 502.9F};
    for (int i = 0; i < BT_SETTINGS_ROW_COUNT; i++) {
        BT_CHECK(near(layout.rows[i].y, row_tops[i]));
        BT_CHECK(near(layout.rows[i].height, 89.6F));
        BT_CHECK(near(layout.controls[i].y, row_tops[i] + 8.4F));
        BT_CHECK(near(layout.controls[i].height, 72.8F));
        BT_CHECK(near(layout.controls[i].x + layout.controls[i].width, 1263.2F));
        BT_CHECK(near(layout.controls[i].width, 392.0F));
    }
}

BT_TEST(view_settings, layout_fills_the_content_width_with_full_width_rows) {
    bt_view_settings_layout layout = bt_view_settings_layout_for(1280.0F, 800.0F);
    bt_view_frame_metrics metrics = bt_view_frame_metrics_for(1280.0F, 800.0F);
    BT_CHECK(near(layout.rows[0].x, metrics.content_x));
    BT_CHECK(near(layout.rows[0].width, metrics.content_w));
}

BT_TEST(view_settings, layout_keeps_the_open_select_inside_the_small_window) {
    bt_view_settings_layout layout = bt_view_settings_layout_for(640.0F, 480.0F);
    for (int i = 0; i < BT_MEMO_MODE_COUNT; i++) {
        bt_ui_rect slot = bt_ui_select_slot(&layout.controls[1], BT_MEMO_MODE_COUNT, i, 480.0F);
        BT_CHECK(slot.y >= 0.0F);
        BT_CHECK(slot.y + slot.height <= 480.0F);
    }
}

BT_TEST(view_settings, enter_state_resets_the_widget_state) {
    bt_app_ctx ctx;
    fixture_settings_ctx("widget-reset", &ctx);
    bt_settings_app_state *app = bt_view_settings_app();
    bt_view_settings_app_init(app);
    bt_view_settings_enter_state(&ctx, app);
    app->widgets.mode_select.open = 1;
    app->widgets.mode_select.cursor = 2;
    app->widgets.cadence.focused = 1;
    app->widgets.reps.focused = 1;
    bt_view_settings_enter_state(&ctx, app);
    BT_CHECK(app->widgets.mode_select.open == 0);
    BT_CHECK(app->widgets.cadence.focused == 0);
    BT_CHECK(app->widgets.reps.focused == 0);
    bt_app_ctx_free(&ctx);
}

BT_TEST(view_settings, select_starts_collapsed_and_opens_on_its_mode_key) {
    bt_app_ctx ctx;
    fixture_settings_ctx("select-keys", &ctx);
    bt_settings_app_state *app = bt_view_settings_app();
    bt_view_settings_app_init(app);
    bt_view_settings_enter_state(&ctx, app);
    BT_CHECK(app->widgets.mode_select.open == 0);
    bt_view_settings_key(&ctx, app, '2');
    bt_view_settings_update_state(&ctx, app);
    BT_CHECK(app->draft.memo_mode == BT_MEMO_IMAGES);
    BT_CHECK(ctx.stores.settings.memo_mode == BT_MEMO_IMAGES);
    BT_CHECK(app->widgets.mode_select.open == 1);
    BT_CHECK(app->widgets.mode_select.cursor == 1);
    bt_view_settings_key(&ctx, app, '3');
    BT_CHECK(app->draft.memo_mode == BT_MEMO_IMAGES);
    BT_CHECK(app->widgets.mode_select.cursor == 2);
    BT_CHECK(app->widgets.mode_select.open == 1);
    bt_app_ctx_free(&ctx);
}

BT_TEST(view_settings, mode_keys_stay_guarded_while_a_number_field_holds_focus) {
    bt_app_ctx ctx;
    fixture_settings_ctx("focus-guard", &ctx);
    bt_settings_app_state *app = bt_view_settings_app();
    bt_view_settings_app_init(app);
    bt_view_settings_enter_state(&ctx, app);
    ctx.input_focus = 1;
    bt_view_settings_key(&ctx, app, '2');
    BT_CHECK(app->draft.memo_mode == BT_MEMO_SYLLABLES);
    BT_CHECK(app->widgets.mode_select.open == 0);
    bt_app_ctx_free(&ctx);
}
