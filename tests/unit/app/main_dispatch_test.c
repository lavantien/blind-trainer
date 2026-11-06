#include "../tools/spawn_util.h"
#include "app/driver_keys.h"
#include "app/window_focus.h"

#include "common.h"

typedef struct {
    const char *title;
    volatile int posted;
    volatile int found;
} dispatch_close_probe;

static unsigned __stdcall dispatch_close_poster(void *raw) {
    dispatch_close_probe *probe = raw;
    for (int attempt = 0; attempt < 750 && !probe->posted; attempt++) {
        void *hwnd = bt_app_driver_find_window(probe->title);
        if (hwnd != NULL) {
            probe->found = 1;
            probe->posted = bt_app_driver_close_request(hwnd);
        }
        Sleep(20);
    }
    return 0;
}

static void dispatch_appdata(const char *tag, char *saved, size_t saved_len) {
    const char *current = getenv("APPDATA");
    snprintf(saved, saved_len, "%s", current != NULL ? current : "");
    const char *dir = ta_temp_dir("dispatch", tag);
    char setting[TA_PATH_MAX];
    snprintf(setting, sizeof setting, "APPDATA=%s", dir);
    _putenv(setting);
}

static void dispatch_restore(const char *saved) {
    char setting[TA_PATH_MAX];
    snprintf(setting, sizeof setting, "APPDATA=%s", saved);
    _putenv(setting);
}

static const char *dispatch_config_with_title(const char *tag, const char *title) {
    static char config_path[TA_PATH_MAX];
    const char *dir = ta_temp_dir("dispatch", tag);
    snprintf(config_path, sizeof config_path, "%s/config.json", dir);
    char text[1024];
    snprintf(text, sizeof text,
             "{\"app\":{\"window\":{\"width\":320,\"height\":240,\"title\":\"%s\"},"
             "\"memo\":{\"cadence_ms\":800,\"mode\":\"per-orbit\"},\"drill\":{\"reps\":4}},"
             "\"srs\":{\"fsrs\":{\"weights\":[0.4,0.2,1.0,3.0,15.0,7.0,0.5,1.4,0.004,1.5,0.1,"
             "1.0,1.9,0.1,0.3,2.2,0.2,3.0,0.5],\"request_retention\":0.9,"
             "\"interval_clamp_days\":[1,365]}}}",
             title);
    ta_write_file(config_path, text);
    return config_path;
}

BT_TEST(main_dispatch, rejects_two_shot_flags_with_usage_error) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "bt-app.exe");
    const char *argv[] = {"--smoke", "--usage", "somewhere", NULL};
    int code = -1;
    char out[512];
    char err[512];
    BT_CHECK_EQ(bt_spawn_capture(exe, argv, NULL, 30000, &code, out, sizeof out, err, sizeof err),
                0);
    BT_CHECK_EQ(code, 2);
    BT_CHECK(strstr(err, "only one of") != NULL);
}

BT_TEST(main_dispatch, runs_the_fatal_shot_tour) {
    char saved_appdata[TA_PATH_MAX];
    dispatch_appdata("fatal-user", saved_appdata, sizeof saved_appdata);
    BT_CHECK(bt_app_focus_sentinel_acquire());
    const char *dir = ta_temp_dir("dispatch", "fatal-shot");
    char missing_config[TA_PATH_MAX];
    snprintf(missing_config, sizeof missing_config, "%s/absent-config.json", dir);
    const char *saved_config = getenv("BT_CONFIG");
    char setting[TA_PATH_MAX * 2];
    snprintf(setting, sizeof setting, "BT_CONFIG=%s", missing_config);
    _putenv(setting);
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "bt-app.exe");
    char dir_arg[TA_PATH_MAX];
    snprintf(dir_arg, sizeof dir_arg, "%s", dir);
    const char *argv[] = {"--fatal-shot", dir_arg, NULL};
    int code = -1;
    char out[512];
    char err[512];
    int rc = bt_spawn_capture(exe, argv, NULL, 120000, &code, out, sizeof out, err, sizeof err);
    if (saved_config != NULL && saved_config[0] != '\0') {
        snprintf(setting, sizeof setting, "BT_CONFIG=%s", saved_config);
    } else {
        snprintf(setting, sizeof setting, "BT_CONFIG=");
    }
    _putenv(setting);
    char path[TA_PATH_MAX];
    snprintf(path, sizeof path, "%s/fatal.png", dir);
    struct _stat info;
    int shot = _stat(path, &info) == 0 && info.st_size > 0;
    fprintf(stderr, "main_dispatch: fatal rc %d code %d shot %d\n", rc, code, shot);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 0);
    BT_CHECK(shot);
    bt_app_focus_sentinel_release();
    dispatch_restore(saved_appdata);
}

BT_TEST(main_dispatch, reports_a_font_failure_and_exits_one) {
    char saved_appdata[TA_PATH_MAX];
    dispatch_appdata("fonts-user", saved_appdata, sizeof saved_appdata);
    const char *dir = ta_temp_dir("dispatch", "fonts-fail");
    char config_path[TA_PATH_MAX];
    snprintf(config_path, sizeof config_path, "%s/config.json", dir);
    ta_write_file(config_path,
                  "{\"app\":{\"window\":{\"width\":320,\"height\":240,\"title\":\"w\"},"
                  "\"memo\":{\"cadence_ms\":800,\"mode\":\"per-orbit\"},\"drill\":{\"reps\":4}},"
                  "\"paths\":{\"fonts\":\"definitely-missing-fonts\"}}");
    const char *saved_config = getenv("BT_CONFIG");
    char setting[TA_PATH_MAX * 2];
    snprintf(setting, sizeof setting, "BT_CONFIG=%s", config_path);
    _putenv(setting);
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "bt-app.exe");
    const char *argv[] = {NULL};
    int code = -1;
    char out[512];
    char err[512];
    int rc = bt_spawn_capture(exe, argv, NULL, 60000, &code, out, sizeof out, err, sizeof err);
    if (saved_config != NULL && saved_config[0] != '\0') {
        snprintf(setting, sizeof setting, "BT_CONFIG=%s", saved_config);
    } else {
        snprintf(setting, sizeof setting, "BT_CONFIG=");
    }
    _putenv(setting);
    fprintf(stderr, "main_dispatch: fonts rc %d code %d err %s\n", rc, code, err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 1);
    dispatch_restore(saved_appdata);
}

BT_TEST(main_dispatch, window_open_failure_exits_one) {
    char saved_appdata[TA_PATH_MAX];
    dispatch_appdata("window-fail-user", saved_appdata, sizeof saved_appdata);
    const char *config_path =
        dispatch_config_with_title("window-fail-config", "dispatch window fail probe");
    const char *saved_config = getenv("BT_CONFIG");
    char saved_config_text[TA_PATH_MAX * 2];
    snprintf(saved_config_text, sizeof saved_config_text, "%s",
             saved_config != NULL ? saved_config : "");
    char setting[TA_PATH_MAX * 2];
    snprintf(setting, sizeof setting, "BT_CONFIG=%s", config_path);
    _putenv(setting);
    _putenv("BT_APP_WINDOW_FAIL=1");
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "bt-app.exe");
    const char *argv[] = {NULL};
    int code = -1;
    char out[512];
    char err[512];
    int rc = bt_spawn_capture(exe, argv, NULL, 60000, &code, out, sizeof out, err, sizeof err);
    _putenv("BT_APP_WINDOW_FAIL=");
    snprintf(setting, sizeof setting, "BT_CONFIG=%s", saved_config_text);
    _putenv(setting);
    fprintf(stderr, "main_dispatch: window fail rc %d code %d\n", rc, code);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 1);
    dispatch_restore(saved_appdata);
}

BT_TEST(main_dispatch, interactive_close_returns_zero) {
    char saved_appdata[TA_PATH_MAX];
    dispatch_appdata("interactive-user", saved_appdata, sizeof saved_appdata);
    const char *config_path =
        dispatch_config_with_title("interactive-config", "dispatch interactive probe");
    const char *saved_config = getenv("BT_CONFIG");
    char saved_config_text[TA_PATH_MAX * 2];
    snprintf(saved_config_text, sizeof saved_config_text, "%s",
             saved_config != NULL ? saved_config : "");
    char setting[TA_PATH_MAX * 2];
    snprintf(setting, sizeof setting, "BT_CONFIG=%s", config_path);
    _putenv(setting);
    BT_CHECK(bt_app_focus_sentinel_acquire());
    dispatch_close_probe probe;
    memset(&probe, 0, sizeof probe);
    probe.title = "dispatch interactive probe";
    uintptr_t thread = _beginthreadex(NULL, 0, dispatch_close_poster, &probe, 0, NULL);
    BT_CHECK(thread != 0);
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "bt-app.exe");
    const char *argv[] = {NULL};
    int code = -1;
    char out[512];
    char err[512];
    int rc = bt_spawn_capture(exe, argv, NULL, 120000, &code, out, sizeof out, err, sizeof err);
    for (int attempt = 0; attempt < 100 && !probe.posted; attempt++) {
        Sleep(20);
    }
    snprintf(setting, sizeof setting, "BT_CONFIG=%s", saved_config_text);
    _putenv(setting);
    fprintf(stderr, "main_dispatch: interactive rc %d code %d posted %d\n", rc, code, probe.posted);
    BT_CHECK(probe.posted);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 0);
    bt_app_focus_sentinel_release();
    dispatch_restore(saved_appdata);
}

typedef struct {
    const char *title;
    unsigned int vk;
    volatile int found;
    volatile int posted;
    volatile int closed;
} dispatch_digit_probe;

static unsigned __stdcall dispatch_digit_poster(void *raw) {
    dispatch_digit_probe *probe = raw;
    for (int attempt = 0; attempt < 150 && !probe->closed; attempt++) {
        void *hwnd = bt_app_driver_find_window(probe->title);
        if (hwnd == NULL) {
            Sleep(300);
            continue;
        }
        probe->found = 1;
        if (bt_app_driver_key_down(hwnd, probe->vk) && bt_app_driver_key_up(hwnd, probe->vk)) {
            probe->posted = 1;
        }
        if (attempt >= 20) {
            probe->closed = bt_app_driver_close_request(hwnd);
        }
        Sleep(300);
    }
    return 0;
}

BT_TEST(main_dispatch, runs_the_smoke_tour_green) {
    char saved_appdata[TA_PATH_MAX];
    dispatch_appdata("smoke-user", saved_appdata, sizeof saved_appdata);
    BT_CHECK(bt_app_focus_sentinel_acquire());
    const char *dir = ta_temp_dir("dispatch", "smoke-shots");
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "bt-app.exe");
    char dir_arg[TA_PATH_MAX];
    snprintf(dir_arg, sizeof dir_arg, "%s", dir);
    const char *argv[] = {"--smoke", dir_arg, NULL};
    int code = -1;
    char out[512];
    char err[512];
    int rc = bt_spawn_capture(exe, argv, NULL, 180000, &code, out, sizeof out, err, sizeof err);
    fprintf(stderr, "main_dispatch: smoke rc %d code %d err %s\n", rc, code, err);
    static const char *const slugs[] = {"menu", "drill",      "memo",  "solve",    "replay",
                                        "algs", "case-drill", "stats", "settings", "fatal"};
    int shots = 0;
    for (size_t i = 0; i < sizeof slugs / sizeof slugs[0]; i++) {
        char path[TA_PATH_MAX];
        snprintf(path, sizeof path, "%s/%s.png", dir, slugs[i]);
        struct _stat info;
        if (_stat(path, &info) == 0 && info.st_size > 0) {
            shots++;
        } else {
            fprintf(stderr, "main_dispatch: smoke missing %s\n", path);
        }
    }
    bt_app_focus_sentinel_release();
    dispatch_restore(saved_appdata);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 0);
    BT_CHECK_EQ(shots, 10);
}

BT_TEST(main_dispatch, fatal_boot_loads_fonts_without_corpus_and_closes_zero) {
    char saved_appdata[TA_PATH_MAX];
    dispatch_appdata("corpus-null-user", saved_appdata, sizeof saved_appdata);
    const char *dir = ta_temp_dir("dispatch", "corpus-null");
    char config_path[TA_PATH_MAX];
    snprintf(config_path, sizeof config_path, "%s/config.json", dir);
    ta_write_file(config_path,
                  "{\"app\":{\"window\":{\"width\":320,\"height\":240,"
                  "\"title\":\"dispatch corpus null probe\"},"
                  "\"memo\":{\"cadence_ms\":800,\"mode\":\"per-orbit\"},\"drill\":{\"reps\":4}},"
                  "\"srs\":{\"fsrs\":{\"weights\":[0.5]}}}");
    const char *saved_config = getenv("BT_CONFIG");
    char saved_config_text[TA_PATH_MAX * 2];
    snprintf(saved_config_text, sizeof saved_config_text, "%s",
             saved_config != NULL ? saved_config : "");
    char setting[TA_PATH_MAX * 2];
    snprintf(setting, sizeof setting, "BT_CONFIG=%s", config_path);
    _putenv(setting);
    BT_CHECK(bt_app_focus_sentinel_acquire());
    dispatch_close_probe close_probe;
    memset(&close_probe, 0, sizeof close_probe);
    close_probe.title = "dispatch corpus null probe";
    uintptr_t thread = _beginthreadex(NULL, 0, dispatch_close_poster, &close_probe, 0, NULL);
    BT_CHECK(thread != 0);
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "bt-app.exe");
    const char *argv[] = {NULL};
    int code = -1;
    char out[512];
    char err[512];
    int rc = bt_spawn_capture(exe, argv, NULL, 120000, &code, out, sizeof out, err, sizeof err);
    for (int attempt = 0; attempt < 100 && !close_probe.posted; attempt++) {
        Sleep(20);
    }
    snprintf(setting, sizeof setting, "BT_CONFIG=%s", saved_config_text);
    _putenv(setting);
    fprintf(stderr, "main_dispatch: corpus null rc %d code %d err %s\n", rc, code, err);
    BT_CHECK(close_probe.posted);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 0);
    bt_app_focus_sentinel_release();
    dispatch_restore(saved_appdata);
}

BT_TEST(main_dispatch, interactive_digit_entry_runs_view_update) {
    char saved_appdata[TA_PATH_MAX];
    dispatch_appdata("digit-user", saved_appdata, sizeof saved_appdata);
    const char *config_path = dispatch_config_with_title("digit-config", "dispatch digit probe");
    const char *saved_config = getenv("BT_CONFIG");
    char saved_config_text[TA_PATH_MAX * 2];
    snprintf(saved_config_text, sizeof saved_config_text, "%s",
             saved_config != NULL ? saved_config : "");
    char setting[TA_PATH_MAX * 2];
    snprintf(setting, sizeof setting, "BT_CONFIG=%s", config_path);
    _putenv(setting);
    BT_CHECK(bt_app_focus_sentinel_acquire());
    dispatch_digit_probe probe;
    memset(&probe, 0, sizeof probe);
    probe.title = "dispatch digit probe";
    probe.vk = (unsigned int)'2';
    uintptr_t thread = _beginthreadex(NULL, 0, dispatch_digit_poster, &probe, 0, NULL);
    BT_CHECK(thread != 0);
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "bt-app.exe");
    const char *argv[] = {NULL};
    int code = -1;
    char out[512];
    char err[512];
    int rc = bt_spawn_capture(exe, argv, NULL, 120000, &code, out, sizeof out, err, sizeof err);
    for (int attempt = 0; attempt < 700 && !probe.closed; attempt++) {
        Sleep(20);
    }
    snprintf(setting, sizeof setting, "BT_CONFIG=%s", saved_config_text);
    _putenv(setting);
    fprintf(stderr, "main_dispatch: digit rc %d code %d found %d posted %d closed %d\n", rc, code,
            probe.found, probe.posted, probe.closed);
    BT_CHECK(probe.found);
    BT_CHECK(probe.posted);
    BT_CHECK(probe.closed);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 0);
    bt_app_focus_sentinel_release();
    dispatch_restore(saved_appdata);
}
