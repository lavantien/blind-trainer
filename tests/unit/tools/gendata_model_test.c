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
    for (size_t i = 0; i < 32; i++) {
        memcpy(manifest.synth_params_sha256 + i * 2, "34", 2);
    }
    manifest.synth_params_sha256[64] = '\0';
    manifest.corpus_sha256 = malloc(65);
    for (size_t i = 0; i < 32; i++) {
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
    for (size_t i = 0; i < 32; i++) {
        memcpy(manifest.corpus_files[0].sha256 + i * 2, "cd", 2);
    }
    manifest.corpus_files[0].sha256[64] = '\0';
    manifest.rows_in_pair_space = 928;
    manifest.rows_other_buffer = 60;
    manifest.cases.path = _strdup("cases.json");
    manifest.cases.sha256 = malloc(65);
    for (size_t i = 0; i < 32; i++) {
        memcpy(manifest.cases.sha256 + i * 2, "ef", 2);
    }
    manifest.cases.sha256[64] = '\0';
    manifest.cases.entries = 818;
    manifest.cases.edge = (bt_gd_orbit_coverage){440, 372, 68};
    manifest.cases.corner = (bt_gd_orbit_coverage){378, 288, 90};
    manifest.specials.path = _strdup("specials.json");
    manifest.specials.sha256 = malloc(65);
    for (size_t i = 0; i < 32; i++) {
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

enum {
    FIELD_DROP,
    FIELD_STRING,
    FIELD_LONG_STRING,
    FIELD_NUMBER,
    FIELD_NEGATIVE,
    FIELD_BIG,
    FIELD_BOOL,
    FIELD_NUM_ARRAY,
};

typedef struct {
    const char *key;
    int kind;
    const char *expect;
} malformed_field;

static void drop_key(bt_json_value *object, const char *key) {
    bt_json_value *dropped = NULL;
    size_t keep = 0;
    for (size_t i = 0; i < object->as.object.count; i++) {
        if (strcmp(object->as.object.keys[i], key) == 0) {
            dropped = object->as.object.values[i];
            continue;
        }
        object->as.object.keys[keep] = object->as.object.keys[i];
        object->as.object.values[keep] = object->as.object.values[i];
        keep++;
    }
    object->as.object.count = keep;
    bt_json_free(dropped);
}

static void mutate_field(bt_json_value *object, const malformed_field *field) {
    switch (field->kind) {
    case FIELD_DROP:
        drop_key(object, field->key);
        break;
    case FIELD_STRING:
        bt_json_object_set(object, field->key, bt_json_new_string("x"));
        break;
    case FIELD_LONG_STRING:
        bt_json_object_set(object, field->key, bt_json_new_string("xy"));
        break;
    case FIELD_NUMBER:
        bt_json_object_set(object, field->key, bt_json_new_number(1.5));
        break;
    case FIELD_NEGATIVE:
        bt_json_object_set(object, field->key, bt_json_new_integer(-1));
        break;
    case FIELD_BIG:
        bt_json_object_set(object, field->key, bt_json_new_integer(4294967296LL));
        break;
    case FIELD_BOOL:
        bt_json_object_set(object, field->key, bt_json_new_bool(1));
        break;
    case FIELD_NUM_ARRAY: {
        bt_json_value *array = bt_json_new_array();
        bt_json_array_push(array, bt_json_new_integer(1));
        bt_json_object_set(object, field->key, array);
        break;
    }
    }
}

static bt_json_value *alg_json(void) {
    bt_gd_vendor_alg alg = vendor_alg_fixture("cherry", 4);
    bt_json_value *value = bt_gd_vendor_alg_to_json(&alg);
    bt_gd_vendor_alg_free(&alg);
    return value;
}

BT_TEST(gendata_model, vendor_alg_rejects_each_malformed_field) {
    static const malformed_field cases[] = {
        {"solver", FIELD_DROP, "solver"},
        {"solver", FIELD_NUMBER, "solver"},
        {"rowid", FIELD_DROP, "rowid"},
        {"rowid", FIELD_STRING, "rowid"},
        {"rowid", FIELD_NEGATIVE, "rowid"},
        {"position", FIELD_BOOL, "position"},
        {"alg", FIELD_NUMBER, "alg"},
        {"qtm", FIELD_DROP, "qtm"},
        {"qtm", FIELD_NEGATIVE, "qtm"},
        {"qtm", FIELD_BIG, "exceeds u32"},
        {"htm", FIELD_STRING, "htm"},
        {"ok", FIELD_DROP, "ok"},
        {"ok", FIELD_STRING, "ok"},
        {"problems", FIELD_NUMBER, "string array"},
        {"problems", FIELD_NUM_ARRAY, "string array"},
        {"notes", FIELD_STRING, "string array"},
        {"commutator", FIELD_DROP, "commutator"},
        {"recon_link", FIELD_NUMBER, "recon_link"},
        {"youtube_link", FIELD_DROP, "youtube_link"},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        bt_json_value *value = alg_json();
        mutate_field(value, &cases[i]);
        bt_gd_vendor_alg out;
        char err[96];
        err[0] = '\0';
        BT_CHECK(bt_gd_vendor_alg_from_json(value, &out, err, sizeof err) != 0);
        BT_CHECK(strstr(err, cases[i].expect) != NULL);
        bt_json_free(value);
    }
}

static bt_json_value *cases_json(void) {
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
    bt_gd_cases_file_free(&file);
    return value;
}

BT_TEST(gendata_model, cases_file_rejects_each_malformed_entry) {
    static const malformed_field cases[] = {
        {"cases", FIELD_DROP, "cases"},           {"cases", FIELD_NUMBER, "cases"},
        {"orbit", FIELD_NUMBER, "malformed"},     {"t1", FIELD_STRING, "malformed"},
        {"t1", FIELD_NEGATIVE, "malformed"},      {"t1_letter", FIELD_LONG_STRING, "malformed"},
        {"t1_letter", FIELD_NUMBER, "malformed"}, {"t2_letter", FIELD_LONG_STRING, "malformed"},
        {"covered", FIELD_STRING, "malformed"},   {"t1_name", FIELD_DROP, "malformed"},
        {"vendor_algs", FIELD_DROP, "malformed"}, {"vendor_algs", FIELD_STRING, "malformed"},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        bt_json_value *value = cases_json();
        if (strcmp(cases[i].key, "cases") == 0) {
            mutate_field(value, &cases[i]);
        } else {
            bt_json_value *entry = bt_json_at(bt_json_get(value, "cases"), 0);
            mutate_field(entry, &cases[i]);
        }
        bt_gd_cases_file out;
        char err[96];
        err[0] = '\0';
        BT_CHECK(bt_gd_cases_file_from_json(value, &out, err, sizeof err) != 0);
        BT_CHECK(strstr(err, cases[i].expect) != NULL);
        bt_json_free(value);
    }
}

BT_TEST(gendata_model, cases_file_breaks_inside_a_vendor_row_and_accepts_empty) {
    bt_json_value *value = cases_json();
    bt_json_value *entry = bt_json_at(bt_json_get(value, "cases"), 0);
    bt_json_value *vendor = bt_json_at(bt_json_get(entry, "vendor_algs"), 0);
    drop_key(vendor, "solver");
    bt_gd_cases_file out;
    char err[96];
    err[0] = '\0';
    BT_CHECK(bt_gd_cases_file_from_json(value, &out, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "solver") != NULL);
    bt_json_free(value);

    bt_json_value *empty = bt_json_new_object();
    bt_json_object_set(empty, "cases", bt_json_new_array());
    bt_gd_cases_file blank;
    BT_CHECK_EQ(bt_gd_cases_file_from_json(empty, &blank, NULL, 0), 0);
    BT_CHECK_EQ(blank.count, 0U);
    bt_gd_cases_file_free(&blank);
    bt_json_free(empty);
}

static bt_json_value *specials_json(void) {
    bt_gd_specials_file file = {.specials = malloc(sizeof(bt_gd_special_entry)), .count = 1};
    file.specials[0] = special_fixture("eggins", "twists", 3);
    bt_json_value *value = bt_gd_specials_file_to_json(&file);
    bt_gd_specials_file_free(&file);
    return value;
}

BT_TEST(gendata_model, specials_file_rejects_each_malformed_entry) {
    static const malformed_field cases[] = {
        {"specials", FIELD_DROP, "specials"},
        {"specials", FIELD_NUMBER, "specials"},
        {"class", FIELD_DROP, "class"},
        {"solver", FIELD_BOOL, "solver"},
        {"rowid", FIELD_NEGATIVE, "rowid"},
        {"qtm", FIELD_BIG, "exceeds u32"},
        {"ok", FIELD_STRING, "ok"},
        {"problems", FIELD_NUM_ARRAY, "string array"},
        {"notes", FIELD_NUMBER, "string array"},
        {"commutator", FIELD_DROP, "commutator"},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        bt_json_value *value = specials_json();
        if (strcmp(cases[i].key, "specials") == 0) {
            mutate_field(value, &cases[i]);
        } else {
            bt_json_value *entry = bt_json_at(bt_json_get(value, "specials"), 0);
            mutate_field(entry, &cases[i]);
        }
        bt_gd_specials_file out;
        char err[96];
        err[0] = '\0';
        BT_CHECK(bt_gd_specials_file_from_json(value, &out, err, sizeof err) != 0);
        BT_CHECK(strstr(err, cases[i].expect) != NULL);
        bt_json_free(value);
    }

    bt_json_value *empty = bt_json_new_object();
    bt_json_object_set(empty, "specials", bt_json_new_array());
    bt_gd_specials_file blank;
    BT_CHECK_EQ(bt_gd_specials_file_from_json(empty, &blank, NULL, 0), 0);
    BT_CHECK_EQ(blank.count, 0U);
    bt_gd_specials_file_free(&blank);
    bt_json_free(empty);
}

static bt_json_value *manifest_json(void) {
    bt_gd_manifest manifest = manifest_fixture();
    bt_json_value *value = bt_gd_manifest_to_json(&manifest);
    bt_gd_manifest_free(&manifest);
    return value;
}

BT_TEST(gendata_model, manifest_rejects_missing_sections_and_entries) {
    static const malformed_field top_cases[] = {
        {"corpus_files", FIELD_DROP, "manifest section"},
        {"corpus_files", FIELD_NUMBER, "manifest section"},
        {"cases", FIELD_DROP, "manifest section"},
        {"specials", FIELD_DROP, "manifest section"},
        {"generated_at", FIELD_NUMBER, "generated_at"},
        {"corpus_rows", FIELD_STRING, "corpus_rows"},
        {"rows_other_buffer", FIELD_NEGATIVE, "rows_other_buffer"},
    };
    for (size_t i = 0; i < sizeof top_cases / sizeof top_cases[0]; i++) {
        bt_json_value *value = manifest_json();
        mutate_field(value, &top_cases[i]);
        bt_gd_manifest out;
        char err[96];
        err[0] = '\0';
        BT_CHECK(bt_gd_manifest_from_json(value, &out, err, sizeof err) != 0);
        BT_CHECK(strstr(err, top_cases[i].expect) != NULL);
        bt_json_free(value);
    }

    static const malformed_field file_cases[] = {
        {"rows", FIELD_DROP, "rows"},
        {"solver", FIELD_NUMBER, "solver"},
    };
    for (size_t i = 0; i < sizeof file_cases / sizeof file_cases[0]; i++) {
        bt_json_value *value = manifest_json();
        bt_json_value *file = bt_json_at(bt_json_get(value, "corpus_files"), 0);
        mutate_field(file, &file_cases[i]);
        bt_gd_manifest out;
        char err[96];
        err[0] = '\0';
        BT_CHECK(bt_gd_manifest_from_json(value, &out, err, sizeof err) != 0);
        BT_CHECK(strstr(err, file_cases[i].expect) != NULL);
        bt_json_free(value);
    }

    struct {
        const char *section;
        malformed_field field;
    } section_cases[] = {
        {"cases", {"edge", FIELD_DROP, "total"}},
        {"cases", {"path", FIELD_DROP, "path"}},
        {"specials", {"counts", FIELD_NUMBER, "manifest section"}},
    };
    for (size_t i = 0; i < sizeof section_cases / sizeof section_cases[0]; i++) {
        bt_json_value *value = manifest_json();
        bt_json_value *section = bt_json_get(value, section_cases[i].section);
        mutate_field(section, &section_cases[i].field);
        bt_gd_manifest out;
        char err[96];
        err[0] = '\0';
        BT_CHECK(bt_gd_manifest_from_json(value, &out, err, sizeof err) != 0);
        BT_CHECK(strstr(err, section_cases[i].field.expect) != NULL);
        bt_json_free(value);
    }

    bt_json_value *value = manifest_json();
    bt_json_value *count = bt_json_at(bt_json_get(bt_json_get(value, "specials"), "counts"), 0);
    mutate_field(count, &(malformed_field){"solver", FIELD_NUMBER, "solver"});
    bt_gd_manifest out;
    char err[96];
    err[0] = '\0';
    BT_CHECK(bt_gd_manifest_from_json(value, &out, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "solver") != NULL);
    bt_json_free(value);
}

BT_TEST(gendata_model, pretty_prints_escapes_floats_and_scalars) {
    bt_json_value *root = bt_json_new_object();
    bt_json_object_set(root, "esc", bt_json_new_string("a\"b\\c\bd\fe\nf\rg\th\001i"));
    bt_json_object_set(root, "f05", bt_json_new_number(0.5));
    bt_json_object_set(root, "f125", bt_json_new_number(1.25));
    bt_json_object_set(root, "f100", bt_json_new_number(100.0));
    bt_json_object_set(root, "f1e300", bt_json_new_number(1e300));
    bt_json_object_set(root, "i5", bt_json_new_integer(5));
    bt_json_object_set(root, "nul", bt_json_new_null());
    bt_json_object_set(root, "no", bt_json_new_bool(0));
    bt_json_object_set(root, "yes", bt_json_new_bool(1));
    bt_strbuf text;
    bt_strbuf_init(&text, 0);
    BT_CHECK_EQ(bt_gd_json_pretty(root, &text), 0);
    BT_CHECK_EQ(strcmp(text.data, "{\n"
                                  "  \"esc\": \"a\\\"b\\\\c\\bd\\fe\\nf\\rg\\th\\u0001i\",\n"
                                  "  \"f05\": 0.5,\n"
                                  "  \"f125\": 1.25,\n"
                                  "  \"f100\": 1e+02,\n"
                                  "  \"f1e300\": 1e+300,\n"
                                  "  \"i5\": 5,\n"
                                  "  \"nul\": null,\n"
                                  "  \"no\": false,\n"
                                  "  \"yes\": true\n"
                                  "}"),
                0);
    bt_json_error jerr;
    bt_json_value *parsed = NULL;
    BT_CHECK_EQ(bt_json_parse(text.data, text.len, &parsed, &jerr), 0);
    bt_strbuf again;
    bt_strbuf_init(&again, 0);
    BT_CHECK_EQ(bt_gd_json_pretty(parsed, &again), 0);
    BT_CHECK_EQ(strcmp(text.data, again.data), 0);
    bt_strbuf_free(&again);
    bt_strbuf_free(&text);
    bt_json_free(parsed);
    bt_json_free(root);
}

BT_TEST(gendata_model, pretty_handles_empty_and_nested_containers) {
    bt_json_value *inner = bt_json_new_array();
    bt_json_array_push(inner, bt_json_new_integer(1));
    bt_json_array_push(inner, bt_json_new_integer(2));
    bt_json_value *single = bt_json_new_array();
    bt_json_array_push(single, bt_json_new_integer(3));
    bt_json_value *nested = bt_json_new_array();
    bt_json_array_push(nested, inner);
    bt_json_array_push(nested, single);
    bt_json_value *leaf = bt_json_new_object();
    bt_json_object_set(leaf, "b", bt_json_new_string("c"));
    bt_json_value *holder = bt_json_new_array();
    bt_json_array_push(holder, leaf);
    bt_json_value *deep = bt_json_new_object();
    bt_json_object_set(deep, "a", holder);
    bt_json_value *root = bt_json_new_object();
    bt_json_object_set(root, "empty_arr", bt_json_new_array());
    bt_json_object_set(root, "empty_obj", bt_json_new_object());
    bt_json_object_set(root, "nested", nested);
    bt_json_object_set(root, "deep", deep);
    bt_strbuf text;
    bt_strbuf_init(&text, 0);
    BT_CHECK_EQ(bt_gd_json_pretty(root, &text), 0);
    BT_CHECK_EQ(strcmp(text.data, "{\n"
                                  "  \"empty_arr\": [],\n"
                                  "  \"empty_obj\": {},\n"
                                  "  \"nested\": [\n"
                                  "    [\n"
                                  "      1,\n"
                                  "      2\n"
                                  "    ],\n"
                                  "    [\n"
                                  "      3\n"
                                  "    ]\n"
                                  "  ],\n"
                                  "  \"deep\": {\n"
                                  "    \"a\": [\n"
                                  "      {\n"
                                  "        \"b\": \"c\"\n"
                                  "      }\n"
                                  "    ]\n"
                                  "  }\n"
                                  "}"),
                0);
    bt_strbuf_free(&text);
    bt_json_free(root);
    bt_strbuf empty;
    bt_strbuf_init(&empty, 0);
    BT_CHECK_EQ(bt_gd_json_pretty(NULL, &empty), -1);
    bt_strbuf_free(&empty);
}

static void replace_string(char **field, const char *text) {
    free(*field);
    *field = _strdup(text);
}

BT_TEST(gendata_model, vendor_alg_eq_rejects_every_field_mismatch) {
    for (int variant = 0; variant < 12; variant++) {
        bt_gd_vendor_alg left = vendor_alg_fixture("cherry", 4);
        bt_json_value *value = bt_gd_vendor_alg_to_json(&left);
        bt_gd_vendor_alg right;
        BT_CHECK_EQ(bt_gd_vendor_alg_from_json(value, &right, NULL, 0), 0);
        switch (variant) {
        case 0:
            replace_string(&right.solver, "eggins");
            break;
        case 1:
            right.rowid += 1;
            break;
        case 2:
            replace_string(&right.position, "UF-UR-UB");
            break;
        case 3:
            replace_string(&right.alg, "R2 U R2");
            break;
        case 4:
            right.qtm += 1;
            break;
        case 5:
            right.htm += 1;
            break;
        case 6:
            right.ok = !right.ok;
            break;
        case 7:
            bt_gd_string_list_free(right.problems, right.problem_count);
            right.problems = NULL;
            right.problem_count = 0;
            break;
        case 8:
            replace_string(&right.notes[0], "twist:UFR:+1");
            break;
        case 9:
            replace_string(&right.commutator, "[R2,U]");
            break;
        case 10:
            replace_string(&right.recon_link, "https://other");
            break;
        case 11:
            replace_string(&right.youtube_link, "https://other");
            break;
        }
        BT_CHECK(!bt_gd_vendor_alg_eq(&left, &right));
        bt_gd_vendor_alg_free(&left);
        bt_gd_vendor_alg_free(&right);
        bt_json_free(value);
    }
}

BT_TEST(gendata_model, cases_file_eq_rejects_entry_and_vendor_mismatch) {
    for (int variant = 0; variant < 10; variant++) {
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
        bt_gd_cases_file left = {.cases = malloc(sizeof(bt_gd_case_entry)), .count = 1};
        left.cases[0] = entry;
        bt_json_value *value = bt_gd_cases_file_to_json(&left);
        bt_json_error jerr;
        bt_json_value *parsed = NULL;
        bt_strbuf text;
        bt_strbuf_init(&text, 0);
        BT_CHECK_EQ(bt_gd_json_pretty(value, &text), 0);
        BT_CHECK_EQ(bt_json_parse(text.data, text.len, &parsed, &jerr), 0);
        bt_gd_cases_file right;
        BT_CHECK_EQ(bt_gd_cases_file_from_json(parsed, &right, NULL, 0), 0);
        switch (variant) {
        case 0:
            right.count = 0;
            break;
        case 1:
            replace_string(&right.cases[0].orbit, "corner");
            break;
        case 2:
            right.cases[0].t1 += 1;
            break;
        case 3:
            right.cases[0].t2 += 1;
            break;
        case 4:
            right.cases[0].t1_letter = 'Z';
            break;
        case 5:
            right.cases[0].t2_letter = 'Z';
            break;
        case 6:
            right.cases[0].covered = !right.cases[0].covered;
            break;
        case 7:
            replace_string(&right.cases[0].t1_name, "UR");
            break;
        case 8:
            right.cases[0].vendor_count = 0;
            break;
        case 9:
            right.cases[0].vendor_algs[0].rowid += 1;
            break;
        }
        BT_CHECK(!bt_gd_cases_file_eq(&left, &right));
        bt_gd_cases_file_free(&left);
        bt_gd_cases_file_free(&right);
        bt_strbuf_free(&text);
        bt_json_free(parsed);
        bt_json_free(value);
    }
}

BT_TEST(gendata_model, specials_file_eq_rejects_every_field_mismatch) {
    for (int variant = 0; variant < 14; variant++) {
        bt_gd_specials_file left = {.specials = malloc(sizeof(bt_gd_special_entry)), .count = 1};
        left.specials[0] = special_fixture("eggins", "twists", 3);
        bt_json_value *value = bt_gd_specials_file_to_json(&left);
        bt_gd_specials_file right;
        BT_CHECK_EQ(bt_gd_specials_file_from_json(value, &right, NULL, 0), 0);
        switch (variant) {
        case 0:
            right.count = 0;
            break;
        case 1:
            replace_string(&right.specials[0].class_name, "flips");
            break;
        case 2:
            replace_string(&right.specials[0].solver, "cherry");
            break;
        case 3:
            right.specials[0].rowid += 1;
            break;
        case 4:
            replace_string(&right.specials[0].position, "UF & UR");
            break;
        case 5:
            replace_string(&right.specials[0].alg, "M2 U M2");
            break;
        case 6:
            right.specials[0].qtm += 1;
            break;
        case 7:
            right.specials[0].htm += 1;
            break;
        case 8:
            right.specials[0].ok = !right.specials[0].ok;
            break;
        case 9:
            right.specials[0].problems = malloc(sizeof(char *));
            right.specials[0].problems[0] = _strdup("wrong");
            right.specials[0].problem_count = 1;
            break;
        case 10:
            right.specials[0].notes = malloc(sizeof(char *));
            right.specials[0].notes[0] = _strdup("wrong");
            right.specials[0].note_count = 1;
            break;
        case 11:
            replace_string(&right.specials[0].commutator, "x");
            break;
        case 12:
            replace_string(&right.specials[0].recon_link, "x");
            break;
        case 13:
            replace_string(&right.specials[0].youtube_link, "x");
            break;
        }
        BT_CHECK(!bt_gd_specials_file_eq(&left, &right));
        bt_gd_specials_file_free(&left);
        bt_gd_specials_file_free(&right);
        bt_json_free(value);
    }
}

BT_TEST(gendata_model, manifest_eq_rejects_every_field_mismatch) {
    for (int variant = 0; variant < 26; variant++) {
        bt_gd_manifest left = manifest_fixture();
        bt_json_value *value = bt_gd_manifest_to_json(&left);
        bt_gd_manifest right;
        BT_CHECK_EQ(bt_gd_manifest_from_json(value, &right, NULL, 0), 0);
        switch (variant) {
        case 0:
            replace_string(&right.generated_at, "2026-10-15T00:00:00Z");
            break;
        case 1:
            replace_string(&right.generator, "other");
            break;
        case 2:
            replace_string(&right.synth_params_sha256, "11");
            break;
        case 3:
            replace_string(&right.corpus_sha256, "22");
            break;
        case 4:
            right.corpus_rows += 1;
            break;
        case 5:
            right.corpus_file_count = 0;
            break;
        case 6:
            right.rows_in_pair_space += 1;
            break;
        case 7:
            right.rows_other_buffer += 1;
            break;
        case 8:
            replace_string(&right.cases.path, "other.json");
            break;
        case 9:
            replace_string(&right.cases.sha256, "33");
            break;
        case 10:
            right.cases.entries += 1;
            break;
        case 11:
            right.cases.edge.total += 1;
            break;
        case 12:
            right.cases.edge.covered += 1;
            break;
        case 13:
            right.cases.edge.uncovered += 1;
            break;
        case 14:
            right.cases.corner.total += 1;
            break;
        case 15:
            right.cases.corner.covered += 1;
            break;
        case 16:
            right.cases.corner.uncovered += 1;
            break;
        case 17:
            replace_string(&right.specials.path, "other.json");
            break;
        case 18:
            replace_string(&right.specials.sha256, "44");
            break;
        case 19:
            right.specials.entries += 1;
            break;
        case 20:
            right.specials.count_count = 0;
            break;
        case 21:
            right.corpus_files[0].rows += 1;
            break;
        case 22:
            replace_string(&right.corpus_files[0].solver, "eggins");
            break;
        case 23:
            right.specials.counts[0].rows += 1;
            break;
        case 24:
            replace_string(&right.specials.counts[0].solver, "eggins");
            break;
        case 25:
            replace_string(&right.specials.counts[0].class_name, "twists");
            break;
        }
        BT_CHECK(!bt_gd_manifest_eq(&left, &right));
        bt_gd_manifest_free(&left);
        bt_gd_manifest_free(&right);
        bt_json_free(value);
    }
}
