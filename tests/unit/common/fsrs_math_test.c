#include "common/bt_harness.h"
#include "common/fsrs_math.h"

#include <float.h>

BT_TEST(fsrs, init_installs_round_to_nearest_even) {
    BT_CHECK_EQ(bt_fsrs_math_init(), 0);
    BT_CHECK_EQ(FLT_ROUNDS, 1);
    BT_CHECK_EQ(bt_fsrs_math_init(), 0);
}

BT_TEST(fsrs, half_even_rounds_ties_to_even) {
    BT_CHECK_EQ(bt_fsrs_math_init(), 0);
    BT_CHECK_EQ(bt_round_half_even(0.5), 0.0);
    BT_CHECK_EQ(bt_round_half_even(1.5), 2.0);
    BT_CHECK_EQ(bt_round_half_even(2.5), 2.0);
    BT_CHECK_EQ(bt_round_half_even(3.5), 4.0);
    BT_CHECK_EQ(bt_round_half_even(-0.5), -0.0);
    BT_CHECK_EQ(bt_round_half_even(-1.5), -2.0);
    BT_CHECK_EQ(bt_round_half_even(-2.5), -2.0);
}

BT_TEST(fsrs, half_even_sign_of_negative_zero_tie) {
    BT_CHECK_EQ(bt_fsrs_math_init(), 0);
    double rounded = bt_round_half_even(-0.5);
    BT_CHECK_EQ(rounded, 0.0);
    BT_CHECK(signbit(rounded) != 0);
}

BT_TEST(fsrs, half_even_non_tie_values) {
    BT_CHECK_EQ(bt_fsrs_math_init(), 0);
    BT_CHECK_EQ(bt_round_half_even(1.4), 1.0);
    BT_CHECK_EQ(bt_round_half_even(1.6), 2.0);
    BT_CHECK_EQ(bt_round_half_even(-1.4), -1.0);
    BT_CHECK_EQ(bt_round_half_even(-1.6), -2.0);
    BT_CHECK_EQ(bt_round_half_even(42.0), 42.0);
    BT_CHECK_EQ(bt_round_half_even(0.0), 0.0);
}
