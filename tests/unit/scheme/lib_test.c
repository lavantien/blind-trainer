#include "scheme/lib.h"

#include "common/bt_harness.h"
#include <string.h>

BT_TEST(lib, crate_id_matches_package) {
    BT_CHECK_EQ(bt_scheme_crate_id(), "bt-scheme");
}
