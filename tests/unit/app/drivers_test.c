#include "app/boot.h"
#include "app/driver_keys.h"
#include "app/drivers.h"
#include "app/window.h"

#include "common.h"

#include "raylib.h"

#include <io.h>
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
    if (!bt_app_window_open(config.window.width, config.window.height, config.window.title,
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
    if (!bt_app_window_open(config.window.width, config.window.height, config.window.title,
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
    if (!bt_app_window_open(config.window.width, config.window.height, config.window.title,
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

BT_TEST(drivers, smoke_main_runs_the_full_tour_green) {
    char saved_appdata[TA_PATH_MAX];
    sandbox_appdata("smoke-user", saved_appdata, sizeof saved_appdata);
    bt_app_ctx ctx;
    bt_app_shot_mode mode;
    char mode_dir[BT_PATH_MAX];
    char error[BT_APP_ERROR_MAX];
    char *argv[] = {(char *)"bt-app", NULL};
    BT_CHECK_EQ(bt_app_init(1, argv, &ctx, &mode, mode_dir, sizeof mode_dir, error, sizeof error),
                0);
    BT_CHECK_EQ(mode, BT_APP_SHOT_NONE);
    const char *dir = ta_temp_dir("drivers", "smoke-tour");
    int code = bt_app_smoke_main(&ctx, dir, 20261012);
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
