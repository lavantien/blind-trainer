#include "ga/lib.h"

#include "common/bt_harness.h"

BT_TEST(lib, crate_id_matches_package) {
    BT_CHECK_EQ(bt_ga_crate_id(), "bt-ga");
}
