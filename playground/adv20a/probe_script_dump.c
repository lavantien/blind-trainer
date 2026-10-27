#include "app/usage_script.h"

#include "raylib.h"

#include <stdio.h>
#include <string.h>

static const char *kind_name(bt_usage_action_kind kind) {
    switch (kind) {
    case BT_USAGE_ACT_PRESS:
        return "Press";
    case BT_USAGE_ACT_PRESS_CHAR:
        return "PressChar";
    case BT_USAGE_ACT_NAV_OPEN:
        return "NavOpen";
    case BT_USAGE_ACT_NAV_MENU:
        return "NavMenu";
    case BT_USAGE_ACT_PIN_DRILL:
        return "PinDrill";
    case BT_USAGE_ACT_PICK_SOLVE_SEED:
        return "PickSolveSeed";
    case BT_USAGE_ACT_WAIT_APP:
        return "WaitApp";
    case BT_USAGE_ACT_WAIT_FONTS:
        return "WaitFonts";
    case BT_USAGE_ACT_WAIT_DRILL:
        return "WaitDrill";
    case BT_USAGE_ACT_WAIT_MEMO:
        return "WaitMemo";
    case BT_USAGE_ACT_WAIT_SOLVE:
        return "WaitSolve";
    case BT_USAGE_ACT_WAIT_CASE_DRILL:
        return "WaitCaseDrill";
    case BT_USAGE_ACT_ELAPSE:
        return "Elapse";
    case BT_USAGE_ACT_SHOT:
        return "Shot";
    }
    return "Unknown";
}

static const char *state_name(bt_app_state state) {
    switch (state) {
    case BT_APP_MENU:
        return "Menu";
    case BT_APP_DRILL:
        return "Drill";
    case BT_APP_MEMO:
        return "Memo";
    case BT_APP_SOLVE:
        return "Solve";
    case BT_APP_ALGS:
        return "Algs";
    case BT_APP_CASE_DRILL:
        return "CaseDrill";
    case BT_APP_STATS:
        return "Stats";
    case BT_APP_SETTINGS:
        return "Settings";
    case BT_APP_REPLAY:
        return "Replay";
    case BT_APP_FATAL:
        return "Fatal";
    }
    return "Other";
}

int main(void) {
    bt_usage_action actions[BT_USAGE_SCRIPT_MAX];
    size_t count = bt_usage_script(0.8F, actions, BT_USAGE_SCRIPT_MAX);
    printf("count %zu\n", count);
    for (size_t i = 0; i < count; i++) {
        const bt_usage_action *a = &actions[i];
        printf("%zu %s", i, kind_name(a->kind));
        switch (a->kind) {
        case BT_USAGE_ACT_PRESS:
            printf(" %d", a->key);
            break;
        case BT_USAGE_ACT_PRESS_CHAR:
            printf(" %d %c", a->key, a->ch);
            break;
        case BT_USAGE_ACT_NAV_OPEN:
        case BT_USAGE_ACT_WAIT_APP:
            printf(" %s", state_name(a->app));
            break;
        case BT_USAGE_ACT_WAIT_DRILL:
        case BT_USAGE_ACT_WAIT_MEMO:
        case BT_USAGE_ACT_WAIT_SOLVE:
        case BT_USAGE_ACT_WAIT_CASE_DRILL:
            printf(" %d", (int)a->drill + (int)a->memo + (int)a->solve + (int)a->case_drill);
            break;
        case BT_USAGE_ACT_ELAPSE:
            printf(" %.6f", (double)a->secs);
            break;
        case BT_USAGE_ACT_SHOT:
            printf(" %s", a->name);
            break;
        default:
            break;
        }
        printf("\n");
    }
    bt_usage_action fatal[BT_USAGE_SCRIPT_MAX];
    size_t fatal_count = bt_usage_fatal_script(fatal, BT_USAGE_SCRIPT_MAX);
    printf("fatal-count %zu\n", fatal_count);
    return 0;
}
