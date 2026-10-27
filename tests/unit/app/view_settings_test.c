#include "app/views/settings.h"
#include "app/views/state.h"
#include "common.h"

#include <stdio.h>
#include <string.h>

BT_TEST(view_settings, draft_round_trips_settings) {
    bt_settings settings;
    settings.memo_cadence_ms = 650;
    settings.memo_mode = BT_MEMO_IMAGES;
    settings.drill_reps = 8;
    bt_settings_draft draft;
    bt_view_settings_draft_from(&settings, &draft);
    bt_settings round;
    bt_view_settings_draft_apply(&draft, &round);
    BT_CHECK(round.memo_cadence_ms == 650);
    BT_CHECK(round.memo_mode == BT_MEMO_IMAGES);
    BT_CHECK(round.drill_reps == 8);
}

BT_TEST(view_settings, clamps_keep_values_usable) {
    BT_CHECK(bt_view_settings_clamp_cadence(0) == 1);
    BT_CHECK(bt_view_settings_clamp_cadence(500) == 500);
    BT_CHECK(bt_view_settings_clamp_cadence(100000) == 10000);
    BT_CHECK(bt_view_settings_clamp_reps(-3) == 1);
    BT_CHECK(bt_view_settings_clamp_reps(999) == 100);
}

BT_TEST(view_settings, mode_rows_cover_every_mode_in_label_order) {
    for (int row = 0; row < BT_MEMO_MODE_COUNT; row++) {
        bt_memo_mode mode;
        BT_CHECK(bt_memo_mode_from_index(row, &mode) == 1);
        BT_CHECK(bt_memo_mode_index(mode) == row);
        BT_CHECK(mode == BT_MEMO_MODE_LABELS[row].mode);
        BT_CHECK(strcmp(bt_memo_mode_label(mode), BT_MEMO_MODE_LABELS[row].label) == 0);
    }
    bt_memo_mode past;
    BT_CHECK(bt_memo_mode_from_index(BT_MEMO_MODE_COUNT, &past) == 0);
    BT_CHECK(bt_memo_mode_parse("per-orbit", &past) == 1 && past == BT_MEMO_PER_ORBIT);
}

BT_TEST(view_settings, hint_line_lists_every_mode) {
    bt_settings_draft draft;
    memset(&draft, 0, sizeof draft);
    draft.memo_mode = BT_MEMO_PER_ORBIT;
    char lines[BT_SETTINGS_LINE_COUNT][BT_SETTINGS_TEXT_MAX];
    bt_view_settings_lines(&draft, lines);
    const char *hint = lines[BT_SETTINGS_LINE_COUNT - 1];
    for (int i = 0; i < BT_MEMO_MODE_COUNT; i++) {
        BT_CHECK(strstr(hint, BT_MEMO_MODE_LABELS[i].label) != NULL);
    }
    BT_CHECK(strcmp(lines[0], "memo cadence 0 ms") == 0);
    BT_CHECK(strcmp(lines[1], "memo mode per-orbit") == 0);
    BT_CHECK(strcmp(lines[2], "drill reps 0") == 0);
    BT_CHECK(strcmp(hint, "1 syllables  2 images  3 per-orbit  edits save immediately") == 0);
}
