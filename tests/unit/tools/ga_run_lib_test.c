#include "ga-run/lib.h"
#include "ga-run/out.h"

#include "common/bt_harness.h"
#include "common/config.h"
#include "common/paths.h"
#include "data/paths.h"
#include "ga/dist.h"
#include "ga/hash.h"
#include "ga/inventory.h"
#include "scheme/pairs.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void synth_inputs(size_t pool_size, bt_gr_run_inputs *inputs) {
    memset(inputs, 0, sizeof *inputs);
    inputs->pool = malloc(pool_size * sizeof *inputs->pool);
    for (size_t i = 0; i < pool_size; i++) {
        snprintf(inputs->pool[i].text, BT_GA_TEXT_MAX, "syllable-%zu", i);
        inputs->pool[i].frequency = (uint64_t)(pool_size - i);
    }
    inputs->pool_count = pool_size;
    bt_case edge[BT_PAIRS_MAX];
    bt_case corner[BT_PAIRS_MAX];
    size_t edge_count = bt_scheme_enumerate_cases(BT_ORBIT_EDGE, edge);
    size_t corner_count = bt_scheme_enumerate_cases(BT_ORBIT_CORNER, corner);
    size_t total = edge_count + corner_count;
    inputs->freq.cases = malloc(total * sizeof *inputs->freq.cases);
    for (size_t i = 0; i < total; i++) {
        const bt_case *case_id = i < edge_count ? &edge[i] : &corner[i - edge_count];
        char letters[2];
        bt_scheme_case_letters(case_id, letters);
        snprintf(inputs->freq.cases[i].orbit, sizeof inputs->freq.cases[i].orbit, "%s",
                 case_id->orbit == BT_ORBIT_EDGE ? "edge" : "corner");
        snprintf(inputs->freq.cases[i].letters, sizeof inputs->freq.cases[i].letters, "%c%c",
                 letters[0], letters[1]);
        inputs->freq.cases[i].count = 1000 - (uint64_t)i;
    }
    inputs->freq.cases_count = total;
    inputs->freq.total_draws = 1;
    inputs->freq.seed = 2;
    inputs->params.seed = 77;
    inputs->params.population = 12;
    inputs->params.generations = 3;
    inputs->params.elite = 2;
    inputs->params.swap = 0.2;
    inputs->params.replace = 0.1;
    inputs->params.w_conf = 0.5f;
    inputs->params.w_flow = 0.3f;
    inputs->params.w_cplx = 0.2f;
    inputs->env.pool_size = pool_size;
    inputs->env.case_count = bt_ga_case_count();
    inputs->env.lengths = malloc(pool_size * sizeof *inputs->env.lengths);
    for (size_t i = 0; i < pool_size; i++) {
        inputs->env.lengths[i] = 3.0f;
    }
    inputs->env.feats = NULL;
    inputs->env.conf_table = NULL;
    inputs->env.flow_table = NULL;
    inputs->env.cooc = NULL;
    inputs->env.cooc_count = 0;
    inputs->env.adj = NULL;
    inputs->env.adj_count = 0;
    inputs->inventory_report.generated = 100;
    inputs->inventory_report.kept = 10;
    inputs->inventory_report.dropped_not_in_wordlist = 90;
    inputs->inventory_report.wordlist_size = 10;
    inputs->inventory_report.wordlist_covered = 9;
    inputs->freq_bytes_len = strlen("synthetic freq fixture");
    inputs->freq_bytes = _strdup("synthetic freq fixture");
}

static void synth_inputs_free(bt_gr_run_inputs *inputs) {
    free(inputs->pool);
    free(inputs->freq.cases);
    free(inputs->env.lengths);
    free(inputs->freq_bytes);
    memset(inputs, 0, sizeof *inputs);
}

static void outcome_with_best(const uint32_t *best, size_t best_count, bt_ga_run_outcome *outcome) {
    memset(outcome, 0, sizeof *outcome);
    outcome->best = malloc((best_count > 0 ? best_count : 1) * sizeof *outcome->best);
    memcpy(outcome->best, best, best_count * sizeof *outcome->best);
    outcome->best_count = best_count;
    outcome->best_fitness = 0.75f;
    outcome->best_parts[0] = 0.1f;
    outcome->best_parts[1] = 0.2f;
    outcome->best_parts[2] = 0.3f;
    outcome->per_gen_best = malloc(sizeof *outcome->per_gen_best);
    outcome->per_gen_best[0] = 0.5f;
    outcome->per_gen_count = 1;
    outcome->best_generation = 2;
    outcome->stagnation_tail = 1;
    outcome->max_stagnation = 5;
    outcome->evaluations = 100;
}

static bt_json_value *parse_json(const char *text) {
    bt_json_value *value = NULL;
    bt_json_error error;
    if (bt_json_parse(text, strlen(text), &value, &error) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "fixture json parse failed: %s", error.message);
        return NULL;
    }
    return value;
}

static void fixture_report(bt_gr_ga_report *report, const float *per_gen, size_t per_gen_count) {
    memset(report, 0, sizeof *report);
    report->run_config =
        parse_json("{\"ga\": {\"seed\": 11, \"population\": 30, \"generations\": 7, \"elite\": 2, "
                   "\"weights\": {\"confusion\": 0.5, \"flow\": 0.3, \"complexity\": 0.2}, "
                   "\"mutation\": {\"swap\": 0.02, \"replace\": 0.01}}}");
    report->inventory.generated = 100;
    report->inventory.kept = 10;
    report->inventory.dropped_not_in_wordlist = 90;
    report->inventory.wordlist_size = 10;
    report->inventory.wordlist_covered = 9;
    report->pool_size = 15;
    snprintf(report->pool_top[0].text, BT_GA_TEXT_MAX, "ma");
    report->pool_top[0].frequency = 500;
    snprintf(report->pool_top[1].text, BT_GA_TEXT_MAX, "to");
    report->pool_top[1].frequency = 400;
    report->pool_top_count = 2;
    report->best_fitness = 0.654321f;
    report->best_parts[0] = 0.1f;
    report->best_parts[1] = 0.2f;
    report->best_parts[2] = 0.3f;
    report->best_generation = 4;
    report->generations_run = 7;
    report->per_gen_best =
        malloc((per_gen_count > 0 ? per_gen_count : 1) * sizeof *report->per_gen_best);
    for (size_t i = 0; i < per_gen_count; i++) {
        report->per_gen_best[i] = per_gen[i];
    }
    report->per_gen_count = per_gen_count;
    report->stagnation_tail = 3;
    report->max_stagnation = 9;
    report->evaluations = 42;
    memset(report->freq_sha256, 'f', 64);
    memset(report->syllables_sha256, 's', 64);
}

static void fixture_report_free(bt_gr_ga_report *report) {
    bt_json_free((bt_json_value *)report->run_config);
    free(report->per_gen_best);
    memset(report, 0, sizeof *report);
}

static void trajectory_of(const char *text, char *out, size_t out_len) {
    const char *start = strstr(text, "## trajectory\n\n");
    if (start == NULL) {
        snprintf(out, out_len, "?");
        return;
    }
    start += strlen("## trajectory\n\n");
    const char *end = strstr(start, "\n\n## artifacts");
    if (end == NULL) {
        snprintf(out, out_len, "?");
        return;
    }
    size_t len = (size_t)(end - start);
    if (len >= out_len) {
        len = out_len - 1;
    }
    memcpy(out, start, len);
    out[len] = '\0';
}

BT_TEST(ga_run_lib, crate_id_matches_package) {
    BT_CHECK_EQ(bt_gr_tool_id(), "bt-ga-run");
}

BT_TEST(ga_run_lib, cross_check_passes_on_generated_vendor_data) {
    bt_json_value *config = bt_data_load_config();
    BT_CHECK(config != NULL);
    bt_gr_offenders offenders = {0};
    int rc = bt_gr_cross_check_cases(config, &offenders);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(offenders.count, (size_t)0);
    bt_gr_offenders_free(&offenders);
    bt_json_free(config);
}

BT_TEST(ga_run_lib, end_to_end_small_run_produces_full_injective_assignment) {
    bt_gr_run_inputs inputs;
    bt_gr_offenders offenders = {0};
    BT_CHECK_EQ(bt_gr_load_run_inputs(&inputs, &offenders), 0);
    if (offenders.count > 0) {
        bt_harness_fail(__FILE__, __LINE__, "load_run_inputs: %s", offenders.items[0]);
    }
    bt_gr_offenders_free(&offenders);
    inputs.params.population = 30;
    inputs.params.generations = 20;
    bt_ga_run_outcome *outcome = bt_ga_run(&inputs.env, &inputs.params);
    BT_CHECK(outcome != NULL);
    bt_gr_syllable_assignment assignment;
    bt_gr_offenders assignment_offenders = {0};
    BT_CHECK_EQ(bt_gr_build_assignment(&inputs, outcome, &assignment, &assignment_offenders), 0);
    BT_CHECK_EQ(assignment_offenders.count, (size_t)0);
    bt_gr_offenders_free(&assignment_offenders);
    BT_CHECK_EQ(assignment.cases_count, bt_ga_case_count());
    for (size_t i = 0; i < assignment.cases_count; i++) {
        for (size_t j = i + 1; j < assignment.cases_count; j++) {
            if (strcmp(assignment.cases[i].syllable, assignment.cases[j].syllable) == 0) {
                bt_harness_fail(__FILE__, __LINE__, "duplicate syllable %s",
                                assignment.cases[i].syllable);
            }
        }
    }
    BT_CHECK(assignment.pool_size >= bt_ga_case_count());
    BT_CHECK_EQ(strlen(assignment.freq_sha256), (size_t)64);
    bt_gr_assignment_free(&assignment);
    bt_ga_run_outcome_free(outcome);
    bt_gr_run_inputs_free(&inputs);
}

BT_TEST(ga_run_lib, config_path_keeps_absolute_paths_and_joins_relative_ones) {
    char out[BT_PATH_MAX];
    BT_CHECK_EQ(bt_gr_config_path("C:/fixtures/config.json", out), 0);
    BT_CHECK(strcmp(out, "C:/fixtures/config.json") == 0);
    BT_CHECK_EQ(bt_gr_config_path("config/config.json", out), 0);
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    char expected[BT_PATH_MAX];
    snprintf(expected, sizeof expected, "%s/config/config.json", root);
    BT_CHECK(strcmp(out, expected) == 0);
    BT_CHECK(strchr(out, '/') != NULL);
}

BT_TEST(ga_run_lib, build_assignment_maps_pool_texts_onto_scheme_cases) {
    bt_gr_run_inputs inputs;
    synth_inputs(900, &inputs);
    uint32_t identity[BT_PAIRS_MAX * 2];
    for (size_t i = 0; i < bt_ga_case_count(); i++) {
        identity[i] = (uint32_t)i;
    }
    bt_ga_run_outcome outcome;
    outcome_with_best(identity, bt_ga_case_count(), &outcome);
    bt_gr_syllable_assignment assignment;
    bt_gr_offenders offenders = {0};
    BT_CHECK_EQ(bt_gr_build_assignment(&inputs, &outcome, &assignment, &offenders), 0);
    BT_CHECK_EQ(offenders.count, (size_t)0);
    bt_gr_offenders_free(&offenders);
    BT_CHECK_EQ(assignment.cases_count, bt_ga_case_count());
    bt_case edge[BT_PAIRS_MAX];
    bt_case corner[BT_PAIRS_MAX];
    size_t edge_count = bt_scheme_enumerate_cases(BT_ORBIT_EDGE, edge);
    size_t corner_count = bt_scheme_enumerate_cases(BT_ORBIT_CORNER, corner);
    BT_CHECK_EQ(edge_count + corner_count, bt_ga_case_count());
    const bt_gr_syllable_case *first = &assignment.cases[0];
    BT_CHECK_EQ(first->orbit, "edge");
    BT_CHECK_EQ(first->letters, "AB");
    BT_CHECK_EQ(first->syllable, "syllable-0");
    BT_CHECK_EQ(first->frequency, (uint64_t)1000);
    const bt_gr_syllable_case *boundary = &assignment.cases[edge_count];
    BT_CHECK_EQ(boundary->orbit, "corner");
    BT_CHECK_EQ(boundary->letters, "AB");
    char boundary_syllable[BT_GA_TEXT_MAX];
    snprintf(boundary_syllable, sizeof boundary_syllable, "syllable-%zu", edge_count);
    BT_CHECK_EQ(boundary->syllable, boundary_syllable);
    BT_CHECK_EQ(boundary->frequency, (uint64_t)(1000 - edge_count));
    const bt_gr_syllable_case *last = &assignment.cases[assignment.cases_count - 1];
    BT_CHECK_EQ(last->orbit, "corner");
    BT_CHECK_EQ(last->letters, "XW");
    BT_CHECK_EQ(assignment.pool_size, (size_t)900);
    BT_CHECK_EQ(assignment.seed, (uint64_t)77);
    BT_CHECK_EQ(assignment.best_fitness, 0.75f);
    char want_freq_sha[BT_GA_SHA256_HEX_LEN];
    bt_ga_sha256_hex(inputs.freq_bytes, inputs.freq_bytes_len, want_freq_sha);
    BT_CHECK(strcmp(assignment.freq_sha256, want_freq_sha) == 0);
    bt_strbuf compact;
    bt_strbuf_init(&compact, 4096);
    BT_CHECK_EQ(bt_gr_cases_text_compact(assignment.cases, assignment.cases_count, &compact), 0);
    char want_syllables_sha[BT_GA_SHA256_HEX_LEN];
    bt_ga_sha256_hex(compact.data, compact.len, want_syllables_sha);
    BT_CHECK(strcmp(assignment.syllables_sha256, want_syllables_sha) == 0);
    bt_strbuf_free(&compact);
    for (size_t i = 0; i < assignment.cases_count; i++) {
        for (size_t j = i + 1; j < assignment.cases_count; j++) {
            if (strcmp(assignment.cases[i].syllable, assignment.cases[j].syllable) == 0) {
                bt_harness_fail(__FILE__, __LINE__, "duplicate syllable %s",
                                assignment.cases[i].syllable);
            }
        }
    }
    bt_gr_assignment_free(&assignment);
    free(outcome.best);
    free(outcome.per_gen_best);
    synth_inputs_free(&inputs);
}

BT_TEST(ga_run_lib, build_assignment_rejects_a_short_chromosome) {
    bt_gr_run_inputs inputs;
    synth_inputs(bt_ga_case_count(), &inputs);
    uint32_t short_best[5] = {0, 1, 2, 3, 4};
    bt_ga_run_outcome outcome;
    outcome_with_best(short_best, 5, &outcome);
    bt_gr_syllable_assignment assignment;
    bt_gr_offenders offenders = {0};
    BT_CHECK_EQ(bt_gr_build_assignment(&inputs, &outcome, &assignment, &offenders), -1);
    BT_CHECK_EQ(offenders.count, (size_t)1);
    BT_CHECK_EQ(offenders.items[0], "best chromosome has 5 genes, expected 818");
    bt_gr_offenders_free(&offenders);
    free(outcome.best);
    free(outcome.per_gen_best);
    synth_inputs_free(&inputs);
}

BT_TEST(ga_run_lib, build_assignment_rejects_a_duplicate_gene) {
    bt_gr_run_inputs inputs;
    synth_inputs(bt_ga_case_count(), &inputs);
    uint32_t best[BT_PAIRS_MAX * 2];
    for (size_t i = 0; i < bt_ga_case_count(); i++) {
        best[i] = (uint32_t)i;
    }
    best[100] = best[99];
    bt_ga_run_outcome outcome;
    outcome_with_best(best, bt_ga_case_count(), &outcome);
    bt_gr_syllable_assignment assignment;
    bt_gr_offenders offenders = {0};
    BT_CHECK_EQ(bt_gr_build_assignment(&inputs, &outcome, &assignment, &offenders), -1);
    BT_CHECK_EQ(offenders.count, (size_t)1);
    BT_CHECK_EQ(offenders.items[0], "duplicate pool index 99 in assignment");
    bt_gr_offenders_free(&offenders);
    free(outcome.best);
    free(outcome.per_gen_best);
    synth_inputs_free(&inputs);
}

BT_TEST(ga_run_lib, build_assignment_rejects_an_out_of_range_gene) {
    bt_gr_run_inputs inputs;
    synth_inputs(bt_ga_case_count(), &inputs);
    uint32_t best[BT_PAIRS_MAX * 2];
    for (size_t i = 0; i < bt_ga_case_count(); i++) {
        best[i] = (uint32_t)i;
    }
    best[0] = (uint32_t)bt_ga_case_count();
    bt_ga_run_outcome outcome;
    outcome_with_best(best, bt_ga_case_count(), &outcome);
    bt_gr_syllable_assignment assignment;
    bt_gr_offenders offenders = {0};
    BT_CHECK_EQ(bt_gr_build_assignment(&inputs, &outcome, &assignment, &offenders), -1);
    BT_CHECK_EQ(offenders.count, (size_t)1);
    BT_CHECK_EQ(offenders.items[0], "pool index 818 outside pool 818");
    bt_gr_offenders_free(&offenders);
    free(outcome.best);
    free(outcome.per_gen_best);
    synth_inputs_free(&inputs);
}

BT_TEST(ga_run_lib, build_assignment_rejects_a_duplicate_syllable) {
    bt_gr_run_inputs inputs;
    synth_inputs(bt_ga_case_count(), &inputs);
    snprintf(inputs.pool[1].text, BT_GA_TEXT_MAX, "%s", inputs.pool[0].text);
    uint32_t identity[BT_PAIRS_MAX * 2];
    for (size_t i = 0; i < bt_ga_case_count(); i++) {
        identity[i] = (uint32_t)i;
    }
    bt_ga_run_outcome outcome;
    outcome_with_best(identity, bt_ga_case_count(), &outcome);
    bt_gr_syllable_assignment assignment;
    bt_gr_offenders offenders = {0};
    BT_CHECK_EQ(bt_gr_build_assignment(&inputs, &outcome, &assignment, &offenders), -1);
    BT_CHECK_EQ(offenders.count, (size_t)1);
    BT_CHECK_EQ(offenders.items[0], "duplicate syllable syllable-0");
    bt_gr_offenders_free(&offenders);
    free(outcome.best);
    free(outcome.per_gen_best);
    synth_inputs_free(&inputs);
}

BT_TEST(ga_run_lib, build_report_copies_run_state_and_the_pool_head) {
    bt_gr_run_inputs inputs;
    synth_inputs(15, &inputs);
    inputs.params.generations = 7;
    uint32_t none[1] = {0};
    bt_ga_run_outcome outcome;
    outcome_with_best(none, 0, &outcome);
    bt_gr_syllable_assignment assignment = {0};
    assignment.pool_size = inputs.pool_count;
    assignment.seed = inputs.params.seed;
    assignment.best_fitness = outcome.best_fitness;
    memset(assignment.freq_sha256, 'f', 64);
    memset(assignment.syllables_sha256, 's', 64);
    bt_json_value *config = parse_json(
        "{\"ga\": {\"seed\": 11, \"population\": 30, \"generations\": 7, \"elite\": 2}}");
    bt_gr_ga_report report;
    bt_gr_build_report(&inputs, &outcome, &assignment, config, &report);
    BT_CHECK_EQ(report.pool_size, (size_t)15);
    BT_CHECK_EQ(report.pool_top_count, (size_t)10);
    BT_CHECK_EQ(report.pool_top[0].text, "syllable-0");
    BT_CHECK_EQ(report.pool_top[0].frequency, (uint64_t)15);
    BT_CHECK_EQ(report.pool_top[9].text, "syllable-9");
    BT_CHECK_EQ(report.pool_top[9].frequency, (uint64_t)6);
    BT_CHECK_EQ(report.best_fitness, 0.75f);
    BT_CHECK_EQ(report.best_parts[0], 0.1f);
    BT_CHECK_EQ(report.best_parts[1], 0.2f);
    BT_CHECK_EQ(report.best_parts[2], 0.3f);
    BT_CHECK_EQ(report.best_generation, (size_t)2);
    BT_CHECK_EQ(report.generations_run, (size_t)7);
    BT_CHECK_EQ(report.per_gen_count, (size_t)1);
    BT_CHECK_EQ(report.per_gen_best[0], 0.5f);
    BT_CHECK_EQ(report.stagnation_tail, (uint32_t)1);
    BT_CHECK_EQ(report.max_stagnation, (uint32_t)5);
    BT_CHECK_EQ(report.evaluations, (uint64_t)100);
    BT_CHECK(strcmp(report.freq_sha256, assignment.freq_sha256) == 0);
    BT_CHECK(strcmp(report.syllables_sha256, assignment.syllables_sha256) == 0);
    BT_CHECK(report.inventory.generated == inputs.inventory_report.generated &&
             report.inventory.kept == inputs.inventory_report.kept &&
             report.inventory.dropped_not_in_wordlist ==
                 inputs.inventory_report.dropped_not_in_wordlist &&
             report.inventory.wordlist_size == inputs.inventory_report.wordlist_size &&
             report.inventory.wordlist_covered == inputs.inventory_report.wordlist_covered);
    bt_strbuf got;
    bt_strbuf_init(&got, 256);
    bt_strbuf want;
    bt_strbuf_init(&want, 256);
    BT_CHECK_EQ(bt_gr_json_value_text(report.run_config, &got), 0);
    BT_CHECK_EQ(bt_gr_json_value_text(config, &want), 0);
    BT_CHECK(got.len == want.len && memcmp(got.data, want.data, got.len) == 0);
    bt_strbuf_free(&got);
    bt_strbuf_free(&want);
    bt_gr_report_free(&report);
    bt_json_free(config);
    free(outcome.best);
    free(outcome.per_gen_best);
    synth_inputs_free(&inputs);
}

BT_TEST(ga_run_lib, render_markdown_prints_config_inventory_pool_result_and_artifacts) {
    float per_gen[10];
    for (size_t i = 0; i < 10; i++) {
        per_gen[i] = (float)i / 100.0f;
    }
    bt_gr_ga_report report;
    fixture_report(&report, per_gen, 10);
    bt_strbuf md;
    bt_strbuf_init(&md, 1024);
    BT_CHECK_EQ(bt_gr_render_markdown(&report, &md), 0);
    const char *text = md.data;
    BT_CHECK(strncmp(text, "# ga run report\n\n## run config\n\n", 30) == 0);
    BT_CHECK(strstr(text, "seed 11 population 30 generations 7 elite 2 tournament 2 weights conf "
                          "0.5 flow 0.3 cplx 0.2 mutation swap 0.02 replace 0.01\n") != NULL);
    BT_CHECK(strstr(text, "generated 100 kept 10 dropped 90 wordlist 10 covered 9 (90.0%)\n") !=
             NULL);
    BT_CHECK(strstr(text, "size 15 top entries\n") != NULL);
    BT_CHECK(strstr(text, "| ma | 500 |\n| to | 400 |\n") != NULL);
    BT_CHECK(strstr(text, "best fitness 0.654321 at generation 4 of 7\n") != NULL);
    BT_CHECK(strstr(text, "parts conf 0.100000 flow 0.200000 cplx 0.300000 (weighted 0.050000 "
                          "0.060000 0.060000)\n") != NULL);
    BT_CHECK(strstr(text, "stagnation tail 3 max 9\nevaluations 42\n") != NULL);
    char trajectory[512];
    trajectory_of(text, trajectory, sizeof trajectory);
    BT_CHECK(strcmp(trajectory,
                    "0.000000 0.010000 0.020000 0.030000 0.040000 0.050000 0.060000 0.070000 "
                    "0.080000 0.090000") == 0);
    char sha_line[96];
    snprintf(sha_line, sizeof sha_line, "freq.json sha256 %s\n", report.freq_sha256);
    BT_CHECK(strstr(text, sha_line) != NULL);
    snprintf(sha_line, sizeof sha_line, "syllables.json sha256 %s\n", report.syllables_sha256);
    BT_CHECK(strstr(text, sha_line) != NULL);
    bt_strbuf_free(&md);
    fixture_report_free(&report);
}

BT_TEST(ga_run_lib, render_markdown_samples_long_trajectories_to_twenty_points) {
    float per_gen[40];
    for (size_t i = 0; i < 40; i++) {
        per_gen[i] = (float)i / 100.0f;
    }
    bt_gr_ga_report report;
    fixture_report(&report, per_gen, 40);
    bt_strbuf md;
    bt_strbuf_init(&md, 1024);
    BT_CHECK_EQ(bt_gr_render_markdown(&report, &md), 0);
    char trajectory[1024];
    trajectory_of(md.data, trajectory, sizeof trajectory);
    size_t points = 1;
    for (const char *cursor = trajectory; *cursor != '\0'; cursor++) {
        if (*cursor == ' ') {
            points++;
        }
    }
    BT_CHECK_EQ(points, (size_t)20);
    BT_CHECK(strncmp(trajectory, "0.000000", 8) == 0);
    BT_CHECK(strstr(trajectory, " 0.380000") != NULL);
    BT_CHECK(strstr(trajectory, "0.010000") == NULL);
    BT_CHECK(strstr(trajectory, "0.390000") == NULL);
    bt_strbuf_free(&md);
    fixture_report_free(&report);
}

BT_TEST(ga_run_lib, render_markdown_defaults_missing_config_sections_to_zero) {
    bt_gr_ga_report report;
    fixture_report(&report, NULL, 0);
    bt_json_value *empty = parse_json("{}");
    report.run_config = empty;
    bt_strbuf md;
    bt_strbuf_init(&md, 512);
    BT_CHECK_EQ(bt_gr_render_markdown(&report, &md), 0);
    BT_CHECK(strstr(md.data, "seed 0 population 0 generations 0 elite 0 tournament 2 weights conf "
                             "0 flow 0 cplx 0 mutation swap 0 replace 0\n") != NULL);
    BT_CHECK(strstr(md.data, "(weighted 0.000000 0.000000 0.000000)\n") != NULL);
    char trajectory[64];
    trajectory_of(md.data, trajectory, sizeof trajectory);
    BT_CHECK_EQ(trajectory[0], '\0');
    bt_strbuf_free(&md);
    bt_json_free(empty);
    report.run_config = NULL;
    fixture_report_free(&report);
}
