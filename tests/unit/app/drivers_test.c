#include "app/boot.h"
#include "app/driver_keys.h"
#include "app/drivers.h"

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
    BT_CHECK_EQ(bt_app_driver_vk_of(KEY_Z), (unsigned)0);
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
    BT_CHECK_EQ(bt_app_init(1, argv, &ctx, &mode, mode_dir, sizeof mode_dir, error,
                           sizeof error),
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

BT_TEST(drivers, smoke_main_fails_when_the_output_dir_cannot_exist) {
    char saved_appdata[TA_PATH_MAX];
    sandbox_appdata("smoke-bad-dir-user", saved_appdata, sizeof saved_appdata);
    bt_app_ctx ctx;
    bt_app_shot_mode mode;
    char mode_dir[BT_PATH_MAX];
    char error[BT_APP_ERROR_MAX];
    char *argv[] = {(char *)"bt-app", NULL};
    BT_CHECK_EQ(bt_app_init(1, argv, &ctx, &mode, mode_dir, sizeof mode_dir, error,
                           sizeof error),
                0);
    int code = bt_app_smoke_main(&ctx, "nul/bad-dir", 20261012);
    BT_CHECK_EQ(code, 1);
    restore_appdata(saved_appdata);
}
