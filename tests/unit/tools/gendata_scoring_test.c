#include "gendata/scoring.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_harness.h"
#include "common/json.h"
#include "common/rng.h"
#include "common/strbuf.h"
#include "core/alg.h"
#include "core/cube.h"
#include "core/moves.h"
#include "scheme/scheme.h"

static const char *REFERENCE_CONFIG =
    "{\"synth\": {\"scoring\": {"
    "\"preferred\": [\"U\", \"D\", \"R\"],"
    "\"preferred_edge\": [\"U\", \"D\", \"R\", \"M\", \"E\", \"S\"],"
    "\"penalty_other\": 2.0,"
    "\"weight_len\": 1.0,"
    "\"weight_subset\": 2.0,"
    "\"weight_regrip\": 1.5,"
    "\"weight_finger\": 0.5,"
    "\"finger_cost\": {"
    "\"U\": [0.2, 0.2], \"D\": [0.2, 0.2],"
    "\"L\": [0.4, 0.4], \"R\": [0.4, 0.4],"
    "\"F\": [0.8, 0.8], \"B\": [0.8, 0.8]"
    "}}}}";

static void reference(bt_gd_scoring_cfg *cfg) {
    bt_json_error jerr;
    bt_json_value *config = NULL;
    if (bt_json_parse(REFERENCE_CONFIG, strlen(REFERENCE_CONFIG), &config, &jerr) != 0) {
        BT_ABORT("%s", "reference scoring config must parse");
    }
    char err[128];
    if (bt_gd_scoring_from_config(config, cfg, err, sizeof err) != 0) {
        BT_ABORT("%s", err);
    }
    bt_json_free(config);
}

static bt_gd_score_parts scored(const char *text, bt_orbit orbit) {
    bt_gd_scoring_cfg cfg;
    reference(&cfg);
    bt_alg alg = {0};
    char bad[64];
    if (!bt_alg_parse(text, &alg, bad, sizeof bad)) {
        BT_ABORT("%s", bad);
    }
    bt_gd_score_parts parts;
    if (!bt_gd_score(alg.items, alg.len, orbit, &cfg, &parts)) {
        BT_ABORT("%s", "sequence must score");
    }
    bt_alg_free(&alg);
    return parts;
}

static void assert_close(float actual, float expected) {
    if (!(fabsf(actual - expected) < 1e-6F)) {
        char line[96];
        snprintf(line, sizeof line, "actual %f vs expected %f", (double)actual, (double)expected);
        BT_ABORT("%s", line);
    }
}

BT_TEST(gendata_scoring, right_hand_home_sequence_costs_its_length_and_finger_only) {
    bt_gd_score_parts parts = scored("R U R'", BT_ORBIT_CORNER);
    BT_CHECK_EQ(parts.qtm, 3U);
    BT_CHECK_EQ(parts.regrips, 0U);
    assert_close(parts.subset, 0.0F);
    assert_close(parts.finger, 0.4F + 0.2F + 0.4F);
    assert_close(parts.total, 1.0F * 3.0F + 0.5F * 1.0F);
}

BT_TEST(gendata_scoring, front_face_displaces_the_hand_and_costs_a_regrip) {
    bt_gd_score_parts parts = scored("R F R'", BT_ORBIT_CORNER);
    BT_CHECK_EQ(parts.qtm, 3U);
    BT_CHECK_EQ(parts.regrips, 1U);
    assert_close(parts.subset, 2.0F);
    assert_close(parts.finger, 0.4F + 0.8F + 0.4F);
    assert_close(parts.total, 1.0F * 3.0F + 2.0F * 2.0F + 1.5F * 1.0F + 0.5F * 1.6F);
}

BT_TEST(gendata_scoring, left_hand_dynamic_front_move_costs_a_regrip_on_return) {
    bt_gd_score_parts parts = scored("L F L'", BT_ORBIT_CORNER);
    BT_CHECK_EQ(parts.regrips, 1U);
    assert_close(parts.subset, 6.0F);
    assert_close(parts.total, 3.0F + 12.0F + 1.5F + 0.8F);
}

BT_TEST(gendata_scoring, slice_turn_is_preferred_for_edges_and_plain_for_corners) {
    bt_gd_score_parts edge = scored("M2", BT_ORBIT_EDGE);
    BT_CHECK_EQ(edge.qtm, 2U);
    BT_CHECK_EQ(edge.regrips, 0U);
    assert_close(edge.subset, 0.0F);
    assert_close(edge.finger, 0.4F);
    assert_close(edge.total, 2.0F + 0.2F);
    bt_gd_score_parts corner = scored("M2", BT_ORBIT_CORNER);
    assert_close(corner.subset, 2.0F);
    assert_close(corner.total, 2.0F + 4.0F + 0.2F);
}

BT_TEST(gendata_scoring, alternating_home_hands_never_regrips) {
    bt_gd_score_parts parts = scored("U L U' L'", BT_ORBIT_CORNER);
    BT_CHECK_EQ(parts.qtm, 4U);
    BT_CHECK_EQ(parts.regrips, 0U);
    assert_close(parts.subset, 4.0F);
    assert_close(parts.finger, 0.2F + 0.4F + 0.2F + 0.4F);
    assert_close(parts.total, 4.0F + 8.0F + 0.5F * 1.2F);
}

BT_TEST(gendata_scoring, front_displacement_charges_the_right_hand_on_its_next_turn) {
    bt_gd_score_parts parts = scored("F R U R'", BT_ORBIT_CORNER);
    BT_CHECK_EQ(parts.qtm, 4U);
    BT_CHECK_EQ(parts.regrips, 1U);
    assert_close(parts.subset, 2.0F);
    assert_close(parts.finger, 0.8F + 0.4F + 0.2F + 0.4F);
    assert_close(parts.total, 4.0F + 4.0F + 1.5F + 0.5F * 1.8F);
}

BT_TEST(gendata_scoring, dynamic_hand_keeps_the_previous_hand_without_charging_it) {
    bt_gd_score_parts parts = scored("L F R", BT_ORBIT_CORNER);
    BT_CHECK_EQ(parts.regrips, 0U);
    assert_close(parts.subset, 4.0F);
    assert_close(parts.finger, 0.4F + 0.8F + 0.4F);
    assert_close(parts.total, 3.0F + 8.0F + 0.5F * 1.6F);
}

BT_TEST(gendata_scoring, consecutive_same_face_turns_are_scored_once_for_grip) {
    bt_gd_score_parts parts = scored("R R'", BT_ORBIT_CORNER);
    BT_CHECK_EQ(parts.qtm, 2U);
    BT_CHECK_EQ(parts.regrips, 0U);
    assert_close(parts.finger, 0.8F);
    assert_close(parts.total, 2.0F + 0.4F);
}

BT_TEST(gendata_scoring, double_turns_count_two_qtm_and_charge_the_cw_finger_lane) {
    bt_gd_score_parts parts = scored("U2", BT_ORBIT_CORNER);
    BT_CHECK_EQ(parts.qtm, 2U);
    assert_close(parts.finger, 0.2F);
    assert_close(parts.total, 2.0F + 0.1F);
}

BT_TEST(gendata_scoring, rotation_moves_reject_the_sequence) {
    bt_gd_scoring_cfg cfg;
    reference(&cfg);
    bt_alg alg = {0};
    char bad[64];
    BT_CHECK(bt_alg_parse("R x U'", &alg, bad, sizeof bad));
    BT_CHECK(!bt_gd_score(alg.items, alg.len, BT_ORBIT_CORNER, &cfg, NULL));
    uint32_t regrips = 0;
    BT_CHECK(!bt_gd_count_regrips(alg.items, alg.len, &regrips));
    bt_alg_free(&alg);
}

BT_TEST(gendata_scoring, empty_sequence_scores_zero) {
    bt_gd_scoring_cfg cfg;
    reference(&cfg);
    bt_gd_score_parts parts;
    BT_CHECK(bt_gd_score(NULL, 0, BT_ORBIT_EDGE, &cfg, &parts));
    BT_CHECK_EQ(parts.qtm, 0U);
    BT_CHECK_EQ(parts.regrips, 0U);
    BT_CHECK_EQ(parts.subset, 0.0F);
    BT_CHECK_EQ(parts.finger, 0.0F);
    BT_CHECK_EQ(parts.total, 0.0F);
}

BT_TEST(gendata_scoring, parse_rejects_a_missing_scoring_section) {
    static const char *empty_synth = "{\"synth\": {}}";
    bt_json_error jerr;
    bt_json_value *config = NULL;
    BT_CHECK_EQ(bt_json_parse(empty_synth, strlen(empty_synth), &config, &jerr), 0);
    bt_gd_scoring_cfg cfg;
    char err[128];
    BT_CHECK(bt_gd_scoring_from_config(config, &cfg, err, sizeof err) != 0);
    BT_CHECK_EQ(strcmp(err, "config section missing: synth.scoring"), 0);
    bt_json_free(config);
}

BT_TEST(gendata_scoring, parse_names_every_missing_field) {
    static const char *const keys[8] = {
        "preferred",     "preferred_edge", "penalty_other", "weight_len",
        "weight_subset", "weight_regrip",  "weight_finger", "finger_cost",
    };
    for (size_t k = 0; k < 8; k++) {
        bt_json_error jerr;
        bt_json_value *config = NULL;
        BT_CHECK_EQ(bt_json_parse(REFERENCE_CONFIG, strlen(REFERENCE_CONFIG), &config, &jerr), 0);
        bt_json_value *scoring = bt_json_get(bt_json_get(config, "synth"), "scoring");
        size_t keep = 0;
        for (size_t i = 0; i < scoring->as.object.count; i++) {
            if (strcmp(scoring->as.object.keys[i], keys[k]) == 0) {
                continue;
            }
            scoring->as.object.keys[keep] = scoring->as.object.keys[i];
            scoring->as.object.values[keep] = scoring->as.object.values[i];
            keep++;
        }
        scoring->as.object.count = keep;
        bt_gd_scoring_cfg cfg;
        char err[128];
        BT_CHECK(bt_gd_scoring_from_config(config, &cfg, err, sizeof err) != 0);
        char expected[96];
        snprintf(expected, sizeof expected, "config section missing: synth.scoring.%s", keys[k]);
        BT_CHECK_EQ(strcmp(err, expected), 0);
        bt_json_free(config);
    }
}

BT_TEST(gendata_scoring, parse_rejects_unknown_lane_letters_and_empty_sets) {
    {
        bt_json_error jerr;
        bt_json_value *config = NULL;
        BT_CHECK_EQ(bt_json_parse(REFERENCE_CONFIG, strlen(REFERENCE_CONFIG), &config, &jerr), 0);
        bt_json_get(bt_json_get(bt_json_get(config, "synth"), "scoring"), "preferred")
            ->as.array.items[0] = bt_json_new_string("X");
        (void)0;
        bt_gd_scoring_cfg cfg;
        char err[128];
        BT_CHECK(bt_gd_scoring_from_config(config, &cfg, err, sizeof err) != 0);
        BT_CHECK_EQ(
            strcmp(err,
                   "synth.scoring.preferred entries must be single lanes from U D L R F B M E S"),
            0);
        bt_json_free(config);
    }
    {
        bt_json_error jerr;
        bt_json_value *config = NULL;
        BT_CHECK_EQ(bt_json_parse(REFERENCE_CONFIG, strlen(REFERENCE_CONFIG), &config, &jerr), 0);
        bt_json_value *lanes =
            bt_json_get(bt_json_get(bt_json_get(config, "synth"), "scoring"), "preferred_edge");
        for (size_t i = 0; i < lanes->as.array.count; i++) {
            bt_json_free(lanes->as.array.items[i]);
        }
        lanes->as.array.count = 0;
        bt_gd_scoring_cfg cfg;
        char err[128];
        BT_CHECK(bt_gd_scoring_from_config(config, &cfg, err, sizeof err) != 0);
        BT_CHECK_EQ(strcmp(err, "synth.scoring.preferred_edge must list at least one lane"), 0);
        bt_json_free(config);
    }
}

BT_TEST(gendata_scoring, parse_rejects_negative_weights_and_nonfinite_numbers) {
    {
        bt_json_error jerr;
        bt_json_value *config = NULL;
        BT_CHECK_EQ(bt_json_parse(REFERENCE_CONFIG, strlen(REFERENCE_CONFIG), &config, &jerr), 0);
        bt_json_object_set(bt_json_get(bt_json_get(config, "synth"), "scoring"), "weight_len",
                           bt_json_new_number(-0.5));
        bt_gd_scoring_cfg cfg;
        char err[128];
        BT_CHECK(bt_gd_scoring_from_config(config, &cfg, err, sizeof err) != 0);
        BT_CHECK_EQ(strcmp(err, "synth.scoring.weight_len must be a finite number >= 0"), 0);
        bt_json_free(config);
    }
    {
        const char *text = "{\"synth\": {\"scoring\": {"
                           "\"preferred\": [\"U\", \"D\", \"R\"],"
                           "\"preferred_edge\": [\"U\", \"D\", \"R\", \"M\", \"E\", \"S\"],"
                           "\"penalty_other\": 1e300,"
                           "\"weight_len\": 1.0,"
                           "\"weight_subset\": 2.0,"
                           "\"weight_regrip\": 1.5,"
                           "\"weight_finger\": 0.5,"
                           "\"finger_cost\": {"
                           "\"U\": [0.2, 0.2], \"D\": [0.2, 0.2],"
                           "\"L\": [0.4, 0.4], \"R\": [0.4, 0.4],"
                           "\"F\": [0.8, 0.8], \"B\": [0.8, 0.8]"
                           "}}}}";
        bt_json_error jerr;
        bt_json_value *config = NULL;
        BT_CHECK_EQ(bt_json_parse(text, strlen(text), &config, &jerr), 0);
        bt_gd_scoring_cfg cfg;
        char err[128];
        BT_CHECK(bt_gd_scoring_from_config(config, &cfg, err, sizeof err) != 0);
        BT_CHECK_EQ(strcmp(err, "synth.scoring.penalty_other must be a finite number"), 0);
        bt_json_free(config);
    }
}

BT_TEST(gendata_scoring, parse_rejects_finger_cost_typos_and_wrong_arities) {
    {
        bt_json_error jerr;
        bt_json_value *config = NULL;
        BT_CHECK_EQ(bt_json_parse(REFERENCE_CONFIG, strlen(REFERENCE_CONFIG), &config, &jerr), 0);
        bt_json_object_set(
            bt_json_get(bt_json_get(bt_json_get(config, "synth"), "scoring"), "finger_cost"), "N",
            bt_json_new_array());
        bt_gd_scoring_cfg cfg;
        char err[128];
        BT_CHECK(bt_gd_scoring_from_config(config, &cfg, err, sizeof err) != 0);
        BT_CHECK_EQ(
            strcmp(err, "synth.scoring.finger_cost must hold exactly the lanes U D L R F B"), 0);
        bt_json_free(config);
    }
    {
        bt_json_error jerr;
        bt_json_value *config = NULL;
        BT_CHECK_EQ(bt_json_parse(REFERENCE_CONFIG, strlen(REFERENCE_CONFIG), &config, &jerr), 0);
        bt_json_value *cost =
            bt_json_get(bt_json_get(bt_json_get(config, "synth"), "scoring"), "finger_cost");
        bt_json_object_set(cost, "R", bt_json_new_array());
        bt_json_array_push(bt_json_get(cost, "R"), bt_json_new_number(0.4));
        bt_gd_scoring_cfg cfg;
        char err[128];
        BT_CHECK(bt_gd_scoring_from_config(config, &cfg, err, sizeof err) != 0);
        BT_CHECK_EQ(strcmp(err, "synth.scoring.finger_cost.R must be [cw, ccw] finite numbers"), 0);
        bt_json_free(config);
    }
    {
        bt_json_error jerr;
        bt_json_value *config = NULL;
        BT_CHECK_EQ(bt_json_parse(REFERENCE_CONFIG, strlen(REFERENCE_CONFIG), &config, &jerr), 0);
        bt_json_value *cost =
            bt_json_get(bt_json_get(bt_json_get(config, "synth"), "scoring"), "finger_cost");
        for (size_t i = 0; i < cost->as.object.count; i++) {
            if (strcmp(cost->as.object.keys[i], "B") == 0) {
                cost->as.object.keys[i] = _strdup("Z");
                break;
            }
        }
        bt_gd_scoring_cfg cfg;
        char err[128];
        BT_CHECK(bt_gd_scoring_from_config(config, &cfg, err, sizeof err) != 0);
        BT_CHECK_EQ(strcmp(err, "synth.scoring.finger_cost.B must be [cw, ccw] finite numbers"), 0);
        bt_json_free(config);
    }
}

BT_TEST(gendata_scoring, parse_maps_lanes_and_costs_into_the_model) {
    bt_gd_scoring_cfg cfg;
    reference(&cfg);
    const bool preferred_want[9] = {true, false, false, true, false, true, false, false, false};
    const bool preferred_edge_want[9] = {true, false, false, true, false, true, true, true, true};
    BT_CHECK_EQ(memcmp(cfg.preferred, preferred_want, sizeof preferred_want), 0);
    BT_CHECK_EQ(memcmp(cfg.preferred_edge, preferred_edge_want, sizeof preferred_edge_want), 0);
    assert_close(cfg.penalty_other, 2.0F);
    assert_close(cfg.weight_len, 1.0F);
    assert_close(cfg.weight_subset, 2.0F);
    assert_close(cfg.weight_regrip, 1.5F);
    assert_close(cfg.weight_finger, 0.5F);
    assert_close(cfg.finger_cost[BT_FACE_F][1], 0.8F);
    assert_close(cfg.finger_cost[BT_FACE_D][0], 0.2F);
}

static void random_sequence(bt_rng *rng, bt_move *out, size_t *count) {
    *count = 1 + bt_rng_below(rng, 24);
    for (size_t i = 0; i < *count; i++) {
        size_t lane = bt_rng_below(rng, 9);
        uint8_t quarter = 1 + (uint8_t)bt_rng_below(rng, 3);
        out[i] = lane < 6 ? bt_move_of_face((uint8_t)lane, quarter)
                          : bt_move_of_slice((uint8_t)(lane - 6), quarter);
    }
}

static bool parts_eq(const bt_gd_score_parts *a, const bt_gd_score_parts *b) {
    if (a->qtm != b->qtm || a->regrips != b->regrips || a->subset != b->subset ||
        a->finger != b->finger) {
        return false;
    }
    return a->total == b->total;
}

BT_TEST(gendata_scoring, score_is_deterministic_and_survives_the_format_round_trip) {
    bt_gd_scoring_cfg cfg;
    reference(&cfg);
    bt_rng rng;
    bt_rng_seed(&rng, 0xb739d5ee4a11f208ULL);
    for (int round = 0; round < 256; round++) {
        bt_move seq[24];
        size_t count = 0;
        random_sequence(&rng, seq, &count);
        bt_orbit orbits[2] = {BT_ORBIT_EDGE, BT_ORBIT_CORNER};
        for (int o = 0; o < 2; o++) {
            bt_gd_score_parts once = {0};
            bt_gd_score_parts twice = {0};
            BT_CHECK(bt_gd_score(seq, count, orbits[o], &cfg, &once));
            BT_CHECK(bt_gd_score(seq, count, orbits[o], &cfg, &twice));
            BT_CHECK(parts_eq(&once, &twice));
            bt_alg alg;
            bt_alg_init(&alg);
            for (size_t i = 0; i < count; i++) {
                bt_alg_push(&alg, seq[i]);
            }
            bt_strbuf text;
            bt_strbuf_init(&text, 0);
            bt_alg_format(&alg, &text);
            bt_alg reparsed;
            char bad[64];
            BT_CHECK(bt_alg_parse(text.data, &reparsed, bad, sizeof bad));
            bt_gd_score_parts third = {0};
            BT_CHECK(bt_gd_score(reparsed.items, reparsed.len, orbits[o], &cfg, &third));
            BT_CHECK(parts_eq(&once, &third));
            bt_alg_free(&reparsed);
            bt_strbuf_free(&text);
            bt_alg_free(&alg);
        }
    }
}

BT_TEST(gendata_scoring, appending_a_penalized_move_strictly_raises_the_total) {
    bt_gd_scoring_cfg cfg;
    reference(&cfg);
    bt_rng rng;
    bt_rng_seed(&rng, 0x1c62f0aa93d74e55ULL);
    for (int round = 0; round < 256; round++) {
        bt_move seq[24];
        size_t count = 0;
        random_sequence(&rng, seq, &count);
        bt_orbit orbits[2] = {BT_ORBIT_EDGE, BT_ORBIT_CORNER};
        for (int o = 0; o < 2; o++) {
            bt_gd_score_parts before = {0};
            BT_CHECK(bt_gd_score(seq, count, orbits[o], &cfg, &before));
            bt_move extended[25];
            memcpy(extended, seq, count * sizeof(bt_move));
            extended[count] = bt_move_of_face(BT_FACE_F, 1);
            bt_gd_score_parts after = {0};
            BT_CHECK(bt_gd_score(extended, count + 1, orbits[o], &cfg, &after));
            BT_CHECK(after.total > before.total);
        }
    }
}

BT_TEST(gendata_scoring, rank_order_is_total) {
    bt_gd_scoring_cfg cfg;
    reference(&cfg);
    bt_rng rng;
    bt_rng_seed(&rng, 0x5ee4bca71d0f9316ULL);
    for (int round = 0; round < 64; round++) {
        bt_move first[24];
        bt_move second[24];
        bt_move third[24];
        size_t first_len = 0;
        size_t second_len = 0;
        size_t third_len = 0;
        random_sequence(&rng, first, &first_len);
        random_sequence(&rng, second, &second_len);
        random_sequence(&rng, third, &third_len);
        bt_orbit orbits[2] = {BT_ORBIT_EDGE, BT_ORBIT_CORNER};
        for (int o = 0; o < 2; o++) {
            int ab = bt_gd_rank_cmp(first, first_len, second, second_len, orbits[o], &cfg);
            int ba = bt_gd_rank_cmp(second, second_len, first, first_len, orbits[o], &cfg);
            BT_CHECK_EQ(ab, -ba);
            int bc = bt_gd_rank_cmp(second, second_len, third, third_len, orbits[o], &cfg);
            int ac = bt_gd_rank_cmp(first, first_len, third, third_len, orbits[o], &cfg);
            if (ab <= 0 && bc <= 0) {
                BT_CHECK(ac <= 0);
            }
            bt_move *batch[3] = {first, second, third};
            size_t lens[3] = {first_len, second_len, third_len};
            for (int pass = 0; pass < 3; pass++) {
                for (int i = 0; i + 1 < 3; i++) {
                    if (bt_gd_rank_cmp(batch[i + 1], lens[i + 1], batch[i], lens[i], orbits[o],
                                       &cfg) < 0) {
                        bt_move *swap = batch[i];
                        batch[i] = batch[i + 1];
                        batch[i + 1] = swap;
                        size_t swap_len = lens[i];
                        lens[i] = lens[i + 1];
                        lens[i + 1] = swap_len;
                    }
                }
            }
            for (int i = 0; i + 1 < 3; i++) {
                BT_CHECK(bt_gd_rank_cmp(batch[i], lens[i], batch[i + 1], lens[i + 1], orbits[o],
                                        &cfg) <= 0);
            }
        }
    }
}
