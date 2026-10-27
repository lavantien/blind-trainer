#include "app/ui.h"

#include "common/bt_harness.h"

#include <string.h>

BT_TEST(ui, hit_testing_covers_the_rect_edges) {
    bt_ui_rect rect = {10.0F, 20.0F, 100.0F, 50.0F};
    BT_CHECK(bt_ui_contains(&rect, 10.0F, 20.0F));
    BT_CHECK(bt_ui_contains(&rect, 109.0F, 69.0F));
    BT_CHECK(!bt_ui_contains(&rect, 110.0F, 20.0F));
    BT_CHECK(!bt_ui_contains(&rect, 10.0F, 70.0F));
    BT_CHECK(!bt_ui_contains(&rect, 9.0F, 19.0F));
}

BT_TEST(ui, button_state_tracks_hover_click_and_press) {
    bt_ui_rect rect = {0.0F, 0.0F, 80.0F, 30.0F};
    bt_ui_input input;
    bt_ui_input_clear(&input);
    input.x = 40.0F;
    input.y = 15.0F;
    bt_ui_button_state state = bt_ui_button_hit(&rect, &input);
    BT_CHECK(state.hovered);
    BT_CHECK(!state.clicked);
    input.click = 1;
    state = bt_ui_button_hit(&rect, &input);
    BT_CHECK(state.clicked);
    input.click = 0;
    input.down = 1;
    state = bt_ui_button_hit(&rect, &input);
    BT_CHECK(state.pressed);
    input.x = 500.0F;
    state = bt_ui_button_hit(&rect, &input);
    BT_CHECK(!state.hovered && !state.clicked && !state.pressed);
}

BT_TEST(ui, list_window_scrolls_and_clamps_to_the_content) {
    bt_ui_list_window window = {0, 0};
    BT_CHECK_EQ(bt_ui_list_scroll(&window, 100, 10, 3), 1);
    BT_CHECK_EQ(window.first, 3);
    bt_ui_list_scroll(&window, 100, 10, -100);
    BT_CHECK_EQ(window.first, 0);
    bt_ui_list_scroll(&window, 100, 10, 500);
    BT_CHECK_EQ(window.first, 90);
    bt_ui_list_scroll(&window, 5, 10, 7);
    BT_CHECK_EQ(window.first, 0);
}

BT_TEST(ui, list_show_reveals_the_selection) {
    bt_ui_list_window window = {50, 0};
    window.selected = 10;
    bt_ui_list_show(&window, 100, 10);
    BT_CHECK_EQ(window.first, 10);
    window.selected = 3;
    bt_ui_list_show(&window, 100, 10);
    BT_CHECK_EQ(window.first, 3);
    window.first = 0;
    window.selected = 95;
    bt_ui_list_show(&window, 100, 10);
    BT_CHECK_EQ(window.first, 86);
}

BT_TEST(ui, field_edit_appends_utf8_and_backspaces_at_the_cursor) {
    char buffer[BT_UI_TEXT_MAX] = "ab";
    size_t cursor = 1;
    bt_ui_input input;
    bt_ui_input_clear(&input);
    int committed = 0;
    int canceled = 0;
    input.chars[0] = 'X';
    input.char_count = 1;
    BT_CHECK_EQ(bt_ui_field_edit(buffer, sizeof buffer, &cursor, &input, &committed, &canceled), 0);
    BT_CHECK_EQ(strcmp(buffer, "aXb"), 0);
    BT_CHECK_EQ((int)cursor, 2);
    BT_CHECK(!committed && !canceled);
    bt_ui_input_clear(&input);
    input.backspace = 1;
    BT_CHECK_EQ(bt_ui_field_edit(buffer, sizeof buffer, &cursor, &input, &committed, &canceled), 0);
    BT_CHECK_EQ(strcmp(buffer, "ab"), 0);
    BT_CHECK_EQ((int)cursor, 1);
}

BT_TEST(ui, field_edit_encodes_vietnamese_and_stops_at_capacity) {
    char buffer[8] = "";
    size_t cursor = 0;
    bt_ui_input input;
    bt_ui_input_clear(&input);
    int committed = 0;
    int canceled = 0;
    input.chars[0] = 0x1EA5;
    input.char_count = 1;
    BT_CHECK_EQ(bt_ui_field_edit(buffer, sizeof buffer, &cursor, &input, &committed, &canceled), 0);
    BT_CHECK_EQ((int)cursor, 3);
    BT_CHECK_EQ((unsigned char)buffer[0], 0xE1U);
    BT_CHECK_EQ((unsigned char)buffer[1], 0xBAU);
    BT_CHECK_EQ((unsigned char)buffer[2], 0xA5U);
    bt_ui_input_clear(&input);
    input.chars[0] = 'a';
    input.chars[1] = 'b';
    input.chars[2] = 'c';
    input.chars[3] = 'd';
    input.chars[4] = 'e';
    input.chars[5] = 'f';
    input.char_count = 6;
    BT_CHECK_EQ(bt_ui_field_edit(buffer, sizeof buffer, &cursor, &input, &committed, &canceled), 0);
    BT_CHECK_EQ(strlen(buffer), 7);
}

BT_TEST(ui, field_enter_commits_and_escape_cancels) {
    char buffer[BT_UI_TEXT_MAX] = "query";
    size_t cursor = 5;
    bt_ui_input input;
    bt_ui_input_clear(&input);
    int committed = 0;
    int canceled = 0;
    input.enter = 1;
    BT_CHECK_EQ(bt_ui_field_edit(buffer, sizeof buffer, &cursor, &input, &committed, &canceled), 0);
    BT_CHECK(committed && !canceled);
    bt_ui_input_clear(&input);
    input.escape = 1;
    BT_CHECK_EQ(bt_ui_field_edit(buffer, sizeof buffer, &cursor, &input, &committed, &canceled), 1);
    BT_CHECK(canceled && !committed);
}

BT_TEST(ui, number_clamp_holds_the_bounds) {
    BT_CHECK_EQ(bt_ui_number_clamp(0, 1, 10000), 1);
    BT_CHECK_EQ(bt_ui_number_clamp(99999, 1, 10000), 10000);
    BT_CHECK_EQ(bt_ui_number_clamp(500, 1, 10000), 500);
    BT_CHECK_EQ(bt_ui_number_clamp(7, 1, 100), 7);
}
