#include "common/bt_harness.h"

int bt_harness_noreturn_pin(void) {
    bt_harness_exit_failure();
}

static void inner_pass(void) {
    BT_CHECK(1);
}

BT_TEST(bt_harness, run_reports_green_suite) {
    bt_test_entry entries[] = {{"inner_pass", inner_pass}};
    BT_CHECK_EQ(bt_harness_run("bt_harness_inner", entries, 1), 0);
}
