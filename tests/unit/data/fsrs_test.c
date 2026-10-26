#include "common/bt_harness.h"
#include "common/json.h"
#include "data/fsrs.h"

#include <math.h>
#include <string.h>

static const double PINNED_WEIGHTS[BT_FSRS_WEIGHT_COUNT] = {
    0.40255, 1.18385, 3.173,  15.69105, 7.1949,  0.5345,  1.4604,  0.0046,
    1.54575, 0.1192,   1.01925, 1.9395,  0.11,    0.29605, 2.2698, 0.2315,
    2.9898,  0.51655,  0.6621,
};

BT_TEST(fsrs, defaults_pin_the_reference_release) {
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    for (int i = 0; i < BT_FSRS_WEIGHT_COUNT; i++) {
        BT_CHECK(cfg.weights[i] == PINNED_WEIGHTS[i]);
    }
    BT_CHECK(cfg.request_retention == 0.9);
    BT_CHECK(cfg.interval_clamp_days[0] == 1);
    BT_CHECK(cfg.interval_clamp_days[1] == 36500);
}

BT_TEST(fsrs, from_value_accepts_the_full_section) {
    const char *text =
        "{\"srs\":{\"fsrs\":{\"weights\":[0.40255,1.18385,3.173,15.69105,7.1949,"
        "0.5345,1.4604,0.0046,1.54575,0.1192,1.01925,1.9395,0.11,0.29605,2.2698,"
        "0.2315,2.9898,0.51655,0.6621],\"request_retention\":0.9,"
        "\"interval_clamp_days\":[7,200]}}}";
    bt_json_error jerr;
    bt_json_value *root = NULL;
    BT_CHECK(bt_json_parse(text, strlen(text), &root, &jerr) == 0);
    bt_fsrs_config cfg;
    char err[96];
    BT_CHECK(bt_fsrs_config_from_json(root, &cfg, err, sizeof err) == 0);
    BT_CHECK(cfg.interval_clamp_days[0] == 7);
    BT_CHECK(cfg.interval_clamp_days[1] == 200);
    bt_json_free(root);
}

BT_TEST(fsrs, from_value_missing_section_is_a_missing_error) {
    bt_json_value *root = bt_json_new_object();
    bt_fsrs_config cfg;
    char err[96];
    BT_CHECK(bt_fsrs_config_from_json(root, &cfg, err, sizeof err) != 0);
    BT_CHECK_EQ(err, "config section missing: srs.fsrs");
    bt_json_free(root);
}

BT_TEST(fsrs, from_value_text_weights_are_malformed) {
    const char *text =
        "{\"srs\":{\"fsrs\":{\"weights\":[\"a\"],\"request_retention\":0.9,"
        "\"interval_clamp_days\":[1,365]}}}";
    bt_json_error jerr;
    bt_json_value *root = NULL;
    bt_json_parse(text, strlen(text), &root, &jerr);
    bt_fsrs_config cfg;
    char err[96];
    BT_CHECK(bt_fsrs_config_from_json(root, &cfg, err, sizeof err) != 0);
    BT_CHECK(strncmp(err, "config section malformed: ", 26) == 0);
    bt_json_free(root);
}

BT_TEST(fsrs, from_value_rejects_out_of_range_retention) {
    const char *text =
        "{\"srs\":{\"fsrs\":{\"weights\":[0.40255,1.18385,3.173,15.69105,7.1949,"
        "0.5345,1.4604,0.0046,1.54575,0.1192,1.01925,1.9395,0.11,0.29605,2.2698,"
        "0.2315,2.9898,0.51655,0.6621],\"request_retention\":0.0,"
        "\"interval_clamp_days\":[1,365]}}}";
    bt_json_error jerr;
    bt_json_value *root = NULL;
    bt_json_parse(text, strlen(text), &root, &jerr);
    bt_fsrs_config cfg;
    char err[96];
    BT_CHECK(bt_fsrs_config_from_json(root, &cfg, err, sizeof err) != 0);
    BT_CHECK(strncmp(err, "config section invalid: ", 24) == 0);
    bt_json_free(root);
}

BT_TEST(fsrs, grade_codes_are_1_to_4) {
    BT_CHECK((int)BT_GRADE_AGAIN == 1);
    BT_CHECK((int)BT_GRADE_HARD == 2);
    BT_CHECK((int)BT_GRADE_GOOD == 3);
    BT_CHECK((int)BT_GRADE_EASY == 4);
}

BT_TEST(fsrs, memory_validity_bounds_stability_and_difficulty) {
    bt_fsrs_memory ok = {3.5, 6.0};
    BT_CHECK(bt_fsrs_memory_is_valid(ok) == 1);
    bt_fsrs_memory negative = {-1.0, 6.0};
    BT_CHECK(bt_fsrs_memory_is_valid(negative) == 0);
    bt_fsrs_memory hard_floor = {1.0, 0.5};
    BT_CHECK(bt_fsrs_memory_is_valid(hard_floor) == 0);
    bt_fsrs_memory over_top = {1.0, 10.5};
    BT_CHECK(bt_fsrs_memory_is_valid(over_top) == 0);
}

BT_TEST(fsrs, initial_stability_uses_the_perturbed_weight) {
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    cfg.weights[0] = 5.0;
    bt_fsrs_memory m = bt_fsrs_memory_initial(BT_GRADE_AGAIN, &cfg);
    BT_CHECK(m.stability == 5.0);
}

BT_TEST(fsrs, next_interval_clamps_to_the_configured_bounds) {
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    cfg.interval_clamp_days[0] = 7;
    cfg.interval_clamp_days[1] = 200;
    BT_CHECK(bt_fsrs_next_interval_days(1e9, &cfg) == 200);
    BT_CHECK(bt_fsrs_next_interval_days(0.001, &cfg) == 7);
}
