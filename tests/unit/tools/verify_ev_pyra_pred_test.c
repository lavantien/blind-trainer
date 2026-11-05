#include "verify_test_util.h"

#include <string.h>

#include "puzzles/pyra.h"
#include "verify/ev_pyra.h"

static void row_of(bt_seed_row *row, const char *key, const char *alg, const char *front) {
    memset(row, 0, sizeof *row);
    snprintf(row->case_key, sizeof row->case_key, "%s", key);
    row->alg = (char *)alg;
    row->front_json = (char *)front;
    row->source_url = (char *)"https://example.test";
    row->source_at = (char *)"2026-10-10";
}

BT_TEST(verify_ev_pyra_pred, case_state_rejects_bad_and_empty_algs) {
    bt_pyra_state state;
    char err[VERIFY_ERR_MAX];
    BT_CHECK(verify_pyra_case_state("zz", &state, err, sizeof err) != 0);
    BT_CHECK(err[0] != '\0');
    BT_CHECK(verify_pyra_case_state("", &state, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "empty") != NULL);
    BT_CHECK_EQ(verify_pyra_case_state("R U' R' U' R U' R'", &state, err, sizeof err), 0);
}

BT_TEST(verify_ev_pyra_pred, l4e_disturbance_names_both_failure_shapes) {
    bt_pyra_state state;
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_pyra_case_state("L", &state, err, sizeof err), 0);
    BT_CHECK(verify_pyra_l4e_disturbance(&state, err, sizeof err) < 0);
    BT_CHECK(strstr(err, "not home in any U layer frame") != NULL);
    BT_CHECK_EQ(verify_pyra_case_state("U", &state, err, sizeof err), 0);
    BT_CHECK(verify_pyra_l4e_disturbance(&state, err, sizeof err) < 0);
    BT_CHECK(strstr(err, "holds 4 to 8") != NULL);
    BT_CHECK_EQ(verify_pyra_case_state("R U' R' U' R U' R'", &state, err, sizeof err), 0);
    BT_CHECK(verify_pyra_l4e_disturbance(&state, err, sizeof err) >= 4);
}

BT_TEST(verify_ev_pyra_pred, verify_l4e_covers_prose_bad_and_passing_rows) {
    bt_seed_row storage[3];
    row_of(&storage[0], "l4e.prose", "", "{\"case\":\"DS1\"}");
    row_of(&storage[1], "l4e.bad", "zz", "{\"case\":\"DS2\"}");
    row_of(&storage[2], "l4e.ok", "R U' R' U' R U' R'", "{\"case\":\"DS2\"}");
    bt_seed_rows rows;
    rows.items = storage;
    rows.count = 3;
    verify_section_result section;
    verify_pyra_verify_l4e(&rows, &section);
    BT_CHECK_EQ(section.count, 3);
    BT_CHECK(!section.rows[0].tier1_ok);
    BT_CHECK_EQ(section.rows[0].tier1_err, "alg is empty");
    BT_CHECK(section.rows[0].tier2.kind == VERIFY_TIER2_SKIPPED);
    BT_CHECK(!section.rows[1].tier1_ok);
    BT_CHECK(section.rows[1].tier2.kind == VERIFY_TIER2_SKIPPED);
    BT_CHECK(section.rows[2].tier1_ok);
    BT_CHECK_EQ(section.rows[2].tier2.kind, VERIFY_TIER2_PASS);
    BT_CHECK(section.honesty_count == 1);
    verify_section_free(&section);
}
