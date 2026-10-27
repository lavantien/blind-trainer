#include "app/window.h"

#include "common.h"

#include "data/corpus.h"

#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    ClearBackground(bt_app_color(bt_theme_background()));
    bt_app_window_close();
    BT_CHECK(!bt_app_window_ready());
}

BT_TEST(window, probe_font_covers_the_corpus_and_vietnamese) {
    bt_app_config config = load_config();
    BT_CHECK(bt_app_window_open(config.window.width, config.window.height, config.window.title,
                                &config.render));
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
    BT_CHECK(book.glyph_count >= 300);
    bt_app_fonts_unload(&book);
    bt_corpus_free(&corpus);
    bt_app_window_close();
}

BT_TEST(window, probe_screenshot_captures_the_window_dims) {
    bt_app_config config = load_config();
    BT_CHECK(bt_app_window_open(config.window.width, config.window.height, config.window.title,
                                &config.render));
    for (unsigned frame = 0; frame < config.render.capture_dwell_frames; frame++) {
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
    BT_CHECK(bt_app_window_open(config.window.width, config.window.height, config.window.title,
                                &config.render));
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
