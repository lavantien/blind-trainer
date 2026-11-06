#include "../tools/spawn_util.h"
#include "app/window_focus.h"

#include "common.h"

BT_TEST(main_usage, runs_the_usage_tour_green) {
    const char *current = getenv("APPDATA");
    char saved[TA_PATH_MAX];
    snprintf(saved, sizeof saved, "%s", current != NULL ? current : "");
    const char *user = ta_temp_dir("main_usage", "user");
    char setting[TA_PATH_MAX];
    snprintf(setting, sizeof setting, "APPDATA=%s", user);
    _putenv(setting);
    bt_app_focus_sentinel_acquire();
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "bt-app.exe");
    const char *dir = ta_temp_dir("main_usage", "shots");
    char dir_arg[TA_PATH_MAX];
    snprintf(dir_arg, sizeof dir_arg, "%s", dir);
    const char *argv[] = {"--usage", dir_arg, NULL};
    int code = -1;
    char out[512];
    char err[512];
    int rc = bt_spawn_capture(exe, argv, NULL, 280000, &code, out, sizeof out, err, sizeof err);
    fprintf(stderr, "main_usage: rc %d code %d err %s\n", rc, code, err);
    bt_app_focus_sentinel_release();
    snprintf(setting, sizeof setting, "APPDATA=%s", saved);
    _putenv(setting);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 0);
}
