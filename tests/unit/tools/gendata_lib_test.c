#include "gendata/lib.h"

#include <stdio.h>
#include <string.h>

#include "common/bt_harness.h"
#include "common/json.h"
#include "data/digest.h"
#include "data/paths.h"

BT_TEST(gendata_lib, crate_id_matches_package) {
    BT_CHECK_EQ(strcmp(bt_gd_crate_id(), "bt-gendata"), 0);
}

BT_TEST(gendata_lib, data_vendor_path_root_only_keeps_the_drive) {
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    BT_CHECK(root[1] == ':');
    char want[BT_PATH_MAX];
    snprintf(want, sizeof want, "%2.2s/dv", root);
    char out[BT_PATH_MAX];
    char err[96];
    bt_json_value *config = bt_json_new_object();
    bt_json_value *paths = bt_json_new_object();
    bt_json_object_set(paths, "data_vendor", bt_json_new_string("/dv"));
    bt_json_object_set(config, "paths", paths);
    BT_CHECK_EQ(bt_gd_data_vendor_path(config, out, err, sizeof err), 0);
    BT_CHECK(strcmp(out, want) == 0);
    bt_json_object_set(paths, "data_vendor", bt_json_new_string("C:dv"));
    BT_CHECK_EQ(bt_gd_data_vendor_path(config, out, err, sizeof err), 0);
    BT_CHECK(strcmp(out, "C:dv") == 0);
    bt_json_free(config);
}

BT_TEST(gendata_lib, coverage_constants_are_self_consistent) {
    BT_CHECK_EQ(BT_GD_EDGE_CASES + BT_GD_CORNER_CASES, BT_GD_TOTAL_CASES);
    BT_CHECK_EQ(BT_GD_EDGE_CASES - BT_GD_EDGE_COVERED, 68);
    BT_CHECK_EQ(BT_GD_CORNER_CASES - BT_GD_CORNER_COVERED, 90);
    BT_CHECK_EQ(BT_GD_EDGE_COVERED + BT_GD_EDGE_GENERATED, BT_GD_EDGE_TOTAL_COVERED);
    BT_CHECK_EQ(BT_GD_CORNER_COVERED + BT_GD_CORNER_GENERATED, BT_GD_CORNER_TOTAL_COVERED);
    BT_CHECK_EQ(BT_GD_EDGE_TOTAL_COVERED + BT_GD_CORNER_TOTAL_COVERED, BT_GD_TOTAL_COVERED);
    BT_CHECK_EQ(BT_GD_TOTAL_CASES - BT_GD_TOTAL_COVERED, 1);
    size_t sum = 0;
    for (size_t i = 0; i < BT_GD_SPECIAL_COUNTS_LEN; i++) {
        sum += BT_GD_SPECIAL_COUNTS[i].rows;
    }
    BT_CHECK_EQ(sum, BT_GD_TOTAL_SPECIALS);
    BT_CHECK_EQ(BT_DATA_TOTAL_ROWS - BT_GD_TOTAL_SPECIALS, 988);
    BT_CHECK_EQ(strcmp(BT_GD_SYNTH_ALGORITHM, "first-setup-plateau-1"), 0);
}

BT_TEST(gendata_lib, synth_params_hash_ignores_key_order_and_tracks_values) {
    const char *plain = "{\"synth\": {\"max_setup\": 3, \"scoring\": {\"penalty_other\": 2.0}}}";
    const char *reordered =
        "{\"synth\": {\"scoring\": {\"penalty_other\": 2.0}, \"max_setup\": 3}}";
    const char *changed = "{\"synth\": {\"max_setup\": 4, \"scoring\": {\"penalty_other\": 2.0}}}";
    bt_json_error jerr;
    bt_json_value *config = NULL;
    bt_json_value *other = NULL;
    bt_json_value *third = NULL;
    BT_CHECK_EQ(bt_json_parse(plain, strlen(plain), &config, &jerr), 0);
    BT_CHECK_EQ(bt_json_parse(reordered, strlen(reordered), &other, &jerr), 0);
    BT_CHECK_EQ(bt_json_parse(changed, strlen(changed), &third, &jerr), 0);
    char a[65];
    char b[65];
    char c[65];
    char err[96];
    BT_CHECK_EQ(bt_gd_synth_params_sha256(config, BT_GD_SYNTH_ALGORITHM, a, err, sizeof err), 0);
    BT_CHECK_EQ(bt_gd_synth_params_sha256(other, BT_GD_SYNTH_ALGORITHM, b, err, sizeof err), 0);
    BT_CHECK_EQ(strcmp(a, b), 0);
    BT_CHECK_EQ(bt_gd_synth_params_sha256(third, BT_GD_SYNTH_ALGORITHM, c, err, sizeof err), 0);
    BT_CHECK(strcmp(a, c) != 0);
    bt_json_value *empty = bt_json_new_object();
    err[0] = '\0';
    BT_CHECK(bt_gd_synth_params_sha256(empty, BT_GD_SYNTH_ALGORITHM, a, err, sizeof err) != 0);
    BT_CHECK_EQ(strcmp(err, "config section missing: synth"), 0);
    bt_json_free(config);
    bt_json_free(other);
    bt_json_free(third);
    bt_json_free(empty);
}

BT_TEST(gendata_lib, synth_params_hash_binds_the_generator_version) {
    const char *text = "{\"synth\": {\"max_setup\": 3}}";
    bt_json_error jerr;
    bt_json_value *config = NULL;
    BT_CHECK_EQ(bt_json_parse(text, strlen(text), &config, &jerr), 0);
    char current[65];
    char again[65];
    char other[65];
    char err[96];
    BT_CHECK_EQ(bt_gd_synth_params_sha256(config, BT_GD_SYNTH_ALGORITHM, current, err, sizeof err),
                0);
    BT_CHECK_EQ(bt_gd_synth_params_sha256(config, BT_GD_SYNTH_ALGORITHM, again, err, sizeof err),
                0);
    BT_CHECK_EQ(strcmp(current, again), 0);
    BT_CHECK_EQ(bt_gd_synth_params_sha256(config, "first-setup-plateau-2", other, err, sizeof err),
                0);
    BT_CHECK(strcmp(current, other) != 0);
    bt_json_free(config);
}
