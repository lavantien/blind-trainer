#include "app/states.h"

#include "common/bt_trace.h"

const bt_app_state BT_APP_TOUR[BT_APP_TOUR_COUNT] = {
    BT_APP_MENU, BT_APP_DRILL,      BT_APP_MEMO,  BT_APP_SOLVE,    BT_APP_REPLAY,
    BT_APP_ALGS, BT_APP_CASE_DRILL, BT_APP_STATS, BT_APP_SETTINGS, BT_APP_FATAL,
};

const bt_app_menu_key BT_APP_MENU_KEYS[BT_APP_MENU_KEY_COUNT] = {
    {'1', BT_APP_DRILL}, {'2', BT_APP_MEMO},  {'3', BT_APP_SOLVE},
    {'4', BT_APP_ALGS},  {'5', BT_APP_STATS}, {'6', BT_APP_SETTINGS},
};

static const char *const BT_APP_STATE_SLUGS[BT_APP_STATE_COUNT] = {
    "bootstrap", "fatal", "menu",       "drill", "memo",     "solve",
    "replay",    "algs",  "case-drill", "stats", "settings",
};

static const char *const BT_APP_STATE_LABELS[BT_APP_STATE_COUNT] = {
    "booting",   "fatal", "menu",       "drill", "memo training", "solve",
    "3d replay", "algs",  "case drill", "stats", "settings",
};

const char *bt_app_state_slug(bt_app_state state) {
    BT_TRACE_ENTER("app.state_slug");
    int raw = (int)state;
    const char *slug = (raw >= 0 && raw < BT_APP_STATE_COUNT) ? BT_APP_STATE_SLUGS[raw] : NULL;
    if (slug == NULL) {
        BT_TRACE_FAIL("app.state_slug", "state %d out of range", raw);
    }
    BT_TRACE_EXIT("app.state_slug");
    return slug;
}

const char *bt_app_state_label(bt_app_state state) {
    BT_TRACE_ENTER("app.state_label");
    int raw = (int)state;
    const char *label = (raw >= 0 && raw < BT_APP_STATE_COUNT) ? BT_APP_STATE_LABELS[raw] : NULL;
    if (label == NULL) {
        BT_TRACE_FAIL("app.state_label", "state %d out of range", raw);
    }
    BT_TRACE_EXIT("app.state_label");
    return label;
}

int bt_app_state_valid(int state) {
    BT_TRACE_ENTER("app.state_valid");
    int valid = state >= 0 && state < BT_APP_STATE_COUNT;
    if (!valid) {
        BT_TRACE_FAIL("app.state_valid", "state %d out of range", state);
    }
    BT_TRACE_EXIT("app.state_valid");
    return valid;
}
