#include "app/driver_keys.h"
#include "app/theme.h"
#include "app/window.h"
#include "app/window_focus.h"

#include "common.h"

#include "data/corpus.h"

#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { BT_WINDOW_CAPTURE_DWELL_FRAMES = 12 };

static void ref_ground(char out[BT_PATH_MAX]) {
    const char *raw = getenv("BT_REF_GROUND");
    if (raw == NULL || raw[0] == '\0') {
        BT_ABORT("BT_REF_GROUND is unset: run tests through bt.py");
    }
    if (raw[0] == '/' || (raw[1] == ':' && (raw[2] == '\\' || raw[2] == '/'))) {
        snprintf(out, BT_PATH_MAX, "%s", raw);
        return;
    }
    char root[BT_PATH_MAX];
    bt_paths_workspace_root(root);
    snprintf(out, BT_PATH_MAX, "%s\\%s", root, raw);
}

static bt_app_config load_config(void) {
    bt_app_config config;
    bt_app_config_defaults(&config);
    bt_json_value *value = NULL;
    char error[BT_APP_ERROR_MAX];
    char path[BT_PATH_MAX];
    bt_data_config_path(path);
    if (bt_app_load_config_value(path, &value, error, sizeof error) == 0) {
        bt_app_config_from_json(value, &config, error, sizeof error);
    }
    bt_json_free(value);
    return config;
}

BT_TEST(window, probe_opens_the_window_at_the_configured_size) {
    bt_app_config config = load_config();
    BT_CHECK(bt_app_window_open(config.window.width, config.window.height, config.window.title,
                                &config.render));
    BT_CHECK(bt_app_window_ready());
    BT_CHECK(!bt_app_focus_style_no_activate(GetWindowHandle()));
    ClearBackground(bt_app_color(bt_theme_background()));
    bt_app_window_close();
    BT_CHECK(!bt_app_window_ready());
}

BT_TEST(window, driven_open_never_takes_the_foreground) {
    bt_app_focus_sentinel_acquire();
    bt_app_config config = load_config();
    BT_CHECK(bt_app_window_open_driven(config.window.width, config.window.height,
                                       config.window.title, &config.render));
    BT_CHECK(bt_app_window_ready());
    BT_CHECK(bt_app_focus_style_no_activate(GetWindowHandle()));
    BT_CHECK(bt_app_focus_foreground_window() != GetWindowHandle());
    void *watch = bt_app_focus_watch_start(config.window.title, bt_app_focus_current_pid());
    BT_CHECK(watch != NULL);
    for (unsigned frame = 0; frame < BT_WINDOW_CAPTURE_DWELL_FRAMES; frame++) {
        bt_app_window_frame_begin();
        ClearBackground(bt_app_color(bt_theme_background()));
        bt_app_window_frame_end();
    }
    BT_CHECK_EQ(bt_app_focus_watch_app_foreground_count(watch), 0);
    bt_app_focus_watch_stop(watch);
    bt_app_window_close();
    BT_CHECK(!bt_app_window_ready());
    bt_app_focus_sentinel_release();
}

enum {
    BT_WINDOW_MOUSE_FAR_PX = 100,
};

static long point_distance_sq(long ax, long ay, long bx, long by) {
    long dx = ax - bx;
    long dy = ay - by;
    return dx * dx + dy * dy;
}

BT_TEST(window, posted_mouse_drives_input_and_pins_the_real_cursor) {
    bt_app_config config = load_config();
    BT_CHECK(bt_app_window_open_driven(config.window.width, config.window.height,
                                       config.window.title, &config.render));
    void *hwnd = GetWindowHandle();
    long cursor_x = 0;
    long cursor_y = 0;
    BT_CHECK(bt_app_focus_cursor_pos(&cursor_x, &cursor_y));
    const int candidates[6][2] = {
        {120, 90}, {60, 40}, {200, 150}, {30, 20}, {640, 400}, {1000, 700},
    };
    int px = 0;
    int py = 0;
    long far_sq = (long)BT_WINDOW_MOUSE_FAR_PX * BT_WINDOW_MOUSE_FAR_PX;
    for (int at = 0; at < 6; at++) {
        long screen_x = 0;
        long screen_y = 0;
        BT_CHECK(bt_app_focus_client_to_screen(hwnd, candidates[at][0], candidates[at][1],
                                               &screen_x, &screen_y));
        if (point_distance_sq(screen_x, screen_y, cursor_x, cursor_y) > far_sq) {
            px = candidates[at][0];
            py = candidates[at][1];
            break;
        }
    }
    BT_CHECK(px != 0);
    BT_CHECK(bt_app_driver_mouse_move(hwnd, px, py));
    BT_CHECK(bt_app_driver_mouse_left_down(hwnd, px, py));
    bt_app_window_frame_begin();
    ClearBackground(bt_app_color(bt_theme_background()));
    bt_app_window_frame_end();
    bt_ui_input input;
    bt_ui_input_clear(&input);
    bt_app_poll_input(&input);
    BT_CHECK_EQ((int)input.x, px);
    BT_CHECK_EQ((int)input.y, py);
    BT_CHECK_EQ(input.click, 1);
    BT_CHECK_EQ(input.down, 1);
    bt_ui_rect target = {
        .x = (float)px - 20.0F,
        .y = (float)py - 10.0F,
        .width = 40.0F,
        .height = 20.0F,
    };
    BT_CHECK(bt_ui_contains(&target, input.x, input.y));
    BT_CHECK(bt_app_driver_mouse_left_up(hwnd, px, py));
    bt_app_window_frame_begin();
    ClearBackground(bt_app_color(bt_theme_background()));
    bt_app_window_frame_end();
    bt_ui_input_clear(&input);
    bt_app_poll_input(&input);
    BT_CHECK_EQ(input.click, 0);
    BT_CHECK_EQ(input.down, 0);
    long cursor_after_x = 0;
    long cursor_after_y = 0;
    long screen_x = 0;
    long screen_y = 0;
    BT_CHECK(bt_app_focus_cursor_pos(&cursor_after_x, &cursor_after_y));
    BT_CHECK(bt_app_focus_client_to_screen(hwnd, px, py, &screen_x, &screen_y));
    BT_CHECK(point_distance_sq(screen_x, screen_y, cursor_after_x, cursor_after_y) > far_sq);
    bt_app_window_close();
}

BT_TEST(window, probe_font_covers_the_corpus_and_vietnamese) {
    bt_app_config config = load_config();
    BT_CHECK(bt_app_window_open_driven(config.window.width, config.window.height,
                                       config.window.title, &config.render));
    char vendor[BT_PATH_MAX];
    ref_ground(vendor);
    bt_corpus corpus;
    bt_corpus_error err;
    if (bt_corpus_load(vendor, &corpus, &err) != 0) {
        bt_app_window_close();
        BT_ABORT("corpus load failed at %s", vendor);
    }
    bt_app_font_book book;
    char error[BT_APP_ERROR_MAX];
    char path[BT_PATH_MAX];
    bt_data_config_path(path);
    if (bt_app_fonts_load(path, &corpus, NULL, 0, &book, error, sizeof error) != 0) {
        bt_corpus_free(&corpus);
        bt_app_window_close();
        BT_ABORT("fonts: %s", error);
    }
    BT_CHECK(book.regular != NULL && book.bold != NULL);
    const Font *regular = book.regular;
    const Font *bold = book.bold;
    const bt_theme *theme = bt_theme_table();
    int want_base = (int)(theme->font_focal * theme->root_rem + 0.5F);
    BT_CHECK(regular->baseSize == want_base && bold->baseSize == want_base);
    BT_CHECK(book.glyph_count >= 300);
    bt_app_fonts_unload(&book);
    bt_corpus_free(&corpus);
    bt_app_window_close();
}

BT_TEST(window, probe_screenshot_captures_the_window_dims) {
    bt_app_config config = load_config();
    BT_CHECK(bt_app_window_open_driven(config.window.width, config.window.height,
                                       config.window.title, &config.render));
    for (unsigned frame = 0; frame < BT_WINDOW_CAPTURE_DWELL_FRAMES; frame++) {
        bt_app_window_frame_begin();
        ClearBackground(bt_app_color(bt_theme_background()));
        DrawRectangle(40, 40, 200, 80, bt_app_color(bt_theme_accent()));
        bt_app_window_frame_end();
    }
    char shot[BT_PATH_MAX];
    snprintf(shot, sizeof shot, "%s/app-window-probe.png", getenv("TEMP"));
    remove(shot);
    BT_CHECK(bt_app_window_capture(shot));
    FILE *file = fopen(shot, "rb");
    BT_CHECK(file != NULL);
    unsigned char header[24];
    size_t got = fread(header, 1, sizeof header, file);
    fclose(file);
    BT_CHECK_EQ(got, 24);
    BT_CHECK_EQ(header[0], 0x89);
    BT_CHECK_EQ(header[12], 'I');
    unsigned width =
        (((unsigned)header[16] * 256U + (unsigned)header[17]) * 256U + (unsigned)header[18]) *
            256U +
        (unsigned)header[19];
    unsigned height =
        (((unsigned)header[20] * 256U + (unsigned)header[21]) * 256U + (unsigned)header[22]) *
            256U +
        (unsigned)header[23];
    BT_CHECK_EQ(width, config.window.width);
    BT_CHECK_EQ(height, config.window.height);
    remove(shot);
    bt_app_window_close();
}

BT_TEST(window, probe_input_poll_runs_clean_without_devices) {
    bt_app_config config = load_config();
    BT_CHECK(bt_app_window_open_driven(config.window.width, config.window.height,
                                       config.window.title, &config.render));
    bt_app_window_frame_begin();
    ClearBackground(bt_app_color(bt_theme_background()));
    bt_app_window_frame_end();
    bt_app_ctx ctx;
    bt_app_ctx_init(&ctx);
    bt_app_poll_keys(&ctx);
    BT_CHECK_EQ(ctx.key_count, 0);
    bt_ui_input input;
    bt_ui_input_clear(&input);
    bt_app_poll_input(&input);
    BT_CHECK(!input.click && !input.escape && !input.enter && !input.backspace);
    BT_CHECK(input.char_count >= 0);
    bt_app_ctx_free(&ctx);
    bt_app_window_close();
}

static void pump_frame(void) {
    bt_app_window_frame_begin();
    ClearBackground(bt_app_color(bt_theme_background()));
    bt_app_window_frame_end();
}

BT_TEST(window, forced_window_failure_fails_both_open_paths) {
    bt_app_config config = load_config();
    _putenv("BT_APP_WINDOW_FAIL=1");
    BT_CHECK(!bt_app_window_open(config.window.width, config.window.height, config.window.title,
                                 &config.render));
    BT_CHECK(!bt_app_window_open_driven(config.window.width, config.window.height,
                                        config.window.title, &config.render));
    _putenv("BT_APP_WINDOW_FAIL=");
    BT_CHECK(!bt_app_window_ready());
}

BT_TEST(window, vsync_flag_is_honored_on_a_normal_open) {
    bt_app_config config = load_config();
    config.render.vsync = 1;
    BT_CHECK(bt_app_window_open(config.window.width, config.window.height, config.window.title,
                                &config.render));
    BT_CHECK(bt_app_window_ready());
    pump_frame();
    bt_app_window_close();
}

BT_TEST(window, capture_to_a_missing_directory_reports_failure) {
    bt_app_config config = load_config();
    BT_CHECK(bt_app_window_open_driven(config.window.width, config.window.height,
                                       config.window.title, &config.render));
    for (unsigned frame = 0; frame < 3; frame++) {
        pump_frame();
    }
    BT_CHECK(!bt_app_window_capture("Z:/bt-no-such-dir-8471/shot.png"));
    bt_app_window_close();
}

BT_TEST(window, posted_keys_and_wheel_reach_the_input_poll) {
    bt_app_config config = load_config();
    BT_CHECK(bt_app_window_open_driven(config.window.width, config.window.height,
                                       config.window.title, &config.render));
    void *hwnd = GetWindowHandle();
    bt_ui_input input;
    BT_CHECK(bt_app_driver_mouse_wheel(hwnd, 1));
    pump_frame();
    bt_ui_input_clear(&input);
    bt_app_poll_input(&input);
    BT_CHECK_EQ(input.wheel, 1);
    BT_CHECK(bt_app_driver_mouse_wheel(hwnd, -1));
    pump_frame();
    bt_ui_input_clear(&input);
    bt_app_poll_input(&input);
    BT_CHECK_EQ(input.wheel, -1);
    const unsigned int vks[5] = {BT_VK_ESCAPE, BT_VK_RETURN, BT_VK_BACK, BT_VK_UP, BT_VK_DOWN};
    for (int at = 0; at < 5; at++) {
        BT_CHECK(bt_app_driver_key_down(hwnd, vks[at]));
        pump_frame();
        bt_ui_input_clear(&input);
        bt_app_poll_input(&input);
        BT_CHECK_EQ(input.escape, vks[at] == BT_VK_ESCAPE ? 1 : 0);
        BT_CHECK_EQ(input.enter, vks[at] == BT_VK_RETURN ? 1 : 0);
        BT_CHECK_EQ(input.backspace, vks[at] == BT_VK_BACK ? 1 : 0);
        BT_CHECK_EQ(input.arrow_up, vks[at] == BT_VK_UP ? 1 : 0);
        BT_CHECK_EQ(input.arrow_down, vks[at] == BT_VK_DOWN ? 1 : 0);
        BT_CHECK(bt_app_driver_key_up(hwnd, vks[at]));
        pump_frame();
    }
    bt_ui_input_clear(&input);
    bt_app_poll_input(&input);
    BT_CHECK(!input.escape && !input.enter && !input.backspace);
    BT_CHECK(!input.arrow_up && !input.arrow_down);
    bt_app_window_close();
}

static void fonts_fixture_config(const char *tag, const char *fonts_json, char out[BT_PATH_MAX]) {
    const char *dir = ta_temp_dir("window", tag);
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s\\config.json", dir);
    ta_write_file(path, fonts_json);
    snprintf(out, BT_PATH_MAX, "%s", path);
}

static void copy_font_file(const char *dst_dir, const char *name) {
    char root[BT_PATH_MAX];
    bt_paths_workspace_root(root);
    char src[BT_PATH_MAX];
    char dst[BT_PATH_MAX];
    snprintf(src, sizeof src, "%s\\assets\\fonts\\%s", root, name);
    snprintf(dst, sizeof dst, "%s\\%s", dst_dir, name);
    FILE *in = fopen(src, "rb");
    FILE *out = fopen(dst, "wb");
    if (in == NULL || out == NULL) {
        if (in != NULL) {
            fclose(in);
        }
        if (out != NULL) {
            fclose(out);
        }
        BT_ABORT("font copy %s", src);
    }
    char buf[4096];
    size_t got;
    while ((got = fread(buf, 1, sizeof buf, in)) > 0) {
        fwrite(buf, 1, got, out);
    }
    fclose(in);
    fclose(out);
}

BT_TEST(window, fonts_load_reads_face_names_and_paths_from_the_config) {
    bt_app_config config = load_config();
    BT_CHECK(bt_app_window_open_driven(config.window.width, config.window.height,
                                       config.window.title, &config.render));
    char cfg[BT_PATH_MAX];
    fonts_fixture_config("fontsjson",
                         "{\"app\":{\"fonts\":{\"regular\":\"NotoSans-Regular.ttf\","
                         "\"bold\":\"NotoSans-Bold.ttf\",\"codepoint_ranges\":[[32,126]]}},"
                         "\"paths\":{\"fonts\":\"assets/fonts\"}}",
                         cfg);
    bt_app_font_book book;
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_app_fonts_load(cfg, NULL, NULL, 0, &book, error, sizeof error), 0);
    BT_CHECK(book.regular != NULL && book.bold != NULL);
    BT_CHECK(book.glyph_count >= 90);
    BT_CHECK(bt_app_fonts_current() != NULL);
    bt_app_fonts_unload(&book);
    bt_app_window_close();
}

BT_TEST(window, fonts_load_fails_when_the_bold_face_is_missing) {
    bt_app_config config = load_config();
    BT_CHECK(bt_app_window_open_driven(config.window.width, config.window.height,
                                       config.window.title, &config.render));
    const char *dir = ta_temp_dir("window", "fontsbold");
    copy_font_file(dir, "NotoSans-Regular.ttf");
    char cfg[BT_PATH_MAX];
    char json[512];
    char slashed[BT_PATH_MAX];
    snprintf(slashed, sizeof slashed, "%s", dir);
    for (char *at = slashed; *at != '\0'; at++) {
        if (*at == '\\') {
            *at = '/';
        }
    }
    snprintf(json, sizeof json,
             "{\"app\":{\"fonts\":{\"codepoint_ranges\":[[32,126]]}},\"paths\":{\"fonts\":\"%s\"}}",
             slashed);
    fonts_fixture_config("fontsbold", json, cfg);
    bt_app_font_book book;
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_app_fonts_load(cfg, NULL, NULL, 0, &book, error, sizeof error), -1);
    BT_CHECK(strstr(error, "font load failed") != NULL);
    BT_CHECK(bt_app_fonts_current() == NULL);
    bt_app_window_close();
}

BT_TEST(window, fonts_load_rejects_a_font_missing_a_requested_codepoint) {
    bt_app_config config = load_config();
    BT_CHECK(bt_app_window_open_driven(config.window.width, config.window.height,
                                       config.window.title, &config.render));
    char cfg[BT_PATH_MAX];
    fonts_fixture_config("fontscjk",
                         "{\"app\":{\"fonts\":{\"codepoint_ranges\":[[32,126],[20013,20013]]}},"
                         "\"paths\":{\"fonts\":\"assets/fonts\"}}",
                         cfg);
    bt_app_font_book book;
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_app_fonts_load(cfg, NULL, NULL, 0, &book, error, sizeof error), -1);
    BT_CHECK(strstr(error, "holds") != NULL);
    bt_app_window_close();
}

BT_TEST(window, fonts_load_reports_a_codepoint_set_overflow) {
    char cfg[BT_PATH_MAX];
    fonts_fixture_config("fontsoverflow",
                         "{\"app\":{\"fonts\":{\"codepoint_ranges\":[[32,2200]]}},"
                         "\"paths\":{\"fonts\":\"assets/fonts\"}}",
                         cfg);
    bt_app_font_book book;
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_app_fonts_load(cfg, NULL, NULL, 0, &book, error, sizeof error), -1);
    BT_CHECK(strstr(error, "codepoint set exceeds") != NULL);
}
