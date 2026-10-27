#include "app/logic.h"

#include "common.h"

#include "core/cube.h"
#include "scheme/scheme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRS_SECTION                                                                                \
    "{\"fsrs\":{\"weights\":[0.40255,1.18385,3.173,15.69105,7.1949,0.5345,1.4604,"                 \
    "0.0046,1.54575,0.1192,1.01925,1.9395,0.11,0.29605,2.2698,0.2315,2.9898,0.51655,"              \
    "0.6621],\"request_retention\":0.9,\"interval_clamp_days\":[1,365]}}"

static const bt_fsrs_config *fsrs_cfg(void) {
    static bt_fsrs_config cfg;
    static int ready = 0;
    if (!ready) {
        bt_fsrs_config_defaults(&cfg);
        ready = 1;
    }
    return &cfg;
}

static void write_app_config(const bt_app_runtime_paths *paths, const char *memo_mode,
                             unsigned cadence, unsigned reps) {
    char text[1024];
    snprintf(text, sizeof text,
             "{\"app\":{\"window\":{\"width\":640,\"height\":480,\"title\":\"t\"},"
             "\"memo\":{\"cadence_ms\":%u,\"mode\":\"%s\"},\"drill\":{\"reps\":%u}},"
             "\"srs\":%s}",
             cadence, memo_mode, reps, SRS_SECTION);
    ta_write_file(paths->config_path, text);
}

static void temp_paths(bt_app_runtime_paths *paths) {
    const char *base = ta_temp_dir("logic", "paths");
    snprintf(paths->config_path, BT_PATH_MAX, "%s\\config\\config.json", base);
    snprintf(paths->vendor_root, BT_PATH_MAX, "%s\\vendor", base);
    snprintf(paths->packs_root, BT_PATH_MAX, "%s\\vendor\\packs", base);
    snprintf(paths->user_root, BT_PATH_MAX, "%s\\user", base);
    snprintf(paths->assets_root, BT_PATH_MAX, "%s\\assets", base);
    write_app_config(paths, "images", 500, 3);
}

static void input_ctx(bt_app_state state, bt_app_ctx *ctx) {
    bt_app_ctx_init(ctx);
    temp_paths(&ctx->paths);
    ctx->state = state;
}

static bt_app_key_event key_escape(int pressed) {
    bt_app_key_event event = {BT_APP_KEY_ESCAPE, 0, pressed};
    return event;
}

static bt_app_key_event key_digit(char digit, int pressed) {
    bt_app_key_event event = {BT_APP_KEY_DIGIT, digit, pressed};
    return event;
}

static bt_settings sample_settings(void) {
    bt_settings settings = {300, BT_MEMO_SYLLABLES, 5};
    return settings;
}

static bt_settings one_settings(void) {
    bt_settings settings = {1, BT_MEMO_IMAGES, 1};
    return settings;
}

BT_TEST(logic, resolve_swaps_missing_roots_to_the_install_layout) {
    bt_app_runtime_paths primary;
    temp_paths(&primary);
    const char *install = ta_temp_dir("logic", "resolve-install");
    char dir[TA_PATH_MAX];
    snprintf(dir, sizeof dir, "%s\\config", install);
    ta_mkdir_p(dir);
    snprintf(dir, sizeof dir, "%s\\ref_ground\\packs", install);
    ta_mkdir_p(dir);
    snprintf(dir, sizeof dir, "%s\\assets", install);
    ta_mkdir_p(dir);
    char config[TA_PATH_MAX];
    snprintf(config, sizeof config, "%s\\config\\config.json", install);
    ta_write_file(config, "{}");
    bt_app_runtime_paths missing;
    snprintf(missing.config_path, BT_PATH_MAX, "%s\\config\\config.absent", primary.config_path);
    snprintf(missing.vendor_root, BT_PATH_MAX, "%s/absent", primary.vendor_root);
    snprintf(missing.packs_root, BT_PATH_MAX, "%s/absent", primary.packs_root);
    snprintf(missing.user_root, BT_PATH_MAX, "%s", primary.user_root);
    snprintf(missing.assets_root, BT_PATH_MAX, "%s/absent", primary.assets_root);
    bt_app_runtime_paths resolved;
    bt_app_paths_resolve(&missing, install, &resolved);
    char wanted[TA_PATH_MAX];
    snprintf(wanted, sizeof wanted, "%s\\config\\config.json", install);
    BT_CHECK_EQ(strcmp(resolved.config_path, wanted), 0);
    snprintf(wanted, sizeof wanted, "%s\\ref_ground", install);
    BT_CHECK_EQ(strcmp(resolved.vendor_root, wanted), 0);
    snprintf(wanted, sizeof wanted, "%s\\ref_ground\\packs", install);
    BT_CHECK_EQ(strcmp(resolved.packs_root, wanted), 0);
    snprintf(wanted, sizeof wanted, "%s\\assets", install);
    BT_CHECK_EQ(strcmp(resolved.assets_root, wanted), 0);
    BT_CHECK_EQ(strcmp(resolved.user_root, primary.user_root), 0);
}

BT_TEST(logic, resolve_keeps_existing_workspace_roots) {
    bt_app_runtime_paths primary;
    temp_paths(&primary);
    ta_mkdir_p(primary.vendor_root);
    ta_mkdir_p(primary.packs_root);
    bt_app_runtime_paths resolved;
    bt_app_paths_resolve(&primary, getenv("TEMP"), &resolved);
    BT_CHECK_EQ(strcmp(resolved.config_path, primary.config_path), 0);
    BT_CHECK_EQ(strcmp(resolved.vendor_root, primary.vendor_root), 0);
    BT_CHECK_EQ(strcmp(resolved.packs_root, primary.packs_root), 0);
    BT_CHECK_EQ(strcmp(resolved.assets_root, primary.assets_root), 0);
}

BT_TEST(logic, resolve_returns_primary_when_nothing_exists) {
    bt_app_runtime_paths primary;
    temp_paths(&primary);
    bt_app_runtime_paths missing;
    snprintf(missing.config_path, BT_PATH_MAX, "%s\\config\\config.absent", primary.config_path);
    snprintf(missing.vendor_root, BT_PATH_MAX, "%s/absent", primary.vendor_root);
    snprintf(missing.packs_root, BT_PATH_MAX, "%s/absent", primary.packs_root);
    snprintf(missing.user_root, BT_PATH_MAX, "%s", primary.user_root);
    snprintf(missing.assets_root, BT_PATH_MAX, "%s/absent", primary.assets_root);
    bt_app_runtime_paths resolved;
    bt_app_paths_resolve(&missing, primary.vendor_root, &resolved);
    BT_CHECK_EQ(strcmp(resolved.config_path, missing.config_path), 0);
    BT_CHECK_EQ(strcmp(resolved.vendor_root, missing.vendor_root), 0);
    BT_CHECK_EQ(strcmp(resolved.assets_root, missing.assets_root), 0);
}

BT_TEST(logic, resolve_without_exe_dir_is_identity) {
    bt_app_runtime_paths primary;
    temp_paths(&primary);
    bt_app_runtime_paths resolved;
    bt_app_paths_resolve(&primary, NULL, &resolved);
    BT_CHECK_EQ(strcmp(resolved.config_path, primary.config_path), 0);
    BT_CHECK_EQ(strcmp(resolved.vendor_root, primary.vendor_root), 0);
    BT_CHECK_EQ(strcmp(resolved.user_root, primary.user_root), 0);
}

BT_TEST(logic, boot_reaches_menu_payload_without_vendor) {
    bt_app_runtime_paths paths;
    temp_paths(&paths);
    bt_app_booted booted;
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_app_boot(&paths, &booted, error, sizeof error), 0);
    BT_CHECK(bt_corpus_is_empty(&booted.corpus));
    BT_CHECK_EQ(booted.warnings.count, 1);
    BT_CHECK(strstr(booted.warnings.items[0], "vendor data missing") != NULL);
    BT_CHECK_EQ(booted.stores.settings.memo_mode, BT_MEMO_IMAGES);
    BT_CHECK_EQ(booted.stores.settings.drill_reps, 3);
    BT_CHECK_EQ(booted.config.window.width, 640);
    bt_app_booted_free(&booted);
}

BT_TEST(logic, boot_fatals_on_partial_vendor) {
    bt_app_runtime_paths paths;
    temp_paths(&paths);
    ta_mkdir_p(paths.vendor_root);
    bt_app_booted booted;
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_app_boot(&paths, &booted, error, sizeof error), -1);
    BT_CHECK(strstr(error, "vendor corpus") != NULL);
    BT_CHECK(strstr(error, "edge.tsv") != NULL);
}

BT_TEST(logic, boot_fatals_on_missing_config_with_reason) {
    bt_app_runtime_paths paths;
    temp_paths(&paths);
    char *base_end = paths.config_path + strlen(paths.config_path);
    while (base_end > paths.config_path && *base_end != '.') {
        base_end--;
    }
    snprintf(base_end, BT_PATH_MAX - (size_t)(base_end - paths.config_path), ".missing");
    bt_app_booted booted;
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_app_boot(&paths, &booted, error, sizeof error), -1);
    BT_CHECK(strstr(error, "cannot read config") != NULL);
    BT_CHECK(strstr(error, "config.missing") != NULL);
}

BT_TEST(logic, boot_fatals_on_missing_app_section) {
    bt_app_runtime_paths paths;
    temp_paths(&paths);
    char text[512];
    snprintf(text, sizeof text, "{\"srs\":%s}", SRS_SECTION);
    ta_write_file(paths.config_path, text);
    bt_app_booted booted;
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_app_boot(&paths, &booted, error, sizeof error), -1);
    BT_CHECK_EQ(strcmp(error, "config app section missing"), 0);
}

BT_TEST(logic, boot_fatals_on_invalid_memo_mode) {
    bt_app_runtime_paths paths;
    temp_paths(&paths);
    write_app_config(&paths, " Morse ", 500, 3);
    bt_app_booted booted;
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_app_boot(&paths, &booted, error, sizeof error), -1);
    BT_CHECK(strstr(error, "app.memo.mode") != NULL);
}

BT_TEST(logic, boot_clamps_out_of_range_settings) {
    bt_app_runtime_paths low;
    temp_paths(&low);
    write_app_config(&low, "images", 0, 0);
    bt_app_booted booted;
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_app_boot(&low, &booted, error, sizeof error), 0);
    BT_CHECK_EQ(booted.stores.settings.memo_cadence_ms, 1);
    BT_CHECK_EQ(booted.stores.settings.drill_reps, 1);
    bt_app_booted_free(&booted);
    bt_app_runtime_paths high;
    temp_paths(&high);
    write_app_config(&high, "images", 99999, 999);
    BT_CHECK_EQ(bt_app_boot(&high, &booted, error, sizeof error), 0);
    BT_CHECK_EQ(booted.stores.settings.memo_cadence_ms, 10000);
    BT_CHECK_EQ(booted.stores.settings.drill_reps, 100);
    bt_app_booted_free(&booted);
}

BT_TEST(logic, highlight_kind_covers_both_target_slots) {
    bt_app_net_state net;
    bt_app_net_state_default(&net);
    bt_app_highlight_kind kind;
    BT_CHECK_EQ(bt_app_net_highlight_kind(&net, BT_SCHEME_EDGE_BUFFER_STICKER, &kind), 1);
    BT_CHECK_EQ(kind, BT_APP_HIGHLIGHT_BUFFER);
    BT_CHECK_EQ(bt_app_net_highlight_kind(&net, 5, &kind), 0);
    net.has_target[0] = 1;
    net.targets[0] = 5;
    net.has_target[1] = 1;
    net.targets[1] = 9;
    BT_CHECK_EQ(bt_app_net_highlight_kind(&net, 5, &kind), 1);
    BT_CHECK_EQ(kind, BT_APP_HIGHLIGHT_TARGET);
    BT_CHECK_EQ(bt_app_net_highlight_kind(&net, 9, &kind), 1);
    BT_CHECK_EQ(kind, BT_APP_HIGHLIGHT_TARGET);
    BT_CHECK_EQ(bt_app_net_highlight_kind(&net, 7, &kind), 0);
}

BT_TEST(logic, last_save_all_ok_requires_every_store) {
    bt_app_last_save none = {0, 0, 0, 0};
    BT_CHECK(!bt_app_last_save_all_ok(&none));
    bt_app_last_save full = {1, 1, 1, 1};
    BT_CHECK(bt_app_last_save_all_ok(&full));
    bt_app_last_save no_algs = {1, 1, 1, 0};
    BT_CHECK(!bt_app_last_save_all_ok(&no_algs));
}

BT_TEST(logic, menu_keys_open_their_views_only_from_menu) {
    for (int i = 0; i < BT_APP_MENU_KEY_COUNT; i++) {
        bt_app_ctx ctx;
        input_ctx(BT_APP_MENU, &ctx);
        bt_app_push_key(&ctx, key_digit(BT_APP_MENU_KEYS[i].digit, 1));
        bt_app_process_keys(&ctx);
        BT_CHECK(ctx.has_pending);
        BT_CHECK_EQ(ctx.pending, BT_APP_MENU_KEYS[i].target);
        bt_app_ctx_free(&ctx);
    }
    bt_app_ctx ctx;
    input_ctx(BT_APP_DRILL, &ctx);
    bt_app_push_key(&ctx, key_digit(BT_APP_MENU_KEYS[0].digit, 1));
    bt_app_process_keys(&ctx);
    BT_CHECK(!ctx.has_pending);
    bt_app_ctx_free(&ctx);
}

BT_TEST(logic, escape_leaves_a_view_but_released_keys_do_nothing) {
    bt_app_ctx ctx;
    input_ctx(BT_APP_DRILL, &ctx);
    bt_app_push_key(&ctx, key_escape(1));
    bt_app_process_keys(&ctx);
    BT_CHECK(ctx.has_pending);
    BT_CHECK_EQ(ctx.pending, BT_APP_MENU);
    BT_CHECK_EQ(bt_app_apply_pending(&ctx), 1);
    bt_app_push_key(&ctx, key_digit(BT_APP_MENU_KEYS[0].digit, 0));
    bt_app_process_keys(&ctx);
    BT_CHECK(!ctx.has_pending);
    bt_app_ctx_free(&ctx);
}

BT_TEST(logic, bootstrap_and_fatal_swallow_buffered_keys) {
    bt_app_ctx ctx;
    input_ctx(BT_APP_BOOTSTRAP, &ctx);
    bt_app_push_key(&ctx, key_escape(1));
    bt_app_push_key(&ctx, key_digit(BT_APP_MENU_KEYS[1].digit, 1));
    bt_app_process_keys(&ctx);
    BT_CHECK(!ctx.has_pending);
    BT_CHECK_EQ(ctx.key_count, 0);
    ctx.state = BT_APP_MENU;
    bt_app_process_keys(&ctx);
    BT_CHECK(!ctx.has_pending);
    bt_app_ctx_free(&ctx);
    input_ctx(BT_APP_FATAL, &ctx);
    bt_app_push_key(&ctx, key_escape(1));
    bt_app_process_keys(&ctx);
    BT_CHECK(!ctx.has_pending);
    bt_app_ctx_free(&ctx);
}

BT_TEST(logic, escape_while_typing_clears_focus_instead_of_navigating) {
    bt_app_ctx ctx;
    input_ctx(BT_APP_ALGS, &ctx);
    ctx.input_focus = 1;
    bt_app_push_key(&ctx, key_escape(1));
    bt_app_process_keys(&ctx);
    BT_CHECK(!ctx.has_pending);
    BT_CHECK(!ctx.input_focus);
    bt_app_ctx_free(&ctx);
}

BT_TEST(logic, nav_to_menu_is_skipped_when_already_there) {
    bt_app_ctx ctx;
    input_ctx(BT_APP_MENU, &ctx);
    bt_app_nav_to_menu(&ctx);
    BT_CHECK(!ctx.has_pending);
    bt_app_ctx_free(&ctx);
    input_ctx(BT_APP_DRILL, &ctx);
    bt_app_nav_to_menu(&ctx);
    BT_CHECK(ctx.has_pending);
    BT_CHECK_EQ(ctx.pending, BT_APP_MENU);
    BT_CHECK_EQ(bt_app_apply_pending(&ctx), 1);
    BT_CHECK_EQ(ctx.state, BT_APP_MENU);
    BT_CHECK(!ctx.has_pending);
    bt_app_ctx_free(&ctx);
    input_ctx(BT_APP_MENU, &ctx);
    bt_app_request_state(&ctx, BT_APP_STATS);
    BT_CHECK(ctx.has_pending);
    BT_CHECK_EQ(ctx.pending, BT_APP_STATS);
    bt_app_ctx_free(&ctx);
}

BT_TEST(logic, settings_change_persists_to_the_user_root) {
    bt_app_ctx ctx;
    input_ctx(BT_APP_MENU, &ctx);
    ctx.stores = (bt_stores){0};
    ctx.stores.settings = sample_settings();
    bt_srs_store_init(&ctx.stores.srs);
    bt_solve_log_init(&ctx.stores.solves);
    bt_user_algs_init(&ctx.stores.user_algs);
    ctx.has_stores = 1;
    bt_settings expected = {640, BT_MEMO_PER_ORBIT, 6};
    BT_CHECK_EQ(bt_app_apply_settings_change(&ctx, &expected), 0);
    BT_CHECK(ctx.stores.settings.memo_cadence_ms == expected.memo_cadence_ms &&
             ctx.stores.settings.memo_mode == expected.memo_mode &&
             ctx.stores.settings.drill_reps == expected.drill_reps);
    BT_CHECK(ctx.last_save.settings_ok);
    bt_stores reloaded;
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_stores_load(ctx.paths.user_root, one_settings(), fsrs_cfg(), &reloaded, error,
                               sizeof error),
                0);
    BT_CHECK(reloaded.settings.memo_cadence_ms == expected.memo_cadence_ms &&
             reloaded.settings.memo_mode == expected.memo_mode &&
             reloaded.settings.drill_reps == expected.drill_reps);
    bt_stores_free(&reloaded);
    bt_app_ctx_free(&ctx);
}

BT_TEST(logic, save_stores_request_persists_every_store) {
    bt_app_ctx ctx;
    input_ctx(BT_APP_MENU, &ctx);
    bt_stores stores = {0};
    stores.settings = sample_settings();
    bt_srs_store_init(&stores.srs);
    bt_solve_log_init(&stores.solves);
    bt_user_algs_init(&stores.user_algs);
    bt_card card;
    memset(&card, 0, sizeof card);
    card.interval_days = 3;
    card.due_day = 4;
    card.reps = 1;
    card.lapses = 0;
    BT_CHECK_EQ(bt_srs_store_set(&stores.srs, "edge/UF", &card), 0);
    bt_solve_record record = {9, 1000, 2000, 0};
    BT_CHECK_EQ(bt_solve_log_push(&stores.solves, record), 0);
    ctx.stores = stores;
    ctx.has_stores = 1;
    BT_CHECK_EQ(bt_app_save_stores(&ctx), 0);
    BT_CHECK(bt_app_last_save_all_ok(&ctx.last_save));
    bt_stores reloaded;
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_stores_load(ctx.paths.user_root, one_settings(), fsrs_cfg(), &reloaded, error,
                               sizeof error),
                0);
    BT_CHECK_EQ(bt_srs_store_len(&reloaded.srs), 1);
    BT_CHECK_EQ(reloaded.solves.count, 1);
    BT_CHECK_EQ(reloaded.settings.drill_reps, 5);
    bt_stores_free(&reloaded);
    bt_app_ctx_free(&ctx);
}
