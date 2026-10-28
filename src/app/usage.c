#include "app/usage.h"

#include "app/smoke.h"
#include "app/views/case_drill.h"
#include "app/views/drill.h"
#include "app/views/memo.h"
#include "app/views/solve.h"
#include "common/bt_trace.h"
#include "scheme/memo.h"
#include "scheme/pairs.h"
#include "scheme/scramble.h"

#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

bt_case bt_usage_pinned_case(void) {
    BT_TRACE_ENTER("app.usage_pinned_case");
    bt_case case_id;
    case_id.orbit = BT_ORBIT_EDGE;
    case_id.t1 = bt_scheme_sticker_of(BT_ORBIT_EDGE, 'A');
    case_id.t2 = bt_scheme_sticker_of(BT_ORBIT_EDGE, 'B');
    BT_TRACE_EXIT("app.usage_pinned_case");
    return case_id;
}

const char *bt_usage_covered_alg_text(const bt_corpus *corpus, const bt_case *case_id) {
    BT_TRACE_ENTER("app.usage_covered_alg_text");
    char position[256];
    bt_view_case_position(case_id, position);
    size_t found[8];
    size_t count =
        bt_corpus_rows_for_position(corpus, bt_view_sheet_of(case_id->orbit), position, found, 8);
    if (count == 0) {
        BT_TRACE_EXIT("app.usage_covered_alg_text");
        return NULL;
    }
    const char *text = corpus->rows[found[0]].alg_text;
    BT_TRACE_EXIT("app.usage_covered_alg_text");
    return text;
}

int bt_usage_fully_covered(const bt_corpus *corpus, const bt_cube *scramble) {
    BT_TRACE_ENTER("app.usage_fully_covered");
    bt_memo memo;
    bt_scheme_memo(scramble, &memo);
    bt_solve_outcome outcome;
    bt_solve_outcome_init(&outcome);
    bt_view_solve_simulate(corpus, scramble, &memo, &outcome);
    int success = outcome.success;
    bt_solve_outcome_free(&outcome);
    BT_TRACE_EXIT("app.usage_fully_covered");
    return success;
}

int bt_usage_covered_seed(const bt_corpus *corpus, uint64_t base, uint64_t attempts,
                          uint64_t *out) {
    BT_TRACE_ENTER("app.usage_covered_seed");
    uint64_t end = base + attempts;
    if (end < base) {
        end = UINT64_MAX;
    }
    for (uint64_t seed = base; seed < end; seed++) {
        bt_cube scramble;
        bt_scheme_random_state(seed, &scramble);
        if (bt_usage_fully_covered(corpus, &scramble)) {
            *out = seed;
            BT_TRACE_EXIT("app.usage_covered_seed");
            return 1;
        }
    }
    BT_TRACE_EXIT("app.usage_covered_seed");
    return 0;
}

static int dir_exists_or_made(const char *dir) {
    struct _stat info;
    if (_stat(dir, &info) == 0) {
        return 0;
    }
    if (_mkdir(dir) == 0) {
        return 0;
    }
    BT_TRACE_FAIL("app.usage_init", "usage output dir %s", dir);
    return -1;
}

void bt_usage_tour_set_actions(bt_usage_tour *tour, const bt_usage_action *actions, size_t count) {
    BT_TRACE_ENTER("app.usage_tour_set_actions");
    free(tour->actions);
    tour->actions = NULL;
    tour->action_count = 0;
    tour->action_cap = 0;
    tour->expected_shots = 0;
    if (count > 0) {
        size_t cap = 16;
        while (cap < count) {
            cap *= 2;
        }
        bt_usage_action *grown = malloc(cap * sizeof *grown);
        if (grown == NULL) {
            BT_TRACE_FAIL("app.usage_tour_set_actions", "allocation of %zu actions failed", cap);
            BT_TRACE_EXIT("app.usage_tour_set_actions");
            return;
        }
        memcpy(grown, actions, count * sizeof *grown);
        tour->actions = grown;
        tour->action_count = count;
        tour->action_cap = cap;
        for (size_t i = 0; i < count; i++) {
            if (actions[i].kind == BT_USAGE_ACT_SHOT) {
                tour->expected_shots++;
            }
        }
    }
    BT_TRACE_EXIT("app.usage_tour_set_actions");
}

void bt_usage_tour_free(bt_usage_tour *tour) {
    BT_TRACE_ENTER("app.usage_tour_free");
    free(tour->actions);
    tour->actions = NULL;
    tour->action_count = 0;
    tour->action_cap = 0;
    BT_TRACE_EXIT("app.usage_tour_free");
}

static int tour_init(bt_usage_tour *tour, const char *dir, const bt_usage_action *actions,
                     size_t count) {
    memset(tour, 0, sizeof *tour);
    if (dir_exists_or_made(dir) != 0) {
        return 1;
    }
    snprintf(tour->dir, sizeof tour->dir, "%s", dir);
    tour->index = 0;
    tour->mode = BT_USAGE_MODE_ACT;
    tour->exit_code = -1;
    bt_usage_tour_set_actions(tour, actions, count);
    return 0;
}

int bt_usage_tour_init(bt_usage_tour *tour, const char *dir, uint64_t seed,
                       unsigned int cadence_ms) {
    BT_TRACE_ENTER("app.usage_tour_init");
    size_t count = 0;
    bt_usage_action *actions = bt_usage_script((float)cadence_ms / 1000.0F, &count);
    if (tour_init(tour, dir, actions, count) != 0) {
        free(actions);
        BT_TRACE_EXIT("app.usage_tour_init");
        return 1;
    }
    free(actions);
    uint64_t seeds[3];
    bt_smoke_seeds(seed, seeds);
    bt_view_memo_app()->seed = seeds[0];
    bt_view_solve_app()->seed = seeds[1];
    bt_view_drill_app()->seed = seeds[2];
    BT_TRACE_EXIT("app.usage_tour_init");
    return 0;
}

int bt_usage_fatal_init(bt_usage_tour *tour, const char *dir) {
    BT_TRACE_ENTER("app.usage_fatal_init");
    size_t count = 0;
    bt_usage_action *actions = bt_usage_fatal_script(&count);
    int ok = tour_init(tour, dir, actions, count);
    free(actions);
    BT_TRACE_EXIT("app.usage_fatal_init");
    return ok != 0 ? 1 : 0;
}

static int file_written(const char *path) {
    struct _stat info;
    if (_stat(path, &info) != 0) {
        return 0;
    }
    return info.st_size > 0;
}

int bt_usage_finish(const bt_usage_tour *tour) {
    BT_TRACE_ENTER("app.usage_finish");
    int all_written = tour->shot_count == tour->expected_shots;
    for (size_t i = 0; all_written && i < tour->shot_count; i++) {
        if (!file_written(tour->shots[i])) {
            all_written = 0;
        }
    }
    BT_TRACE_EXIT("app.usage_finish");
    return all_written ? 0 : 1;
}

static void stuck_out(bt_usage_tour *tour, const char *detail) {
    fprintf(stderr, "usage tour stuck at action %zu: %s\n", tour->index, detail);
    tour->mode = BT_USAGE_MODE_DONE;
    tour->exit_code = 1;
}

static int wait_matches(const bt_usage_wait *wait, const bt_app_ctx *ctx, int fonts_ready) {
    switch (wait->kind) {
    case BT_USAGE_WAIT_APP:
        return ctx->state == wait->app;
    case BT_USAGE_WAIT_FONTS:
        return fonts_ready;
    case BT_USAGE_WAIT_DRILL:
        return ctx->drill_phase == wait->drill;
    case BT_USAGE_WAIT_MEMO:
        return ctx->memo_phase == wait->memo;
    case BT_USAGE_WAIT_SOLVE:
        return ctx->solve_phase == wait->solve;
    case BT_USAGE_WAIT_CASE_DRILL:
        return ctx->case_drill_phase == wait->case_drill;
    }
    return 0;
}

static const char *wait_name(const bt_usage_wait *wait) {
    switch (wait->kind) {
    case BT_USAGE_WAIT_APP:
        return "App";
    case BT_USAGE_WAIT_FONTS:
        return "Fonts";
    case BT_USAGE_WAIT_DRILL:
        return "Drill";
    case BT_USAGE_WAIT_MEMO:
        return "Memo";
    case BT_USAGE_WAIT_SOLVE:
        return "Solve";
    case BT_USAGE_WAIT_CASE_DRILL:
        return "CaseDrill";
    }
    return "Unknown";
}

int bt_usage_step(bt_usage_tour *tour, bt_app_ctx *ctx, int fonts_ready, float dt,
                  bt_usage_key_request *key_request, char shot_request[BT_PATH_MAX],
                  int *has_shot_request) {
    BT_TRACE_ENTER("app.usage_step");
    key_request->has_key = 0;
    key_request->key = 0;
    key_request->ch = 0;
    *has_shot_request = 0;
    shot_request[0] = '\0';
    tour->exit_code = -1;
    bt_usage_mode_kind mode = tour->mode;
    switch (mode) {
    case BT_USAGE_MODE_ACT: {
        if (tour->index >= tour->action_count) {
            tour->exit_code = bt_usage_finish(tour);
            tour->mode = BT_USAGE_MODE_DONE;
            BT_TRACE_EXIT("app.usage_step");
            return tour->exit_code;
        }
        const bt_usage_action *action = &tour->actions[tour->index];
        switch (action->kind) {
        case BT_USAGE_ACT_PRESS:
            key_request->has_key = 1;
            key_request->key = action->key;
            tour->index++;
            tour->mode = BT_USAGE_MODE_ACT;
            break;
        case BT_USAGE_ACT_PRESS_CHAR:
            key_request->has_key = 1;
            key_request->key = action->key;
            key_request->ch = action->ch;
            tour->index++;
            tour->mode = BT_USAGE_MODE_ACT;
            break;
        case BT_USAGE_ACT_NAV_OPEN:
            bt_app_request_state(ctx, action->app);
            tour->index++;
            tour->mode = BT_USAGE_MODE_ACT;
            break;
        case BT_USAGE_ACT_NAV_MENU:
            bt_app_nav_to_menu(ctx);
            tour->index++;
            tour->mode = BT_USAGE_MODE_ACT;
            break;
        case BT_USAGE_ACT_PIN_DRILL:
            bt_view_drill_set_pending(bt_usage_pinned_case());
            tour->index++;
            tour->mode = BT_USAGE_MODE_ACT;
            break;
        case BT_USAGE_ACT_PICK_SOLVE_SEED: {
            uint64_t found = 0;
            int ok = 0;
            if (ctx->has_corpus) {
                ok = bt_usage_covered_seed(&ctx->vendor_corpus, bt_view_solve_app()->seed,
                                           BT_USAGE_SOLVE_SEED_ATTEMPTS, &found);
            }
            if (ok) {
                bt_view_solve_app()->seed = found;
                tour->index++;
                tour->mode = BT_USAGE_MODE_ACT;
            } else {
                stuck_out(tour, "no fully covered solve scramble found");
            }
            break;
        }
        case BT_USAGE_ACT_WAIT_APP:
            tour->mode = BT_USAGE_MODE_WAIT_UNTIL;
            tour->wait.kind = BT_USAGE_WAIT_APP;
            tour->wait.app = action->app;
            tour->wait.waited = 0.0F;
            break;
        case BT_USAGE_ACT_WAIT_FONTS:
            tour->mode = BT_USAGE_MODE_WAIT_UNTIL;
            tour->wait.kind = BT_USAGE_WAIT_FONTS;
            tour->wait.waited = 0.0F;
            break;
        case BT_USAGE_ACT_WAIT_DRILL:
            tour->mode = BT_USAGE_MODE_WAIT_UNTIL;
            tour->wait.kind = BT_USAGE_WAIT_DRILL;
            tour->wait.drill = action->drill;
            tour->wait.waited = 0.0F;
            break;
        case BT_USAGE_ACT_WAIT_MEMO:
            tour->mode = BT_USAGE_MODE_WAIT_UNTIL;
            tour->wait.kind = BT_USAGE_WAIT_MEMO;
            tour->wait.memo = action->memo;
            tour->wait.waited = 0.0F;
            break;
        case BT_USAGE_ACT_WAIT_SOLVE:
            tour->mode = BT_USAGE_MODE_WAIT_UNTIL;
            tour->wait.kind = BT_USAGE_WAIT_SOLVE;
            tour->wait.solve = action->solve;
            tour->wait.waited = 0.0F;
            break;
        case BT_USAGE_ACT_WAIT_CASE_DRILL:
            tour->mode = BT_USAGE_MODE_WAIT_UNTIL;
            tour->wait.kind = BT_USAGE_WAIT_CASE_DRILL;
            tour->wait.case_drill = action->case_drill;
            tour->wait.waited = 0.0F;
            break;
        case BT_USAGE_ACT_ELAPSE:
            tour->mode = BT_USAGE_MODE_ELAPSE;
            tour->elapse_left = action->secs;
            break;
        case BT_USAGE_ACT_SHOT:
            snprintf(tour->shot.path, sizeof tour->shot.path, "%s/%s.png", tour->dir, action->name);
            tour->mode = BT_USAGE_MODE_SHOT;
            tour->shot.clock = -BT_USAGE_DWELL_SECS;
            tour->shot.taken = 0;
            break;
        }
        break;
    }
    case BT_USAGE_MODE_WAIT_UNTIL: {
        if (wait_matches(&tour->wait, ctx, fonts_ready)) {
            tour->index++;
            tour->mode = BT_USAGE_MODE_ACT;
        } else if (tour->wait.waited + dt >= BT_USAGE_STUCK_LIMIT_SECS) {
            char detail[64];
            snprintf(detail, sizeof detail, "wait %s timed out", wait_name(&tour->wait));
            stuck_out(tour, detail);
        } else {
            tour->wait.waited += dt;
        }
        break;
    }
    case BT_USAGE_MODE_ELAPSE:
        if (tour->elapse_left - dt <= 0.0F) {
            tour->index++;
            tour->mode = BT_USAGE_MODE_ACT;
        } else {
            tour->elapse_left -= dt;
        }
        break;
    case BT_USAGE_MODE_SHOT:
        if (tour->shot.clock + dt < 0.0F) {
            tour->shot.clock += dt;
            break;
        }
        if (!tour->shot.taken) {
            remove(tour->shot.path);
            snprintf(shot_request, BT_PATH_MAX, "%s", tour->shot.path);
            *has_shot_request = 1;
            tour->shot.clock += dt;
            tour->shot.taken = 1;
            break;
        }
        if (file_written(tour->shot.path)) {
            snprintf(tour->shots[tour->shot_count], BT_PATH_MAX, "%s", tour->shot.path);
            tour->shot_count++;
            tour->index++;
            tour->mode = BT_USAGE_MODE_ACT;
        } else if (tour->shot.clock + dt >= BT_USAGE_STUCK_LIMIT_SECS) {
            char detail[BT_PATH_MAX + 32];
            snprintf(detail, sizeof detail, "shot %s never landed", tour->shot.path);
            stuck_out(tour, detail);
        } else {
            tour->shot.clock += dt;
        }
        break;
    case BT_USAGE_MODE_DONE:
        tour->mode = BT_USAGE_MODE_DONE;
        break;
    }
    BT_TRACE_EXIT("app.usage_step");
    return tour->exit_code;
}
