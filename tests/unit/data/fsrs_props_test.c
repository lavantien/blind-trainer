#include "common/bt_harness.h"
#include "common/json.h"
#include "common/rng.h"
#include "data/fsrs.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WEIGHTS_TEXT                                                                                               \
    "[0.40255, 1.18385, 3.173, 15.69105, 7.1949, 0.5345, 1.4604, 0.0046, 1.54575, 0.1192, 1.01925, 1.9395, 0.11, " \
    "0.29605, 2.2698, 0.2315, 2.9898, 0.51655, 0.6621]"

static const double DEFAULT_WEIGHTS[BT_FSRS_WEIGHT_COUNT] = {
    0.40255, 1.18385, 3.173, 15.69105, 7.1949, 0.5345, 1.4604, 0.0046,  1.54575, 0.1192,
    1.01925, 1.9395,  0.11,  0.29605,  2.2698, 0.2315, 2.9898, 0.51655, 0.6621,
};

static void golden_config(bt_fsrs_config *cfg) {
    memcpy(cfg->weights, DEFAULT_WEIGHTS, sizeof DEFAULT_WEIGHTS);
    cfg->request_retention = 0.9;
    cfg->interval_clamp_days[0] = 1;
    cfg->interval_clamp_days[1] = 36500;
}

static void app_config(bt_fsrs_config *cfg) {
    memcpy(cfg->weights, DEFAULT_WEIGHTS, sizeof DEFAULT_WEIGHTS);
    cfg->request_retention = 0.9;
    cfg->interval_clamp_days[0] = 1;
    cfg->interval_clamp_days[1] = 365;
}

static void hub_section_text(char *out, size_t out_size, const char *weights, const char *retention,
                             const char *clamps) {
    snprintf(out, out_size,
             "{\"srs\":{\"fsrs\":{\"weights\":%s,\"request_retention\":%s,\"interval_clamp_days\":"
             "%s}}}",
             weights, retention, clamps);
}

static int config_from_text(const char *text, bt_fsrs_config *out) {
    bt_json_error jerr;
    bt_json_value *root = NULL;
    if (bt_json_parse(text, strlen(text), &root, &jerr) != 0) {
        return -1;
    }
    char error[96];
    int rc = bt_fsrs_config_from_json(root, out, error, sizeof error);
    bt_json_free(root);
    return rc;
}

BT_TEST(fsrs_props, weight_count_is_nineteen) {
    BT_CHECK(BT_FSRS_WEIGHT_COUNT == 19);
    BT_CHECK((int)BT_GRADE_EASY == 4);
    BT_CHECK((int)BT_GRADE_AGAIN == 1);
}

BT_TEST(fsrs_props, from_value_parses_the_approved_hub_section) {
    char text[512];
    hub_section_text(text, sizeof text, WEIGHTS_TEXT, "0.9", "[1, 365]");
    bt_fsrs_config cfg;
    BT_CHECK(config_from_text(text, &cfg) == 0);
    for (int i = 0; i < BT_FSRS_WEIGHT_COUNT; i++) {
        BT_CHECK(cfg.weights[i] == DEFAULT_WEIGHTS[i]);
    }
    BT_CHECK(cfg.request_retention == 0.9);
    BT_CHECK(cfg.interval_clamp_days[0] == 1);
    BT_CHECK(cfg.interval_clamp_days[1] == 365);
}

BT_TEST(fsrs_props, from_value_rejects_missing_or_malformed_sections) {
    bt_fsrs_config cfg;
    char error[96];
    bt_json_value *root = bt_json_new_object();
    BT_CHECK(bt_fsrs_config_from_json(root, &cfg, error, sizeof error) != 0);
    BT_CHECK_EQ(error, "config section missing: srs.fsrs");
    bt_json_free(root);
    char text[512];
    hub_section_text(text, sizeof text, "[0.40255]", "0.9", "[1, 365]");
    BT_CHECK(config_from_text(text, &cfg) != 0);
    hub_section_text(text, sizeof text, WEIGHTS_TEXT, "0.9", "[1]");
    BT_CHECK(config_from_text(text, &cfg) != 0);
    hub_section_text(text, sizeof text,
                     "[\"x\", 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, "
                     "14.0, 15.0, 16.0, 17.0, 18.0]",
                     "0.9", "[1, 365]");
    BT_CHECK(config_from_text(text, &cfg) != 0);
}

BT_TEST(fsrs_props, config_error_messages_are_distinct) {
    bt_fsrs_config cfg;
    char error[96];
    bt_json_value *empty = bt_json_new_object();
    BT_CHECK(bt_fsrs_config_from_json(empty, &cfg, error, sizeof error) != 0);
    BT_CHECK_EQ(error, "config section missing: srs.fsrs");
    bt_json_free(empty);
    char text[512];
    hub_section_text(text, sizeof text, "[0.40255]", "0.9", "[1, 365]");
    bt_json_error jerr;
    bt_json_value *root = NULL;
    BT_CHECK(bt_json_parse(text, strlen(text), &root, &jerr) == 0);
    BT_CHECK(bt_fsrs_config_from_json(root, &cfg, error, sizeof error) != 0);
    BT_CHECK(strncmp(error, "config section malformed: ", 26) == 0);
    bt_json_free(root);
    bt_json_value *section = bt_json_new_object();
    bt_json_value *weights = bt_json_new_array();
    for (int i = 0; i < BT_FSRS_WEIGHT_COUNT; i++) {
        bt_json_array_push(weights, bt_json_new_number(i == 0 ? INFINITY : DEFAULT_WEIGHTS[i]));
    }
    bt_json_object_set(section, "weights", weights);
    bt_json_object_set(section, "request_retention", bt_json_new_number(0.9));
    bt_json_value *clamps = bt_json_new_array();
    bt_json_array_push(clamps, bt_json_new_integer(1));
    bt_json_array_push(clamps, bt_json_new_integer(365));
    bt_json_object_set(section, "interval_clamp_days", clamps);
    bt_json_value *host = bt_json_new_object();
    bt_json_value *srs = bt_json_new_object();
    bt_json_object_set(srs, "fsrs", section);
    bt_json_object_set(host, "srs", srs);
    BT_CHECK(bt_fsrs_config_from_json(host, &cfg, error, sizeof error) != 0);
    BT_CHECK_EQ(error, "config section invalid: weights must be finite");
    bt_json_free(host);
}

BT_TEST(fsrs_props, from_value_rejects_non_finite_and_out_of_range_retention) {
    const char *retentions[6] = {"1e999", "-1e999", "0.0", "-0.5", "1.5", "2"};
    bt_fsrs_config cfg;
    char text[512];
    for (int i = 0; i < 6; i++) {
        hub_section_text(text, sizeof text, WEIGHTS_TEXT, retentions[i], "[1, 365]");
        BT_CHECK(config_from_text(text, &cfg) != 0);
    }
}

BT_TEST(fsrs_props, from_value_rejects_non_finite_weights_end_to_end) {
    bt_fsrs_config cfg;
    char text[512];
    hub_section_text(text, sizeof text,
                     "[1e999, 1.18385, 3.173, 15.69105, 7.1949, 0.5345, 1.4604, 0.0046, 1.54575, "
                     "0.1192, 1.01925, 1.9395, 0.11, 0.29605, 2.2698, 0.2315, 2.9898, 0.51655, "
                     "0.6621]",
                     "0.9", "[1, 365]");
    BT_CHECK(config_from_text(text, &cfg) != 0);
    hub_section_text(text, sizeof text,
                     "[-1e999, 1.18385, 3.173, 15.69105, 7.1949, 0.5345, 1.4604, 0.0046, 1.54575, "
                     "0.1192, 1.01925, 1.9395, 0.11, 0.29605, 2.2698, 0.2315, 2.9898, 0.51655, "
                     "0.6621]",
                     "0.9", "[1, 365]");
    BT_CHECK(config_from_text(text, &cfg) != 0);
}

BT_TEST(fsrs_props, from_value_rejects_bad_interval_clamps) {
    const char *clamps[4] = {"[0, 365]", "[10, 5]", "[-1, 365]", "[100, 99]"};
    bt_fsrs_config cfg;
    char text[512];
    for (int i = 0; i < 4; i++) {
        hub_section_text(text, sizeof text, WEIGHTS_TEXT, "0.9", clamps[i]);
        BT_CHECK(config_from_text(text, &cfg) != 0);
    }
    hub_section_text(text, sizeof text, WEIGHTS_TEXT, "0.9", "[365, 3650]");
    BT_CHECK(config_from_text(text, &cfg) == 0);
    BT_CHECK(cfg.interval_clamp_days[0] == 365);
    BT_CHECK(cfg.interval_clamp_days[1] == 3650);
}

BT_TEST(fsrs_props, from_value_accepts_retention_one_and_equal_clamp_bounds) {
    bt_fsrs_config cfg;
    char text[512];
    hub_section_text(text, sizeof text, WEIGHTS_TEXT, "1.0", "[1, 365]");
    BT_CHECK(config_from_text(text, &cfg) == 0);
    BT_CHECK(cfg.request_retention == 1.0);
    hub_section_text(text, sizeof text, WEIGHTS_TEXT, "0.9", "[7, 7]");
    BT_CHECK(config_from_text(text, &cfg) == 0);
    BT_CHECK(cfg.interval_clamp_days[0] == 7);
    BT_CHECK(cfg.interval_clamp_days[1] == 7);
}

BT_TEST(fsrs_props, state_validity_rejects_non_finite_and_out_of_domain_values) {
    bt_fsrs_config cfg;
    golden_config(&cfg);
    bt_grade grades[4] = {BT_GRADE_AGAIN, BT_GRADE_HARD, BT_GRADE_GOOD, BT_GRADE_EASY};
    for (int i = 0; i < 4; i++) {
        BT_CHECK(bt_fsrs_memory_is_valid(bt_fsrs_memory_initial(grades[i], &cfg)) == 1);
    }
    double bad_stability[5] = {0.0, -1.0, INFINITY, NAN, -INFINITY};
    for (int i = 0; i < 5; i++) {
        bt_fsrs_memory memory = {bad_stability[i], 5.0};
        BT_CHECK(bt_fsrs_memory_is_valid(memory) == 0);
    }
    double bad_difficulty[5] = {NAN, INFINITY, 0.5, 10.5, -1.0};
    for (int i = 0; i < 5; i++) {
        bt_fsrs_memory memory = {3.0, bad_difficulty[i]};
        BT_CHECK(bt_fsrs_memory_is_valid(memory) == 0);
    }
}

BT_TEST(fsrs_props, defaults_expose_the_reference_settings) {
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    for (int i = 0; i < BT_FSRS_WEIGHT_COUNT; i++) {
        BT_CHECK(cfg.weights[i] == DEFAULT_WEIGHTS[i]);
    }
    BT_CHECK(cfg.request_retention == 0.9);
    BT_CHECK(cfg.interval_clamp_days[0] == 1);
    BT_CHECK(cfg.interval_clamp_days[1] == 36500);
    bt_fsrs_config golden;
    golden_config(&golden);
    BT_CHECK(cfg.request_retention == golden.request_retention);
    BT_CHECK(cfg.interval_clamp_days[0] == golden.interval_clamp_days[0]);
    BT_CHECK(cfg.interval_clamp_days[1] == golden.interval_clamp_days[1]);
}

BT_TEST(fsrs_props, initial_state_uses_the_grade_weight_and_clamps_difficulty) {
    bt_fsrs_config cfg;
    golden_config(&cfg);
    struct {
        bt_grade grade;
        double stability;
        double difficulty;
    } cases[4] = {
        {BT_GRADE_AGAIN, 0.40255, 7.1949},
        {BT_GRADE_HARD, 1.18385, 6.488305268471453},
        {BT_GRADE_GOOD, 3.173, 5.282434422319005},
        {BT_GRADE_EASY, 15.69105, 3.2245015893713678},
    };
    for (int i = 0; i < 4; i++) {
        bt_fsrs_memory state = bt_fsrs_memory_initial(cases[i].grade, &cfg);
        BT_CHECK(fabs(state.stability - cases[i].stability) < 1e-12);
        BT_CHECK(fabs(state.difficulty - cases[i].difficulty) < 1e-12);
    }
}

BT_TEST(fsrs_props, initial_state_floors_stability_and_clamps_difficulty_bounds) {
    bt_fsrs_config cfg;
    golden_config(&cfg);
    cfg.weights[0] = 0.01;
    cfg.weights[4] = 99.0;
    bt_fsrs_memory low = bt_fsrs_memory_initial(BT_GRADE_AGAIN, &cfg);
    BT_CHECK(low.stability == 0.1);
    BT_CHECK(low.difficulty == 10.0);
    cfg.weights[4] = -99.0;
    cfg.weights[5] = 0.0;
    bt_fsrs_memory high = bt_fsrs_memory_initial(BT_GRADE_EASY, &cfg);
    BT_CHECK(high.difficulty == 1.0);
}

BT_TEST(fsrs_props, retrievability_is_one_at_zero_elapsed_and_decays_with_time) {
    bt_fsrs_config cfg;
    golden_config(&cfg);
    bt_fsrs_memory state = bt_fsrs_memory_initial(BT_GRADE_GOOD, &cfg);
    BT_CHECK(bt_fsrs_memory_retrievability(state, 0.0) == 1.0);
    bt_fsrs_memory integer_stability = {3.0, state.difficulty};
    double at_stability = bt_fsrs_memory_retrievability(integer_stability, 3.0);
    BT_CHECK(fabs(at_stability - 0.9) < 1e-12);
    BT_CHECK(bt_fsrs_memory_retrievability(state, 100.0) < at_stability);
    BT_CHECK(bt_fsrs_memory_retrievability(state, 100.0) > 0.0);
    BT_CHECK(bt_fsrs_memory_retrievability(state, 1000000.0) > 0.0);
}

BT_TEST(fsrs_props, negative_elapsed_clamps_to_full_retrievability) {
    bt_fsrs_config cfg;
    golden_config(&cfg);
    bt_fsrs_memory state = bt_fsrs_memory_initial(BT_GRADE_GOOD, &cfg);
    BT_CHECK(bt_fsrs_memory_retrievability(state, -5.0) == 1.0);
}

BT_TEST(fsrs_props, same_day_reviews_scale_stability_without_retrievability_input) {
    bt_fsrs_config cfg;
    golden_config(&cfg);
    bt_fsrs_memory state = bt_fsrs_memory_initial(BT_GRADE_GOOD, &cfg);
    struct {
        bt_fsrs_memory memory;
        bt_grade grade;
    } cases[4] = {
        {bt_fsrs_memory_review(state, BT_GRADE_AGAIN, 0.0, &cfg), BT_GRADE_AGAIN},
        {bt_fsrs_memory_review(state, BT_GRADE_HARD, 0.0, &cfg), BT_GRADE_HARD},
        {bt_fsrs_memory_review(state, BT_GRADE_GOOD, 0.0, &cfg), BT_GRADE_GOOD},
        {bt_fsrs_memory_review(state, BT_GRADE_EASY, 0.0, &cfg), BT_GRADE_EASY},
    };
    for (int i = 0; i < 4; i++) {
        double scale = exp(cfg.weights[17] * (((double)cases[i].grade) - 3.0 + cfg.weights[18]));
        BT_CHECK(fabs(cases[i].memory.stability - state.stability * scale) < 1e-9);
    }
}

BT_TEST(fsrs_props, long_gap_good_raises_stability_and_forget_lapses_drop_it) {
    bt_fsrs_config cfg;
    golden_config(&cfg);
    bt_fsrs_memory state = bt_fsrs_memory_initial(BT_GRADE_GOOD, &cfg);
    bt_fsrs_memory reviewed = bt_fsrs_memory_review(state, BT_GRADE_GOOD, 7.0, &cfg);
    BT_CHECK(reviewed.stability > state.stability);
    bt_fsrs_memory lapsed = bt_fsrs_memory_review(reviewed, BT_GRADE_AGAIN, 30.0, &cfg);
    BT_CHECK(lapsed.stability < reviewed.stability);
    BT_CHECK(lapsed.stability > 0.0);
}

BT_TEST(fsrs_props, post_lapse_stability_never_exceeds_the_short_term_ceiling) {
    bt_fsrs_config cfg;
    golden_config(&cfg);
    bt_fsrs_memory state = {400.0, 1.0};
    bt_fsrs_memory lapsed = bt_fsrs_memory_review(state, BT_GRADE_AGAIN, 1000.0, &cfg);
    double ceiling = 400.0 / exp(cfg.weights[17] * cfg.weights[18]);
    BT_CHECK(lapsed.stability <= ceiling + 1e-9);
}

BT_TEST(fsrs_props, difficulty_stays_inside_bounds_under_extreme_walks) {
    bt_fsrs_config cfg;
    golden_config(&cfg);
    bt_fsrs_memory state = bt_fsrs_memory_initial(BT_GRADE_AGAIN, &cfg);
    for (int i = 0; i < 50; i++) {
        state = bt_fsrs_memory_review(state, BT_GRADE_AGAIN, 1.0, &cfg);
        BT_CHECK(state.difficulty >= 1.0 && state.difficulty <= 10.0);
    }
    bt_fsrs_memory easy = bt_fsrs_memory_initial(BT_GRADE_EASY, &cfg);
    for (int i = 0; i < 50; i++) {
        easy = bt_fsrs_memory_review(easy, BT_GRADE_EASY, 1.0, &cfg);
        BT_CHECK(easy.difficulty >= 1.0 && easy.difficulty <= 10.0);
    }
}

BT_TEST(fsrs_props, interval_clamps_at_both_bounds) {
    bt_fsrs_config cfg;
    app_config(&cfg);
    BT_CHECK(bt_fsrs_next_interval_days(0.01, &cfg) == 1);
    BT_CHECK(bt_fsrs_next_interval_days(1000000.0, &cfg) == 365);
    BT_CHECK(bt_fsrs_next_interval_days(120.4, &cfg) == 120);
    bt_fsrs_config wide;
    golden_config(&wide);
    BT_CHECK(bt_fsrs_next_interval_days(1000000.0, &wide) == 36500);
}

BT_TEST(fsrs_props, interval_respects_a_raised_floor_and_lets_the_cap_win_last) {
    bt_fsrs_config cfg;
    memcpy(cfg.weights, DEFAULT_WEIGHTS, sizeof DEFAULT_WEIGHTS);
    cfg.request_retention = 0.9;
    cfg.interval_clamp_days[0] = 7;
    cfg.interval_clamp_days[1] = 365;
    BT_CHECK(bt_fsrs_next_interval_days(0.01, &cfg) == 7);
    BT_CHECK(bt_fsrs_next_interval_days(3.0, &cfg) == 7);
    BT_CHECK(bt_fsrs_next_interval_days(10.0, &cfg) == 10);
    bt_fsrs_config inverted;
    memcpy(inverted.weights, DEFAULT_WEIGHTS, sizeof DEFAULT_WEIGHTS);
    inverted.request_retention = 0.9;
    inverted.interval_clamp_days[0] = 10;
    inverted.interval_clamp_days[1] = 5;
    BT_CHECK(bt_fsrs_next_interval_days(3.0, &inverted) == 5);
    BT_CHECK(bt_fsrs_next_interval_days(400.0, &inverted) == 5);
}

BT_TEST(fsrs_props, interval_rounds_ties_to_even_like_the_reference) {
    bt_fsrs_config cfg;
    golden_config(&cfg);
    BT_CHECK(bt_fsrs_next_interval_days(2.5, &cfg) == 2);
    BT_CHECK(bt_fsrs_next_interval_days(3.5, &cfg) == 4);
    BT_CHECK(bt_fsrs_next_interval_days(8.5, &cfg) == 8);
    BT_CHECK(bt_fsrs_next_interval_days(9.5, &cfg) == 10);
}

BT_TEST(fsrs_props, fractional_elapsed_floors_to_whole_days_like_the_reference) {
    bt_fsrs_config cfg;
    golden_config(&cfg);
    bt_fsrs_memory state = bt_fsrs_memory_initial(BT_GRADE_GOOD, &cfg);
    bt_fsrs_memory same_day = bt_fsrs_memory_review(state, BT_GRADE_GOOD, 0.0, &cfg);
    double elapsed_values[3] = {0.0, 0.5, -0.5};
    for (int i = 0; i < 3; i++) {
        bt_fsrs_memory next = bt_fsrs_memory_review(state, BT_GRADE_GOOD, elapsed_values[i], &cfg);
        BT_CHECK(next.stability == same_day.stability);
        BT_CHECK(next.difficulty == same_day.difficulty);
        BT_CHECK(bt_fsrs_memory_retrievability(state, elapsed_values[i]) == 1.0);
    }
    bt_fsrs_memory one_day = bt_fsrs_memory_review(state, BT_GRADE_GOOD, 1.0, &cfg);
    bt_fsrs_memory one_and_half = bt_fsrs_memory_review(state, BT_GRADE_GOOD, 1.5, &cfg);
    BT_CHECK(one_and_half.stability == one_day.stability);
    BT_CHECK(one_and_half.difficulty == one_day.difficulty);
    BT_CHECK(fabs(one_day.stability - 5.869141822641378) < 1e-12);
    bt_fsrs_memory nearly_three = bt_fsrs_memory_review(state, BT_GRADE_GOOD, 2.9, &cfg);
    BT_CHECK(fabs(nearly_three.stability - 8.384375357393525) < 1e-12);
    BT_CHECK(bt_fsrs_next_interval_days(nearly_three.stability, &cfg) == 8);
}

BT_TEST(fsrs_props, config_round_trips_through_the_json_section) {
    bt_fsrs_config cfg;
    golden_config(&cfg);
    bt_json_value *section = bt_json_new_object();
    bt_json_value *weights = bt_json_new_array();
    for (int i = 0; i < BT_FSRS_WEIGHT_COUNT; i++) {
        bt_json_array_push(weights, bt_json_new_number(cfg.weights[i]));
    }
    bt_json_object_set(section, "weights", weights);
    bt_json_object_set(section, "request_retention", bt_json_new_number(cfg.request_retention));
    bt_json_value *clamps = bt_json_new_array();
    bt_json_array_push(clamps, bt_json_new_integer(cfg.interval_clamp_days[0]));
    bt_json_array_push(clamps, bt_json_new_integer(cfg.interval_clamp_days[1]));
    bt_json_object_set(section, "interval_clamp_days", clamps);
    bt_json_value *root = bt_json_new_object();
    bt_json_value *srs = bt_json_new_object();
    bt_json_object_set(srs, "fsrs", section);
    bt_json_object_set(root, "srs", srs);
    bt_strbuf text;
    bt_strbuf_init(&text, 256);
    bt_json_serialize(root, &text);
    bt_fsrs_config back;
    BT_CHECK(config_from_text(text.data, &back) == 0);
    for (int i = 0; i < BT_FSRS_WEIGHT_COUNT; i++) {
        BT_CHECK(back.weights[i] == cfg.weights[i]);
    }
    BT_CHECK(back.request_retention == cfg.request_retention);
    BT_CHECK(back.interval_clamp_days[0] == cfg.interval_clamp_days[0]);
    BT_CHECK(back.interval_clamp_days[1] == cfg.interval_clamp_days[1]);
    BT_CHECK(strstr(text.data, "request_retention") != NULL);
    bt_strbuf_free(&text);
    bt_json_free(root);
    bt_fsrs_memory state = bt_fsrs_memory_initial(BT_GRADE_GOOD, &cfg);
    bt_json_value *state_json = bt_json_new_object();
    bt_json_object_set(state_json, "stability", bt_json_new_number(state.stability));
    bt_json_object_set(state_json, "difficulty", bt_json_new_number(state.difficulty));
    bt_strbuf state_text;
    bt_strbuf_init(&state_text, 64);
    bt_json_serialize(state_json, &state_text);
    bt_json_error jerr;
    bt_json_value *state_back = NULL;
    BT_CHECK(bt_json_parse(state_text.data, state_text.len, &state_back, &jerr) == 0);
    BT_CHECK(state_back->as.object.count == 2);
    BT_CHECK(bt_json_get(state_back, "stability")->as.number == state.stability);
    BT_CHECK(bt_json_get(state_back, "difficulty")->as.number == state.difficulty);
    bt_json_free(state_back);
    bt_strbuf_free(&state_text);
    bt_json_free(state_json);
    bt_strbuf weights_text;
    bt_strbuf_init(&weights_text, 256);
    bt_json_value *weights_json = bt_json_new_array();
    for (int i = 0; i < BT_FSRS_WEIGHT_COUNT; i++) {
        bt_json_array_push(weights_json, bt_json_new_number(DEFAULT_WEIGHTS[i]));
    }
    bt_json_serialize(weights_json, &weights_text);
    bt_json_error werr;
    bt_json_value *weights_back = NULL;
    BT_CHECK(bt_json_parse(weights_text.data, weights_text.len, &weights_back, &werr) == 0);
    BT_CHECK(weights_back->as.array.count == BT_FSRS_WEIGHT_COUNT);
    for (int i = 0; i < BT_FSRS_WEIGHT_COUNT; i++) {
        BT_CHECK(bt_json_at(weights_back, (size_t)i)->as.number == DEFAULT_WEIGHTS[i]);
    }
    bt_json_free(weights_back);
    bt_strbuf_free(&weights_text);
    bt_json_free(weights_json);
}

BT_TEST(fsrs_props, good_reviews_grow_stability_monotonically) {
    bt_fsrs_config cfg;
    golden_config(&cfg);
    bt_rng rng;
    bt_rng_seed(&rng, 20261012);
    for (int trial = 0; trial < 128; trial++) {
        bt_grade seed_grade = (bt_grade)(1 + bt_rng_below(&rng, 4));
        double elapsed = 1.0 + (double)bt_rng_below(&rng, 3649000) / 1000.0;
        size_t rounds = 1 + (size_t)bt_rng_below(&rng, 9);
        bt_fsrs_memory state = bt_fsrs_memory_initial(seed_grade, &cfg);
        for (size_t i = 0; i < rounds; i++) {
            bt_fsrs_memory next = bt_fsrs_memory_review(state, BT_GRADE_GOOD, elapsed, &cfg);
            BT_CHECK(next.stability > state.stability);
            state = next;
        }
    }
}

BT_TEST(fsrs_props, intervals_respect_clamps_and_grow_with_stability) {
    bt_rng rng;
    bt_rng_seed(&rng, 20261013);
    for (int trial = 0; trial < 128; trial++) {
        bt_fsrs_config cfg;
        memcpy(cfg.weights, DEFAULT_WEIGHTS, sizeof DEFAULT_WEIGHTS);
        cfg.request_retention = 0.7 + (double)bt_rng_below(&rng, 290) / 1000.0;
        cfg.interval_clamp_days[0] = 1;
        cfg.interval_clamp_days[1] = (long long)(30 + bt_rng_below(&rng, 36470));
        double stability_a = 0.1 + (double)bt_rng_below(&rng, 990) / 10.0;
        double growth = 1.0 + (double)bt_rng_below(&rng, 90) / 10.0;
        double stability_b = stability_a * growth;
        long long ivl_a = bt_fsrs_next_interval_days(stability_a, &cfg);
        long long ivl_b = bt_fsrs_next_interval_days(stability_b, &cfg);
        BT_CHECK(ivl_a >= 1);
        BT_CHECK(ivl_a <= cfg.interval_clamp_days[1]);
        BT_CHECK(ivl_b >= ivl_a);
        BT_CHECK(ivl_b <= cfg.interval_clamp_days[1]);
    }
}

BT_TEST(fsrs_props, random_walks_keep_state_finite_and_in_bounds) {
    bt_fsrs_config cfg;
    golden_config(&cfg);
    bt_rng rng;
    bt_rng_seed(&rng, 20261014);
    for (int trial = 0; trial < 64; trial++) {
        bt_fsrs_memory state = bt_fsrs_memory_initial((bt_grade)(1 + bt_rng_below(&rng, 4)), &cfg);
        size_t rounds = 1 + (size_t)bt_rng_below(&rng, 39);
        for (size_t i = 0; i < rounds; i++) {
            double elapsed = (double)bt_rng_below(&rng, 3650);
            state =
                bt_fsrs_memory_review(state, (bt_grade)(1 + bt_rng_below(&rng, 4)), elapsed, &cfg);
            BT_CHECK(isfinite(state.stability));
            BT_CHECK(state.stability > 0.0);
            BT_CHECK(state.difficulty >= 1.0 && state.difficulty <= 10.0);
            BT_CHECK(bt_fsrs_memory_is_valid(state) == 1);
            BT_CHECK(bt_fsrs_memory_retrievability(state, elapsed) <= 1.0);
        }
    }
}
