#include "verify_test_util.h"

#include <string.h>

#include "data/digest.h"
#include "verify/lib.h"

BT_TEST(verify_lib, crate_id_matches_package) {
    BT_CHECK_EQ(verify_crate_id(), "bt-verify");
}

BT_TEST(verify_lib, sheet_expectations_sum_to_the_corpus_total) {
    size_t sum = 0;
    for (size_t at = 0; at < VERIFY_SHEET_PIN_COUNT; at++) {
        sum += VERIFY_SHEET_EXPECTED_ROWS[at].rows;
    }
    BT_CHECK_EQ(sum, BT_DATA_TOTAL_ROWS);
}

BT_TEST(verify_lib, artifacts_dir_from_root_only_keeps_the_base_drive) {
    char out[BT_PATH_MAX];
    verify_artifacts_dir_from("/out", "R:/ws", out);
    BT_CHECK(strcmp(out, "R:/out/verify") == 0);
    verify_artifacts_dir_from("C:ga", "R:/ws", out);
    BT_CHECK(strcmp(out, "C:ga/verify") == 0);
    verify_artifacts_dir_from("rel", "R:/ws", out);
    BT_CHECK(strcmp(out, "R:/ws/rel/verify") == 0);
}
