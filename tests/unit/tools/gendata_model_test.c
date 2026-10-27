#include "gendata/model.h"

#include <stdlib.h>
#include <string.h>

#include "common/bt_harness.h"
#include "common/strbuf.h"

static bt_gd_vendor_alg vendor_alg_fixture(const char *solver, size_t rowid) {
    bt_gd_vendor_alg alg = {
        .solver = _strdup(solver),
        .rowid = rowid,
        .position = _strdup("UF-UB-UR"),
        .alg = _strdup("R2 U' S R2 S' R2 U R2"),
        .qtm = 12,
        .htm = 9,
        .ok = true,
        .problems = NULL,
        .problem_count = 0,
        .notes = NULL,
        .note_count = 0,
        .commutator = _strdup("R2 U':[S,R2]"),
        .recon_link = _strdup("https://re.con"),
        .youtube_link = _strdup("https://youtu.be/x"),
    };
    alg.problem_count = 1;
    alg.problems = malloc(sizeof(char *));
    alg.problems[0] = _strdup("centers moved");
    alg.note_count = 1;
    alg.notes = malloc(sizeof(char *));
    alg.notes[0] = _strdup("flip:UF");
    return alg;
}

static bt_gd_special_entry special_fixture(const char *solver, const char *class_name,
                                           size_t rowid) {
    return (bt_gd_special_entry){
        .class_name = _strdup(class_name),
        .solver = _strdup(solver),
        .rowid = rowid,
        .position = _strdup("UF & UB"),
        .alg = _strdup("M2 U M2 U2 M2 U M2"),
        .qtm = 14,
        .htm = 7,
        .ok = false,
        .problems = NULL,
        .problem_count = 0,
        .notes = NULL,
        .note_count = 0,
        .commutator = _strdup(""),
        .recon_link = _strdup(""),
        .youtube_link = _strdup(""),
    };
}

BT_TEST(gendata_model, vendor_alg_round_trips_and_keeps_its_field_names) {
    bt_gd_vendor_alg alg = vendor_alg_fixture("cherry", 4);
    bt_json_value *value = bt_gd_vendor_alg_to_json(&alg);
    BT_CHECK(value != NULL);
    bt_strbuf text;
    bt_strbuf_init(&text, 0);
    BT_CHECK_EQ(bt_gd_json_pretty(value, &text), 0);
    BT_CHECK_EQ(strcmp(text.data, "{\n"
                                  "  \"solver\": \"cherry\",\n"
                                  "  \"rowid\": 4,\n"
                                  "  \"position\": \"UF-UB-UR\",\n"
                                  "  \"alg\": \"R2 U' S R2 S' R2 U R2\",\n"
                                  "  \"qtm\": 12,\n"
                                  "  \"htm\": 9,\n"
                                  "  \"ok\": true,\n"
                                  "  \"problems\": [\n"
                                  "    \"centers moved\"\n"
                                  "  ],\n"
                                  "  \"notes\": [\n"
                                  "    \"flip:UF\"\n"
                                  "  ],\n"
                                  "  \"commutator\": \"R2 U':[S,R2]\",\n"
                                  "  \"recon_link\": \"https://re.con\",\n"
                                  "  \"youtube_link\": \"https://youtu.be/x\"\n"
                                  "}"),
                0);
    bt_gd_vendor_alg back;
    BT_CHECK_EQ(bt_gd_vendor_alg_from_json(value, &back, NULL, 0), 0);
    BT_CHECK(bt_gd_vendor_alg_eq(&alg, &back));
    bt_gd_vendor_alg_free(&alg);
    bt_gd_vendor_alg_free(&back);
    bt_strbuf_free(&text);
    bt_json_free(value);
}

BT_TEST(gendata_model, case_entry_and_files_round_trip) {
    bt_gd_case_entry entry = {
        .orbit = _strdup("edge"),
        .t1 = 24,
        .t2 = 25,
        .t1_letter = 'A',
        .t2_letter = 'B',
        .t1_name = _strdup("UB"),
        .t2_name = _strdup("UR"),
        .covered = true,
        .vendor_algs = malloc(sizeof(bt_gd_vendor_alg)),
        .vendor_count = 1,
    };
    entry.vendor_algs[0] = vendor_alg_fixture("cherry", 2);
    bt_gd_cases_file file = {.cases = malloc(sizeof(bt_gd_case_entry)), .count = 1};
    file.cases[0] = entry;
    bt_json_value *value = bt_gd_cases_file_to_json(&file);
    BT_CHECK(value != NULL);
    bt_strbuf text;
    bt_strbuf_init(&text, 0);
    BT_CHECK_EQ(bt_gd_json_pretty(value, &text), 0);
    bt_json_error jerr;
    bt_json_value *parsed = NULL;
    BT_CHECK_EQ(bt_json_parse(text.data, text.len, &parsed, &jerr), 0);
    bt_gd_cases_file back;
    BT_CHECK_EQ(bt_gd_cases_file_from_json(parsed, &back, NULL, 0), 0);
    BT_CHECK(bt_gd_cases_file_eq(&file, &back));
    bt_gd_cases_file_free(&file);
    bt_gd_cases_file_free(&back);
    bt_strbuf_free(&text);
    bt_json_free(value);
    bt_json_free(parsed);
}

BT_TEST(gendata_model, specials_file_round_trips) {
    bt_gd_specials_file file = {.specials = malloc(sizeof(bt_gd_special_entry)), .count = 1};
    file.specials[0] = special_fixture("eggins", "twists", 3);
    bt_json_value *value = bt_gd_specials_file_to_json(&file);
    BT_CHECK(value != NULL);
    bt_strbuf text;
    bt_strbuf_init(&text, 0);
    BT_CHECK_EQ(bt_gd_json_pretty(value, &text), 0);
    bt_json_error jerr;
    bt_json_value *parsed = NULL;
    BT_CHECK_EQ(bt_json_parse(text.data, text.len, &parsed, &jerr), 0);
    bt_gd_specials_file back;
    BT_CHECK_EQ(bt_gd_specials_file_from_json(parsed, &back, NULL, 0), 0);
    BT_CHECK(bt_gd_specials_file_eq(&file, &back));
    bt_gd_specials_file_free(&file);
    bt_gd_specials_file_free(&back);
    bt_strbuf_free(&text);
    bt_json_free(value);
    bt_json_free(parsed);
}

static bt_gd_manifest manifest_fixture(void) {
    bt_gd_manifest manifest = {0};
    manifest.generated_at = _strdup("2026-10-14T00:00:00Z");
    manifest.generator = _strdup("bt-gendata");
    manifest.synth_params_sha256 = malloc(65);
    for (int i = 0; i < 32; i++) {
        memcpy(manifest.synth_params_sha256 + i * 2, "34", 2);
    }
    manifest.synth_params_sha256[64] = '\0';
    manifest.corpus_sha256 = malloc(65);
    for (int i = 0; i < 32; i++) {
        memcpy(manifest.corpus_sha256 + i * 2, "ab", 2);
    }
    manifest.corpus_sha256[64] = '\0';
    manifest.corpus_rows = 1077;
    manifest.corpus_file_count = 1;
    manifest.corpus_files = malloc(sizeof(bt_gd_corpus_file_entry));
    manifest.corpus_files[0].solver = _strdup("cherry");
    manifest.corpus_files[0].sheet = _strdup("edge");
    manifest.corpus_files[0].rows = 551;
    manifest.corpus_files[0].sha256 = malloc(65);
    for (int i = 0; i < 32; i++) {
        memcpy(manifest.corpus_files[0].sha256 + i * 2, "cd", 2);
    }
    manifest.corpus_files[0].sha256[64] = '\0';
    manifest.rows_in_pair_space = 928;
    manifest.rows_other_buffer = 60;
    manifest.cases.path = _strdup("cases.json");
    manifest.cases.sha256 = malloc(65);
    for (int i = 0; i < 32; i++) {
        memcpy(manifest.cases.sha256 + i * 2, "ef", 2);
    }
    manifest.cases.sha256[64] = '\0';
    manifest.cases.entries = 818;
    manifest.cases.edge = (bt_gd_orbit_coverage){440, 372, 68};
    manifest.cases.corner = (bt_gd_orbit_coverage){378, 288, 90};
    manifest.specials.path = _strdup("specials.json");
    manifest.specials.sha256 = malloc(65);
    for (int i = 0; i < 32; i++) {
        memcpy(manifest.specials.sha256 + i * 2, "12", 2);
    }
    manifest.specials.sha256[64] = '\0';
    manifest.specials.entries = 89;
    manifest.specials.count_count = 2;
    manifest.specials.counts = malloc(2 * sizeof(bt_gd_class_count));
    manifest.specials.counts[0].solver = _strdup("cherry");
    manifest.specials.counts[0].class_name = _strdup("flips");
    manifest.specials.counts[0].rows = 25;
    manifest.specials.counts[1].solver = _strdup("eggins");
    manifest.specials.counts[1].class_name = _strdup("flips");
    manifest.specials.counts[1].rows = 0;
    return manifest;
}

BT_TEST(gendata_model, manifest_round_trips_with_coverage_and_counts) {
    bt_gd_manifest manifest = manifest_fixture();
    bt_json_value *value = bt_gd_manifest_to_json(&manifest);
    BT_CHECK(value != NULL);
    bt_gd_manifest back;
    BT_CHECK_EQ(bt_gd_manifest_from_json(value, &back, NULL, 0), 0);
    BT_CHECK(bt_gd_manifest_eq(&manifest, &back));
    bt_gd_manifest_free(&manifest);
    bt_gd_manifest_free(&back);
    bt_json_free(value);
}

BT_TEST(gendata_model, manifest_without_synth_params_hash_is_rejected) {
    bt_gd_manifest manifest = manifest_fixture();
    bt_json_value *value = bt_gd_manifest_to_json(&manifest);
    BT_CHECK(value != NULL);
    size_t keep = 0;
    for (size_t i = 0; i < value->as.object.count; i++) {
        if (strcmp(value->as.object.keys[i], "synth_params_sha256") == 0) {
            continue;
        }
        value->as.object.keys[keep] = value->as.object.keys[i];
        value->as.object.values[keep] = value->as.object.values[i];
        keep++;
    }
    value->as.object.count = keep;
    bt_gd_manifest back;
    char err[96];
    err[0] = '\0';
    BT_CHECK(bt_gd_manifest_from_json(value, &back, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "synth_params_sha256") != NULL);
    bt_gd_manifest_free(&manifest);
    bt_json_free(value);
}
