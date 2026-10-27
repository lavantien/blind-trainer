#include "app/states.h"

#include "common/bt_harness.h"

BT_TEST(states, tour_walks_the_views_then_fatal) {
    BT_CHECK_EQ(BT_APP_TOUR_COUNT, 10);
    BT_CHECK_EQ(BT_APP_TOUR[0], BT_APP_MENU);
    BT_CHECK_EQ(BT_APP_TOUR[1], BT_APP_DRILL);
    BT_CHECK_EQ(BT_APP_TOUR[2], BT_APP_MEMO);
    BT_CHECK_EQ(BT_APP_TOUR[3], BT_APP_SOLVE);
    BT_CHECK_EQ(BT_APP_TOUR[4], BT_APP_REPLAY);
    BT_CHECK_EQ(BT_APP_TOUR[5], BT_APP_ALGS);
    BT_CHECK_EQ(BT_APP_TOUR[6], BT_APP_CASE_DRILL);
    BT_CHECK_EQ(BT_APP_TOUR[7], BT_APP_STATS);
    BT_CHECK_EQ(BT_APP_TOUR[8], BT_APP_SETTINGS);
    BT_CHECK_EQ(BT_APP_TOUR[9], BT_APP_FATAL);
}

BT_TEST(states, menu_keys_open_the_six_views) {
    BT_CHECK_EQ(BT_APP_MENU_KEY_COUNT, 6);
    const char expected_keys[BT_APP_MENU_KEY_COUNT] = {'1', '2', '3', '4', '5', '6'};
    const bt_app_state expected_targets[BT_APP_MENU_KEY_COUNT] = {
        BT_APP_DRILL, BT_APP_MEMO, BT_APP_SOLVE, BT_APP_ALGS, BT_APP_STATS, BT_APP_SETTINGS,
    };
    for (int i = 0; i < BT_APP_MENU_KEY_COUNT; i++) {
        BT_CHECK_EQ(BT_APP_MENU_KEYS[i].digit, expected_keys[i]);
        BT_CHECK_EQ(BT_APP_MENU_KEYS[i].target, expected_targets[i]);
    }
}

BT_TEST(states, slug_and_label_strings_match_the_app_surface) {
    BT_CHECK_EQ(strcmp(bt_app_state_slug(BT_APP_BOOTSTRAP), "bootstrap"), 0);
    BT_CHECK_EQ(strcmp(bt_app_state_slug(BT_APP_FATAL), "fatal"), 0);
    BT_CHECK_EQ(strcmp(bt_app_state_slug(BT_APP_MENU), "menu"), 0);
    BT_CHECK_EQ(strcmp(bt_app_state_slug(BT_APP_DRILL), "drill"), 0);
    BT_CHECK_EQ(strcmp(bt_app_state_slug(BT_APP_MEMO), "memo"), 0);
    BT_CHECK_EQ(strcmp(bt_app_state_slug(BT_APP_SOLVE), "solve"), 0);
    BT_CHECK_EQ(strcmp(bt_app_state_slug(BT_APP_REPLAY), "replay"), 0);
    BT_CHECK_EQ(strcmp(bt_app_state_slug(BT_APP_ALGS), "algs"), 0);
    BT_CHECK_EQ(strcmp(bt_app_state_slug(BT_APP_CASE_DRILL), "case-drill"), 0);
    BT_CHECK_EQ(strcmp(bt_app_state_slug(BT_APP_STATS), "stats"), 0);
    BT_CHECK_EQ(strcmp(bt_app_state_slug(BT_APP_SETTINGS), "settings"), 0);
    BT_CHECK_EQ(strcmp(bt_app_state_label(BT_APP_BOOTSTRAP), "booting"), 0);
    BT_CHECK_EQ(strcmp(bt_app_state_label(BT_APP_FATAL), "fatal"), 0);
    BT_CHECK_EQ(strcmp(bt_app_state_label(BT_APP_MENU), "menu"), 0);
    BT_CHECK_EQ(strcmp(bt_app_state_label(BT_APP_DRILL), "drill"), 0);
    BT_CHECK_EQ(strcmp(bt_app_state_label(BT_APP_MEMO), "memo training"), 0);
    BT_CHECK_EQ(strcmp(bt_app_state_label(BT_APP_SOLVE), "solve"), 0);
    BT_CHECK_EQ(strcmp(bt_app_state_label(BT_APP_REPLAY), "3d replay"), 0);
    BT_CHECK_EQ(strcmp(bt_app_state_label(BT_APP_ALGS), "algs"), 0);
    BT_CHECK_EQ(strcmp(bt_app_state_label(BT_APP_CASE_DRILL), "case drill"), 0);
    BT_CHECK_EQ(strcmp(bt_app_state_label(BT_APP_STATS), "stats"), 0);
    BT_CHECK_EQ(strcmp(bt_app_state_label(BT_APP_SETTINGS), "settings"), 0);
}

BT_TEST(states, subphase_defaults_start_each_phase_ladder) {
    BT_CHECK_EQ(BT_DRILL_CARD, 0);
    BT_CHECK_EQ(BT_CASE_DRILL_SHOW, 0);
    BT_CHECK_EQ(BT_SOLVE_READY, 0);
    BT_CHECK_EQ(BT_MEMO_INTRO, 0);
    BT_CHECK_EQ(BT_DRILL_REPS, BT_DRILL_CARD + 1);
    BT_CHECK_EQ(BT_DRILL_RATE, BT_DRILL_REPS + 1);
    BT_CHECK_EQ(BT_CASE_DRILL_GRADE, BT_CASE_DRILL_SHOW + 1);
    BT_CHECK_EQ(BT_SOLVE_MEMO, BT_SOLVE_READY + 1);
    BT_CHECK_EQ(BT_SOLVE_EXEC, BT_SOLVE_MEMO + 1);
    BT_CHECK_EQ(BT_SOLVE_REVIEW, BT_SOLVE_EXEC + 1);
    BT_CHECK_EQ(BT_MEMO_STREAM, BT_MEMO_INTRO + 1);
    BT_CHECK_EQ(BT_MEMO_RECALL, BT_MEMO_STREAM + 1);
    BT_CHECK_EQ(BT_MEMO_REVIEW, BT_MEMO_RECALL + 1);
}
