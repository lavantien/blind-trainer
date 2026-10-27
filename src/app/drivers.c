#include "app/drivers.h"

#include "app/boot.h"
#include "app/driver_keys.h"
#include "app/smoke.h"
#include "app/theme.h"
#include "app/usage.h"
#include "app/usage_script.h"
#include "app/views/case_drill.h"
#include "app/views/drill.h"
#include "app/views/memo.h"
#include "app/views/net_view.h"
#include "app/views/replay.h"
#include "app/views/settings.h"
#include "app/views/solve.h"
#include "app/views/state.h"
#include "app/views/stats.h"
#include "app/window.h"
#include "common/bt_trace.h"

#include "raylib.h"
#include "rlgl.h"

#include <direct.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

unsigned int bt_app_driver_vk_of(int raylib_key) {
    BT_TRACE_ENTER("app.driver_vk_of");
    unsigned int vk = 0;
    switch (raylib_key) {
    case KEY_SPACE:
        vk = BT_VK_SPACE;
        break;
    case KEY_ENTER:
        vk = BT_VK_RETURN;
        break;
    case KEY_ESCAPE:
        vk = BT_VK_ESCAPE;
        break;
    case KEY_DOWN:
        vk = BT_VK_DOWN;
        break;
    case KEY_UP:
        vk = BT_VK_UP;
        break;
    case KEY_RIGHT:
        vk = BT_VK_RIGHT;
        break;
    case KEY_LEFT:
        vk = BT_VK_LEFT;
        break;
    case KEY_SLASH:
        vk = BT_VK_OEM_2;
        break;
    case KEY_ONE:
        vk = '1';
        break;
    case KEY_TWO:
        vk = '2';
        break;
    case KEY_THREE:
        vk = '3';
        break;
    case KEY_FOUR:
        vk = '4';
        break;
    case KEY_B:
        vk = 'B';
        break;
    case KEY_D:
        vk = 'D';
        break;
    case KEY_N:
        vk = 'N';
        break;
    case KEY_P:
        vk = 'P';
        break;
    case KEY_O:
        vk = 'O';
        break;
    case KEY_X:
        vk = 'X';
        break;
    case KEY_V:
        vk = 'V';
        break;
    case KEY_TAB:
        vk = BT_VK_TAB;
        break;
    case KEY_BACKSPACE:
        vk = BT_VK_BACK;
        break;
    default:
        vk = 0;
        break;
    }
    BT_TRACE_EXIT("app.driver_vk_of");
    return vk;
}

static int dir_ready(const char *dir) {
    struct _stat info;
    if (_stat(dir, &info) == 0) {
        return 1;
    }
    return _mkdir(dir) == 0;
}

static int open_window_and_fonts(bt_app_ctx *ctx, bt_app_font_book *book) {
    if (!bt_app_window_open(ctx->config.window.width, ctx->config.window.height,
                            ctx->config.window.title, &ctx->config.render)) {
        BT_TRACE_FAIL("app.drivers", "window open failed");
        return 0;
    }
    char font_error[BT_APP_ERROR_MAX];
    const bt_corpus *corpus = ctx->has_corpus ? &ctx->vendor_corpus : NULL;
    if (bt_app_fonts_load(ctx->paths.config_path, corpus, NULL, 0, book, font_error,
                          sizeof font_error) != 0) {
        fprintf(stderr, "%s\n", font_error);
        bt_app_window_close();
        BT_TRACE_FAIL("app.drivers", "%s", font_error);
        return 0;
    }
    return 1;
}

static void driver_teardown(bt_app_ctx *ctx, bt_app_font_book *book) {
    BT_TRACE_ENTER("app.driver_teardown");
    if (book != NULL) {
        bt_app_fonts_unload(book);
    }
    bt_app_window_close();
    bt_app_ctx_free(ctx);
    bt_app_views_dispose_shared();
    bt_view_drill_app_free(bt_view_drill_app());
    bt_view_memo_app_free(bt_view_memo_app());
    bt_view_replay_app_free();
    bt_view_stats_app_free(bt_view_stats_app());
    bt_view_settings_app_free(bt_view_settings_app());
    BT_TRACE_EXIT("app.driver_teardown");
}

static void arm_smoke_resources(uint64_t seed) {
    uint64_t seeds[3];
    bt_smoke_seeds(seed, seeds);
    bt_view_memo_app()->seed = seeds[0];
    bt_view_solve_app()->seed = seeds[1];
    bt_view_drill_app()->seed = seeds[2];
    bt_drill_case tour_case;
    memset(&tour_case, 0, sizeof tour_case);
    strcpy(tour_case.case_ref.event, "3x3");
    strcpy(tour_case.case_ref.method, "cfop");
    strcpy(tour_case.case_ref.stage, "pll");
    strcpy(tour_case.case_ref.case_key, "smoke");
    strcpy(tour_case.seed_alg, "R U R'");
    strcpy(tour_case.alg, "R U R'");
    bt_drill_case_queue tour_queue;
    bt_drill_case_queue_init(&tour_queue);
    bt_drill_case_queue_push(&tour_queue, &tour_case);
    bt_view_case_drill_set_pending(&tour_queue);
    bt_drill_case_queue_free(&tour_queue);
    bt_app_views_shared()->net.frozen = 1;
}

static int smoke_loop(bt_app_ctx *ctx, bt_smoke_tour *tour) {
    BT_TRACE_ENTER("app.smoke_loop");
    bt_ui_input input;
    char pending[BT_PATH_MAX];
    int has_pending = 0;
    int code = -1;
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        bt_app_poll_keys(ctx);
        bt_app_process_keys(ctx);
        bt_ui_input_clear(&input);
        bt_app_poll_input(&input);
        bt_app_frame_input_set(&input);
        if (BT_APP_STATE_TABLE[ctx->state].update != NULL) {
            BT_APP_STATE_TABLE[ctx->state].update(ctx);
        }
        bt_app_transition_frame(ctx);
        bt_view_net_update(&ctx->net, &bt_app_views_shared()->net, dt);
        bt_app_window_frame_begin();
        ClearBackground(bt_app_color(bt_theme_background()));
        if (BT_APP_STATE_TABLE[ctx->state].draw != NULL) {
            BT_APP_STATE_TABLE[ctx->state].draw(ctx);
        }
        if (has_pending) {
            rlDrawRenderBatchActive();
            TakeScreenshot(pending);
            has_pending = 0;
        }
        bt_app_window_frame_end();
        bt_app_frame_input_set(NULL);
        char request[BT_PATH_MAX];
        int has_request = 0;
        code = bt_smoke_step(tour, ctx, bt_app_fonts_current() != NULL, dt, request, &has_request);
        if (has_request) {
            snprintf(pending, sizeof pending, "%s", request);
            has_pending = 1;
        }
        if (code >= 0) {
            break;
        }
    }
    BT_TRACE_EXIT("app.smoke_loop");
    return code >= 0 ? code : 0;
}

int bt_app_smoke_main(bt_app_ctx *ctx, const char *dir, uint64_t seed) {
    BT_TRACE_ENTER("app.smoke_main");
    if (!dir_ready(dir)) {
        BT_TRACE_FAIL("app.smoke_main", "output dir %s", dir);
        bt_app_ctx_free(ctx);
        BT_TRACE_EXIT("app.smoke_main");
        return 1;
    }
    arm_smoke_resources(seed);
    bt_app_font_book book;
    if (!open_window_and_fonts(ctx, &book)) {
        bt_app_ctx_free(ctx);
        BT_TRACE_EXIT("app.smoke_main");
        return 1;
    }
    bt_smoke_tour tour;
    bt_smoke_tour_init(&tour, dir);
    BT_APP_STATE_TABLE[ctx->state].enter(ctx);
    bt_app_apply_pending(ctx);
    int code = smoke_loop(ctx, &tour);
    driver_teardown(ctx, &book);
    BT_TRACE_EXIT("app.smoke_main");
    return code;
}

static int usage_loop(bt_app_ctx *ctx, bt_usage_tour *tour) {
    BT_TRACE_ENTER("app.usage_loop");
    bt_ui_input input;
    char pending[BT_PATH_MAX];
    int has_pending = 0;
    unsigned int key_vk = 0;
    unsigned int key_char = 0;
    int key_phase = 0;
    int code = -1;
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        bt_app_poll_keys(ctx);
        bt_app_process_keys(ctx);
        bt_ui_input_clear(&input);
        bt_app_poll_input(&input);
        bt_app_frame_input_set(&input);
        if (BT_APP_STATE_TABLE[ctx->state].update != NULL) {
            BT_APP_STATE_TABLE[ctx->state].update(ctx);
        }
        bt_app_transition_frame(ctx);
        bt_view_net_update(&ctx->net, &bt_app_views_shared()->net, dt);
        bt_app_window_frame_begin();
        ClearBackground(bt_app_color(bt_theme_background()));
        if (BT_APP_STATE_TABLE[ctx->state].draw != NULL) {
            BT_APP_STATE_TABLE[ctx->state].draw(ctx);
        }
        if (has_pending) {
            rlDrawRenderBatchActive();
            TakeScreenshot(pending);
            has_pending = 0;
        }
        bt_app_window_frame_end();
        bt_app_frame_input_set(NULL);
        void *hwnd = GetWindowHandle();
        if (key_phase == 2) {
            if (!bt_app_driver_key_up(hwnd, key_vk)) {
                BT_TRACE_FAIL("app.usage_loop", "key %u up not delivered", key_vk);
            }
            key_phase = 0;
        }
        bt_usage_key_request keys;
        char request[BT_PATH_MAX];
        int has_request = 0;
        code = bt_usage_step(tour, ctx, bt_app_fonts_current() != NULL, dt, &keys, request,
                             &has_request);
        if (keys.has_key && key_phase == 0) {
            key_vk = bt_app_driver_vk_of(keys.key);
            key_char = (unsigned char)keys.ch;
            if (key_vk != 0) {
                int sent = bt_app_driver_key_down(hwnd, key_vk) &&
                           (key_char == 0 || bt_app_driver_key_char(hwnd, key_char));
                if (!sent) {
                    BT_TRACE_FAIL("app.usage_loop", "key %u down not delivered", key_vk);
                }
                key_phase = 2;
            } else if (key_char != 0 && !bt_app_driver_key_char(hwnd, key_char)) {
                BT_TRACE_FAIL("app.usage_loop", "char %u not delivered", key_char);
            }
        }
        if (has_request) {
            snprintf(pending, sizeof pending, "%s", request);
            has_pending = 1;
        }
        if (code >= 0) {
            break;
        }
    }
    BT_TRACE_EXIT("app.usage_loop");
    return code >= 0 ? code : 1;
}

static int usage_run(bt_app_ctx *ctx, const char *dir, int fatal) {
    if (!dir_ready(dir)) {
        BT_TRACE_FAIL("app.usage_main", "output dir %s", dir);
        bt_app_ctx_free(ctx);
        return 1;
    }
    bt_app_font_book book;
    if (!open_window_and_fonts(ctx, &book)) {
        bt_app_ctx_free(ctx);
        return 1;
    }
    bt_usage_tour tour;
    int init_ok;
    if (fatal) {
        init_ok = bt_usage_fatal_init(&tour, dir);
    } else {
        init_ok =
            bt_usage_tour_init(&tour, dir, ctx->config.smoke_seed, ctx->config.memo.cadence_ms);
    }
    if (init_ok != 0) {
        bt_usage_tour_free(&tour);
        driver_teardown(ctx, &book);
        return 1;
    }
    BT_APP_STATE_TABLE[ctx->state].enter(ctx);
    bt_app_apply_pending(ctx);
    SetExitKey(KEY_NULL);
    int code = usage_loop(ctx, &tour);
    bt_usage_tour_free(&tour);
    driver_teardown(ctx, &book);
    return code;
}

int bt_app_usage_main(bt_app_ctx *ctx, const char *dir) {
    BT_TRACE_ENTER("app.usage_main");
    int code = usage_run(ctx, dir, 0);
    BT_TRACE_EXIT("app.usage_main");
    return code;
}

int bt_app_fatal_main(bt_app_ctx *ctx, const char *dir) {
    BT_TRACE_ENTER("app.fatal_main");
    int code = usage_run(ctx, dir, 1);
    BT_TRACE_EXIT("app.fatal_main");
    return code;
}
