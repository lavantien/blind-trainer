#include <stdio.h>

extern void bt_test_gendata_build_build_attaches_a_verified_corner_three_style_to_its_case(void);
extern void bt_test_gendata_build_build_attaches_a_verified_edge_three_style_to_its_case(void);
extern void bt_test_gendata_build_build_catalogs_the_whole_pair_space_from_an_empty_corpus(void);
extern void bt_test_gendata_build_build_counts_other_buffer_walks_without_covering(void);
extern void bt_test_gendata_build_build_flags_non_three_style_and_wrong_orbit_rows(void);
extern void bt_test_gendata_build_build_routes_special_sheets_with_their_verdicts(void);
extern void bt_test_gendata_build_manifest_counts_orbits_and_special_classes(void);
extern void bt_test_gendata_build_regen_is_needed_without_a_manifest_or_on_any_hash_mismatch(void);
extern void bt_test_gendata_build_regen_is_skipped_only_when_both_hashes_match(void);

typedef void (*test_fn)(void);

int main(void) {
    setvbuf(stderr, NULL, _IONBF, 0);
    static const char *names[] = {
        "attaches_corner",   "attaches_edge", "catalogs",  "other_buffer",
        "flags_non_three",   "routes_special", "manifest_counts",
        "regen_needed",      "regen_skipped",
    };
    static const test_fn fns[] = {
        bt_test_gendata_build_build_attaches_a_verified_corner_three_style_to_its_case,
        bt_test_gendata_build_build_attaches_a_verified_edge_three_style_to_its_case,
        bt_test_gendata_build_build_catalogs_the_whole_pair_space_from_an_empty_corpus,
        bt_test_gendata_build_build_counts_other_buffer_walks_without_covering,
        bt_test_gendata_build_build_flags_non_three_style_and_wrong_orbit_rows,
        bt_test_gendata_build_build_routes_special_sheets_with_their_verdicts,
        bt_test_gendata_build_manifest_counts_orbits_and_special_classes,
        bt_test_gendata_build_regen_is_needed_without_a_manifest_or_on_any_hash_mismatch,
        bt_test_gendata_build_regen_is_skipped_only_when_both_hashes_match,
    };
    for (int i = 0; i < 9; i++) {
        fprintf(stderr, "start %d %s\n", i, names[i]);
        fns[i]();
        fprintf(stderr, "done %d %s\n", i, names[i]);
    }
    return 0;
}
