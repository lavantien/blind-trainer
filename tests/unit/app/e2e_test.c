#include "app/logic.h"

#include "common.h"

#include "data/paths.h"

#include <stdio.h>
#include <string.h>

#define SRS_SECTION                                                                                \
    "{\"fsrs\":{\"weights\":[0.40255,1.18385,3.173,15.69105,7.1949,0.5345,1.4604,"                 \
    "0.0046,1.54575,0.1192,1.01925,1.9395,0.11,0.29605,2.2698,0.2315,2.9898,0.51655,"              \
    "0.6621],\"request_retention\":0.9,\"interval_clamp_days\":[1,365]}}"

static void flow_paths(bt_app_runtime_paths *paths, const char *tag) {
    const char *base = ta_temp_dir("e2e", tag);
    snprintf(paths->config_path, BT_PATH_MAX, "%s/config/config.json", base);
    snprintf(paths->vendor_root, BT_PATH_MAX, "%s/vendor", base);
    snprintf(paths->packs_root, BT_PATH_MAX, "%s/vendor/packs", base);
    snprintf(paths->user_root, BT_PATH_MAX, "%s/user", base);
    snprintf(paths->assets_root, BT_PATH_MAX, "%s/assets", base);
    char text[1024];
    snprintf(text, sizeof text,
             "{\"app\":{\"window\":{\"width\":640,\"height\":480,\"title\":\"e2e\"},"
             "\"memo\":{\"cadence_ms\":700,\"mode\":\"syllables\"},\"drill\":{\"reps\":4}},"
             "\"srs\":%s}",
             SRS_SECTION);
    ta_write_file(paths->config_path, text);
}

static void headless_ctx(bt_app_ctx *ctx, const bt_app_runtime_paths *paths) {
    bt_app_ctx_init(ctx);
    ctx->paths = *paths;
    bt_app_bootstrap(ctx);
    bt_app_apply_pending(ctx);
}

static int settings_equal(const bt_settings *left, const bt_settings *right) {
    return left->memo_cadence_ms == right->memo_cadence_ms && left->memo_mode == right->memo_mode &&
           left->drill_reps == right->drill_reps;
}

static int file_present(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    fclose(file);
    return 1;
}

BT_TEST(e2e, bootstrap_reaches_menu_headless) {
    bt_app_runtime_paths paths;
    flow_paths(&paths, "menu");
    bt_app_ctx ctx;
    headless_ctx(&ctx, &paths);
    BT_CHECK_EQ(ctx.state, BT_APP_MENU);
    BT_CHECK_EQ(ctx.stores.settings.memo_cadence_ms, 700U);
    BT_CHECK_EQ(ctx.stores.settings.memo_mode, BT_MEMO_SYLLABLES);
    BT_CHECK_EQ(ctx.stores.settings.drill_reps, 4U);
    BT_CHECK_EQ(ctx.vendor_corpus.count, (size_t)0);
    bt_app_ctx_free(&ctx);
}

BT_TEST(e2e, settings_mutation_persists_across_reload) {
    bt_app_runtime_paths paths;
    flow_paths(&paths, "persist");
    bt_settings mutated = {1234, BT_MEMO_IMAGES, 9};
    bt_app_ctx first;
    headless_ctx(&first, &paths);
    BT_CHECK_EQ(first.state, BT_APP_MENU);
    BT_CHECK_EQ(bt_app_apply_settings_change(&first, &mutated), 0);
    BT_CHECK_EQ(first.last_save.settings_ok, 1);
    BT_CHECK(settings_equal(&first.stores.settings, &mutated));
    char db[BT_PATH_MAX];
    bt_data_db_path_at(paths.user_root, db);
    BT_CHECK(file_present(db));
    bt_app_ctx_free(&first);

    bt_app_ctx second;
    headless_ctx(&second, &paths);
    BT_CHECK_EQ(second.state, BT_APP_MENU);
    BT_CHECK(settings_equal(&second.stores.settings, &mutated));
    bt_app_ctx_free(&second);
}

BT_TEST(e2e, per_orbit_mode_survives_reload) {
    bt_app_runtime_paths paths;
    flow_paths(&paths, "perorbit");
    bt_app_ctx first;
    headless_ctx(&first, &paths);
    BT_CHECK_EQ(first.state, BT_APP_MENU);
    BT_CHECK_EQ(first.stores.settings.memo_mode, BT_MEMO_SYLLABLES);
    bt_settings mutated = first.stores.settings;
    mutated.memo_mode = BT_MEMO_PER_ORBIT;
    BT_CHECK_EQ(bt_app_apply_settings_change(&first, &mutated), 0);
    BT_CHECK_EQ(first.last_save.settings_ok, 1);
    BT_CHECK(settings_equal(&first.stores.settings, &mutated));
    char db[BT_PATH_MAX];
    bt_data_db_path_at(paths.user_root, db);
    BT_CHECK(file_present(db));
    bt_app_ctx_free(&first);

    bt_app_ctx second;
    headless_ctx(&second, &paths);
    BT_CHECK_EQ(second.state, BT_APP_MENU);
    BT_CHECK(settings_equal(&second.stores.settings, &mutated));
    bt_app_ctx_free(&second);
}

BT_TEST(e2e, save_stores_persists_into_the_db) {
    bt_app_runtime_paths paths;
    flow_paths(&paths, "savestores");
    bt_app_ctx ctx;
    headless_ctx(&ctx, &paths);
    BT_CHECK_EQ(ctx.state, BT_APP_MENU);
    BT_CHECK_EQ(bt_app_save_stores(&ctx), 0);
    BT_CHECK(bt_app_last_save_all_ok(&ctx.last_save));
    char db[BT_PATH_MAX];
    bt_data_db_path_at(paths.user_root, db);
    BT_CHECK(file_present(db));
    bt_fsrs_config fsrs;
    bt_fsrs_config_defaults(&fsrs);
    bt_stores reloaded;
    char error[256];
    bt_settings fallback = {1, BT_MEMO_IMAGES, 1};
    BT_CHECK_EQ(bt_stores_load(paths.user_root, fallback, &fsrs, &reloaded, error, sizeof error),
                0);
    BT_CHECK_EQ(reloaded.settings.memo_cadence_ms, 700U);
    BT_CHECK_EQ(reloaded.settings.drill_reps, 4U);
    bt_stores_free(&reloaded);
    bt_app_ctx_free(&ctx);
}

BT_TEST(e2e, fatal_on_missing_config_shows_reason) {
    bt_app_runtime_paths paths;
    flow_paths(&paths, "fatal");
    snprintf(paths.config_path, BT_PATH_MAX, "%s.absent", paths.config_path);
    bt_app_ctx ctx;
    headless_ctx(&ctx, &paths);
    BT_CHECK_EQ(ctx.state, BT_APP_FATAL);
    BT_CHECK(strstr(ctx.fatal_reason, "cannot read config") != NULL);
    BT_CHECK(strstr(ctx.fatal_reason, paths.config_path) != NULL);
    bt_app_ctx_free(&ctx);
}

BT_TEST(e2e, nav_intents_drive_state_machine) {
    bt_app_runtime_paths paths;
    flow_paths(&paths, "nav");
    bt_app_ctx ctx;
    headless_ctx(&ctx, &paths);
    BT_CHECK_EQ(ctx.state, BT_APP_MENU);
    bt_app_state targets[] = {
        BT_APP_DRILL, BT_APP_MEMO, BT_APP_SOLVE, BT_APP_ALGS, BT_APP_STATS, BT_APP_SETTINGS,
    };
    for (size_t i = 0; i < sizeof targets / sizeof targets[0]; i++) {
        bt_app_request_state(&ctx, targets[i]);
        bt_app_apply_pending(&ctx);
        BT_CHECK_EQ(ctx.state, targets[i]);
        bt_app_nav_to_menu(&ctx);
        bt_app_apply_pending(&ctx);
        BT_CHECK_EQ(ctx.state, BT_APP_MENU);
    }
    bt_app_ctx_free(&ctx);
}
