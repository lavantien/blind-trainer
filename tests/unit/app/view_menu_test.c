#include "app/views/menu.h"

#include "common.h"

#include <stdio.h>
#include <string.h>

static bt_str_list list_of(const char *const *items, size_t count) {
    bt_str_list list;
    bt_str_list_init(&list);
    for (size_t i = 0; i < count; i++) {
        bt_str_list_push(&list, items[i]);
    }
    return list;
}

BT_TEST(view_menu, menu_rows_list_every_key_with_its_description) {
    bt_view_menu_row rows[BT_VIEW_MENU_ROW_COUNT];
    bt_view_menu_rows(rows);
    const char *titles[BT_VIEW_MENU_ROW_COUNT] = {
        "1 drill", "2 memo training", "3 solve", "4 algs", "5 stats", "6 settings",
    };
    for (int i = 0; i < BT_VIEW_MENU_ROW_COUNT; i++) {
        char title[BT_VIEW_MENU_TEXT_MAX];
        bt_view_menu_title(&rows[i], title);
        BT_CHECK(strcmp(title, titles[i]) == 0);
    }
    BT_CHECK(strcmp(rows[0].description, "physical 3-style pair drilling with spaced repetition") ==
             0);
    BT_CHECK(strcmp(rows[1].description, "streaming memo on seeded scrambles") == 0);
    BT_CHECK(strcmp(rows[2].description, "the timed full cycle") == 0);
    BT_CHECK(strcmp(rows[3].description, "browse 818 cases and the event repos") == 0);
    BT_CHECK(strcmp(rows[4].description, "solve history and review load") == 0);
    BT_CHECK(strcmp(rows[5].description, "cadence, memo mode, reps") == 0);
}

BT_TEST(view_menu, warning_context_joins_long_warning_lists_in_full) {
    char big[300];
    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = 0;
    const char *items[3] = {big, big, big};
    bt_str_list list = list_of(items, 3);
    char *context = bt_view_menu_warning_context(&list);
    size_t wanted = (sizeof big - 1) * 3 + strlen(" | ") * 2;
    BT_CHECK(strlen(context) == wanted);
    free(context);
    bt_str_list_free(&list);
}

BT_TEST(view_menu, warning_context_joins_boot_warnings) {
    bt_str_list empty;
    bt_str_list_init(&empty);
    char *context = bt_view_menu_warning_context(&empty);
    BT_CHECK(context != NULL && strcmp(context, "") == 0);
    free(context);
    bt_str_list_free(&empty);

    const char *one_items[1] = {"vendor data missing"};
    bt_str_list one = list_of(one_items, 1);
    char *one_text = bt_view_menu_warning_context(&one);
    BT_CHECK(one_text != NULL && strcmp(one_text, "vendor data missing") == 0);
    free(one_text);
    bt_str_list_free(&one);

    const char *two_items[2] = {"a", "b"};
    bt_str_list two = list_of(two_items, 2);
    char *two_text = bt_view_menu_warning_context(&two);
    BT_CHECK(two_text != NULL && strcmp(two_text, "a | b") == 0);
    free(two_text);
    bt_str_list_free(&two);
}
