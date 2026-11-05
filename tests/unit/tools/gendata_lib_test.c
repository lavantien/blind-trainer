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
    BT_CHECK_EQ(BT_GD_EDGE_COVERED + BT_GD_EDGE_GENERATED + BT_GD_EDGE_DERIVED,
                BT_GD_EDGE_TOTAL_COVERED);
    BT_CHECK_EQ(BT_GD_CORNER_COVERED + BT_GD_CORNER_GENERATED + BT_GD_CORNER_DERIVED,
                BT_GD_CORNER_TOTAL_COVERED);
    BT_CHECK_EQ(BT_GD_EDGE_TOTAL_COVERED + BT_GD_CORNER_TOTAL_COVERED, BT_GD_TOTAL_COVERED);
    BT_CHECK_EQ(BT_GD_TOTAL_CASES - BT_GD_TOTAL_COVERED, 0);
    size_t sum = 0;
    for (size_t i = 0; i < BT_GD_SPECIAL_COUNTS_LEN; i++) {
        sum += BT_GD_SPECIAL_COUNTS[i].rows;
    }
    BT_CHECK_EQ(sum, BT_GD_TOTAL_SPECIALS);
    BT_CHECK_EQ(BT_DATA_TOTAL_ROWS - BT_GD_TOTAL_SPECIALS, 988);
    BT_CHECK_EQ(strcmp(BT_GD_SYNTH_ALGORITHM, "first-setup-plateau-1+inverse-1"), 0);
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

BT_TEST(gendata_lib, synth_params_hash_pins_the_rust_era_golden) {
    bt_json_value *config = bt_data_load_config();
    BT_CHECK(config != NULL);
    char hash[65];
    char err[96];
    BT_CHECK_EQ(bt_gd_synth_params_sha256(config, BT_GD_SYNTH_ALGORITHM, hash, err, sizeof err), 0);
    BT_CHECK_EQ(strcmp(hash, "d7050d616b2d64269416e425ca9bc6771a877fc94441439a09f4e40755d6cb55"),
                0);
    bt_json_free(config);
}

BT_TEST(gendata_lib, synth_params_hash_pins_the_serde_float_boundaries) {
    static const char *configs[] = {
        "{\"synth\": {\"penalty_other\": 1e16}}", "{\"synth\": {\"penalty_other\": 0.0000001}}",
        "{\"synth\": {\"penalty_other\": 1e15}}", "{\"synth\": {\"penalty_other\": 0.00001}}",
        "{\"synth\": {\"penalty_other\": 2.0}}",
    };
    static const char *want[] = {
        "a8ba4ce6105497928a2ce2ac8ac3ec70c72f4de75ec37f31134fd479edafa72d",
        "75c8b2e90a1b156c09a70b73484c62166cea430813ab0792f92a8f6329148160",
        "abcd3e14741e9abb52b03110478e07a73e771e4eb0796793ad038330756a033f",
        "d1fd453f4428a4df984a49aeafb0ee97a8a0c69a332445fb86f6f1ca7b07b5c6",
        "43bd657df0c56260b0ab27e37c7bdeaed7d00c6b18179ee1c9735e1641db023e",
    };
    for (size_t i = 0; i < 5; i++) {
        bt_json_error jerr;
        bt_json_value *config = NULL;
        BT_CHECK_EQ(bt_json_parse(configs[i], strlen(configs[i]), &config, &jerr), 0);
        char hash[65];
        char err[96];
        BT_CHECK_EQ(bt_gd_synth_params_sha256(config, BT_GD_SYNTH_ALGORITHM, hash, err, sizeof err),
                    0);
        BT_CHECK_EQ(strcmp(hash, want[i]), 0);
        bt_json_free(config);
    }
}

BT_TEST(gendata_lib, limits_from_config_reads_the_hub_fields) {
    const char *text =
        "{\"synth\": {\"max_setup\": 3, \"max_insert\": 4, \"node_budget\": 50000000}}";
    bt_json_error jerr;
    bt_json_value *config = NULL;
    BT_CHECK_EQ(bt_json_parse(text, strlen(text), &config, &jerr), 0);
    bt_gd_synth_limits lim;
    char err[96];
    err[0] = '\0';
    BT_CHECK_EQ(bt_gd_limits_from_config(config, &lim, err, sizeof err), 0);
    BT_CHECK_EQ(lim.max_setup, 3U);
    BT_CHECK_EQ(lim.max_insert, 4U);
    BT_CHECK_EQ(lim.node_budget, 50000000U);
    bt_json_free(config);
}

BT_TEST(gendata_lib, limits_from_config_rejects_each_malformed_field) {
    static const char *configs[] = {
        "{}",
        "{\"synth\": {\"max_insert\": 4, \"node_budget\": 50000000}}",
        "{\"synth\": {\"max_setup\": 3, \"node_budget\": 50000000}}",
        "{\"synth\": {\"max_setup\": 3, \"max_insert\": 4}}",
        "{\"synth\": {\"max_setup\": \"3\", \"max_insert\": 4, \"node_budget\": 50000000}}",
        "{\"synth\": {\"max_setup\": 3, \"max_insert\": 2.5, \"node_budget\": 50000000}}",
        "{\"synth\": {\"max_setup\": -1, \"max_insert\": 4, \"node_budget\": 50000000}}",
    };
    static const char *want[] = {
        "config section missing: synth",
        "config section missing: synth.max_setup",
        "config section missing: synth.max_insert",
        "config section missing: synth.node_budget",
        "config section missing: synth.max_setup",
        "config section missing: synth.max_insert",
        "config section missing: synth.max_setup",
    };
    for (size_t i = 0; i < 7; i++) {
        bt_json_error jerr;
        bt_json_value *config = NULL;
        BT_CHECK_EQ(bt_json_parse(configs[i], strlen(configs[i]), &config, &jerr), 0);
        bt_gd_synth_limits lim;
        char err[96];
        err[0] = '\0';
        BT_CHECK(bt_gd_limits_from_config(config, &lim, err, sizeof err) != 0);
        BT_CHECK_EQ(strcmp(err, want[i]), 0);
        bt_json_free(config);
    }
}

BT_TEST(gendata_lib, data_vendor_path_rejects_missing_and_malformed_fields) {
    bt_json_value *empty = bt_json_new_object();
    char out[BT_PATH_MAX];
    char err[96];
    err[0] = '\0';
    BT_CHECK(bt_gd_data_vendor_path(empty, out, err, sizeof err) != 0);
    BT_CHECK_EQ(strcmp(err, "config section missing: paths.data_vendor"), 0);
    bt_json_free(empty);
    const char *text = "{\"paths\": {\"data_vendor\": 7}}";
    bt_json_error jerr;
    bt_json_value *config = NULL;
    BT_CHECK_EQ(bt_json_parse(text, strlen(text), &config, &jerr), 0);
    err[0] = '\0';
    BT_CHECK(bt_gd_data_vendor_path(config, out, err, sizeof err) != 0);
    BT_CHECK_EQ(strcmp(err, "config section missing: paths.data_vendor"), 0);
    bt_json_free(config);
}
