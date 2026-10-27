#ifndef BT_APP_STATES_H
#define BT_APP_STATES_H

#define BT_APP_TOUR_COUNT 10
#define BT_APP_MENU_KEY_COUNT 6

typedef enum {
    BT_APP_BOOTSTRAP = 0,
    BT_APP_FATAL,
    BT_APP_MENU,
    BT_APP_DRILL,
    BT_APP_MEMO,
    BT_APP_SOLVE,
    BT_APP_REPLAY,
    BT_APP_ALGS,
    BT_APP_CASE_DRILL,
    BT_APP_STATS,
    BT_APP_SETTINGS,
    BT_APP_STATE_COUNT,
} bt_app_state;

typedef enum {
    BT_DRILL_CARD = 0,
    BT_DRILL_REPS,
    BT_DRILL_RATE,
} bt_drill_phase;

typedef enum {
    BT_CASE_DRILL_SHOW = 0,
    BT_CASE_DRILL_GRADE,
} bt_case_drill_phase;

typedef enum {
    BT_SOLVE_READY = 0,
    BT_SOLVE_MEMO,
    BT_SOLVE_EXEC,
    BT_SOLVE_REVIEW,
} bt_solve_phase;

typedef enum {
    BT_MEMO_INTRO = 0,
    BT_MEMO_STREAM,
    BT_MEMO_RECALL,
    BT_MEMO_REVIEW,
} bt_memo_phase;

typedef struct {
    char digit;
    bt_app_state target;
} bt_app_menu_key;

extern const bt_app_state BT_APP_TOUR[BT_APP_TOUR_COUNT];
extern const bt_app_menu_key BT_APP_MENU_KEYS[BT_APP_MENU_KEY_COUNT];

const char *bt_app_state_slug(bt_app_state state);
const char *bt_app_state_label(bt_app_state state);
int bt_app_state_valid(int state);

#endif
