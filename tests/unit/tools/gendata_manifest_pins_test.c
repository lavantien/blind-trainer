#include "gendata/build.h"
#include "gendata/model.h"

#include <stdlib.h>
#include <string.h>

#include "common/bt_harness.h"
#include "common/json.h"
#include "common/paths.h"
#include "common/strbuf.h"
#include "data/corpus.h"
#include "data/digest.h"
#include "unit/tools/spawn_util.h"

static void ref_ground(char out[BT_PATH_MAX]) {
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    snprintf(out, BT_PATH_MAX, "%s/ref_ground", root);
}

static void loaded(bt_corpus *corpus) {
    char root[BT_PATH_MAX];
    ref_ground(root);
    bt_corpus_error err;
    if (bt_corpus_load(root, corpus, &err) != 0) {
        char text[BT_PATH_MAX + 96];
        bt_corpus_error_string(&err, text, sizeof text);
        BT_ABORT("%s", text);
    }
}

static void digest(bt_corpus_digest *out) {
    char root[BT_PATH_MAX];
    ref_ground(root);
    char err[192];
    if (bt_digest_corpus(root, out, err, sizeof err) != 0) {
        BT_ABORT("%s", err);
    }
}

static void stamped(const bt_gd_built *built, bt_gd_manifest *out) {
    bt_json_value *cases_value = bt_gd_cases_file_to_json(&built->cases);
    bt_json_value *specials_value = bt_gd_specials_file_to_json(&built->specials);
    bt_strbuf cases;
    bt_strbuf specials;
    bt_strbuf_init(&cases, 0);
    bt_strbuf_init(&specials, 0);
    if (bt_gd_json_pretty(cases_value, &cases) != 0 ||
        bt_gd_json_pretty(specials_value, &specials) != 0) {
        BT_ABORT("%s", "pretty serialization failed");
    }
    char cases_sha[65];
    char specials_sha[65];
    bt_digest_sha256_hex(cases.data, cases.len, cases_sha);
    bt_digest_sha256_hex(specials.data, specials.len, specials_sha);
    bt_corpus_digest dig;
    digest(&dig);
    bt_gd_manifest_make(built, &dig, cases_sha, specials_sha,
                        "b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0",
                        "2026-10-12T00:00:00Z", out);
    bt_strbuf_free(&cases);
    bt_strbuf_free(&specials);
    bt_json_free(cases_value);
    bt_json_free(specials_value);
}

BT_TEST(gendata_manifest_pins, manifest_reports_the_coverage_md_numbers) {
    bt_corpus corpus;
    loaded(&corpus);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    bt_gd_manifest record;
    stamped(&built, &record);
    BT_CHECK_EQ(strcmp(record.generated_at, "2026-10-12T00:00:00Z"), 0);
    BT_CHECK_EQ(strcmp(record.generator, "bt-gendata"), 0);
    BT_CHECK_EQ(record.corpus_rows, 1077U);
    BT_CHECK_EQ(record.corpus_file_count, 12U);
    BT_CHECK_EQ(record.rows_in_pair_space, 796U);
    BT_CHECK_EQ(record.rows_other_buffer, 192U);
    BT_CHECK_EQ(strcmp(record.cases.path, "cases.json"), 0);
    BT_CHECK_EQ(record.cases.entries, 818U);
    BT_CHECK_EQ(record.cases.edge.total, 440U);
    BT_CHECK_EQ(record.cases.edge.covered, 372U);
    BT_CHECK_EQ(record.cases.edge.uncovered, 68U);
    BT_CHECK_EQ(record.cases.corner.total, 378U);
    BT_CHECK_EQ(record.cases.corner.covered, 288U);
    BT_CHECK_EQ(record.cases.corner.uncovered, 90U);
    BT_CHECK_EQ(strcmp(record.specials.path, "specials.json"), 0);
    BT_CHECK_EQ(record.specials.entries, 89U);
    static const char *const want_solver[8] = {
        "cherry", "cherry", "cherry", "cherry", "eggins", "eggins", "eggins", "eggins",
    };
    static const char *const want_class[8] = {
        "flips", "twists", "parity", "ltct", "flips", "twists", "parity", "ltct",
    };
    static const size_t want_rows[8] = {25, 20, 26, 8, 0, 5, 2, 3};
    BT_CHECK_EQ(record.specials.count_count, 8U);
    for (size_t i = 0; i < 8; i++) {
        BT_CHECK_EQ(strcmp(record.specials.counts[i].solver, want_solver[i]), 0);
        BT_CHECK_EQ(strcmp(record.specials.counts[i].class_name, want_class[i]), 0);
        BT_CHECK_EQ(record.specials.counts[i].rows, want_rows[i]);
    }
    bt_gd_manifest_free(&record);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_manifest_pins, manifest_output_hashes_match_the_serialized_files) {
    bt_corpus corpus;
    loaded(&corpus);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    bt_json_value *cases_value = bt_gd_cases_file_to_json(&built.cases);
    bt_json_value *specials_value = bt_gd_specials_file_to_json(&built.specials);
    bt_strbuf cases;
    bt_strbuf specials;
    bt_strbuf_init(&cases, 0);
    bt_strbuf_init(&specials, 0);
    BT_CHECK_EQ(bt_gd_json_pretty(cases_value, &cases), 0);
    BT_CHECK_EQ(bt_gd_json_pretty(specials_value, &specials), 0);
    bt_gd_manifest record;
    stamped(&built, &record);
    char cases_sha[65];
    char specials_sha[65];
    bt_digest_sha256_hex(cases.data, cases.len, cases_sha);
    bt_digest_sha256_hex(specials.data, specials.len, specials_sha);
    BT_CHECK_EQ(strcmp(record.cases.sha256, cases_sha), 0);
    BT_CHECK_EQ(strcmp(record.specials.sha256, specials_sha), 0);
    BT_CHECK_EQ(strlen(record.cases.sha256), 64U);
    BT_CHECK(strcmp(record.cases.sha256, record.specials.sha256) != 0);
    bt_gd_manifest_free(&record);
    bt_strbuf_free(&cases);
    bt_strbuf_free(&specials);
    bt_json_free(cases_value);
    bt_json_free(specials_value);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_manifest_pins, output_files_round_trip_through_serde) {
    bt_corpus corpus;
    loaded(&corpus);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    bt_json_value *cases_value = bt_gd_cases_file_to_json(&built.cases);
    bt_json_value *specials_value = bt_gd_specials_file_to_json(&built.specials);
    bt_strbuf cases;
    bt_strbuf specials;
    bt_strbuf_init(&cases, 0);
    bt_strbuf_init(&specials, 0);
    BT_CHECK_EQ(bt_gd_json_pretty(cases_value, &cases), 0);
    BT_CHECK_EQ(bt_gd_json_pretty(specials_value, &specials), 0);
    bt_json_error jerr;
    bt_json_value *cases_parsed = NULL;
    bt_json_value *specials_parsed = NULL;
    BT_CHECK_EQ(bt_json_parse(cases.data, cases.len, &cases_parsed, &jerr), 0);
    BT_CHECK_EQ(bt_json_parse(specials.data, specials.len, &specials_parsed, &jerr), 0);
    bt_gd_cases_file cases_back;
    bt_gd_specials_file specials_back;
    BT_CHECK_EQ(bt_gd_cases_file_from_json(cases_parsed, &cases_back, NULL, 0), 0);
    BT_CHECK_EQ(bt_gd_specials_file_from_json(specials_parsed, &specials_back, NULL, 0), 0);
    BT_CHECK(bt_gd_cases_file_eq(&built.cases, &cases_back));
    BT_CHECK(bt_gd_specials_file_eq(&built.specials, &specials_back));
    BT_CHECK_EQ(strncmp(cases.data, "{\n  \"cases\": [", 14) == 0, 1);
    BT_CHECK_EQ(strncmp(specials.data, "{\n  \"specials\": [", 17) == 0, 1);
    bt_gd_cases_file_free(&cases_back);
    bt_gd_specials_file_free(&specials_back);
    bt_json_free(cases_parsed);
    bt_json_free(specials_parsed);
    bt_strbuf_free(&cases);
    bt_strbuf_free(&specials);
    bt_json_free(cases_value);
    bt_json_free(specials_value);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_manifest_pins, specials_carry_classes_links_and_solver_grouping) {
    bt_corpus corpus;
    loaded(&corpus);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    int solver_rank = 0;
    size_t with_links = 0;
    for (size_t i = 0; i < built.specials.count; i++) {
        const bt_gd_special_entry *entry = &built.specials.specials[i];
        int rank;
        if (strcmp(entry->solver, "cherry") == 0) {
            rank = 0;
        } else if (strcmp(entry->solver, "eggins") == 0) {
            rank = 1;
        } else {
            BT_ABORT("%s", "unknown solver in specials");
            return;
        }
        BT_CHECK(rank >= solver_rank);
        if (rank > solver_rank) {
            solver_rank = rank;
        }
        bool class_ok = false;
        if (strcmp(entry->class_name, "flips") == 0 || strcmp(entry->class_name, "twists") == 0 ||
            strcmp(entry->class_name, "parity") == 0 || strcmp(entry->class_name, "ltct") == 0) {
            class_ok = true;
        }
        BT_CHECK(class_ok);
        BT_CHECK(entry->rowid >= 2);
        if (entry->recon_link[0] != '\0') {
            with_links++;
        }
    }
    BT_CHECK(with_links > 0);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}
