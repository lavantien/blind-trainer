#include "app/boot.h"
#include "app/views/drill.h"
#include "app/views/memo.h"
#include "app/views/net_view.h"
#include "app/views/state.h"
#include "app/window.h"
#include "common/bt_trace.h"

#include "raylib.h"

#include <stdio.h>

int bt_app_main(int argc, char **argv) {
    BT_TRACE_ENTER("app.main");
    bt_app_ctx ctx;
    bt_app_shot_mode mode;
    char dir[BT_PATH_MAX];
    char error[BT_APP_ERROR_MAX];
    if (bt_app_init(argc, argv, &ctx, &mode, dir, sizeof dir, error, sizeof error) != 0) {
        fprintf(stderr, "%s\n", error);
        BT_TRACE_FAIL("app.main", "%s", error);
        BT_TRACE_EXIT("app.main");
        return 2;
    }
    if (!bt_app_window_open(ctx.config.window.width, ctx.config.window.height,
                            ctx.config.window.title, &ctx.config.render)) {
        bt_app_ctx_free(&ctx);
        BT_TRACE_FAIL("app.main", "window open failed");
        BT_TRACE_EXIT("app.main");
        return 1;
    }
    bt_app_font_book book;
    char font_error[BT_APP_ERROR_MAX];
    const bt_corpus *corpus = ctx.has_corpus ? &ctx.vendor_corpus : NULL;
    int fonts_ok = bt_app_fonts_load(ctx.paths.config_path, corpus, NULL, 0, &book, font_error,
                                     sizeof font_error);
    if (fonts_ok != 0) {
        fprintf(stderr, "%s\n", font_error);
        bt_app_window_close();
        bt_app_ctx_free(&ctx);
        BT_TRACE_FAIL("app.main", "%s", font_error);
        BT_TRACE_EXIT("app.main");
        return 1;
    }
    bt_ui_input input;
    BT_APP_STATE_TABLE[ctx.state].enter(&ctx);
    bt_app_apply_pending(&ctx);
    while (!WindowShouldClose()) {
        bt_app_poll_keys(&ctx);
        bt_app_process_keys(&ctx);
        bt_ui_input_clear(&input);
        bt_app_poll_input(&input);
        if (BT_APP_STATE_TABLE[ctx.state].update != NULL) {
            BT_APP_STATE_TABLE[ctx.state].update(&ctx);
        }
        if (bt_app_apply_pending(&ctx) && BT_APP_STATE_TABLE[ctx.state].enter != NULL) {
            BT_APP_STATE_TABLE[ctx.state].enter(&ctx);
            bt_app_apply_pending(&ctx);
        }
        bt_view_net_update(&ctx.net, &bt_app_views_shared()->net, GetFrameTime());
        bt_app_window_frame_begin();
        ClearBackground(bt_app_color(bt_theme_background()));
        if (BT_APP_STATE_TABLE[ctx.state].draw != NULL) {
            BT_APP_STATE_TABLE[ctx.state].draw(&ctx);
        }
        bt_app_window_frame_end();
    }
    bt_app_fonts_unload(&book);
    bt_app_window_close();
    bt_app_ctx_free(&ctx);
    bt_app_views_dispose_shared();
    bt_view_drill_app_free(bt_view_drill_app());
    bt_view_memo_app_free(bt_view_memo_app());
    BT_TRACE_EXIT("app.main");
    return 0;
}

int main(int argc, char **argv) {
    return bt_app_main(argc, argv);
}
