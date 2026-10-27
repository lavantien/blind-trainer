#include "app/smoke.h"

#include "common/bt_trace.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

void bt_smoke_seeds(uint64_t seed, uint64_t out[3]) {
    BT_TRACE_ENTER("app.smoke_seeds");
    out[0] = seed;
    out[1] = seed + 1;
    out[2] = seed + 2;
    BT_TRACE_EXIT("app.smoke_seeds");
}

void bt_smoke_tour_init(bt_smoke_tour *tour, const char *dir) {
    BT_TRACE_ENTER("app.smoke_tour_init");
    memset(tour, 0, sizeof *tour);
    snprintf(tour->dir, sizeof tour->dir, "%s", dir);
    tour->phase.kind = BT_SMOKE_WAIT_FOR_BOOT;
    tour->exit_code = -1;
    BT_TRACE_EXIT("app.smoke_tour_init");
}

void bt_smoke_shot_path(const bt_smoke_tour *tour, bt_app_state state, char out[BT_PATH_MAX]) {
    BT_TRACE_ENTER("app.smoke_shot_path");
    const char *slug = bt_app_state_slug(state);
    snprintf(out, BT_PATH_MAX, "%s/%s.png", tour->dir, slug != NULL ? slug : "unknown");
    BT_TRACE_EXIT("app.smoke_shot_path");
}

void bt_smoke_shoot(const char *path) {
    BT_TRACE_ENTER("app.smoke_shoot");
    remove(path);
    BT_TRACE_EXIT("app.smoke_shoot");
}

static int file_written(const char *path) {
    struct _stat info;
    if (_stat(path, &info) != 0) {
        return 0;
    }
    return info.st_size > 0;
}

static int path_matches_slug_png(const char *path, bt_app_state state) {
    const char *slug = bt_app_state_slug(state);
    if (slug == NULL) {
        return 0;
    }
    char suffix[64];
    snprintf(suffix, sizeof suffix, "%s.png", slug);
    size_t path_len = strlen(path);
    size_t suffix_len = strlen(suffix);
    if (path_len < suffix_len) {
        return 0;
    }
    const char *base = strrchr(path, '/');
    if (base == NULL) {
        return 0;
    }
    return strcmp(path + path_len - suffix_len, suffix) == 0;
}

static int entered_matches_tour(const bt_smoke_tour *tour) {
    if (tour->entered_count != BT_APP_TOUR_COUNT) {
        return 0;
    }
    for (size_t i = 0; i < BT_APP_TOUR_COUNT; i++) {
        if (tour->entered[i] != BT_APP_TOUR[i]) {
            return 0;
        }
    }
    return 1;
}

int bt_smoke_finish(const bt_smoke_tour *tour) {
    BT_TRACE_ENTER("app.smoke_finish");
    int all_written = tour->shot_count == BT_APP_TOUR_COUNT;
    for (size_t i = 0; all_written && i < tour->shot_count; i++) {
        if (!file_written(tour->shots[i].path)
            || !path_matches_slug_png(tour->shots[i].path, tour->shots[i].state)) {
            all_written = 0;
        }
    }
    int ok = entered_matches_tour(tour) && all_written;
    BT_TRACE_EXIT("app.smoke_finish");
    return ok ? 0 : 1;
}

static void advance_tour(bt_smoke_tour *tour, bt_app_ctx *ctx) {
    if (tour->entered_count < BT_APP_TOUR_COUNT) {
        bt_app_state target = BT_APP_TOUR[tour->entered_count];
        if (target == BT_APP_FATAL) {
            snprintf(ctx->fatal_reason, sizeof ctx->fatal_reason, "%s", BT_SMOKE_FATAL_REASON);
        }
        bt_app_request_state(ctx, target);
        tour->entered[tour->entered_count++] = target;
        tour->phase.kind = BT_SMOKE_DWELL;
        tour->phase.state = target;
        tour->phase.waited = 0.0F;
        tour->phase.stuck = 0.0F;
    } else {
        tour->phase.kind = BT_SMOKE_DONE;
        tour->exit_code = bt_smoke_finish(tour);
    }
}

int bt_smoke_step(bt_smoke_tour *tour, bt_app_ctx *ctx, int fonts_ready, float dt,
                  char shot_request[BT_PATH_MAX], int *has_shot_request) {
    BT_TRACE_ENTER("app.smoke_step");
    *has_shot_request = 0;
    shot_request[0] = '\0';
    tour->exit_code = -1;
    if (ctx->fatal_reason[0] != '\0' && strcmp(ctx->fatal_reason, BT_SMOKE_FATAL_REASON) != 0) {
        tour->phase.kind = BT_SMOKE_DONE;
        tour->exit_code = 1;
        BT_TRACE_EXIT("app.smoke_step");
        return 1;
    }
    bt_smoke_phase phase = tour->phase;
    switch (phase.kind) {
    case BT_SMOKE_WAIT_FOR_BOOT:
        if (phase.waited >= BT_SMOKE_STUCK_LIMIT_SECS) {
            tour->phase.kind = BT_SMOKE_DONE;
            tour->exit_code = 1;
            BT_TRACE_EXIT("app.smoke_step");
            return 1;
        }
        if (ctx->state == BT_APP_MENU && fonts_ready) {
            tour->entered[tour->entered_count++] = BT_APP_MENU;
            tour->phase.kind = BT_SMOKE_DWELL;
            tour->phase.state = BT_APP_MENU;
            tour->phase.waited = 0.0F;
            tour->phase.stuck = 0.0F;
        } else {
            tour->phase.kind = BT_SMOKE_WAIT_FOR_BOOT;
            tour->phase.waited = phase.waited + dt;
        }
        break;
    case BT_SMOKE_DWELL:
        if (ctx->state == phase.state) {
            float waited = phase.waited + dt;
            if (waited >= BT_SMOKE_DWELL_SECS) {
                bt_smoke_shot_path(tour, phase.state, tour->shots[tour->shot_count].path);
                tour->shots[tour->shot_count].state = phase.state;
                tour->shot_count++;
                bt_smoke_shoot(tour->shots[tour->shot_count - 1].path);
                snprintf(shot_request, BT_PATH_MAX, "%s", tour->shots[tour->shot_count - 1].path);
                *has_shot_request = 1;
                tour->phase.kind = BT_SMOKE_WAIT_FOR_SHOT;
                tour->phase.state = phase.state;
                snprintf(tour->phase.path, sizeof tour->phase.path, "%s", shot_request);
            } else {
                tour->phase.kind = BT_SMOKE_DWELL;
                tour->phase.waited = waited;
                tour->phase.stuck = 0.0F;
            }
        } else {
            float stuck = phase.stuck + dt;
            if (stuck >= BT_SMOKE_STUCK_LIMIT_SECS) {
                tour->phase.kind = BT_SMOKE_DONE;
                tour->exit_code = 3;
                BT_TRACE_EXIT("app.smoke_step");
                return 3;
            }
            tour->phase.kind = BT_SMOKE_DWELL;
            tour->phase.waited = phase.waited;
            tour->phase.stuck = stuck;
        }
        break;
    case BT_SMOKE_WAIT_FOR_SHOT:
        if (!file_written(phase.path)) {
            break;
        }
        advance_tour(tour, ctx);
        break;
    case BT_SMOKE_DONE:
        tour->phase.kind = BT_SMOKE_DONE;
        break;
    }
    BT_TRACE_EXIT("app.smoke_step");
    return tour->exit_code;
}
