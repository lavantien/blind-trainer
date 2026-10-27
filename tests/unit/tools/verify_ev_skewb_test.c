#include "verify_test_util.h"

#include <string.h>

#include "verify/ev_skewb.h"

static bt_seed_row sarah_row(const char *key, const char *alg) {
    bt_seed_row row = {0};
    snprintf(row.event, sizeof row.event, "%s", "skewb");
    snprintf(row.method, sizeof row.method, "%s", "sarah_intermediate");
    snprintf(row.stage, sizeof row.stage, "%s", "sarah");
    snprintf(row.case_key, sizeof row.case_key, "%s", key);
    row.alg = (char *)alg;
    row.front_json = (char *)"{\"case\":\"probe\"}";
    row.source_url = (char *)"https://example.test";
    row.source_at = (char *)"2026-10-10";
    row.content_hash[0] = '\0';
    return row;
}

BT_TEST(verify_ev_skewb, a_rotation_only_alg_fails_tier1_as_empty) {
    bt_seed_row rows_items[1] = {sarah_row("sarah.probe", "y2")};
    bt_seed_rows rows = {rows_items, 1};
    verify_section_result result;
    verify_skewb_verify_sarah(&rows, &result);
    BT_CHECK_EQ(result.count, 1);
    BT_CHECK(!result.rows[0].tier1_ok);
    BT_CHECK_EQ(result.rows[0].tier1_err, "alg is empty after rotation stripping");
    BT_CHECK_EQ(result.rows[0].tier2.kind, VERIFY_TIER2_SKIPPED);
    BT_CHECK_EQ(result.rows[0].tier2.skip, "sarah stage tags are not state-derivable");
    BT_CHECK_EQ(result.honesty_count, 1);
    verify_section_free(&result);
}

BT_TEST(verify_ev_skewb, sarah_rows_pass_tier1_after_rotation_stripping) {
    bt_seed_row rows_items[3] = {
        sarah_row("sarah.01", "L F' L'"),
        sarah_row("sarah.05", "y2 F' L F L' F' L F L'"),
        sarah_row("sarah.15", "R' F R F' y2 R' F R F' z2 x' R' F R F' y2 R' F R F'"),
    };
    bt_seed_rows rows = {rows_items, 3};
    verify_section_result result;
    verify_skewb_verify_sarah(&rows, &result);
    for (size_t at = 0; at < result.count; at++) {
        BT_CHECK(result.rows[at].tier1_ok);
    }
    verify_section_free(&result);
}
