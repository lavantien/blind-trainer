#include "app/usage_script.h"

#include "common/bt_trace.h"

#include "raylib.h"

static bt_usage_action *g_out;
static size_t g_cap;
static size_t g_at;

static void begin(bt_usage_action *out, size_t cap) {
    g_out = out;
    g_cap = cap;
    g_at = 0;
}

static void emit(bt_usage_action action) {
    if (g_at >= g_cap) {
        BT_TRACE_FAIL("app.usage_script", "capacity %zu exceeded", g_cap);
        return;
    }
    g_out[g_at++] = action;
}

static void press(int key) {
    bt_usage_action action = {0};
    action.kind = BT_USAGE_ACT_PRESS;
    action.key = key;
    emit(action);
}

static void press_char(int key, char ch) {
    bt_usage_action action = {0};
    action.kind = BT_USAGE_ACT_PRESS_CHAR;
    action.key = key;
    action.ch = ch;
    emit(action);
}

static void nav_open(bt_app_state app) {
    bt_usage_action action = {0};
    action.kind = BT_USAGE_ACT_NAV_OPEN;
    action.app = app;
    emit(action);
}

static void nav_menu(void) {
    bt_usage_action action = {0};
    action.kind = BT_USAGE_ACT_NAV_MENU;
    emit(action);
}

static void pin_drill(void) {
    bt_usage_action action = {0};
    action.kind = BT_USAGE_ACT_PIN_DRILL;
    emit(action);
}

static void pick_solve_seed(void) {
    bt_usage_action action = {0};
    action.kind = BT_USAGE_ACT_PICK_SOLVE_SEED;
    emit(action);
}

static void wait_app(bt_app_state app) {
    bt_usage_action action = {0};
    action.kind = BT_USAGE_ACT_WAIT_APP;
    action.app = app;
    emit(action);
}

static void wait_fonts(void) {
    bt_usage_action action = {0};
    action.kind = BT_USAGE_ACT_WAIT_FONTS;
    emit(action);
}

static void wait_drill(bt_drill_phase drill) {
    bt_usage_action action = {0};
    action.kind = BT_USAGE_ACT_WAIT_DRILL;
    action.drill = drill;
    emit(action);
}

static void wait_memo(bt_memo_phase memo) {
    bt_usage_action action = {0};
    action.kind = BT_USAGE_ACT_WAIT_MEMO;
    action.memo = memo;
    emit(action);
}

static void wait_solve(bt_solve_phase solve) {
    bt_usage_action action = {0};
    action.kind = BT_USAGE_ACT_WAIT_SOLVE;
    action.solve = solve;
    emit(action);
}

static void wait_case_drill(bt_case_drill_phase case_drill) {
    bt_usage_action action = {0};
    action.kind = BT_USAGE_ACT_WAIT_CASE_DRILL;
    action.case_drill = case_drill;
    emit(action);
}

static void elapse(float secs) {
    bt_usage_action action = {0};
    action.kind = BT_USAGE_ACT_ELAPSE;
    action.secs = secs;
    emit(action);
}

static void shot(const char *name) {
    bt_usage_action action = {0};
    action.kind = BT_USAGE_ACT_SHOT;
    action.name = name;
    emit(action);
}

size_t bt_usage_script(float cadence_secs, bt_usage_action *out, size_t cap) {
    BT_TRACE_ENTER("app.usage_script");
    float partial = cadence_secs * 3.25F;
    begin(out, cap);
    wait_app(BT_APP_MENU);
    wait_fonts();
    shot("menu");
    pin_drill();
    nav_open(BT_APP_DRILL);
    wait_drill(BT_DRILL_CARD);
    shot("drill-card");
    press(KEY_SPACE);
    wait_drill(BT_DRILL_REPS);
    shot("drill-reps");
    press(KEY_SPACE);
    elapse(0.1F);
    press(KEY_SPACE);
    elapse(0.1F);
    press(KEY_SPACE);
    elapse(0.1F);
    press(KEY_SPACE);
    elapse(0.1F);
    press(KEY_SPACE);
    elapse(0.1F);
    press(KEY_SPACE);
    elapse(0.1F);
    wait_drill(BT_DRILL_RATE);
    shot("drill-rate-a");
    press(KEY_THREE);
    elapse(0.1F);
    shot("drill-rate-b");
    press(KEY_THREE);
    wait_drill(BT_DRILL_CARD);
    nav_menu();
    wait_app(BT_APP_MENU);
    nav_open(BT_APP_MEMO);
    wait_memo(BT_MEMO_INTRO);
    shot("memo-intro");
    press(KEY_SPACE);
    wait_memo(BT_MEMO_STREAM);
    elapse(partial);
    shot("memo-stream");
    wait_memo(BT_MEMO_RECALL);
    shot("memo-recall");
    press(KEY_SPACE);
    wait_memo(BT_MEMO_REVIEW);
    shot("memo-review");
    nav_menu();
    wait_app(BT_APP_MENU);
    pick_solve_seed();
    nav_open(BT_APP_SOLVE);
    wait_solve(BT_SOLVE_READY);
    shot("solve-ready");
    press(KEY_SPACE);
    wait_solve(BT_SOLVE_MEMO);
    elapse(partial);
    shot("solve-memo");
    press(KEY_SPACE);
    wait_solve(BT_SOLVE_EXEC);
    elapse(1.6F);
    shot("solve-exec");
    press(KEY_SPACE);
    wait_solve(BT_SOLVE_REVIEW);
    shot("solve-review");
    nav_menu();
    wait_app(BT_APP_MENU);
    nav_open(BT_APP_ALGS);
    wait_app(BT_APP_ALGS);
    shot("algs");
    press(KEY_SLASH);
    elapse(0.1F);
    press_char(KEY_B, 'b');
    elapse(0.3F);
    press(KEY_DOWN);
    elapse(0.1F);
    press(KEY_DOWN);
    elapse(0.3F);
    shot("algs-search");
    nav_menu();
    wait_app(BT_APP_MENU);
    nav_open(BT_APP_ALGS);
    wait_app(BT_APP_ALGS);
    press(KEY_RIGHT);
    elapse(0.1F);
    press(KEY_DOWN);
    elapse(0.1F);
    press(KEY_RIGHT);
    elapse(0.1F);
    press(KEY_RIGHT);
    elapse(0.3F);
    press(KEY_D);
    wait_app(BT_APP_CASE_DRILL);
    wait_case_drill(BT_CASE_DRILL_SHOW);
    shot("case-drill-show");
    press(KEY_SPACE);
    wait_case_drill(BT_CASE_DRILL_GRADE);
    shot("case-drill-grade");
    press(KEY_ONE);
    elapse(0.1F);
    wait_case_drill(BT_CASE_DRILL_SHOW);
    press(KEY_SPACE);
    elapse(0.1F);
    wait_case_drill(BT_CASE_DRILL_GRADE);
    press(KEY_TWO);
    elapse(0.1F);
    wait_case_drill(BT_CASE_DRILL_SHOW);
    press(KEY_SPACE);
    elapse(0.1F);
    wait_case_drill(BT_CASE_DRILL_GRADE);
    press(KEY_THREE);
    elapse(0.1F);
    wait_case_drill(BT_CASE_DRILL_SHOW);
    press(KEY_SPACE);
    elapse(0.1F);
    wait_case_drill(BT_CASE_DRILL_GRADE);
    press(KEY_FOUR);
    elapse(0.1F);
    wait_case_drill(BT_CASE_DRILL_SHOW);
    press(KEY_ESCAPE);
    wait_app(BT_APP_MENU);
    nav_open(BT_APP_STATS);
    wait_app(BT_APP_STATS);
    shot("stats");
    nav_menu();
    wait_app(BT_APP_MENU);
    nav_open(BT_APP_SETTINGS);
    wait_app(BT_APP_SETTINGS);
    shot("settings");
    BT_TRACE_EXIT("app.usage_script");
    return g_at;
}

size_t bt_usage_fatal_script(bt_usage_action *out, size_t cap) {
    BT_TRACE_ENTER("app.usage_fatal_script");
    begin(out, cap);
    wait_app(BT_APP_FATAL);
    wait_fonts();
    shot("fatal");
    BT_TRACE_EXIT("app.usage_fatal_script");
    return g_at;
}
