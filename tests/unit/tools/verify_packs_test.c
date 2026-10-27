#include "verify_test_util.h"

#include <string.h>

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
