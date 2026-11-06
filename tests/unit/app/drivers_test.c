#include "app/boot.h"
#include "app/driver_keys.h"
#include "app/drivers.h"
#include "app/theme.h"
#include "app/window.h"
#include "app/window_focus.h"

#include "common.h"

#include "raylib.h"

#include <io.h>
#include <process.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

BT_TEST(drivers, vk_of_maps_the_script_keys_to_virtual_keys) {
    BT_CHECK_EQ(bt_app_driver_vk_of(KEY_SPACE), (unsigned)BT_VK_SPACE);
    BT_CHECK_EQ(bt_app_driver_vk_of(KEY_ENTER), (unsigned)BT_VK_RETURN);
    BT_CHECK_EQ(bt_app_driver_vk_of(KEY_ESCAPE), (unsigned)BT_VK_ESCAPE);
    BT_CHECK_EQ(bt_app_driver_vk_of(KEY_DOWN), (unsigned)BT_VK_DOWN);
    BT_CHECK_EQ(bt_app_driver_vk_of(KEY_UP), (unsigned)BT_VK_UP);
    BT_CHECK_EQ(bt_app_driver_vk_of(KEY_RIGHT), (unsigned)BT_VK_RIGHT);
    BT_CHECK_EQ(bt_app_driver_vk_of(KEY_THREE), (unsigned)'3');
    BT_CHECK_EQ(bt_app_driver_vk_of(KEY_ONE), (unsigned)'1');
    BT_CHECK_EQ(bt_app_driver_vk_of(KEY_FOUR), (unsigned)'4');
    BT_CHECK_EQ(bt_app_driver_vk_of(KEY_SLASH), (unsigned)BT_VK_OEM_2);
    BT_CHECK_EQ(bt_app_driver_vk_of(KEY_B), (unsigned)'B');
    BT_CHECK_EQ(bt_app_driver_vk_of(KEY_D), (unsigned)'D');
    BT_CHECK_EQ(bt_app_driver_vk_of(KEY_O), (unsigned)'O');
    BT_CHECK_EQ(bt_app_driver_vk_of(KEY_X), (unsigned)'X');
    BT_CHECK_EQ(bt_app_driver_vk_of(KEY_V), (unsigned)'V');
    BT_CHECK_EQ(bt_app_driver_vk_of(KEY_TAB), (unsigned)BT_VK_TAB);
    BT_CHECK_EQ(bt_app_driver_vk_of(KEY_BACKSPACE), (unsigned)BT_VK_BACK);
    BT_CHECK_EQ(bt_app_driver_vk_of(KEY_Z), (unsigned)0);
}

BT_TEST(drivers, posted_arrow_keys_reach_the_polled_key_state) {
    bt_app_config config;
    bt_app_config_defaults(&config);
    config.window.width = 320;
    config.window.height = 240;
    snprintf(config.window.title, sizeof config.window.title, "%s", "drivers arrow test");
    if (!bt_app_window_open_driven(config.window.width, config.window.height, config.window.title,
                                   &config.render)) {
        return;
    }
    void *hwnd = GetWindowHandle();
    BT_CHECK(bt_app_driver_key_down(hwnd, bt_app_driver_vk_of(KEY_RIGHT)));
    BeginDrawing();
    EndDrawing();
    BT_CHECK(IsKeyPressed(KEY_RIGHT));
    BT_CHECK(bt_app_driver_key_up(hwnd, bt_app_driver_vk_of(KEY_RIGHT)));
    BeginDrawing();
    EndDrawing();
    BT_CHECK(!IsKeyPressed(KEY_RIGHT));
    CloseWindow();
}

BT_TEST(drivers, posted_escape_routes_the_app_to_the_menu) {
    bt_app_config config;
    bt_app_config_defaults(&config);
    config.window.width = 320;
    config.window.height = 240;
    snprintf(config.window.title, sizeof config.window.title, "%s", "drivers escape test");
    if (!bt_app_window_open_driven(config.window.width, config.window.height, config.window.title,
                                   &config.render)) {
        return;
    }
    bt_app_ctx ctx;
    bt_app_ctx_init(&ctx);
    bt_app_apply_pending(&ctx);
    ctx.state = BT_APP_ALGS;
    ctx.input_focus = 1;
    bt_app_request_state(&ctx, BT_APP_MENU);
    bt_app_apply_pending(&ctx);
    bt_app_request_state(&ctx, BT_APP_ALGS);
    bt_app_apply_pending(&ctx);
    void *hwnd = GetWindowHandle();
    BT_CHECK(bt_app_driver_key_down(hwnd, bt_app_driver_vk_of(KEY_ESCAPE)));
    BeginDrawing();
    EndDrawing();
    bt_app_poll_keys(&ctx);
    bt_app_process_keys(&ctx);
    bt_app_apply_pending(&ctx);
    BT_CHECK(ctx.state == BT_APP_MENU);
    BT_CHECK(bt_app_driver_key_up(hwnd, bt_app_driver_vk_of(KEY_ESCAPE)));
    BeginDrawing();
    EndDrawing();
    bt_app_ctx_free(&ctx);
    CloseWindow();
}

BT_TEST(drivers, escape_keeps_the_window_open_while_close_still_works) {
    bt_app_config config;
    bt_app_config_defaults(&config);
    config.window.width = 320;
    config.window.height = 240;
    snprintf(config.window.title, sizeof config.window.title, "%s", "drivers close test");
    if (!bt_app_window_open_driven(config.window.width, config.window.height, config.window.title,
                                   &config.render)) {
        return;
    }
    void *hwnd = GetWindowHandle();
    BT_CHECK(bt_app_driver_key_down(hwnd, bt_app_driver_vk_of(KEY_ESCAPE)));
    BeginDrawing();
    EndDrawing();
    BT_CHECK(!WindowShouldClose());
    BT_CHECK(bt_app_driver_key_up(hwnd, bt_app_driver_vk_of(KEY_ESCAPE)));
    BeginDrawing();
    EndDrawing();
    BT_CHECK(!WindowShouldClose());
    BT_CHECK(bt_app_driver_close_request(hwnd));
    BeginDrawing();
    EndDrawing();
    BT_CHECK(WindowShouldClose());
    CloseWindow();
}

BT_TEST(drivers, window_config_setup_initializes_the_render_fields_the_window_consumes) {
    bt_app_config config;
    bt_app_config reference;
    memset(&config, 0xA5, sizeof config);
    bt_app_config_defaults(&config);
    bt_app_config_defaults(&reference);
    config.window.width = 320;
    config.window.height = 240;
    snprintf(config.window.title, sizeof config.window.title, "%s", "drivers config pin");
    BT_CHECK_EQ(config.render.target_fps, reference.render.target_fps);
    BT_CHECK_EQ(config.render.vsync, reference.render.vsync);
}

static void sandbox_appdata(const char *tag, char *saved, size_t saved_len) {
    const char *current = getenv("APPDATA");
    snprintf(saved, saved_len, "%s", current != NULL ? current : "");
    const char *dir = ta_temp_dir("drivers", tag);
    char setting[TA_PATH_MAX];
    snprintf(setting, sizeof setting, "APPDATA=%s", dir);
    _putenv(setting);
}

static void restore_appdata(const char *saved) {
    char setting[TA_PATH_MAX];
    snprintf(setting, sizeof setting, "APPDATA=%s", saved);
    _putenv(setting);
}

static const char *config_with_title(const char *tag, const char *title) {
    static char config_path[TA_PATH_MAX];
    const char *dir = ta_temp_dir("drivers", tag);
    snprintf(config_path, sizeof config_path, "%s/config.json", dir);
    char text[512];
    snprintf(text, sizeof text,
             "{\"app\":{\"window\":{\"width\":320,\"height\":240,\"title\":\"%s\"},"
             "\"memo\":{\"cadence_ms\":800,\"mode\":\"per-orbit\"},\"drill\":{\"reps\":4}}}",
             title);
    ta_write_file(config_path, text);
    return config_path;
}

static void swap_config(const char *path, char *saved, size_t saved_len) {
    const char *current = getenv("BT_CONFIG");
    snprintf(saved, saved_len, "%s", current != NULL && current[0] != '\0' ? current : "");
    char setting[TA_PATH_MAX * 2];
    snprintf(setting, sizeof setting, "BT_CONFIG=%s", path);
    _putenv(setting);
}

static void unswap_config(const char *saved) {
    char setting[TA_PATH_MAX * 2];
    snprintf(setting, sizeof setting, "BT_CONFIG=%s", saved);
    _putenv(setting);
}

typedef struct {
    const char *title;
    volatile int posted;
    volatile int found;
} smoke_close_probe;

static unsigned __stdcall smoke_close_poster(void *raw) {
    smoke_close_probe *probe = raw;
    for (int attempt = 0; attempt < 750 && !probe->posted; attempt++) {
        void *hwnd = bt_app_driver_find_window(probe->title);
        if (hwnd != NULL) {
            probe->found = 1;
            probe->posted = bt_app_driver_close_request(hwnd);
        }
        WaitTime(0.02);
    }
    return 0;
}

BT_TEST(drivers, smoke_interrupted_by_window_close_exits_nonzero) {
    char saved_appdata[TA_PATH_MAX];
    sandbox_appdata("smoke-close-user", saved_appdata, sizeof saved_appdata);
    char saved_config[TA_PATH_MAX];
    swap_config(config_with_title("smoke-close-config", "drivers interrupted probe"), saved_config,
                sizeof saved_config);
    bt_app_focus_sentinel_acquire();
    bt_app_ctx ctx;
    bt_app_shot_mode mode;
    char mode_dir[BT_PATH_MAX];
    char error[BT_APP_ERROR_MAX];
    char *argv[] = {(char *)"bt-app", NULL};
    BT_CHECK_EQ(bt_app_init(1, argv, &ctx, &mode, mode_dir, sizeof mode_dir, error, sizeof error),
                0);
    const char *dir = ta_temp_dir("drivers", "smoke-interrupted");
    smoke_close_probe probe;
    memset(&probe, 0, sizeof probe);
    probe.title = ctx.config.window.title;
    uintptr_t thread = _beginthreadex(NULL, 0, smoke_close_poster, &probe, 0, NULL);
    BT_CHECK(thread != 0);
    int code = bt_app_smoke_main(&ctx, dir, 20261012);
    for (int attempt = 0; attempt < 100 && !probe.posted; attempt++) {
        WaitTime(0.02);
    }
    fprintf(stderr, "drivers: interrupted smoke code %d posted %d\n", code, probe.posted);
    BT_CHECK(probe.posted);
    BT_CHECK_EQ(code, 1);
    bt_app_focus_sentinel_release();
    unswap_config(saved_config);
    restore_appdata(saved_appdata);
}

BT_TEST(drivers, smoke_main_runs_the_full_tour_green) {
    char saved_appdata[TA_PATH_MAX];
    sandbox_appdata("smoke-user", saved_appdata, sizeof saved_appdata);
    bt_app_focus_sentinel_acquire();
    bt_app_ctx ctx;
    bt_app_shot_mode mode;
    char mode_dir[BT_PATH_MAX];
    char error[BT_APP_ERROR_MAX];
    char *argv[] = {(char *)"bt-app", NULL};
    BT_CHECK_EQ(bt_app_init(1, argv, &ctx, &mode, mode_dir, sizeof mode_dir, error, sizeof error),
                0);
    BT_CHECK_EQ(mode, BT_APP_SHOT_NONE);
    void *watch = bt_app_focus_watch_start(ctx.config.window.title, bt_app_focus_current_pid());
    BT_CHECK(watch != NULL);
    const char *dir = ta_temp_dir("drivers", "smoke-tour");
    int code = bt_app_smoke_main(&ctx, dir, 20261012);
    BT_CHECK_EQ(bt_app_focus_watch_app_foreground_count(watch), 0);
    BT_CHECK_EQ(bt_app_focus_watch_cursor_jump_count(watch), 0);
    bt_app_focus_watch_stop(watch);
    bt_app_focus_sentinel_release();
    if (code != 0) {
        fprintf(stderr, "drivers: smoke code %d fatal [%s]\n", code, ctx.fatal_reason);
    }
    BT_CHECK_EQ(code, 0);
    restore_appdata(saved_appdata);
    for (size_t i = 0; i < BT_APP_TOUR_COUNT; i++) {
        const char *slug = bt_app_state_slug(BT_APP_TOUR[i]);
        char path[TA_PATH_MAX];
        snprintf(path, sizeof path, "%s/%s.png", dir, slug);
        struct _stat info;
        if (_stat(path, &info) != 0 || info.st_size <= 0) {
            fprintf(stderr, "drivers: missing shot %s (code %d)\n", path, code);
        }
        BT_CHECK(_stat(path, &info) == 0);
        BT_CHECK(info.st_size > 0);
    }
}

static int shot_has_distinct_colors(const char *path) {
    Image image = LoadImage(path);
    if (image.width <= 0 || image.height <= 0) {
        UnloadImage(image);
        return 0;
    }
    Color *pixels = LoadImageColors(image);
    int first_r = pixels[0].r;
    int first_g = pixels[0].g;
    int first_b = pixels[0].b;
    int distinct = 0;
    for (int y = 0; !distinct && y < image.height; y++) {
        for (int x = 0; !distinct && x < image.width; x++) {
            Color c = pixels[y * image.width + x];
            if (c.r != first_r || c.g != first_g || c.b != first_b) {
                distinct = 1;
            }
        }
    }
    UnloadImageColors(pixels);
    UnloadImage(image);
    return distinct;
}

BT_TEST(drivers, smoke_tour_shots_carry_distinct_colors) {
    char saved_appdata[TA_PATH_MAX];
    sandbox_appdata("smoke-colors-user", saved_appdata, sizeof saved_appdata);
    bt_app_ctx ctx;
    bt_app_shot_mode mode;
    char mode_dir[BT_PATH_MAX];
    char error[BT_APP_ERROR_MAX];
    char *argv[] = {(char *)"bt-app", NULL};
    BT_CHECK_EQ(bt_app_init(1, argv, &ctx, &mode, mode_dir, sizeof mode_dir, error, sizeof error),
                0);
    const char *dir = ta_temp_dir("drivers", "smoke-colors");
    BT_CHECK_EQ(bt_app_smoke_main(&ctx, dir, 20261012), 0);
    for (size_t i = 0; i < BT_APP_TOUR_COUNT; i++) {
        if (BT_APP_TOUR[i] == BT_APP_FATAL) {
            continue;
        }
        char path[TA_PATH_MAX];
        snprintf(path, sizeof path, "%s/%s.png", dir, bt_app_state_slug(BT_APP_TOUR[i]));
        if (!shot_has_distinct_colors(path)) {
            fprintf(stderr, "drivers: blank shot %s\n", path);
        }
        BT_CHECK(shot_has_distinct_colors(path));
    }
    restore_appdata(saved_appdata);
}

BT_TEST(drivers, smoke_main_fails_when_the_output_dir_cannot_exist) {
    char saved_appdata[TA_PATH_MAX];
    sandbox_appdata("smoke-bad-dir-user", saved_appdata, sizeof saved_appdata);
    bt_app_ctx ctx;
    bt_app_shot_mode mode;
    char mode_dir[BT_PATH_MAX];
    char error[BT_APP_ERROR_MAX];
    char *argv[] = {(char *)"bt-app", NULL};
    BT_CHECK_EQ(bt_app_init(1, argv, &ctx, &mode, mode_dir, sizeof mode_dir, error, sizeof error),
                0);
    int code = bt_app_smoke_main(&ctx, "nul/bad-dir", 20261012);
    BT_CHECK_EQ(code, 1);
    restore_appdata(saved_appdata);
}

BT_TEST(drivers, driven_font_path_sets_the_root_for_the_window_height) {
    char saved_appdata[TA_PATH_MAX];
    sandbox_appdata("smoke-root-user", saved_appdata, sizeof saved_appdata);
    bt_app_ctx ctx;
    bt_app_shot_mode mode;
    char mode_dir[BT_PATH_MAX];
    char error[BT_APP_ERROR_MAX];
    char *argv[] = {(char *)"bt-app", NULL};
    BT_CHECK_EQ(bt_app_init(1, argv, &ctx, &mode, mode_dir, sizeof mode_dir, error, sizeof error),
                0);
    unsigned int real_height = ctx.config.window.height;
    unsigned int probe_height = real_height > 600U ? 600U : 1200U;
    ctx.config.window.height = probe_height;
    bt_theme_set_root_for((float)real_height);
    float before = bt_theme_table()->root_rem;
    bt_theme_set_root_for((float)probe_height);
    float expected = bt_theme_table()->root_rem;
    bt_theme_set_root_for((float)real_height);
    BT_CHECK(expected != before);
    const char *dir = ta_temp_dir("drivers", "smoke-root");
    int code = bt_app_smoke_main(&ctx, dir, 20261012);
    if (code != 0) {
        fprintf(stderr, "drivers: root probe smoke code %d fatal [%s]\n", code, ctx.fatal_reason);
    }
    BT_CHECK_EQ(code, 0);
    BT_CHECK_EQ(bt_theme_table()->root_rem, expected);
    bt_theme_set_root_for((float)real_height);
    restore_appdata(saved_appdata);
}
