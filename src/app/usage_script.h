#ifndef BT_APP_USAGE_SCRIPT_H
#define BT_APP_USAGE_SCRIPT_H

#include "app/states.h"

#include <stddef.h>

#define BT_USAGE_SCRIPT_MAX 124
#define BT_USAGE_SHOT_MAX 19

typedef enum {
    BT_USAGE_ACT_PRESS = 0,
    BT_USAGE_ACT_PRESS_CHAR,
    BT_USAGE_ACT_NAV_OPEN,
    BT_USAGE_ACT_NAV_MENU,
    BT_USAGE_ACT_PIN_DRILL,
    BT_USAGE_ACT_PICK_SOLVE_SEED,
    BT_USAGE_ACT_WAIT_APP,
    BT_USAGE_ACT_WAIT_FONTS,
    BT_USAGE_ACT_WAIT_DRILL,
    BT_USAGE_ACT_WAIT_MEMO,
    BT_USAGE_ACT_WAIT_SOLVE,
    BT_USAGE_ACT_WAIT_CASE_DRILL,
    BT_USAGE_ACT_ELAPSE,
    BT_USAGE_ACT_SHOT,
} bt_usage_action_kind;

typedef struct {
    bt_usage_action_kind kind;
    int key;
    char ch;
    bt_app_state app;
    bt_drill_phase drill;
    bt_memo_phase memo;
    bt_solve_phase solve;
    bt_case_drill_phase case_drill;
    float secs;
    const char *name;
} bt_usage_action;

size_t bt_usage_script(float cadence_secs, bt_usage_action *out, size_t cap);
size_t bt_usage_fatal_script(bt_usage_action *out, size_t cap);

#endif
