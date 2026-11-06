#include "verify_test_util.h"

#include <string.h>

#include "common/alloc.h"
#include "data/seedpack/registry.h"
#include "verify/packs.h"

static size_t mini_registry(const char *rows, bt_pack_entry **out_entries) {
    bt_seed_error err;
    size_t count = 0;
    int rc = bt_seedpack_parse_registry(rows, out_entries, &count, &err);
    if (rc != 0) {
        char text[512];
        bt_seed_error_string(&err, text, sizeof text);
        BT_ABORT("registry parses: %s", text);
    }
    return count;
}

static bool offenders_contain(const verify_strlist *list, const char *needle) {
    for (size_t at = 0; at < list->count; at++) {
        if (strstr(list->items[at], needle) != NULL) {
            return true;
        }
    }
    return false;
}

BT_TEST(verify_packs, a_matching_entry_and_pin_pair_produce_no_offenders) {
    bt_pack_entry *entries = NULL;
    size_t count = mini_registry(
        "[{\"file\":\"twoby2_cll.tsv\",\"event\":\"2x2\",\"method\":\"cll\",\"stage\":\"cll\","
        "\"n\":null,\"expected_rows\":42}]",
        &entries);
    verify_strlist offenders;
    verify_strlist_init(&offenders);
    verify_pinned_vs_entries(entries, count, &offenders);
    BT_CHECK(!offenders_contain(&offenders, "twoby2_cll"));
    verify_strlist_free(&offenders);
    bt_seedpack_free_entries(entries);
}

BT_TEST(verify_packs, row_drift_between_registry_and_pin_is_an_offender) {
    bt_pack_entry *entries = NULL;
    size_t count = mini_registry(
        "[{\"file\":\"twoby2_cll.tsv\",\"event\":\"2x2\",\"method\":\"cll\",\"stage\":\"cll\","
        "\"n\":null,\"expected_rows\":41}]",
        &entries);
    verify_strlist offenders;
    verify_strlist_init(&offenders);
    verify_pinned_vs_entries(entries, count, &offenders);
    BT_CHECK(offenders_contain(&offenders,
                               "2x2/cll/cll/twoby2_cll.tsv registry holds 41 rows, pin holds 42"));
    verify_strlist_free(&offenders);
    bt_seedpack_free_entries(entries);
}

BT_TEST(verify_packs, an_unpinned_registry_entry_is_an_offender) {
    bt_pack_entry *entries = NULL;
    size_t count = mini_registry(
        "[{\"file\":\"new_pack.tsv\",\"event\":\"fto\",\"method\":\"l4t\",\"stage\":\"l4t\","
        "\"n\":null,\"expected_rows\":10}]",
        &entries);
    verify_strlist offenders;
    verify_strlist_init(&offenders);
    verify_pinned_vs_entries(entries, count, &offenders);
    BT_CHECK(offenders_contain(&offenders, "new_pack.tsv"));
    BT_CHECK(offenders_contain(&offenders, "not pinned"));
    verify_strlist_free(&offenders);
    bt_seedpack_free_entries(entries);
}

BT_TEST(verify_packs, a_pin_without_a_registry_entry_is_an_offender) {
    verify_strlist offenders;
    verify_strlist_init(&offenders);
    verify_pinned_vs_entries(NULL, 0, &offenders);
    BT_CHECK(offenders_contain(&offenders, "missing from the registry"));
    verify_strlist_free(&offenders);
}

BT_TEST(verify_packs, nspec_text_renders_all_three_shapes) {
    char out[64];
    bt_nspec unset = {BT_NSPEC_UNSET, 0, 0, {0}};
    verify_nspec_text(&unset, out, sizeof out);
    BT_CHECK_EQ(out, "unset");
    bt_nspec fixed = {BT_NSPEC_FIXED, 3, 0, {0}};
    verify_nspec_text(&fixed, out, sizeof out);
    BT_CHECK_EQ(out, "n=3");
    bt_nspec sizes = {BT_NSPEC_SIZES, 0, 2, {4, 5}};
    verify_nspec_text(&sizes, out, sizeof out);
    BT_CHECK_EQ(out, "n=4,5");
}

BT_TEST(verify_packs, puzzle_for_event_maps_every_known_event) {
    verify_puzzle puzzle = VERIFY_PUZZLE_CORE;
    char err[128];
    BT_CHECK_EQ(verify_puzzle_for_event("3x3", &puzzle, err, sizeof err), 0);
    BT_CHECK_EQ(puzzle, VERIFY_PUZZLE_CORE);
    BT_CHECK_EQ(verify_puzzle_for_event("oh", &puzzle, err, sizeof err), 0);
    BT_CHECK_EQ(puzzle, VERIFY_PUZZLE_CORE);
    BT_CHECK_EQ(verify_puzzle_for_event("3bld", &puzzle, err, sizeof err), 0);
    BT_CHECK_EQ(puzzle, VERIFY_PUZZLE_CORE);
    BT_CHECK_EQ(verify_puzzle_for_event("2x2", &puzzle, err, sizeof err), 0);
    BT_CHECK_EQ(puzzle, VERIFY_PUZZLE_NXN);
    BT_CHECK_EQ(verify_puzzle_for_event("sq1", &puzzle, err, sizeof err), 0);
    BT_CHECK_EQ(puzzle, VERIFY_PUZZLE_SQ1);
    BT_CHECK_EQ(verify_puzzle_for_event("pyraminx", &puzzle, err, sizeof err), 0);
    BT_CHECK_EQ(puzzle, VERIFY_PUZZLE_PYRA);
    BT_CHECK_EQ(verify_puzzle_for_event("skewb", &puzzle, err, sizeof err), 0);
    BT_CHECK_EQ(puzzle, VERIFY_PUZZLE_SKEWB);
    BT_CHECK_EQ(verify_puzzle_for_event("megaminx", &puzzle, err, sizeof err), 0);
    BT_CHECK_EQ(puzzle, VERIFY_PUZZLE_MEGA);
    BT_CHECK_EQ(verify_puzzle_for_event("fto", &puzzle, err, sizeof err), 0);
    BT_CHECK_EQ(puzzle, VERIFY_PUZZLE_FTO);
    BT_CHECK(verify_puzzle_for_event("bigcube", &puzzle, err, sizeof err) != 0);
    BT_CHECK(strcmp(err, "bigcube rows resolve their own n") == 0);
    BT_CHECK(verify_puzzle_for_event("4x4", &puzzle, err, sizeof err) != 0);
    BT_CHECK(strcmp(err, "unknown event 4x4") == 0);
}

BT_TEST(verify_packs, pinned_vs_registry_reports_an_unloadable_registry) {
    verify_strlist offenders;
    verify_pinned_vs_registry("Z:/no-such-packs-root", &offenders);
    BT_CHECK(offenders.count > 0);
    verify_strlist_free(&offenders);
}

BT_TEST(verify_packs, tier2_constructors_carry_their_kind_and_reason) {
    verify_tier2 pass = verify_tier2_pass();
    BT_CHECK(pass.kind == VERIFY_TIER2_PASS);
    verify_tier2 fail = verify_tier2_fail("probe");
    BT_CHECK(fail.kind == VERIFY_TIER2_FAIL);
    BT_CHECK(strcmp(fail.fail, "probe") == 0);
    verify_tier2 skipped = verify_tier2_skipped("no predicate");
    BT_CHECK(skipped.kind == VERIFY_TIER2_SKIPPED);
    BT_CHECK(strcmp(skipped.skip, "no predicate") == 0);
}

BT_TEST(verify_packs, honesty_notes_stop_at_the_cap_and_strlist_pushf_formats) {
    verify_section_result section;
    verify_section_init(&section);
    char note[VERIFY_HONESTY_MAX];
    for (int at = 0; at <= VERIFY_HONESTY_CAP + 2; at++) {
        snprintf(note, sizeof note, "note %d", at);
        verify_section_push_honesty(&section, note);
    }
    BT_CHECK_EQ(section.honesty_count, VERIFY_HONESTY_CAP);
    verify_section_free(&section);
    verify_strlist list;
    verify_strlist_init(&list);
    verify_strlist_pushf(&list, "row %d of %d", 3, 9);
    BT_CHECK_EQ(list.count, 1U);
    BT_CHECK(strcmp(list.items[0], "row 3 of 9") == 0);
    verify_strlist_free(&list);
}

BT_TEST(verify_packs, rows_for_an_unimplemented_section_still_report_one_outcome_each) {
    bt_seed_row storage[2];
    memset(storage, 0, sizeof storage);
    snprintf(storage[0].case_key, sizeof storage[0].case_key, "%s", "weird.1");
    storage[0].alg = (char *)"R U R' U'";
    storage[0].front_json = (char *)"{}";
    storage[0].source_url = (char *)"https://example.test";
    storage[0].source_at = (char *)"2026-10-10";
    snprintf(storage[1].case_key, sizeof storage[1].case_key, "%s", "weird.2");
    storage[1].alg = (char *)"R U2 R'";
    storage[1].front_json = (char *)"{}";
    storage[1].source_url = (char *)"https://example.test";
    storage[1].source_at = (char *)"2026-10-10";
    bt_seed_rows rows;
    rows.items = storage;
    rows.count = 2;
    bt_pack_entry entry;
    memset(&entry, 0, sizeof entry);
    snprintf(entry.file, sizeof entry.file, "%s", "weird.tsv");
    snprintf(entry.event, sizeof entry.event, "%s", "sq1");
    snprintf(entry.method, sizeof entry.method, "%s", "cfop");
    snprintf(entry.stage, sizeof entry.stage, "%s", "pll");
    verify_section_result section;
    verify_rows(&entry, &rows, &section);
    BT_CHECK_EQ(section.count, 2U);
    BT_CHECK(!section.rows[0].tier1_ok);
    BT_CHECK(strstr(section.rows[0].tier1_err, "is not implemented") != NULL);
    BT_CHECK(section.rows[0].tier2.kind == VERIFY_TIER2_SKIPPED);
    verify_section_free(&section);
}

BT_TEST(verify_packs, section_and_strlist_pushes_fail_closed_when_growth_starves) {
    verify_section_result section;
    verify_section_init(&section);
    verify_row_outcome row;
    memset(&row, 0, sizeof row);
    bt_alloc_fail_arm(0);
    verify_section_push_row(&section, &row);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(section.count, 0U);
    BT_CHECK_EQ((int)bt_alloc_fail_count() > 0, 1);
    verify_section_free(&section);
    verify_strlist list;
    verify_strlist_init(&list);
    bt_alloc_fail_arm(0);
    verify_strlist_push(&list, "offender");
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(list.count, 0U);
    verify_strlist_free(&list);
}
