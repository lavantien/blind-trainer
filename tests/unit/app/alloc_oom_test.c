#include "app/persist.h"
#include "app/usage_script.h"
#include "app/views/algs/browse.h"
#include "app/views/menu.h"
#include "app/views/solve.h"
#include "app/views/state.h"

#include "common.h"
#include "common/alloc.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

BT_TEST(alloc_oom, str_list_push_fails_closed_on_both_allocation_arms) {
    bt_str_list list;
    bt_str_list_init(&list);
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_str_list_push(&list, "alpha"), -1);
    BT_CHECK_EQ(list.count, 0u);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(bt_str_list_push(&list, "alpha"), 0);
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_str_list_push(&list, "beta"), -1);
    BT_CHECK_EQ(list.count, 1u);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(bt_str_list_push(&list, "beta"), 0);
    BT_CHECK_EQ(list.count, 2u);
    bt_str_list_free(&list);
}

BT_TEST(alloc_oom, str_list_sorted_insert_fails_closed_under_failure) {
    bt_str_list list;
    bt_str_list_init(&list);
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_str_list_sorted_insert_unique(&list, "b"), -1);
    BT_CHECK_EQ(list.count, 0u);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(bt_str_list_sorted_insert_unique(&list, "b"), 0);
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_str_list_sorted_insert_unique(&list, "a"), -1);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(bt_str_list_sorted_insert_unique(&list, "a"), 0);
    BT_CHECK_EQ(list.count, 2u);
    bt_str_list_free(&list);
}

BT_TEST(alloc_oom, syllable_book_insert_fails_closed_on_every_allocation_arm) {
    bt_syllable_book book;
    bt_syllable_book_init(&book);
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_syllable_book_insert(&book, "edge", "AB", "lai"), -1);
    BT_CHECK_EQ(book.count, 0u);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(bt_syllable_book_insert(&book, "edge", "AB", "lai"), 0);
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_syllable_book_insert(&book, "edge", "AB", "mo"), -1);
    BT_CHECK(strcmp(bt_syllable_book_for(&book, "edge", "AB"), "lai") == 0);
    bt_alloc_fail_disarm();
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_syllable_book_insert(&book, "corner", "CD", "nhu"), -1);
    bt_alloc_fail_disarm();
    bt_alloc_fail_arm(1);
    BT_CHECK_EQ(bt_syllable_book_insert(&book, "corner", "CD", "nhu"), -1);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(bt_syllable_book_insert(&book, "corner", "CD", "nhu"), 0);
    BT_CHECK_EQ(book.count, 2u);
    bt_syllable_book_free(&book);
}

BT_TEST(alloc_oom, solve_broken_push_fails_closed_and_keeps_the_count) {
    bt_solve_outcome outcome;
    bt_solve_outcome_init(&outcome);
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_solve_outcome_push_broken(&outcome, "UB"), -1);
    BT_CHECK_EQ(outcome.broken_count, 0u);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(bt_solve_outcome_push_broken(&outcome, "UB"), 0);
    BT_CHECK_EQ(outcome.broken_count, 1u);
    bt_solve_outcome_free(&outcome);
}

BT_TEST(alloc_oom, menu_warning_context_returns_null_under_allocation_failure) {
    bt_str_list warnings;
    bt_str_list_init(&warnings);
    BT_CHECK_EQ(bt_str_list_push(&warnings, "the sq1 pack lost a row and hid its event"), 0);
    BT_CHECK_EQ(bt_str_list_push(&warnings, "the megaminx pack lost a row and hid its event"), 0);
    bt_alloc_fail_arm(0);
    BT_CHECK(bt_view_menu_warning_context(&warnings) == NULL);
    bt_alloc_fail_disarm();
    bt_alloc_fail_arm(1);
    BT_CHECK(bt_view_menu_warning_context(&warnings) == NULL);
    bt_alloc_fail_disarm();
    char *joined = bt_view_menu_warning_context(&warnings);
    BT_CHECK(joined != NULL);
    BT_CHECK(strstr(joined, "sq1 pack") != NULL);
    BT_CHECK(strstr(joined, "megaminx pack") != NULL);
    free(joined);
    bt_str_list_free(&warnings);
}

BT_TEST(alloc_oom, repo_browser_default_survives_a_failed_arms_allocation) {
    bt_repo_browser browser;
    bt_alloc_fail_arm(0);
    bt_repo_browser_default(&browser);
    bt_alloc_fail_disarm();
    BT_CHECK(bt_repo_browser_has_seed_arms(&browser) == false);
    bt_repo_browser_free(&browser);
}

BT_TEST(alloc_oom, repo_arms_survives_allocation_failure_at_every_depth) {
    bt_pack_entry packs[2];
    memset(packs, 0, sizeof packs);
    snprintf(packs[0].event, sizeof packs[0].event, "3x3");
    snprintf(packs[0].method, sizeof packs[0].method, "cfop");
    snprintf(packs[0].stage, sizeof packs[0].stage, "pll");
    snprintf(packs[1].event, sizeof packs[1].event, "2x2");
    snprintf(packs[1].method, sizeof packs[1].method, "lbl");
    snprintf(packs[1].stage, sizeof packs[1].stage, "oll");
    bt_str_list hidden;
    bt_str_list_init(&hidden);
    for (size_t ok = 0; ok <= 16; ok++) {
        bt_alloc_fail_arm(ok);
        size_t arm_count = 99;
        bt_repo_node *arms = bt_repo_arms(packs, 2, &hidden, NULL, 0, &arm_count);
        bt_alloc_fail_disarm();
        if (arms != NULL) {
            for (size_t i = 0; i < arm_count; i++) {
                bt_repo_node_free(&arms[i]);
            }
            free(arms);
        } else {
            BT_CHECK_EQ(arm_count, 0u);
        }
    }
    bt_str_list_free(&hidden);
}

BT_TEST(alloc_oom, usage_script_degrades_to_a_prefix_under_allocation_failure) {
    size_t full_count = 0;
    bt_usage_action *full = bt_usage_script(0.05f, &full_count);
    BT_CHECK(full != NULL);
    BT_CHECK(full_count > 40);
    size_t count = 999;
    bt_alloc_fail_arm(0);
    bt_usage_action *script = bt_usage_script(0.05f, &count);
    bt_alloc_fail_disarm();
    BT_CHECK(script == NULL);
    BT_CHECK_EQ(count, 0u);
    free(script);
    count = 999;
    bt_alloc_fail_arm(1);
    script = bt_usage_script(0.05f, &count);
    bt_alloc_fail_disarm();
    BT_CHECK(script != NULL);
    BT_CHECK(count > 0);
    BT_CHECK(count < full_count);
    BT_CHECK_EQ(script[0].kind, full[0].kind);
    free(script);
    free(full);
}
