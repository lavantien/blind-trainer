#include "app/theme.h"
#include "app/views/fatal.h"
#include "app/window.h"

#include "common.h"

#include "raylib.h"
#include "rlgl.h"

#include <stdio.h>
#include <string.h>

static void ctx_with(const char *reason, const char *const *warnings, size_t count,
                     bt_app_ctx *ctx) {
    bt_app_ctx_init(ctx);
    snprintf(ctx->fatal_reason, sizeof ctx->fatal_reason, "%s", reason);
    bt_str_list_init(&ctx->boot_warnings);
    for (size_t i = 0; i < count; i++) {
        bt_str_list_push(&ctx->boot_warnings, warnings[i]);
    }
}

BT_TEST(view_fatal, lines_lead_with_the_reason_then_the_warnings) {
    const char *warnings[] = {"seed pack trimmed", "solve log read failed"};
    bt_app_ctx ctx;
    ctx_with("cannot read config", warnings, 2, &ctx);
    char lines[BT_FATAL_LINES_MAX][BT_FATAL_TEXT_MAX];
    size_t count = bt_view_fatal_lines(&ctx, lines, BT_FATAL_LINES_MAX);
    BT_CHECK_EQ(count, (size_t)3);
    BT_CHECK(strcmp(lines[0], "cannot read config") == 0);
    BT_CHECK(strcmp(lines[1], "seed pack trimmed") == 0);
    BT_CHECK(strcmp(lines[2], "solve log read failed") == 0);
    bt_app_ctx_free(&ctx);
}

BT_TEST(view_fatal, an_empty_reason_still_leads_the_lines) {
    bt_app_ctx ctx;
    ctx_with("", NULL, 0, &ctx);
    char lines[BT_FATAL_LINES_MAX][BT_FATAL_TEXT_MAX];
    size_t count = bt_view_fatal_lines(&ctx, lines, BT_FATAL_LINES_MAX);
    BT_CHECK_EQ(count, (size_t)1);
    BT_CHECK_EQ(lines[0][0], '\0');
    bt_app_ctx_free(&ctx);
}

BT_TEST(view_fatal, lines_stop_at_the_capacity_bound) {
    const char *warnings[BT_FATAL_LINES_MAX + 8];
    char texts[BT_FATAL_LINES_MAX + 8][16];
    for (size_t i = 0; i < BT_FATAL_LINES_MAX + 8; i++) {
        snprintf(texts[i], sizeof texts[i], "w%zu", i);
        warnings[i] = texts[i];
    }
    bt_app_ctx ctx;
    ctx_with("reason", warnings, BT_FATAL_LINES_MAX + 8, &ctx);
    char lines[BT_FATAL_LINES_MAX][BT_FATAL_TEXT_MAX];
    size_t count = bt_view_fatal_lines(&ctx, lines, BT_FATAL_LINES_MAX);
    BT_CHECK_EQ(count, (size_t)BT_FATAL_LINES_MAX);
    bt_app_ctx_free(&ctx);
}

static int fatal_frame_has_variance(const bt_app_ctx *ctx, const char *path) {
    if (!bt_app_window_open(640, 480, "fatal view test", NULL)) {
        return 0;
    }
    bt_app_font_book book;
    char error[BT_APP_ERROR_MAX];
    if (bt_app_fonts_load("config/config.json", NULL, NULL, 0, &book, error, sizeof error) != 0) {
        bt_app_window_close();
        return 0;
    }
    for (int i = 0; i < 6; i++) {
        BeginDrawing();
        ClearBackground(bt_app_color(bt_theme_background()));
        bt_view_fatal_draw(ctx, book.regular, book.bold, 640.0F, 480.0F);
        if (i == 5) {
            rlDrawRenderBatchActive();
            TakeScreenshot(path);
        }
        EndDrawing();
    }
    int ok = 0;
    Image image = LoadImage(path);
    if (image.width > 0) {
        Color *pixels = LoadImageColors(image);
        int distinct = 0;
        for (int y = 0; !distinct && y < image.height; y++) {
            for (int x = 1; !distinct && x < image.width; x++) {
                Color left = pixels[y * image.width + x - 1];
                Color right = pixels[y * image.width + x];
                if (left.r != right.r || left.g != right.g || left.b != right.b) {
                    distinct = 1;
                }
            }
        }
        UnloadImageColors(pixels);
        ok = distinct;
    }
    UnloadImage(image);
    bt_app_fonts_unload(&book);
    bt_app_window_close();
    return ok;
}

BT_TEST(view_fatal, the_frame_renders_title_and_lines_under_a_real_window) {
    const char *warnings[] = {"seed pack trimmed"};
    bt_app_ctx ctx;
    ctx_with("cannot read config", warnings, 1, &ctx);
    const char *dir = ta_temp_dir("fatal", "probe");
    char path[TA_PATH_MAX];
    snprintf(path, sizeof path, "%s/fatal.png", dir);
    remove(path);
    int framed = fatal_frame_has_variance(&ctx, path);
    BT_CHECK(framed);
    bt_app_ctx_free(&ctx);
}
