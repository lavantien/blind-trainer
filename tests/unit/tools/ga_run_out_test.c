#include "ga-run/lib.h"
#include "ga-run/out.h"

#include "common/bt_harness.h"
#include "common/json.h"
#include "common/strbuf.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bt_json_value *parse_json(const char *text) {
    bt_json_value *value = NULL;
    bt_json_error error;
    if (bt_json_parse(text, strlen(text), &value, &error) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "fixture json parse failed: %s", error.message);
        return NULL;
    }
    return value;
}

static void fixture_report(bt_gr_ga_report *report) {
    memset(report, 0, sizeof *report);
    report->run_config = parse_json("{\"ga\": {\"seed\": 11}}");
    report->inventory.generated = 100;
    report->inventory.kept = 10;
    report->inventory.dropped_not_in_wordlist = 90;
    report->inventory.wordlist_size = 10;
    report->inventory.wordlist_covered = 9;
    report->pool_size = 15;
    snprintf(report->pool_top[0].text, BT_GA_TEXT_MAX, "ma");
    report->pool_top[0].frequency = 500;
    report->pool_top_count = 1;
    report->best_fitness = 0.654321F;
    report->best_parts[0] = 0.1F;
    report->best_parts[1] = 0.2F;
    report->best_parts[2] = 0.3F;
    report->best_generation = 4;
    report->generations_run = 7;
    float per_gen[2] = {0.5F, 0.25F};
    report->per_gen_best = malloc(sizeof per_gen);
    memcpy(report->per_gen_best, per_gen, sizeof per_gen);
    report->per_gen_count = 2;
    report->stagnation_tail = 3;
    report->max_stagnation = 9;
    report->evaluations = 42;
    report->eval_used_cuda = 0;
    memset(report->freq_sha256, 'f', 64);
    memset(report->syllables_sha256, 's', 64);
}

static void fixture_report_free(bt_gr_ga_report *report) {
    bt_json_free((bt_json_value *)report->run_config);
    free(report->per_gen_best);
    memset(report, 0, sizeof *report);
}

static void sha_line(char fill, char *out, size_t cap) {
    char sha[BT_GA_SHA256_HEX_LEN];
    memset(sha, fill, BT_GA_SHA256_HEX_LEN - 1);
    sha[BT_GA_SHA256_HEX_LEN - 1] = '\0';
    snprintf(out, cap, "\"%s\"", sha);
}

BT_TEST(ga_run_out, report_text_serializes_the_full_schema) {
    bt_gr_ga_report report;
    fixture_report(&report);
    bt_strbuf text;
    bt_strbuf_init(&text, 1024);
    BT_CHECK_EQ(bt_gr_report_text(&report, &text), 0);
    char freq_line[BT_GA_SHA256_HEX_LEN + 8];
    char syllables_line[BT_GA_SHA256_HEX_LEN + 8];
    sha_line('f', freq_line, sizeof freq_line);
    sha_line('s', syllables_line, sizeof syllables_line);
    bt_strbuf want;
    bt_strbuf_init(&want, 1024);
    bt_strbuf_append_str(
        &want,
        "{\n"
        "  \"run_config\": {\n"
        "    \"ga\": {\n"
        "      \"seed\": 11\n"
        "    }\n"
        "  },\n"
        "  \"inventory\": {\n"
        "    \"generated\": 100,\n"
        "    \"kept\": 10,\n"
        "    \"dropped_not_in_wordlist\": 90,\n"
        "    \"wordlist_size\": 10,\n"
        "    \"wordlist_covered\": 9\n"
        "  },\n"
        "  \"pool_size\": 15,\n"
        "  \"pool_top\": [\n"
        "    [\n"
        "      \"ma\",\n"
        "      500\n"
        "    ]\n"
        "  ],\n"
        "  \"best_fitness\": 0.654321,\n"
        "  \"best_parts\": [\n"
        "    0.1,\n"
        "    0.2,\n"
        "    0.3\n"
        "  ],\n"
        "  \"best_generation\": 4,\n"
        "  \"generations_run\": 7,\n"
        "  \"per_gen_best\": [\n"
        "    0.5,\n"
        "    0.25\n"
        "  ],\n"
        "  \"stagnation_tail\": 3,\n"
        "  \"max_stagnation\": 9,\n"
        "  \"evaluations\": 42,\n"
        "  \"eval_used_cuda\": 0,\n");
    bt_strbuf_appendf(&want, "  \"freq_sha256\": %s,\n", freq_line);
    bt_strbuf_appendf(&want, "  \"syllables_sha256\": %s\n", syllables_line);
    bt_strbuf_append_str(&want, "}");
    BT_CHECK_EQ(text.len, want.len);
    BT_CHECK(memcmp(text.data, want.data, text.len) == 0);
    bt_strbuf_free(&want);
    bt_strbuf_free(&text);
    fixture_report_free(&report);
}

BT_TEST(ga_run_out, report_text_renders_empty_pool_and_trajectory) {
    bt_gr_ga_report report;
    fixture_report(&report);
    report.pool_top_count = 0;
    report.per_gen_count = 0;
    bt_strbuf text;
    bt_strbuf_init(&text, 1024);
    BT_CHECK_EQ(bt_gr_report_text(&report, &text), 0);
    BT_CHECK(strstr(text.data, "\"pool_top\": [],") != NULL);
    BT_CHECK(strstr(text.data, "\"per_gen_best\": [],") != NULL);
    bt_strbuf_free(&text);
    fixture_report_free(&report);
}

BT_TEST(ga_run_out, report_text_rejects_nonfinite_floats) {
    bt_gr_ga_report report;
    fixture_report(&report);
    report.best_fitness = NAN;
    bt_strbuf text;
    bt_strbuf_init(&text, 1024);
    BT_CHECK_EQ(bt_gr_report_text(&report, &text), -1);
    bt_strbuf_free(&text);
    fixture_report_free(&report);
    fixture_report(&report);
    report.best_parts[1] = INFINITY;
    bt_strbuf_init(&text, 1024);
    BT_CHECK_EQ(bt_gr_report_text(&report, &text), -1);
    bt_strbuf_free(&text);
    fixture_report_free(&report);
}

BT_TEST(ga_run_out, assignment_text_prints_two_cases_pretty) {
    bt_gr_syllable_case cases[2];
    memset(cases, 0, sizeof cases);
    snprintf(cases[0].orbit, sizeof cases[0].orbit, "edge");
    snprintf(cases[0].letters, sizeof cases[0].letters, "AB");
    snprintf(cases[0].syllable, sizeof cases[0].syllable, "ma");
    cases[0].frequency = 500;
    snprintf(cases[1].orbit, sizeof cases[1].orbit, "corner");
    snprintf(cases[1].letters, sizeof cases[1].letters, "QT");
    snprintf(cases[1].syllable, sizeof cases[1].syllable, "khong");
    cases[1].frequency = 7;
    bt_gr_syllable_assignment assignment = {0};
    assignment.cases = cases;
    assignment.cases_count = 2;
    assignment.pool_size = 2;
    assignment.seed = 77;
    assignment.best_fitness = 0.5F;
    memset(assignment.freq_sha256, 'f', 64);
    memset(assignment.syllables_sha256, 's', 64);
    bt_strbuf text;
    bt_strbuf_init(&text, 512);
    BT_CHECK_EQ(bt_gr_assignment_text(&assignment, &text), 0);
    char freq_line[BT_GA_SHA256_HEX_LEN + 8];
    char syllables_line[BT_GA_SHA256_HEX_LEN + 8];
    sha_line('f', freq_line, sizeof freq_line);
    sha_line('s', syllables_line, sizeof syllables_line);
    bt_strbuf want;
    bt_strbuf_init(&want, 512);
    bt_strbuf_append_str(
        &want,
        "{\n"
        "  \"cases\": [\n"
        "    {\n"
        "      \"orbit\": \"edge\",\n"
        "      \"letters\": \"AB\",\n"
        "      \"syllable\": \"ma\",\n"
        "      \"frequency\": 500\n"
        "    },\n"
        "    {\n"
        "      \"orbit\": \"corner\",\n"
        "      \"letters\": \"QT\",\n"
        "      \"syllable\": \"khong\",\n"
        "      \"frequency\": 7\n"
        "    }\n"
        "  ],\n"
        "  \"pool_size\": 2,\n"
        "  \"seed\": 77,\n"
        "  \"best_fitness\": 0.5,\n");
    bt_strbuf_appendf(&want, "  \"freq_sha256\": %s,\n", freq_line);
    bt_strbuf_appendf(&want, "  \"syllables_sha256\": %s\n", syllables_line);
    bt_strbuf_append_str(&want, "}");
    BT_CHECK_EQ(text.len, want.len);
    BT_CHECK(memcmp(text.data, want.data, text.len) == 0);
    bt_strbuf_free(&want);
    bt_strbuf_free(&text);
}

BT_TEST(ga_run_out, assignment_text_rejects_a_nonfinite_best_fitness) {
    bt_gr_syllable_assignment assignment = {0};
    assignment.best_fitness = NAN;
    bt_strbuf text;
    bt_strbuf_init(&text, 128);
    BT_CHECK_EQ(bt_gr_assignment_text(&assignment, &text), -1);
    bt_strbuf_free(&text);
}

BT_TEST(ga_run_out, cases_text_compact_prints_an_empty_list) {
    bt_strbuf text;
    bt_strbuf_init(&text, 64);
    BT_CHECK_EQ(bt_gr_cases_text_compact(NULL, 0, &text), 0);
    BT_CHECK_EQ(text.data, "[]");
    bt_strbuf_free(&text);
}

BT_TEST(ga_run_out, display_f64_covers_the_plain_forms) {
    const struct {
        double value;
        const char *want;
    } rows[] = {
        {0.0, "0"},
        {12.0, "12"},
        {-2.5, "-2.5"},
        {0.15, "0.15"},
        {1e-5, "0.00001"},
        {1e-7, "0.0000001"},
        {1e15, "1000000000000000"},
    };
    for (size_t i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        bt_strbuf text;
        bt_strbuf_init(&text, 64);
        if (bt_gr_display_f64(&text, rows[i].value) != 0) {
            bt_harness_fail(__FILE__, __LINE__, "display of %g failed", rows[i].value);
        }
        BT_CHECK_EQ(text.data, rows[i].want);
        bt_strbuf_free(&text);
    }
    bt_strbuf text;
    bt_strbuf_init(&text, 256);
    BT_CHECK_EQ(bt_gr_display_f64(&text, 1e100), 0);
    BT_CHECK_EQ(text.len, (size_t)101);
    BT_CHECK_EQ(text.data[0], '1');
    for (size_t i = 1; i < text.len; i++) {
        if (text.data[i] != '0') {
            bt_harness_fail(__FILE__, __LINE__, "digit %zu of 1e100 is %c", i, text.data[i]);
        }
    }
    bt_strbuf_free(&text);
    bt_strbuf_init(&text, 64);
    BT_CHECK_EQ(bt_gr_display_f64(&text, NAN), -1);
    bt_strbuf_free(&text);
    bt_strbuf_init(&text, 64);
    BT_CHECK_EQ(bt_gr_display_f64(&text, -INFINITY), -1);
    bt_strbuf_free(&text);
}

BT_TEST(ga_run_out, json_value_text_covers_the_remaining_shapes) {
    bt_json_value *value = parse_json(
        "{\"arr\": [], \"obj\": {}, \"neg_sci\": -1e-7, \"zero\": 0.0, \"neg_fixed\": -12.5, "
        "\"f15\": 1e15, \"q\": \"a\\\"b\", \"bs\": \"c\\\\d\", \"ctl\": \"e\\u0001f\", "
        "\"nest\": {\"in\": [1, [2, 3], {\"deep\": true}]}}");
    bt_strbuf text;
    bt_strbuf_init(&text, 512);
    BT_CHECK_EQ(bt_gr_json_value_text(value, &text), 0);
    const char *want =
        "{\n"
        "  \"arr\": [],\n"
        "  \"bs\": \"c\\\\d\",\n"
        "  \"ctl\": \"e\\u0001f\",\n"
        "  \"f15\": 1000000000000000.0,\n"
        "  \"neg_fixed\": -12.5,\n"
        "  \"neg_sci\": -1e-07,\n"
        "  \"nest\": {\n"
        "    \"in\": [\n"
        "      1,\n"
        "      [\n"
        "        2,\n"
        "        3\n"
        "      ],\n"
        "      {\n"
        "        \"deep\": true\n"
        "      }\n"
        "    ]\n"
        "  },\n"
        "  \"obj\": {},\n"
        "  \"q\": \"a\\\"b\",\n"
        "  \"zero\": 0.0\n"
        "}";
    BT_CHECK_EQ(text.data, want);
    bt_strbuf_free(&text);
    bt_json_free(value);
}

BT_TEST(ga_run_out, json_value_text_sorts_a_wide_object_on_the_heap) {
    bt_strbuf input;
    bt_strbuf_init(&input, 512);
    bt_strbuf_append_str(&input, "{");
    for (int i = 16; i >= 0; i--) {
        bt_strbuf_appendf(&input, "%s\"k%02d\": %d", i == 16 ? "" : ",", i, i);
    }
    bt_strbuf_append_str(&input, "}");
    bt_json_value *value = parse_json(input.data);
    bt_strbuf_free(&input);
    bt_strbuf text;
    bt_strbuf_init(&text, 512);
    BT_CHECK_EQ(bt_gr_json_value_text(value, &text), 0);
    bt_strbuf want;
    bt_strbuf_init(&want, 512);
    bt_strbuf_append_str(&want, "{\n");
    for (int i = 0; i <= 16; i++) {
        bt_strbuf_appendf(&want, "  \"k%02d\": %d%s\n", i, i, i == 16 ? "" : ",");
    }
    bt_strbuf_append_str(&want, "}");
    BT_CHECK_EQ(text.len, want.len);
    BT_CHECK(memcmp(text.data, want.data, text.len) == 0);
    bt_strbuf_free(&want);
    bt_strbuf_free(&text);
    bt_json_free(value);
}

BT_TEST(ga_run_out, json_value_text_rejects_a_null_value) {
    bt_strbuf text;
    bt_strbuf_init(&text, 64);
    BT_CHECK_EQ(bt_gr_json_value_text(NULL, &text), -1);
    bt_strbuf_free(&text);
}
