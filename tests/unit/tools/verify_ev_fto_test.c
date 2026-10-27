#include "verify_test_util.h"

#include <string.h>

#include "verify/ev_fto.h"

static bt_seed_row row(const char *key, const char *alg) {
    bt_seed_row seed = {0};
    snprintf(seed.event, sizeof seed.event, "%s", "fto");
    snprintf(seed.method, sizeof seed.method, "%s", "l4t_redux");
    snprintf(seed.stage, sizeof seed.stage, "%s", "l4t");
    snprintf(seed.case_key, sizeof seed.case_key, "%s", key);
    seed.alg = (char *)alg;
    seed.front_json = (char *)"{\"case\":\"probe\"}";
    seed.source_url = (char *)"https://example.test";
    seed.source_at = (char *)"2026-10-10";
    seed.content_hash[0] = '\0';
    return seed;
}

BT_TEST(verify_ev_fto, a_pack_alg_passes_tier1_and_reports_the_honest_skip) {
    bt_seed_row rows_items[1] = {row("l4t.probe", "U BR U BR' F U F'")};
    bt_seed_rows rows = {rows_items, 1};
    verify_section_result result;
    verify_fto_verify_l4t(&rows, &result);
    BT_CHECK_EQ(result.count, 1);
    BT_CHECK(result.rows[0].tier1_ok);
    BT_CHECK_EQ(result.rows[0].tier2.kind, VERIFY_TIER2_SKIPPED);
    BT_CHECK_EQ(result.rows[0].tier2.skip, "l4t case families have no state predicate");
    BT_CHECK_EQ(result.honesty_count, 1);
    verify_section_free(&result);
}

BT_TEST(verify_ev_fto, a_bad_token_fails_tier1) {
    bt_seed_row rows_items[1] = {row("l4t.bad", "Q")};
    bt_seed_rows rows = {rows_items, 1};
    verify_section_result result;
    verify_fto_verify_l4t(&rows, &result);
    BT_CHECK(!result.rows[0].tier1_ok);
    verify_section_free(&result);
}

BT_TEST(verify_ev_fto, an_empty_alg_fails_tier1) {
    bt_seed_row rows_items[1] = {row("l4t.empty", "")};
    bt_seed_rows rows = {rows_items, 1};
    verify_section_result result;
    verify_fto_verify_l4t(&rows, &result);
    BT_CHECK(!result.rows[0].tier1_ok);
    BT_CHECK_EQ(result.rows[0].tier1_err, "alg is empty");
    verify_section_free(&result);
}
