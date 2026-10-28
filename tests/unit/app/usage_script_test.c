#include "app/usage_script.h"

#include "common.h"

#include "raylib.h"

#include <string.h>

static bt_usage_action *g_actions;
static size_t g_count;

static size_t fill_script(void) {
    g_actions = bt_usage_script(0.8F, &g_count);
    BT_CHECK(g_actions != NULL);
    BT_CHECK(g_count > 0);
    return g_count;
}

static size_t shot_names(const bt_usage_action *g_actions, size_t count,
                         const char *out[BT_USAGE_SHOT_MAX]) {
    size_t names = 0;
    for (size_t i = 0; i < count; i++) {
        if (g_actions[i].kind == BT_USAGE_ACT_SHOT) {
            BT_CHECK(names < BT_USAGE_SHOT_MAX);
            out[names++] = g_actions[i].name;
        }
    }
    return names;
}

static int name_before(const bt_usage_action *g_actions, size_t count, const char *name,
                       size_t *out) {
    for (size_t i = 0; i < count; i++) {
        if (g_actions[i].kind == BT_USAGE_ACT_SHOT && strcmp(g_actions[i].name, name) == 0) {
            *out = i;
            return 1;
        }
    }
    return 0;
}

static size_t position(const bt_usage_action *g_actions, size_t count,
                       int (*pred)(const bt_usage_action *)) {
    for (size_t i = 0; i < count; i++) {
        if (pred(&g_actions[i])) {
            return i;
        }
    }
    BT_ABORT("action missing");
    return count;
}

static int is_wait_drill_rate(const bt_usage_action *action) {
    return action->kind == BT_USAGE_ACT_WAIT_DRILL && action->drill == BT_DRILL_RATE;
}

static int is_shot_drill_rate_a(const bt_usage_action *action) {
    return action->kind == BT_USAGE_ACT_SHOT && strcmp(action->name, "drill-rate-a") == 0;
}

static int is_wait_memo_review(const bt_usage_action *action) {
    return action->kind == BT_USAGE_ACT_WAIT_MEMO && action->memo == BT_MEMO_REVIEW;
}

static int is_shot_memo_review(const bt_usage_action *action) {
    return action->kind == BT_USAGE_ACT_SHOT && strcmp(action->name, "memo-review") == 0;
}

static int is_pick_solve_seed(const bt_usage_action *action) {
    return action->kind == BT_USAGE_ACT_PICK_SOLVE_SEED;
}

static int is_nav_open_solve(const bt_usage_action *action) {
    return action->kind == BT_USAGE_ACT_NAV_OPEN && action->app == BT_APP_SOLVE;
}

static int is_nav_open_stats(const bt_usage_action *action) {
    return action->kind == BT_USAGE_ACT_NAV_OPEN && action->app == BT_APP_STATS;
}

static int is_shot_solve_review(const bt_usage_action *action) {
    return action->kind == BT_USAGE_ACT_SHOT && strcmp(action->name, "solve-review") == 0;
}

static int is_wait_app_case_drill(const bt_usage_action *action) {
    return action->kind == BT_USAGE_ACT_WAIT_APP && action->app == BT_APP_CASE_DRILL;
}

static int is_wait_app_menu(const bt_usage_action *action) {
    return action->kind == BT_USAGE_ACT_WAIT_APP && action->app == BT_APP_MENU;
}

static int is_press(const bt_usage_action *action) {
    return action->kind == BT_USAGE_ACT_PRESS || action->kind == BT_USAGE_ACT_PRESS_CHAR;
}

BT_TEST(usage_script, shots_are_unique_and_complete) {
    size_t count = fill_script();
    const char *names[BT_USAGE_SHOT_MAX];
    size_t total = shot_names(g_actions, count, names);
    BT_CHECK_EQ(total, (size_t)19);
    for (size_t i = 0; i < total; i++) {
        for (size_t j = i + 1; j < total; j++) {
            BT_CHECK(strcmp(names[i], names[j]) != 0);
        }
    }
    BT_CHECK(strcmp(names[0], "menu") == 0);
    BT_CHECK(strcmp(names[total - 1], "settings") == 0);
    const char *expected[] = {
        "algs",
        "algs-search",
        "case-drill-grade",
        "case-drill-show",
        "drill-card",
        "drill-rate-a",
        "drill-rate-b",
        "drill-reps",
        "menu",
        "memo-intro",
        "memo-recall",
        "memo-review",
        "memo-stream",
        "settings",
        "solve-exec",
        "solve-memo",
        "solve-ready",
        "solve-review",
        "stats",
    };
    BT_CHECK_EQ(total, sizeof expected / sizeof expected[0]);
    for (size_t i = 0; i < total; i++) {
        int found = 0;
        for (size_t j = 0; j < total; j++) {
            if (strcmp(names[j], expected[i]) == 0) {
                found = 1;
                break;
            }
        }
        BT_CHECK(found);
    }
}

BT_TEST(usage_script, never_issues_two_presses_back_to_back) {
    size_t count = fill_script();
    for (size_t i = 1; i < count; i++) {
        BT_CHECK(!(is_press(&g_actions[i - 1]) && is_press(&g_actions[i])));
    }
}

BT_TEST(usage_script, sequences_each_phase_before_its_shot) {
    size_t count = fill_script();
    BT_CHECK(position(g_actions, count, is_wait_drill_rate) <
             position(g_actions, count, is_shot_drill_rate_a));
    BT_CHECK(position(g_actions, count, is_wait_memo_review) <
             position(g_actions, count, is_shot_memo_review));
    BT_CHECK(position(g_actions, count, is_pick_solve_seed) <
             position(g_actions, count, is_nav_open_solve));
    BT_CHECK(position(g_actions, count, is_nav_open_stats) >
             position(g_actions, count, is_shot_solve_review));
}

BT_TEST(usage_script, the_case_drill_leg_drives_the_repo_and_all_four_grade_keys) {
    size_t count = fill_script();
    size_t entry = position(g_actions, count, is_wait_app_case_drill);
    int nav[5];
    size_t nav_count = 0;
    for (size_t i = entry; i-- > 0;) {
        if (g_actions[i].kind == BT_USAGE_ACT_PRESS && nav_count < 5) {
            nav[nav_count++] = g_actions[i].key;
        }
    }
    BT_CHECK_EQ(nav_count, (size_t)5);
    BT_CHECK_EQ(nav[4], KEY_RIGHT);
    BT_CHECK_EQ(nav[3], KEY_DOWN);
    BT_CHECK_EQ(nav[2], KEY_RIGHT);
    BT_CHECK_EQ(nav[1], KEY_RIGHT);
    BT_CHECK_EQ(nav[0], KEY_D);
    size_t exit = entry;
    for (size_t i = entry; i < count; i++) {
        if (is_wait_app_menu(&g_actions[i])) {
            exit = i;
            break;
        }
    }
    BT_CHECK(exit > entry);
    BT_CHECK(g_actions[exit - 1].kind == BT_USAGE_ACT_PRESS);
    BT_CHECK_EQ(g_actions[exit - 1].key, KEY_ESCAPE);
    int drills[16];
    size_t drill_count = 0;
    for (size_t i = entry; i < exit; i++) {
        if (g_actions[i].kind == BT_USAGE_ACT_PRESS) {
            BT_CHECK(drill_count < 16);
            drills[drill_count++] = g_actions[i].key;
        }
    }
    BT_CHECK_EQ(drill_count, (size_t)9);
    BT_CHECK_EQ(drills[0], KEY_SPACE);
    BT_CHECK_EQ(drills[1], KEY_ONE);
    BT_CHECK_EQ(drills[2], KEY_SPACE);
    BT_CHECK_EQ(drills[3], KEY_TWO);
    BT_CHECK_EQ(drills[4], KEY_SPACE);
    BT_CHECK_EQ(drills[5], KEY_THREE);
    BT_CHECK_EQ(drills[6], KEY_SPACE);
    BT_CHECK_EQ(drills[7], KEY_FOUR);
    BT_CHECK_EQ(drills[8], KEY_ESCAPE);
    BT_CHECK(g_actions[exit + 1].kind == BT_USAGE_ACT_NAV_OPEN);
    BT_CHECK_EQ(g_actions[exit + 1].app, BT_APP_STATS);
}

BT_TEST(usage_script, fatal_script_shoots_only_the_fatal_screen) {
    size_t count = 0;
    bt_usage_action *actions = bt_usage_fatal_script(&count);
    BT_CHECK_EQ(count, (size_t)3);
    BT_CHECK(actions[0].kind == BT_USAGE_ACT_WAIT_APP);
    BT_CHECK_EQ(actions[0].app, BT_APP_FATAL);
    BT_CHECK(actions[1].kind == BT_USAGE_ACT_WAIT_FONTS);
    BT_CHECK(actions[2].kind == BT_USAGE_ACT_SHOT);
    BT_CHECK(strcmp(actions[2].name, "fatal") == 0);
    free(g_actions);
}

BT_TEST(usage_script, elapsed_times_are_frame_dwells_or_cadence_scaled) {
    size_t count = fill_script();
    double partial = 0.8 * 3.25;
    size_t stream_waits = 0;
    size_t frame_dwells = 0;
    for (size_t i = 0; i < count; i++) {
        if (g_actions[i].kind != BT_USAGE_ACT_ELAPSE) {
            continue;
        }
        double secs = (double)g_actions[i].secs;
        int near_partial = (secs - partial) < 1e-6 && (partial - secs) < 1e-6;
        int near_frame = (secs - 0.1) < 1e-6 && (0.1 - secs) < 1e-6;
        int near_slow = (secs - 0.3) < 1e-6 && (0.3 - secs) < 1e-6;
        int near_exec = (secs - 1.6) < 1e-6 && (1.6 - secs) < 1e-6;
        BT_CHECK(near_partial || near_frame || near_slow || near_exec);
        if (near_partial) {
            stream_waits++;
        }
        if (near_frame) {
            frame_dwells++;
        }
    }
    BT_CHECK_EQ(stream_waits, (size_t)2);
    BT_CHECK(frame_dwells > 0);
}

BT_TEST(usage_script, script_count_is_stable_across_cadences) {
    size_t count = fill_script();
    size_t at;
    BT_CHECK(name_before(g_actions, count, "menu", &at));
    BT_CHECK_EQ(at, (size_t)2);
    bt_usage_action *actions = bt_usage_script(0.5F, &at);
    BT_CHECK_EQ(at, count);
    free(actions);
    free(g_actions);
    g_actions = NULL;
}
