#include "app/theme.h"
#include "app/views/frame.h"
#include "app/views/memo.h"
#include "app/views/solve.h"
#include "app/views/state.h"
#include "app/window.h"
#include "data/paths.h"

#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char STATUS_LONG[] =
    "parity UF-UB, UFR-UBR; flip UF & UL; flip UR & FL; flip UB & DF; flip DL & UB; flip FU & BR; "
    "flip LU & RF; twist UFR x2; twist UBL x1; twist DFR x2; twist DBR x1; twist ULB x2; "
    "twist URF x1; twist DLF x2; twist DBL x1; r replay  space next  1..4 rate pairs";

enum {
    PROBE_W = 1280,
    PROBE_H = 800,
    DWELL_FRAMES = 6,
};

static int ink_extent(const char *path, int y0, int y1, int *leftmost, int *rightmost) {
    Image image = LoadImage(path);
    if (image.width <= 0) {
        return 0;
    }
    Color *pixels = LoadImageColors(image);
    Color back = bt_app_color(bt_theme_background());
    int found = 0;
    int left = image.width;
    int right = -1;
    for (int y = y0; y <= y1 && y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            Color pixel = pixels[y * image.width + x];
            int dist = abs(pixel.r - back.r) + abs(pixel.g - back.g) + abs(pixel.b - back.b);
            if (dist > 180) {
                found = 1;
                if (x < left) {
                    left = x;
                }
                if (x > right) {
                    right = x;
                }
            }
        }
    }
    UnloadImageColors(pixels);
    UnloadImage(image);
    *leftmost = left;
    *rightmost = right;
    return found;
}

static int capture(const char *path, void (*draw)(void)) {
    for (unsigned frame = 0; frame < DWELL_FRAMES; frame++) {
        bt_app_window_frame_begin();
        ClearBackground(bt_app_color(bt_theme_background()));
        draw();
        bt_app_window_frame_end();
    }
    remove(path);
    return bt_app_window_capture(path);
}

static const bt_app_font_book *g_book;

static void draw_memo_rows(void) {
    bt_memo_rows rows;
    snprintf(rows.focal, sizeof rows.focal, "%s", "tả");
    snprintf(rows.recent, sizeof rows.recent, "%s", "AB CD EF");
    snprintf(rows.status, sizeof rows.status, "%s", STATUS_LONG);
    bt_view_memo_draw(&rows, g_book->regular, g_book->bold, (float)PROBE_W, (float)PROBE_H);
}

static void fill_solve_ctx(bt_app_ctx *ctx) {
    bt_app_ctx_init(ctx);
    ctx->state = BT_APP_SOLVE;
    ctx->has_corpus = 1;
    bt_corpus_init(&ctx->vendor_corpus);
    ctx->has_stores = 1;
    memset(&ctx->stores, 0, sizeof ctx->stores);
    ctx->stores.settings.memo_cadence_ms = 800;
    bt_srs_store_init(&ctx->stores.srs);
    bt_solve_log_init(&ctx->stores.solves);
    bt_user_algs_init(&ctx->stores.user_algs);
    bt_str_list_init(&ctx->stores.seeds.hidden);
    bt_str_list_init(&ctx->stores.seeds.warnings);
    ctx->solve_phase = BT_SOLVE_REVIEW;
}

static void draw_solve_review(void) {
    bt_app_ctx ctx;
    fill_solve_ctx(&ctx);
    bt_view_solve_app_state *app = bt_view_solve_app();
    bt_view_solve_app_init(app);
    app->session.has_finished = 1;
    app->session.finished.has_outcome = 1;
    app->session.finished.outcome.success = 0;
    bt_solve_outcome_push_broken(&app->session.finished.outcome,
                                 "residue unsupported: TwistTriple(53, 53, 53)");
    bt_solve_outcome_push_broken(&app->session.finished.outcome, "UF-UB, UFR-UBR[UBL] uncovered");
    bt_view_solve_draw(&ctx, g_book->regular, g_book->bold, (float)PROBE_W, (float)PROBE_H);
    bt_solve_outcome_free(&app->session.finished.outcome);
    bt_app_views_dispose_shared();
    bt_app_ctx_free(&ctx);
}

int main(void) {
    if (!bt_app_window_open_driven(PROBE_W, PROBE_H, "draw probe", NULL)) {
        puts("no window");
        return 1;
    }
    bt_app_font_book book;
    char error[BT_APP_ERROR_MAX];
    char path[BT_PATH_MAX];
    bt_data_config_path(path);
    if (bt_app_fonts_load(path, NULL, NULL, 0, &book, error, sizeof error) != 0) {
        bt_app_window_close();
        printf("fonts: %s\n", error);
        return 1;
    }
    g_book = &book;
    const bt_theme *theme = bt_theme_table();
    bt_view_frame_metrics metrics = bt_view_frame_metrics_for((float)PROBE_W, (float)PROBE_H);
    bt_view_solve_text_column column = bt_view_solve_text_column_for((float)PROBE_W, (float)PROBE_H);
    float body = theme->font_body * theme->root_rem;
    int fail = 0;
    char shot[BT_PATH_MAX];
    snprintf(shot, sizeof shot, "%s/bt-lane-e-memo.png", getenv("TEMP"));
    if (capture(shot, draw_memo_rows)) {
        int status_top = (int)(metrics.content_y + metrics.content_h - body * 3.0F) - 4;
        int status_bottom = (int)(metrics.content_y + metrics.content_h);
        int left = 0;
        int right = 0;
        if (ink_extent(shot, status_top, status_bottom, &left, &right)) {
            int in_bounds = left >= (int)metrics.content_x - 2 &&
                            right <= (int)(metrics.content_x + metrics.content_w) + 2;
            printf("memo status band ink %d..%d bounds %d..%d %s\n", left, right,
                   (int)metrics.content_x, (int)(metrics.content_x + metrics.content_w),
                   in_bounds ? "ok" : "OUT");
            fail += !in_bounds;
        } else {
            puts("memo status band empty");
            fail += 1;
        }
    } else {
        puts("memo capture failed");
        fail += 1;
    }
    remove(shot);
    snprintf(shot, sizeof shot, "%s/bt-lane-e-solve.png", getenv("TEMP"));
    if (capture(shot, draw_solve_review)) {
        int left = 0;
        int right = 0;
        int y0 = (int)metrics.content_y + 4;
        int y1 = (int)(metrics.content_y + metrics.content_h);
        if (ink_extent(shot, y0, y1, &left, &right)) {
            int in_bounds = right <= (int)(metrics.content_x + metrics.content_w) + 2;
            printf("solve text ink right %d column %d..%d %s\n", right, (int)column.x,
                   (int)(column.x + column.width), in_bounds ? "ok" : "OUT");
            fail += !in_bounds;
        } else {
            puts("solve band empty");
            fail += 1;
        }
    } else {
        puts("solve capture failed");
        fail += 1;
    }
    remove(shot);
    bt_app_fonts_unload(&book);
    bt_app_window_close();
    printf("probe fail=%d\n", fail);
    return fail == 0 ? 0 : 1;
}
