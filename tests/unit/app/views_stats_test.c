#include "app/boot.h"
#include "app/views/frame.h"
#include "app/views/settings.h"
#include "app/views/state.h"
#include "app/views/stats.h"
#include "app/window.h"
#include "common.h"

#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    PROBE_W = 640,
    PROBE_H = 480,
    DWELL_FRAMES = 6,
};

static bt_settings default_settings(void) {
    bt_settings settings;
    settings.memo_cadence_ms = 800;
    settings.memo_mode = BT_MEMO_SYLLABLES;
    settings.drill_reps = 4;
    return settings;
}

static void fixture_ctx(const char *tag, bt_app_state state, bt_app_ctx *ctx) {
    bt_app_ctx_init(ctx);
    ctx->state = state;
    bt_fsrs_config_defaults(&ctx->fsrs);
    snprintf(ctx->paths.user_root, sizeof ctx->paths.user_root, "%s",
             ta_temp_dir("views_stats", tag));
    char error[BT_APP_ERROR_MAX];
    if (bt_stores_load(ctx->paths.user_root, default_settings(), &ctx->fsrs, &ctx->stores, error,
                       sizeof error) != 0) {
        BT_ABORT("fixture stores: %s", error);
    }
    ctx->has_stores = 1;
}

BT_TEST(views_stats, stats_aggregate_seeded_solve_and_srs_fixtures) {
    bt_app_ctx ctx;
    fixture_ctx("aggregate", BT_APP_STATS, &ctx);
    char root[TA_PATH_MAX];
    snprintf(root, sizeof root, "%s", ctx.paths.user_root);
    char solves_path[TA_PATH_MAX];
    snprintf(solves_path, sizeof solves_path, "%s/solves/solves.json", root);
    ta_write_file(solves_path,
                  "{\"solves\": ["
                  "{\"started_ms\": 1, \"memo_ms\": 20000, \"exec_ms\": 10000, \"success\": true}, "
                  "{\"started_ms\": 2, \"memo_ms\": 30000, \"exec_ms\": 20000, \"success\": false}"
                  "]}");
    char srs_path[TA_PATH_MAX];
    snprintf(srs_path, sizeof srs_path, "%s/srs/srs.json", root);
    ta_write_file(
        srs_path,
        "{\"cards\": {"
        "\"edge/UF-UB-UR\": {\"ease\": 2.5, \"interval_days\": 1, \"due_day\": 0, "
        "\"reps\": 2, \"lapses\": 0, \"reviews\": ["
        "{\"ts_days\": 100, \"quality\": 5, \"elapsed_days\": 0, \"scheduled_days\": 1}, "
        "{\"ts_days\": 101, \"quality\": 4, \"elapsed_days\": 1, \"scheduled_days\": 1}]}, "
        "\"corner/UFR-UBL-UBR\": {\"ease\": 2.5, \"interval_days\": 1, \"due_day\": 99999, "
        "\"reps\": 1, \"lapses\": 0, \"reviews\": ["
        "{\"ts_days\": 100, \"quality\": 2, \"elapsed_days\": 0, \"scheduled_days\": 1}]}"
        "}}");
    char error[BT_APP_ERROR_MAX];
    bt_stores_free(&ctx.stores);
    if (bt_stores_load(root, default_settings(), &ctx.fsrs, &ctx.stores, error, sizeof error) !=
        0) {
        BT_ABORT("reload stores: %s", error);
    }
    bt_stats_app_state *app = bt_view_stats_app();
    bt_view_stats_app_init(app);
    bt_view_stats_enter_state(&ctx, app);
    BT_CHECK(app->snap.solves == 2);
    BT_CHECK(app->snap.memo_mean_ms == 25000.0);
    BT_CHECK(app->snap.memo_median_ms == 25000.0);
    BT_CHECK(app->snap.exec_mean_ms == 15000.0);
    BT_CHECK(app->snap.exec_median_ms == 15000.0);
    BT_CHECK(app->snap.due_edge == 1);
    BT_CHECK(app->snap.due_corner == 0);
    BT_CHECK(app->snap.streak == 2);
    size_t count = 0;
    bt_stats_block *blocks = bt_view_stats_blocks(&app->snap, &count);
    BT_CHECK(blocks != NULL && count == 8);
    const char *values[8] = {"2", "2", "25.0s", "25.0s", "15.0s", "15.0s", "1", "0"};
    for (size_t i = 0; i < 8; i++) {
        BT_CHECK(strcmp(blocks[i].value, values[i]) == 0);
    }
    int has_streak = 0;
    int has_edge = 0;
    int has_corner = 0;
    for (size_t i = 0; i < count; i++) {
        has_streak = has_streak || strcmp(blocks[i].label, "verified drill streak") == 0;
        has_edge = has_edge || strcmp(blocks[i].label, "due edge") == 0;
        has_corner = has_corner || strcmp(blocks[i].label, "due corner") == 0;
    }
    BT_CHECK(has_streak && has_edge && has_corner);
    bt_view_stats_blocks_free(blocks);
    bt_app_ctx_free(&ctx);
}

BT_TEST(views_stats, stats_due_events_read_the_case_card_repo) {
    bt_app_ctx ctx;
    fixture_ctx("due-events", BT_APP_STATS, &ctx);
    char db_path[BT_PATH_MAX];
    bt_data_db_path_at(ctx.paths.user_root, db_path);
    bt_db db;
    bt_db_error db_err;
    if (bt_db_open_at(db_path, &db, &db_err) != 0) {
        char text[BT_PATH_MAX];
        bt_db_error_string(&db_err, text, sizeof text);
        BT_ABORT("fixture db: %s", text);
    }
    bt_card fresh;
    bt_card_init(&fresh, 0);
    bt_card scheduled;
    bt_card_init(&scheduled, 0);
    scheduled.due_day = 999999;
    const char *keys[2] = {"cll.1", "cll.2"};
    for (int i = 0; i < 2; i++) {
        bt_case_ref ref;
        bt_case_ref_of(&ref, "2x2", "cll", "cll", keys[i]);
        if (bt_repo_cards_save_card(&db, &ref, i == 0 ? &fresh : &scheduled, &db_err) != 0) {
            char text[BT_PATH_MAX];
            bt_db_error_string(&db_err, text, sizeof text);
            BT_ABORT("seed card: %s", text);
        }
    }
    bt_db_close(&db);
    bt_stats_app_state *app = bt_view_stats_app();
    bt_view_stats_app_init(app);
    bt_view_stats_enter_state(&ctx, app);
    BT_CHECK(app->snap.due_event_count == 1);
    BT_CHECK(strcmp(app->snap.due_events[0].event, "2x2") == 0);
    BT_CHECK(app->snap.due_events[0].due == 1);
    size_t count = 0;
    bt_stats_block *blocks = bt_view_stats_blocks(&app->snap, &count);
    BT_CHECK(blocks != NULL && count == 9);
    BT_CHECK(strcmp(blocks[8].label, "due 2x2") == 0);
    bt_view_stats_blocks_free(blocks);
    bt_app_ctx_free(&ctx);
}

BT_TEST(views_stats, settings_change_persists_across_store_reload) {
    bt_app_ctx ctx;
    fixture_ctx("persist", BT_APP_SETTINGS, &ctx);
    char root[TA_PATH_MAX];
    snprintf(root, sizeof root, "%s", ctx.paths.user_root);
    bt_settings_app_state *app = bt_view_settings_app();
    bt_view_settings_app_init(app);
    bt_view_settings_enter_state(&ctx, app);
    BT_CHECK(app->draft.memo_cadence_ms == 800);
    BT_CHECK(app->draft.memo_mode == BT_MEMO_SYLLABLES);
    BT_CHECK(app->draft.drill_reps == 4);
    app->draft.memo_cadence_ms = 1234;
    app->draft.memo_mode = BT_MEMO_IMAGES;
    app->draft.drill_reps = 9;
    app->changed = 1;
    bt_view_settings_update_state(&ctx, app);
    BT_CHECK(ctx.stores.settings.memo_cadence_ms == 1234);
    BT_CHECK(ctx.stores.settings.memo_mode == BT_MEMO_IMAGES);
    BT_CHECK(ctx.stores.settings.drill_reps == 9);
    char db_path[BT_PATH_MAX];
    bt_data_db_path_at(root, db_path);
    BT_CHECK(fopen(db_path, "rb") != NULL);
    bt_app_ctx_free(&ctx);
    bt_stores fresh;
    char error[BT_APP_ERROR_MAX];
    if (bt_stores_load(root, default_settings(), NULL, &fresh, error, sizeof error) != 0) {
        BT_ABORT("reload: %s", error);
    }
    BT_CHECK(fresh.settings.memo_cadence_ms == 1234);
    BT_CHECK(fresh.settings.memo_mode == BT_MEMO_IMAGES);
    BT_CHECK(fresh.settings.drill_reps == 9);
    bt_stores_free(&fresh);
}

BT_TEST(views_stats, settings_mode_keys_toggle_the_draft) {
    bt_app_ctx ctx;
    fixture_ctx("mode-keys", BT_APP_SETTINGS, &ctx);
    bt_settings_app_state *app = bt_view_settings_app();
    bt_view_settings_app_init(app);
    bt_view_settings_enter_state(&ctx, app);
    bt_view_settings_key(&ctx, app, '2');
    bt_view_settings_update_state(&ctx, app);
    BT_CHECK(app->draft.memo_mode == BT_MEMO_IMAGES);
    BT_CHECK(ctx.stores.settings.memo_mode == BT_MEMO_IMAGES);
    BT_CHECK(app->widgets.mode_select.open == 1);
    bt_view_settings_key(&ctx, app, '1');
    BT_CHECK(app->draft.memo_mode == BT_MEMO_IMAGES);
    BT_CHECK(app->widgets.mode_select.cursor == 0);
    app->widgets.mode_select.open = 0;
    bt_view_settings_key(&ctx, app, '1');
    bt_view_settings_update_state(&ctx, app);
    BT_CHECK(app->draft.memo_mode == BT_MEMO_SYLLABLES);
    BT_CHECK(ctx.stores.settings.memo_mode == BT_MEMO_SYLLABLES);
    ctx.input_focus = 1;
    bt_view_settings_key(&ctx, app, '2');
    BT_CHECK(app->draft.memo_mode == BT_MEMO_SYLLABLES);
    bt_app_ctx_free(&ctx);
}

static int shot_has_variance(const char *path) {
    Image image = LoadImage(path);
    if (image.width <= 0 || image.height <= 0) {
        UnloadImage(image);
        return 0;
    }
    Color *pixels = LoadImageColors(image);
    long long variance = 0;
    for (int y = 0; y < image.height; y++) {
        for (int x = 1; x < image.width; x++) {
            Color left = pixels[y * image.width + x - 1];
            Color right = pixels[y * image.width + x];
            variance += left.r != right.r || left.g != right.g || left.b != right.b;
        }
    }
    UnloadImageColors(pixels);
    UnloadImage(image);
    return variance > 0;
}

BT_TEST(views_stats, stats_draw_renders_blocks_under_real_fonts) {
    if (!bt_app_window_open_driven(PROBE_W, PROBE_H, "views stats test", NULL)) {
        BT_CHECK(0);
        return;
    }
    bt_app_font_book book;
    char error[BT_APP_ERROR_MAX];
    char config_path[BT_PATH_MAX];
    bt_data_config_path(config_path);
    if (bt_app_fonts_load(config_path, NULL, NULL, 0, &book, error, sizeof error) != 0) {
        bt_app_window_close();
        BT_ABORT("fonts: %s", error);
    }
    bt_stats_snapshot snap;
    memset(&snap, 0, sizeof snap);
    snap.solves = 2;
    snap.streak = 4;
    snap.memo_mean_ms = 25000.0;
    snap.memo_median_ms = 25000.0;
    snap.exec_mean_ms = 15000.0;
    snap.exec_median_ms = 15000.0;
    snap.due_edge = 1;
    snap.due_corner = 2;
    for (unsigned frame = 0; frame < DWELL_FRAMES; frame++) {
        bt_app_window_frame_begin();
        ClearBackground(bt_app_color(bt_theme_background()));
        bt_view_stats_draw(&snap, book.regular, book.bold, (float)PROBE_W, (float)PROBE_H);
        bt_app_window_frame_end();
    }
    char shot[BT_PATH_MAX];
    snprintf(shot, sizeof shot, "%s/app-views-stats-%d.png", getenv("TEMP"), _getpid());
    remove(shot);
    BT_CHECK(bt_app_window_capture(shot));
    BT_CHECK(shot_has_variance(shot));
    remove(shot);
    bt_app_fonts_unload(&book);
    bt_app_window_close();
}

BT_TEST(views_stats, settings_draw_edits_the_draft_from_widget_input) {
    if (!bt_app_window_open_driven(PROBE_W, PROBE_H, "views settings test", NULL)) {
        BT_CHECK(0);
        return;
    }
    bt_app_font_book book;
    char error[BT_APP_ERROR_MAX];
    char config_path[BT_PATH_MAX];
    bt_data_config_path(config_path);
    if (bt_app_fonts_load(config_path, NULL, NULL, 0, &book, error, sizeof error) != 0) {
        bt_app_window_close();
        BT_ABORT("fonts: %s", error);
    }
    bt_view_settings_layout layout = bt_view_settings_layout_for((float)PROBE_W, (float)PROBE_H);
    bt_settings_draft draft;
    draft.memo_cadence_ms = 800;
    draft.memo_mode = BT_MEMO_SYLLABLES;
    draft.drill_reps = 6;
    bt_ui_input input;
    bt_ui_input_clear(&input);
    input.x = layout.controls[2].x + layout.controls[2].width - layout.controls[2].height / 2.0F;
    input.y = layout.controls[2].y + layout.controls[2].height / 2.0F;
    int changed = 0;
    int focus = 0;
    bt_settings_widgets widgets;
    memset(&widgets, 0, sizeof widgets);
    for (unsigned frame = 0; frame < DWELL_FRAMES; frame++) {
        input.click = frame == 0;
        bt_app_window_frame_begin();
        ClearBackground(bt_app_color(bt_theme_background()));
        bt_view_settings_draw(&draft, &changed, &focus, &widgets, book.regular, book.bold,
                              (float)PROBE_W, (float)PROBE_H, &input);
        bt_app_window_frame_end();
    }
    BT_CHECK(changed == 1);
    BT_CHECK(draft.drill_reps == 7);
    bt_ui_input open;
    bt_ui_input_clear(&open);
    open.x = layout.controls[1].x + layout.controls[1].width / 2.0F;
    open.y = layout.controls[1].y + layout.controls[1].height / 2.0F;
    for (unsigned frame = 0; frame < DWELL_FRAMES; frame++) {
        open.click = frame == 0;
        bt_app_window_frame_begin();
        ClearBackground(bt_app_color(bt_theme_background()));
        bt_view_settings_draw(&draft, &changed, &focus, &widgets, book.regular, book.bold,
                              (float)PROBE_W, (float)PROBE_H, &open);
        bt_app_window_frame_end();
    }
    BT_CHECK(widgets.mode_select.open == 1);
    changed = 0;
    bt_ui_input pick;
    bt_ui_input_clear(&pick);
    bt_ui_rect pick_slot =
        bt_ui_select_slot(&layout.controls[1], BT_MEMO_MODE_COUNT, 2, (float)PROBE_H);
    pick.x = pick_slot.x + pick_slot.width / 2.0F;
    pick.y = pick_slot.y + pick_slot.height / 2.0F;
    for (unsigned frame = 0; frame < DWELL_FRAMES; frame++) {
        pick.click = frame == 0;
        bt_app_window_frame_begin();
        ClearBackground(bt_app_color(bt_theme_background()));
        bt_view_settings_draw(&draft, &changed, &focus, &widgets, book.regular, book.bold,
                              (float)PROBE_W, (float)PROBE_H, &pick);
        bt_app_window_frame_end();
    }
    BT_CHECK(changed == 1);
    BT_CHECK(draft.memo_mode == BT_MEMO_PER_ORBIT);
    BT_CHECK(widgets.mode_select.open == 0);
    char shot[BT_PATH_MAX];
    snprintf(shot, sizeof shot, "%s/app-views-settings-%d.png", getenv("TEMP"), _getpid());
    remove(shot);
    BT_CHECK(bt_app_window_capture(shot));
    BT_CHECK(shot_has_variance(shot));
    remove(shot);
    bt_app_fonts_unload(&book);
    bt_app_window_close();
}

BT_TEST(views_stats, settings_mode_keys_apply_the_first_match_only) {
    bt_app_ctx ctx;
    fixture_ctx("first-match", BT_APP_SETTINGS, &ctx);
    bt_settings_app_state *app = bt_view_settings_app();
    bt_view_settings_app_init(app);
    bt_view_settings_enter_state(&ctx, app);
    int keys[2] = {'2', '1'};
    bt_view_settings_keys(&ctx, app, keys, 2);
    BT_CHECK(app->draft.memo_mode == BT_MEMO_IMAGES);
    bt_view_settings_update_state(&ctx, app);
    BT_CHECK(ctx.stores.settings.memo_mode == BT_MEMO_IMAGES);
    bt_app_ctx_free(&ctx);
}

static int draw_settings_frames(bt_settings_draft *draft, int *changed, int *focus,
                                bt_settings_widgets *widgets, const bt_app_font_book *book,
                                float window_w, float window_h, bt_ui_input *input) {
    for (unsigned frame = 0; frame < DWELL_FRAMES; frame++) {
        input->click = frame == 0;
        bt_app_window_frame_begin();
        ClearBackground(bt_app_color(bt_theme_background()));
        bt_view_settings_draw(draft, changed, focus, widgets, book->regular, book->bold, window_w,
                              window_h, input);
        bt_app_window_frame_end();
    }
    return 0;
}

BT_TEST(views_stats, open_select_consumes_the_overlap_band_before_the_reps_field) {
    if (!bt_app_window_open_driven(1280, 800, "views select modal test", NULL)) {
        BT_CHECK(0);
        return;
    }
    bt_app_font_book book;
    char error[BT_APP_ERROR_MAX];
    char config_path[BT_PATH_MAX];
    bt_data_config_path(config_path);
    if (bt_app_fonts_load(config_path, NULL, NULL, 0, &book, error, sizeof error) != 0) {
        bt_app_window_close();
        BT_ABORT("fonts: %s", error);
    }
    bt_view_settings_layout layout = bt_view_settings_layout_for(1280.0F, 800.0F);
    bt_ui_rect reps = layout.controls[2];
    bt_ui_rect slot0 = bt_ui_select_slot(&layout.controls[1], BT_MEMO_MODE_COUNT, 0, 800.0F);
    BT_CHECK(slot0.y < reps.y + reps.height);
    BT_CHECK(slot0.y + slot0.height > reps.y);
    bt_settings_draft draft;
    draft.memo_cadence_ms = 800;
    draft.memo_mode = BT_MEMO_IMAGES;
    draft.drill_reps = 100;
    int changed = 0;
    int focus = 0;
    bt_settings_widgets widgets;
    memset(&widgets, 0, sizeof widgets);
    bt_ui_input input;
    bt_ui_input_clear(&input);
    input.x = layout.controls[1].x + layout.controls[1].width / 2.0F;
    input.y = layout.controls[1].y + layout.controls[1].height / 2.0F;
    draw_settings_frames(&draft, &changed, &focus, &widgets, &book, 1280.0F, 800.0F, &input);
    BT_CHECK(widgets.mode_select.open == 1);
    changed = 0;
    bt_ui_input pick;
    bt_ui_input_clear(&pick);
    pick.x = layout.controls[1].x + layout.controls[1].width / 2.0F;
    pick.y = (slot0.y > reps.y ? slot0.y : reps.y) + 5.0F;
    draw_settings_frames(&draft, &changed, &focus, &widgets, &book, 1280.0F, 800.0F, &pick);
    BT_CHECK(changed == 1);
    BT_CHECK(draft.memo_mode == BT_MEMO_SYLLABLES);
    BT_CHECK(widgets.mode_select.open == 0);
    BT_CHECK(widgets.reps.focused == 0);
    BT_CHECK(draft.drill_reps == 100);
    widgets.mode_select.open = 1;
    bt_ui_input stepper;
    bt_ui_input_clear(&stepper);
    stepper.x = reps.x + reps.height / 2.0F;
    stepper.y = (slot0.y > reps.y ? slot0.y : reps.y) + 5.0F;
    draw_settings_frames(&draft, &changed, &focus, &widgets, &book, 1280.0F, 800.0F, &stepper);
    BT_CHECK(draft.drill_reps == 100);
    BT_CHECK(draft.memo_mode == BT_MEMO_SYLLABLES);
    bt_app_fonts_unload(&book);
    bt_app_window_close();
}

BT_TEST(views_stats, transition_frame_clears_the_published_input) {
    bt_app_ctx ctx;
    fixture_ctx("transition", BT_APP_MENU, &ctx);
    bt_app_request_state(&ctx, BT_APP_SETTINGS);
    bt_ui_input input;
    bt_ui_input_clear(&input);
    input.click = 1;
    bt_app_frame_input_set(&input);
    int transitioned = bt_app_transition_frame(&ctx);
    BT_CHECK(transitioned == 1);
    BT_CHECK(ctx.state == BT_APP_SETTINGS);
    BT_CHECK(bt_app_frame_input() == NULL);
    bt_app_ctx_free(&ctx);
}

BT_TEST(views_stats, settings_number_field_typed_entry_through_the_seam) {
    if (!bt_app_window_open_driven(PROBE_W, PROBE_H, "views settings typing test", NULL)) {
        BT_CHECK(0);
        return;
    }
    bt_app_font_book book;
    char error[BT_APP_ERROR_MAX];
    char config_path[BT_PATH_MAX];
    bt_data_config_path(config_path);
    if (bt_app_fonts_load(config_path, NULL, NULL, 0, &book, error, sizeof error) != 0) {
        bt_app_window_close();
        BT_ABORT("fonts: %s", error);
    }
    bt_view_settings_layout layout = bt_view_settings_layout_for((float)PROBE_W, (float)PROBE_H);
    bt_app_ctx ctx;
    fixture_ctx("typing", BT_APP_SETTINGS, &ctx);
    bt_settings_app_state *app = bt_view_settings_app();
    bt_view_settings_app_init(app);
    bt_view_settings_enter_state(&ctx, app);
    BT_CHECK(app->draft.drill_reps == 4);
    bt_ui_input focus;
    bt_ui_input_clear(&focus);
    focus.x = layout.controls[2].x + layout.controls[2].width / 2.0F;
    focus.y = layout.controls[2].y + layout.controls[2].height / 2.0F;
    focus.click = 1;
    bt_app_frame_input_set(&focus);
    bt_app_window_frame_begin();
    ClearBackground(bt_app_color(bt_theme_background()));
    bt_view_settings_row_draw(&ctx);
    bt_app_window_frame_end();
    bt_app_frame_input_set(NULL);
    BT_CHECK(ctx.input_focus == 1);
    BT_CHECK(app->draft.drill_reps == 4);
    bt_view_settings_key(&ctx, app, '2');
    BT_CHECK(app->draft.memo_mode == BT_MEMO_SYLLABLES);
    bt_ui_input typed;
    bt_ui_input_clear(&typed);
    typed.chars[0] = '1';
    typed.chars[1] = '2';
    typed.char_count = 2;
    bt_app_frame_input_set(&typed);
    bt_app_window_frame_begin();
    ClearBackground(bt_app_color(bt_theme_background()));
    bt_view_settings_row_draw(&ctx);
    bt_app_window_frame_end();
    bt_app_frame_input_set(NULL);
    BT_CHECK(app->draft.drill_reps == 12);
    BT_CHECK(app->changed == 1);
    BT_CHECK(app->draft.memo_mode == BT_MEMO_SYLLABLES);
    bt_ui_input blur;
    bt_ui_input_clear(&blur);
    blur.x = layout.rows[0].x + 10.0F;
    blur.y = layout.rows[0].y + 10.0F;
    blur.click = 1;
    bt_app_frame_input_set(&blur);
    bt_app_window_frame_begin();
    ClearBackground(bt_app_color(bt_theme_background()));
    bt_view_settings_row_draw(&ctx);
    bt_app_window_frame_end();
    bt_app_frame_input_set(NULL);
    BT_CHECK(ctx.input_focus == 0);
    BT_CHECK(app->draft.drill_reps == 12);
    bt_app_ctx_free(&ctx);
    bt_app_fonts_unload(&book);
    bt_app_window_close();
}

BT_TEST(views_stats, settings_row_adapter_consumes_the_published_frame_input) {
    if (!bt_app_window_open_driven(PROBE_W, PROBE_H, "views settings seam test", NULL)) {
        BT_CHECK(0);
        return;
    }
    bt_app_font_book book;
    char error[BT_APP_ERROR_MAX];
    char config_path[BT_PATH_MAX];
    bt_data_config_path(config_path);
    if (bt_app_fonts_load(config_path, NULL, NULL, 0, &book, error, sizeof error) != 0) {
        bt_app_window_close();
        BT_ABORT("fonts: %s", error);
    }
    bt_view_settings_layout layout = bt_view_settings_layout_for((float)PROBE_W, (float)PROBE_H);
    bt_app_ctx ctx;
    fixture_ctx("seam", BT_APP_SETTINGS, &ctx);
    bt_settings_app_state *app = bt_view_settings_app();
    bt_view_settings_app_init(app);
    bt_view_settings_enter_state(&ctx, app);
    BT_CHECK(app->draft.drill_reps == 4);
    bt_ui_input input;
    bt_ui_input_clear(&input);
    input.x = layout.controls[2].x + layout.controls[2].width - layout.controls[2].height / 2.0F;
    input.y = layout.controls[2].y + layout.controls[2].height / 2.0F;
    for (unsigned frame = 0; frame < DWELL_FRAMES; frame++) {
        input.click = frame == 0;
        bt_app_frame_input_set(&input);
        bt_app_window_frame_begin();
        ClearBackground(bt_app_color(bt_theme_background()));
        bt_view_settings_row_draw(&ctx);
        bt_app_window_frame_end();
        bt_app_frame_input_set(NULL);
    }
    BT_CHECK(app->draft.drill_reps == 5);
    BT_CHECK(app->changed == 1);
    bt_ui_input open;
    bt_ui_input_clear(&open);
    open.x = layout.controls[1].x + layout.controls[1].width / 2.0F;
    open.y = layout.controls[1].y + layout.controls[1].height / 2.0F;
    for (unsigned frame = 0; frame < DWELL_FRAMES; frame++) {
        open.click = frame == 0;
        bt_app_frame_input_set(&open);
        bt_app_window_frame_begin();
        ClearBackground(bt_app_color(bt_theme_background()));
        bt_view_settings_row_draw(&ctx);
        bt_app_window_frame_end();
        bt_app_frame_input_set(NULL);
    }
    BT_CHECK(app->widgets.mode_select.open == 1);
    bt_ui_input pick;
    bt_ui_input_clear(&pick);
    bt_ui_rect pick_slot =
        bt_ui_select_slot(&layout.controls[1], BT_MEMO_MODE_COUNT, 2, (float)PROBE_H);
    pick.x = pick_slot.x + pick_slot.width / 2.0F;
    pick.y = pick_slot.y + pick_slot.height / 2.0F;
    for (unsigned frame = 0; frame < DWELL_FRAMES; frame++) {
        pick.click = frame == 0;
        bt_app_frame_input_set(&pick);
        bt_app_window_frame_begin();
        ClearBackground(bt_app_color(bt_theme_background()));
        bt_view_settings_row_draw(&ctx);
        bt_app_window_frame_end();
        bt_app_frame_input_set(NULL);
    }
    BT_CHECK(app->draft.memo_mode == BT_MEMO_PER_ORBIT);
    bt_view_settings_update_state(&ctx, app);
    BT_CHECK(ctx.stores.settings.memo_mode == BT_MEMO_PER_ORBIT);
    bt_app_ctx_free(&ctx);
    bt_app_fonts_unload(&book);
    bt_app_window_close();
}

BT_TEST(views_stats, settings_mode_select_keyboard_flow_through_the_seam) {
    if (!bt_app_window_open_driven(PROBE_W, PROBE_H, "views settings select test", NULL)) {
        BT_CHECK(0);
        return;
    }
    bt_app_font_book book;
    char error[BT_APP_ERROR_MAX];
    char config_path[BT_PATH_MAX];
    bt_data_config_path(config_path);
    if (bt_app_fonts_load(config_path, NULL, NULL, 0, &book, error, sizeof error) != 0) {
        bt_app_window_close();
        BT_ABORT("fonts: %s", error);
    }
    bt_view_settings_layout layout = bt_view_settings_layout_for((float)PROBE_W, (float)PROBE_H);
    bt_app_ctx ctx;
    fixture_ctx("select-flow", BT_APP_SETTINGS, &ctx);
    bt_settings_app_state *app = bt_view_settings_app();
    bt_view_settings_app_init(app);
    bt_view_settings_enter_state(&ctx, app);
    bt_ui_input quiet;
    bt_ui_input_clear(&quiet);
    bt_app_frame_input_set(&quiet);
    bt_app_window_frame_begin();
    ClearBackground(bt_app_color(bt_theme_background()));
    bt_view_settings_row_draw(&ctx);
    bt_app_window_frame_end();
    bt_app_frame_input_set(NULL);
    BT_CHECK(app->widgets.mode_select.open == 0);
    BT_CHECK(ctx.input_focus == 0);
    bt_ui_input open;
    bt_ui_input_clear(&open);
    open.x = layout.controls[1].x + layout.controls[1].width / 2.0F;
    open.y = layout.controls[1].y + layout.controls[1].height / 2.0F;
    open.click = 1;
    bt_app_frame_input_set(&open);
    bt_app_window_frame_begin();
    ClearBackground(bt_app_color(bt_theme_background()));
    bt_view_settings_row_draw(&ctx);
    bt_app_window_frame_end();
    bt_app_frame_input_set(NULL);
    BT_CHECK(app->widgets.mode_select.open == 1);
    BT_CHECK(ctx.input_focus == 1);
    BT_CHECK(app->draft.memo_mode == BT_MEMO_SYLLABLES);
    bt_ui_input down;
    bt_ui_input_clear(&down);
    down.arrow_down = 1;
    bt_app_frame_input_set(&down);
    bt_app_window_frame_begin();
    ClearBackground(bt_app_color(bt_theme_background()));
    bt_view_settings_row_draw(&ctx);
    bt_app_window_frame_end();
    bt_app_frame_input_set(NULL);
    BT_CHECK(app->widgets.mode_select.cursor == 1);
    BT_CHECK(app->draft.memo_mode == BT_MEMO_SYLLABLES);
    bt_ui_input enter;
    bt_ui_input_clear(&enter);
    enter.enter = 1;
    bt_app_frame_input_set(&enter);
    bt_app_window_frame_begin();
    ClearBackground(bt_app_color(bt_theme_background()));
    bt_view_settings_row_draw(&ctx);
    bt_app_window_frame_end();
    bt_app_frame_input_set(NULL);
    BT_CHECK(app->widgets.mode_select.open == 0);
    BT_CHECK(app->draft.memo_mode == BT_MEMO_IMAGES);
    bt_view_settings_update_state(&ctx, app);
    BT_CHECK(ctx.stores.settings.memo_mode == BT_MEMO_IMAGES);
    bt_app_frame_input_set(&open);
    bt_app_window_frame_begin();
    ClearBackground(bt_app_color(bt_theme_background()));
    bt_view_settings_row_draw(&ctx);
    bt_app_window_frame_end();
    bt_app_frame_input_set(NULL);
    BT_CHECK(app->widgets.mode_select.open == 1);
    bt_ui_input escape;
    bt_ui_input_clear(&escape);
    escape.escape = 1;
    bt_app_frame_input_set(&escape);
    bt_app_window_frame_begin();
    ClearBackground(bt_app_color(bt_theme_background()));
    bt_view_settings_row_draw(&ctx);
    bt_app_window_frame_end();
    bt_app_frame_input_set(NULL);
    BT_CHECK(app->widgets.mode_select.open == 0);
    BT_CHECK(app->draft.memo_mode == BT_MEMO_IMAGES);
    BT_CHECK(ctx.state == BT_APP_SETTINGS);
    bt_app_ctx_free(&ctx);
    bt_app_fonts_unload(&book);
    bt_app_window_close();
}
