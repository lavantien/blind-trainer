#include "app/smoke.h"

#include "common.h"

#include <io.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int file_size(const char *path, unsigned long *out) {
    struct _stat info;
    if (_stat(path, &info) != 0) {
        return 0;
    }
    *out = (unsigned long)info.st_size;
    return 1;
}

static void land_file(const char *path) {
    ta_write_file(path, "png");
}

static void step_world(bt_smoke_tour *tour, bt_app_ctx *ctx, int fonts_ready, float dt,
                       char request[TA_PATH_MAX], int *has_request) {
    *has_request = 0;
    request[0] = '\0';
    tour->exit_code = bt_smoke_step(tour, ctx, fonts_ready, dt, request, has_request);
}

BT_TEST(smoke, tour_covers_all_views_once) {
    int seen[BT_APP_STATE_COUNT] = {0};
    for (size_t i = 0; i < BT_APP_TOUR_COUNT; i++) {
        BT_CHECK(!seen[BT_APP_TOUR[i]]);
        seen[BT_APP_TOUR[i]] = 1;
    }
    BT_CHECK_EQ(BT_APP_TOUR_COUNT, 10);
    BT_CHECK_EQ(BT_APP_TOUR[0], BT_APP_MENU);
    BT_CHECK_EQ(BT_APP_TOUR[BT_APP_TOUR_COUNT - 1], BT_APP_FATAL);
    BT_CHECK_EQ(BT_APP_TOUR[5], BT_APP_ALGS);
    BT_CHECK_EQ(BT_APP_TOUR[6], BT_APP_CASE_DRILL);
}

BT_TEST(smoke, shot_paths_are_unique_per_state) {
    bt_smoke_tour tour;
    bt_smoke_tour_init(&tour, "shots");
    const char *paths[BT_SMOKE_SHOT_MAX];
    char buffers[BT_SMOKE_SHOT_MAX][TA_PATH_MAX];
    for (size_t i = 0; i < BT_APP_TOUR_COUNT; i++) {
        bt_smoke_shot_path(&tour, BT_APP_TOUR[i], buffers[i]);
        paths[i] = buffers[i];
    }
    for (size_t i = 0; i < BT_APP_TOUR_COUNT; i++) {
        for (size_t j = i + 1; j < BT_APP_TOUR_COUNT; j++) {
            BT_CHECK(strcmp(paths[i], paths[j]) != 0);
        }
    }
    BT_CHECK(strcmp(paths[0], "shots/menu.png") == 0);
    BT_CHECK(strcmp(paths[BT_APP_TOUR_COUNT - 1], "shots/fatal.png") == 0);
}

BT_TEST(smoke, seeds_pin_the_tour_determinism) {
    uint64_t seeds[3];
    bt_smoke_seeds(20261012, seeds);
    BT_CHECK_EQ(seeds[0], (uint64_t)20261012);
    BT_CHECK_EQ(seeds[1], (uint64_t)20261013);
    BT_CHECK_EQ(seeds[2], (uint64_t)20261014);
    bt_smoke_seeds(UINT64_MAX, seeds);
    BT_CHECK_EQ(seeds[0], (uint64_t)UINT64_MAX);
    BT_CHECK_EQ(seeds[1], (uint64_t)0);
    BT_CHECK_EQ(seeds[2], (uint64_t)1);
}

BT_TEST(smoke, a_boot_fatal_aborts_the_tour_with_error_one) {
    bt_app_ctx ctx;
    bt_app_ctx_init(&ctx);
    strcpy(ctx.fatal_reason, "cannot read config");
    bt_smoke_tour tour;
    bt_smoke_tour_init(&tour, ta_temp_dir("smoke", "boot-fatal"));
    ctx.state = BT_APP_BOOTSTRAP;
    char request[TA_PATH_MAX];
    int has_request = 0;
    step_world(&tour, &ctx, 1, 0.1F, request, &has_request);
    BT_CHECK_EQ(tour.exit_code, 1);
    BT_CHECK_EQ(tour.phase.kind, BT_SMOKE_DONE);
    bt_app_ctx_free(&ctx);
}

BT_TEST(smoke, wait_for_boot_exits_with_error_one_after_the_stuck_limit) {
    bt_app_ctx ctx;
    bt_app_ctx_init(&ctx);
    bt_smoke_tour tour;
    bt_smoke_tour_init(&tour, ta_temp_dir("smoke", "boot-stuck"));
    ctx.state = BT_APP_BOOTSTRAP;
    tour.phase.kind = BT_SMOKE_WAIT_FOR_BOOT;
    tour.phase.waited = 9.95F;
    char request[TA_PATH_MAX];
    int has_request = 0;
    step_world(&tour, &ctx, 1, 0.1F, request, &has_request);
    BT_CHECK_EQ(tour.phase.kind, BT_SMOKE_WAIT_FOR_BOOT);
    step_world(&tour, &ctx, 1, 0.1F, request, &has_request);
    BT_CHECK_EQ(tour.exit_code, 1);
    BT_CHECK_EQ(tour.phase.kind, BT_SMOKE_DONE);
    bt_app_ctx_free(&ctx);
}

BT_TEST(smoke, wait_for_boot_needs_both_menu_and_ready_fonts) {
    bt_app_ctx ctx;
    bt_app_ctx_init(&ctx);
    bt_smoke_tour tour;
    bt_smoke_tour_init(&tour, ta_temp_dir("smoke", "boot-wait"));
    ctx.state = BT_APP_MENU;
    char request[TA_PATH_MAX];
    int has_request = 0;
    step_world(&tour, &ctx, 0, 0.1F, request, &has_request);
    BT_CHECK_EQ(tour.phase.kind, BT_SMOKE_WAIT_FOR_BOOT);
    BT_CHECK_F64_NEAR((double)tour.phase.waited, 0.1, 1e-6);
    ctx.state = BT_APP_BOOTSTRAP;
    step_world(&tour, &ctx, 1, 0.1F, request, &has_request);
    BT_CHECK_EQ(tour.phase.kind, BT_SMOKE_WAIT_FOR_BOOT);
    BT_CHECK_F64_NEAR((double)tour.phase.waited, 0.2, 1e-6);
    ctx.state = BT_APP_MENU;
    step_world(&tour, &ctx, 1, 0.1F, request, &has_request);
    BT_CHECK_EQ(tour.phase.kind, BT_SMOKE_DWELL);
    BT_CHECK_EQ(tour.phase.state, BT_APP_MENU);
    BT_CHECK_EQ(tour.entered_count, (size_t)1);
    BT_CHECK_EQ(tour.entered[0], BT_APP_MENU);
    bt_app_ctx_free(&ctx);
}

BT_TEST(smoke, dwell_shoots_once_then_waits_for_the_file) {
    const char *dir = ta_temp_dir("smoke", "dwell-shoot");
    bt_app_ctx ctx;
    bt_app_ctx_init(&ctx);
    bt_smoke_tour tour;
    bt_smoke_tour_init(&tour, dir);
    ctx.state = BT_APP_MENU;
    tour.phase.kind = BT_SMOKE_DWELL;
    tour.phase.state = BT_APP_MENU;
    tour.phase.waited = 0.45F;
    tour.entered_count = 1;
    tour.entered[0] = BT_APP_MENU;
    char request[TA_PATH_MAX];
    int has_request = 0;
    step_world(&tour, &ctx, 1, 0.1F, request, &has_request);
    BT_CHECK_EQ(tour.shot_count, (size_t)1);
    BT_CHECK_EQ(tour.shots[0].state, BT_APP_MENU);
    char expected[TA_PATH_MAX];
    snprintf(expected, sizeof expected, "%s/menu.png", dir);
    BT_CHECK(strcmp(tour.shots[0].path, expected) == 0);
    BT_CHECK_EQ(tour.phase.kind, BT_SMOKE_WAIT_FOR_SHOT);
    BT_CHECK_EQ(tour.phase.state, BT_APP_MENU);
    BT_CHECK(has_request);
    BT_CHECK(strcmp(request, expected) == 0);
    bt_app_ctx_free(&ctx);
}

BT_TEST(smoke, dwell_times_out_with_error_three_when_the_state_never_arrives) {
    bt_app_ctx ctx;
    bt_app_ctx_init(&ctx);
    bt_smoke_tour tour;
    bt_smoke_tour_init(&tour, ta_temp_dir("smoke", "dwell-stuck"));
    ctx.state = BT_APP_MENU;
    tour.phase.kind = BT_SMOKE_DWELL;
    tour.phase.state = BT_APP_MEMO;
    tour.phase.stuck = 9.95F;
    tour.entered_count = 1;
    tour.entered[0] = BT_APP_MEMO;
    char request[TA_PATH_MAX];
    int has_request = 0;
    step_world(&tour, &ctx, 1, 0.1F, request, &has_request);
    BT_CHECK_EQ(tour.exit_code, 3);
    BT_CHECK_EQ(tour.phase.kind, BT_SMOKE_DONE);
    bt_app_ctx_free(&ctx);
}

BT_TEST(smoke, wait_for_shot_blocks_until_the_file_lands_then_advances) {
    const char *dir = ta_temp_dir("smoke", "shot-wait");
    bt_app_ctx ctx;
    bt_app_ctx_init(&ctx);
    bt_smoke_tour tour;
    bt_smoke_tour_init(&tour, dir);
    ctx.state = BT_APP_MENU;
    tour.phase.kind = BT_SMOKE_WAIT_FOR_SHOT;
    tour.phase.state = BT_APP_MENU;
    snprintf(tour.phase.path, sizeof tour.phase.path, "%s/menu.png", dir);
    tour.entered_count = 1;
    tour.entered[0] = BT_APP_MENU;
    char request[TA_PATH_MAX];
    int has_request = 0;
    step_world(&tour, &ctx, 1, 0.1F, request, &has_request);
    BT_CHECK_EQ(tour.phase.kind, BT_SMOKE_WAIT_FOR_SHOT);
    land_file(tour.phase.path);
    step_world(&tour, &ctx, 1, 0.1F, request, &has_request);
    BT_CHECK_EQ(tour.entered_count, (size_t)2);
    BT_CHECK_EQ(tour.entered[0], BT_APP_MENU);
    BT_CHECK_EQ(tour.entered[1], BT_APP_DRILL);
    BT_CHECK_EQ(tour.phase.kind, BT_SMOKE_DWELL);
    BT_CHECK_EQ(tour.phase.state, BT_APP_DRILL);
    BT_CHECK(ctx.has_pending);
    BT_CHECK_EQ(ctx.pending, BT_APP_DRILL);
    BT_CHECK_EQ(ctx.fatal_reason[0], '\0');
    bt_app_ctx_free(&ctx);
}

BT_TEST(smoke, the_fatal_stop_sets_the_fatal_reason) {
    const char *dir = ta_temp_dir("smoke", "fatal-stop");
    bt_app_ctx ctx;
    bt_app_ctx_init(&ctx);
    bt_smoke_tour tour;
    bt_smoke_tour_init(&tour, dir);
    char path[TA_PATH_MAX];
    snprintf(path, sizeof path, "%s/settings.png", dir);
    land_file(path);
    ctx.state = BT_APP_SETTINGS;
    tour.phase.kind = BT_SMOKE_WAIT_FOR_SHOT;
    tour.phase.state = BT_APP_SETTINGS;
    strcpy(tour.phase.path, path);
    for (size_t i = 0; i < BT_APP_TOUR_COUNT - 1; i++) {
        tour.entered[i] = BT_APP_TOUR[i];
    }
    tour.entered_count = BT_APP_TOUR_COUNT - 1;
    char request[TA_PATH_MAX];
    int has_request = 0;
    step_world(&tour, &ctx, 1, 0.1F, request, &has_request);
    BT_CHECK_EQ(tour.entered_count, (size_t)BT_APP_TOUR_COUNT);
    BT_CHECK_EQ(tour.phase.kind, BT_SMOKE_DWELL);
    BT_CHECK_EQ(tour.phase.state, BT_APP_FATAL);
    BT_CHECK(strcmp(ctx.fatal_reason, BT_SMOKE_FATAL_REASON) == 0);
    BT_CHECK(ctx.has_pending);
    BT_CHECK_EQ(ctx.pending, BT_APP_FATAL);
    bt_app_ctx_free(&ctx);
}

BT_TEST(smoke, a_full_tour_with_landed_shots_exits_successfully) {
    const char *dir = ta_temp_dir("smoke", "full-tour");
    bt_app_ctx ctx;
    bt_app_ctx_init(&ctx);
    bt_smoke_tour tour;
    bt_smoke_tour_init(&tour, dir);
    ctx.state = BT_APP_MENU;
    char request[TA_PATH_MAX];
    int has_request = 0;
    for (int frame = 0; frame < 400; frame++) {
        step_world(&tour, &ctx, 1, 0.1F, request, &has_request);
        if (tour.phase.kind == BT_SMOKE_DONE) {
            break;
        }
        if (tour.phase.kind == BT_SMOKE_DWELL) {
            ctx.state = tour.phase.state;
            ctx.has_pending = 0;
        } else if (has_request) {
            unsigned long size = 0;
            if (!file_size(request, &size)) {
                land_file(request);
            }
        }
    }
    BT_CHECK_EQ(tour.phase.kind, BT_SMOKE_DONE);
    BT_CHECK_EQ(tour.entered_count, (size_t)BT_APP_TOUR_COUNT);
    for (size_t i = 0; i < BT_APP_TOUR_COUNT; i++) {
        BT_CHECK_EQ(tour.entered[i], BT_APP_TOUR[i]);
    }
    BT_CHECK_EQ(tour.shot_count, (size_t)BT_APP_TOUR_COUNT);
    BT_CHECK(strcmp(ctx.fatal_reason, BT_SMOKE_FATAL_REASON) == 0);
    BT_CHECK_EQ(tour.exit_code, 0);
    bt_app_ctx_free(&ctx);
}

static void landed_tour(bt_smoke_tour *tour, const char *dir, const bt_app_state *entered,
                        const char *bytes) {
    bt_smoke_tour_init(tour, dir);
    for (size_t i = 0; i < BT_APP_TOUR_COUNT; i++) {
        tour->entered[i] = entered[i];
        tour->shots[i].state = BT_APP_TOUR[i];
        bt_smoke_shot_path(tour, BT_APP_TOUR[i], tour->shots[i].path);
        if (bytes[0] != '\0') {
            ta_write_file(tour->shots[i].path, bytes);
        } else {
            FILE *file = fopen(tour->shots[i].path, "wb");
            if (file != NULL) {
                fclose(file);
            }
        }
    }
    tour->entered_count = BT_APP_TOUR_COUNT;
    tour->shot_count = BT_APP_TOUR_COUNT;
    tour->phase.kind = BT_SMOKE_DONE;
}

BT_TEST(smoke, finish_accepts_only_complete_and_ordered_tours) {
    bt_smoke_tour tour;
    landed_tour(&tour, ta_temp_dir("smoke", "finish-ok"), BT_APP_TOUR, "png");
    BT_CHECK_EQ(bt_smoke_finish(&tour), 0);
}

BT_TEST(smoke, finish_rejects_empty_shot_files) {
    bt_smoke_tour tour;
    landed_tour(&tour, ta_temp_dir("smoke", "finish-empty"), BT_APP_TOUR, "");
    BT_CHECK_EQ(bt_smoke_finish(&tour), 1);
}

BT_TEST(smoke, finish_rejects_out_of_order_tours) {
    bt_app_state reversed[BT_APP_TOUR_COUNT];
    for (size_t i = 0; i < BT_APP_TOUR_COUNT; i++) {
        reversed[i] = BT_APP_TOUR[BT_APP_TOUR_COUNT - 1 - i];
    }
    bt_smoke_tour tour;
    landed_tour(&tour, ta_temp_dir("smoke", "finish-order"), reversed, "png");
    BT_CHECK_EQ(bt_smoke_finish(&tour), 1);
}

BT_TEST(smoke, finish_rejects_shot_paths_named_for_another_state) {
    bt_smoke_tour tour;
    landed_tour(&tour, ta_temp_dir("smoke", "finish-renamed"), BT_APP_TOUR, "png");
    char stray[TA_PATH_MAX];
    const char *dir = ta_temp_dir("smoke", "finish-renamed-parent");
    snprintf(stray, sizeof stray, "%s/renamed.png", dir);
    ta_write_file(stray, "png");
    strcpy(tour.shots[BT_APP_TOUR_COUNT - 1].path, stray);
    BT_CHECK_EQ(bt_smoke_finish(&tour), 1);
}

BT_TEST(smoke, shoot_removes_a_stale_file_before_requesting) {
    const char *dir = ta_temp_dir("smoke", "shoot-stale");
    char path[TA_PATH_MAX];
    snprintf(path, sizeof path, "%s/menu.png", dir);
    ta_write_file(path, "stale");
    bt_smoke_shoot(path);
    unsigned long size = 0;
    BT_CHECK(!file_size(path, &size));
}

BT_TEST(smoke, finish_rejects_suffix_names_that_share_the_component) {
    bt_smoke_tour tour;
    landed_tour(&tour, ta_temp_dir("smoke", "finish-suffix"), BT_APP_TOUR, "png");
    strcpy(tour.shots[0].path, "xmenu.png");
    ta_write_file("xmenu.png", "png");
    BT_CHECK_EQ(bt_smoke_finish(&tour), 1);
    remove("xmenu.png");
}

BT_TEST(smoke, finish_accepts_a_bare_slug_file_name) {
    bt_smoke_tour tour;
    landed_tour(&tour, ta_temp_dir("smoke", "finish-bare"), BT_APP_TOUR, "png");
    strcpy(tour.shots[0].path, "menu.png");
    ta_write_file("menu.png", "png");
    BT_CHECK_EQ(bt_smoke_finish(&tour), 0);
    remove("menu.png");
}
