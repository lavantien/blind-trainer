#include "common/bt_harness.h"
#include "data/notation.h"

BT_TEST(notation, rotation_tokens_cover_all_three_axes_and_suffixes) {
    const char *yes[] = {"x", "y", "z", "x2", "y'", "z2'", "x'2"};
    for (size_t i = 0; i < sizeof yes / sizeof yes[0]; i++) {
        BT_CHECK(bt_notation_is_rotation(yes[i]) == 1);
    }
    const char *no[] = {"F", "R'", "L2", "u", "w", "xx", ""};
    for (size_t i = 0; i < sizeof no / sizeof no[0]; i++) {
        BT_CHECK(bt_notation_is_rotation(no[i]) == 0);
    }
}

BT_TEST(notation, strip_rotations_removes_regrip_labels_only) {
    char out[128];
    BT_CHECK(bt_notation_strip_rotations("y2 R' F R F'", out, sizeof out) == 0);
    BT_CHECK_EQ(out, "R' F R F'");
    BT_CHECK(bt_notation_strip_rotations("y2 F' L F L' F' L F L'", out, sizeof out) == 0);
    BT_CHECK_EQ(out, "F' L F L' F' L F L'");
    BT_CHECK(bt_notation_strip_rotations("R' F R F' y2 R' F R F' z2 x' R' F R F'", out,
                                         sizeof out) == 0);
    BT_CHECK_EQ(out, "R' F R F' R' F R F' R' F R F'");
    BT_CHECK(bt_notation_strip_rotations("L F' L'", out, sizeof out) == 0);
    BT_CHECK_EQ(out, "L F' L'");
    BT_CHECK(bt_notation_strip_rotations("y2 y'", out, sizeof out) == 0);
    BT_CHECK_EQ(out, "");
}
