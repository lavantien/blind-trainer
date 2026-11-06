#include "ga-run/lib.h"
#include "scheme/pairs.h"

#include "common/bt_harness.h"
#include "common/json.h"
#include "common/paths.h"
#include "common/strbuf.h"
#include "data/paths.h"

#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void scratch_dir(const char *name, char out[BT_PATH_MAX]) {
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    snprintf(out, BT_PATH_MAX, "%s/build/test-tmp/ga-run-inputs/%s", root, name);
    for (char *cursor = out; *cursor != '\0'; cursor++) {
        if (*cursor == '\\') {
            *cursor = '/';
        }
    }
    _mkdir("build");
    _mkdir("build/test-tmp");
    _mkdir("build/test-tmp/ga-run-inputs");
    _mkdir(out);
}

static void write_text(const char *dir, const char *name, const char *text) {
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        BT_ABORT("cannot write %s", path);
    }
    fwrite(text, 1, strlen(text), file);
    fclose(file);
}

static bt_json_value *config_with_paths(const char *vendor_abs, const char *ref_doc_abs) {
    char text[BT_PATH_MAX * 2 + 256];
    snprintf(text, sizeof text, "{\"paths\": {\"data_vendor\": \"%s\", \"ref_doc\": \"%s\"}}",
             vendor_abs, ref_doc_abs);
    bt_json_value *config = NULL;
    bt_json_error error;
    if (bt_json_parse(text, strlen(text), &config, &error) != 0) {
        BT_ABORT("fixture config does not parse: %s", error.message);
    }
    return config;
}

static void write_full_cases(const char *vendor, const char *drop, const char *bump) {
    bt_strbuf text;
    bt_strbuf_init(&text, 65536);
    bt_strbuf_append_str(&text, "{\"cases\": [");
    bt_case edge[BT_PAIRS_MAX];
    bt_case corner[BT_PAIRS_MAX];
    size_t edge_count = bt_scheme_enumerate_cases(BT_ORBIT_EDGE, edge);
    size_t corner_count = bt_scheme_enumerate_cases(BT_ORBIT_CORNER, corner);
    for (size_t i = 0; i < edge_count + corner_count; i++) {
        const bt_case *case_id = i < edge_count ? &edge[i] : &corner[i - edge_count];
        char l1[2] = {bt_scheme_letter_of(case_id->t1), '\0'};
        char l2[2] = {bt_scheme_letter_of(case_id->t2), '\0'};
        const char *l1_field = bump != NULL && i == 0 ? bump : l1;
        bt_strbuf_appendf(&text, "%s{", i > 0 ? "," : "");
        int need_sep = 0;
        if (drop == NULL || strcmp(drop, "orbit") != 0) {
            bt_strbuf_appendf(&text, "%s\"orbit\": \"%s\"", need_sep ? ", " : "",
                              case_id->orbit == BT_ORBIT_EDGE ? "edge" : "corner");
            need_sep = 1;
        }
        if (drop == NULL || strcmp(drop, "t1") != 0) {
            bt_strbuf_appendf(&text, "%s\"t1\": %u", need_sep ? ", " : "", (unsigned)case_id->t1);
            need_sep = 1;
        }
        if (drop == NULL || strcmp(drop, "t2") != 0) {
            bt_strbuf_appendf(&text, "%s\"t2\": %u", need_sep ? ", " : "", (unsigned)case_id->t2);
            need_sep = 1;
        }
        if (drop == NULL || strcmp(drop, "t1_letter") != 0) {
            bt_strbuf_appendf(&text, "%s\"t1_letter\": \"%s\"", need_sep ? ", " : "", l1_field);
            need_sep = 1;
        }
        if (drop == NULL || strcmp(drop, "t2_letter") != 0) {
            bt_strbuf_appendf(&text, "%s\"t2_letter\": \"%s\"", need_sep ? ", " : "", l2);
        }
        bt_strbuf_append_str(&text, "}");
    }
    bt_strbuf_append_str(&text, "]}");
    write_text(vendor, "cases.json", text.data);
    bt_strbuf_free(&text);
}

BT_TEST(ga_run_inputs, cross_check_names_each_rejected_vendor_shape) {
    struct {
        const char *body;
        const char *want;
    } rows[] = {
        {"{}", "cases.json missing cases array"},
        {"{\"cases\": {}}", "cases.json missing cases array"},
        {
            "{\"cases\": [{\"orbit\": \"edge\", \"t1\": 0, \"t2\": 0, \"t1_letter\": \"a\", "
            "\"t2_letter\": \"b\"}]}",
            "cases.json has 1 rows, scheme enumerates",
        },
    };
    char vendor[BT_PATH_MAX];
    scratch_dir("xcheck", vendor);
    for (size_t i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        write_text(vendor, "cases.json", rows[i].body);
        bt_json_value *config = config_with_paths(vendor, "ref_doc");
        bt_gr_offenders offenders = {0};
        BT_CHECK_EQ(bt_gr_cross_check_cases(config, &offenders), -1);
        BT_CHECK(offenders.count >= 1);
        if (offenders.count >= 1 && strstr(offenders.items[0], rows[i].want) == NULL) {
            bt_harness_fail(__FILE__, __LINE__, "shape %zu said: %s", i, offenders.items[0]);
        }
        bt_gr_offenders_free(&offenders);
        bt_json_free(config);
    }
    struct {
        const char *drop;
        const char *want;
    } row_shapes[] = {
        {"orbit", "cases.json row missing orbit"},
        {"t1", "cases.json row missing t1 or t2"},
        {"t1_letter", "cases.json row missing t1_letter"},
        {"t2_letter", "cases.json row missing t2_letter"},
    };
    for (size_t i = 0; i < sizeof row_shapes / sizeof row_shapes[0]; i++) {
        write_full_cases(vendor, row_shapes[i].drop, NULL);
        bt_json_value *config = config_with_paths(vendor, "ref_doc");
        bt_gr_offenders offenders = {0};
        BT_CHECK_EQ(bt_gr_cross_check_cases(config, &offenders), -1);
        BT_CHECK(offenders.count >= 1);
        if (offenders.count >= 1 && strstr(offenders.items[0], row_shapes[i].want) == NULL) {
            bt_harness_fail(__FILE__, __LINE__, "drop %s said: %s", row_shapes[i].drop,
                            offenders.items[0]);
        }
        bt_gr_offenders_free(&offenders);
        bt_json_free(config);
    }
    write_full_cases(vendor, NULL, "ax");
    bt_json_value *config = config_with_paths(vendor, "ref_doc");
    bt_gr_offenders offenders = {0};
    BT_CHECK_EQ(bt_gr_cross_check_cases(config, &offenders), -1);
    BT_CHECK(offenders.count >= 1);
    if (offenders.count >= 1 && strstr(offenders.items[0], "letters axB vs scheme AB") == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "letters row said: %s", offenders.items[0]);
    }
    bt_gr_offenders_free(&offenders);
    bt_json_free(config);
}

BT_TEST(ga_run_inputs, cross_check_reports_a_missing_file_a_missing_key_and_a_bad_parse) {
    bt_gr_offenders offenders = {0};
    bt_json_value *config =
        config_with_paths("build/test-tmp/ga-run-inputs/absent-vendor", "ref_doc");
    BT_CHECK_EQ(bt_gr_cross_check_cases(config, &offenders), -1);
    BT_CHECK(offenders.count >= 1);
    if (offenders.count >= 1) {
        BT_CHECK(strstr(offenders.items[0], "read ") != NULL);
    }
    bt_gr_offenders_free(&offenders);
    bt_json_free(config);

    bt_json_value *no_key = NULL;
    bt_json_error error;
    const char *body = "{\"paths\": {\"ref_doc\": \"ref_doc\"}}";
    if (bt_json_parse(body, strlen(body), &no_key, &error) != 0) {
        BT_ABORT("fixture does not parse: %s", error.message);
    }
    BT_CHECK_EQ(bt_gr_cross_check_cases(no_key, &offenders), -1);
    bt_gr_offenders_free(&offenders);
    bt_json_free(no_key);

    char vendor[BT_PATH_MAX];
    scratch_dir("xcheck", vendor);
    write_text(vendor, "cases.json", "{ not json");
    config = config_with_paths(vendor, "ref_doc");
    BT_CHECK_EQ(bt_gr_cross_check_cases(config, &offenders), -1);
    BT_CHECK(offenders.count >= 1);
    if (offenders.count >= 1) {
        BT_CHECK(strstr(offenders.items[0], "parse cases.json") != NULL);
    }
    bt_gr_offenders_free(&offenders);
    bt_json_free(config);
}

static const char *GA_TINY =
    "{\"seed\": 77, \"population\": 30, \"generations\": 20, \"elite\": 2, \"tournament\": 2, "
    "\"pool_top_k\": 1600, \"weights\": {\"confusion\": 1.0, \"flow\": 0.5, \"complexity\": 0.1}, "
    "\"mutation\": {\"swap\": 0.15, \"replace\": 0.05}, \"islands\": {\"count\": 1, "
    "\"migrate_every\": 0, \"migrate_count\": 1}, \"adaptive_mutation\": {\"stagnation_threshold\": "
    "0, \"swap_scale\": 1.0, \"replace_scale\": 1.0}, \"elite_carry\": {\"policy\": \"count\", "
    "\"fraction\": 0.0}, \"restart\": {\"diversity_threshold\": 0.0, \"inject_fraction\": 0.5}}";

typedef struct {
    const char *name;
    const char *paths;
    const char *ga;
    const char *want;
} load_case;

static void run_load_case(const load_case *row) {
    char dir[BT_PATH_MAX];
    scratch_dir(row->name, dir);
    char config_text[BT_PATH_MAX * 2 + 1024];
    snprintf(config_text, sizeof config_text, "{\"paths\": %s, \"ga\": %s}", row->paths,
             row->ga != NULL ? row->ga : GA_TINY);
    write_text(dir, "config.json", config_text);
    const char *prior = getenv("BT_CONFIG");
    char saved[BT_PATH_MAX];
    snprintf(saved, sizeof saved, "%s", prior != NULL ? prior : "");
    char config_path[BT_PATH_MAX];
    snprintf(config_path, sizeof config_path, "%s/config.json", dir);
    _putenv_s("BT_CONFIG", config_path);
    bt_gr_run_inputs inputs;
    bt_gr_offenders offenders = {0};
    int rc = bt_gr_load_run_inputs(&inputs, &offenders);
    bt_gr_run_inputs_free(&inputs);
    if (saved[0] != '\0') {
        _putenv_s("BT_CONFIG", saved);
    } else {
        _putenv("BT_CONFIG=");
    }
    BT_CHECK_EQ(rc, -1);
    BT_CHECK(offenders.count >= 1);
    if (offenders.count >= 1) {
        BT_CHECK(strstr(offenders.items[0], row->want) != NULL);
    }
    bt_gr_offenders_free(&offenders);
}

BT_TEST(ga_run_inputs, load_reports_each_broken_input_stage) {
    char empty_ga_dir[BT_PATH_MAX];
    scratch_dir("empty-data", empty_ga_dir);
    char half_ga_dir[BT_PATH_MAX];
    scratch_dir("half-data", half_ga_dir);
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    char freq_src[BT_PATH_MAX];
    snprintf(freq_src, sizeof freq_src, "%s/data/ga/freq.json", root);
    FILE *source = fopen(freq_src, "rb");
    BT_CHECK(source != NULL);
    if (source != NULL) {
        char freq_dst[BT_PATH_MAX];
        snprintf(freq_dst, sizeof freq_dst, "%s/freq.json", half_ga_dir);
        FILE *sink = fopen(freq_dst, "wb");
        int byte = 0;
        while ((byte = fgetc(source)) != EOF) {
            fputc(byte, sink);
        }
        fclose(sink);
        fclose(source);
    }
    char empty_paths[BT_PATH_MAX + 96];
    snprintf(empty_paths, sizeof empty_paths,
             "{\"ref_doc\": \"ref_doc\", \"data_vendor\": \"data/vendor\", \"artifacts\": "
             "\"build/test-tmp/ga-run-inputs\", \"data_ga\": \"%s\"}",
             empty_ga_dir);
    char half_paths[BT_PATH_MAX + 96];
    snprintf(half_paths, sizeof half_paths,
             "{\"ref_doc\": \"ref_doc\", \"data_vendor\": \"data/vendor\", \"artifacts\": "
             "\"build/test-tmp/ga-run-inputs\", \"data_ga\": \"%s\"}",
             half_ga_dir);
    char bad_ref_doc_paths[BT_PATH_MAX + 128];
    snprintf(bad_ref_doc_paths, sizeof bad_ref_doc_paths,
             "{\"ref_doc\": \"%s\", \"data_vendor\": \"data/vendor\", \"artifacts\": "
             "\"build/test-tmp/ga-run-inputs\", \"data_ga\": \"%s\"}",
             empty_ga_dir, half_ga_dir);
    char bad_vendor_paths[BT_PATH_MAX + 128];
    snprintf(bad_vendor_paths, sizeof bad_vendor_paths,
             "{\"ref_doc\": \"ref_doc\", \"data_vendor\": \"%s\", \"artifacts\": "
             "\"build/test-tmp/ga-run-inputs\", \"data_ga\": \"%s\"}",
             empty_ga_dir, half_ga_dir);
    static const char *const small_pool_ga =
        "{\"seed\": 77, \"population\": 30, \"generations\": 20, \"elite\": 2, "
        "\"tournament\": 2, \"pool_top_k\": 5, \"weights\": {\"confusion\": 1.0, \"flow\": 0.5, "
        "\"complexity\": 0.1}, \"mutation\": {\"swap\": 0.15, \"replace\": 0.05}, "
        "\"elite_carry\": {\"policy\": \"count\", \"fraction\": 0.0}}";
    static const char *const no_tournament_ga =
        "{\"seed\": 77, \"population\": 30, \"generations\": 20, \"elite\": 2, "
        "\"pool_top_k\": 1600, \"weights\": {\"confusion\": 1.0, \"flow\": 0.5, "
        "\"complexity\": 0.1}, \"mutation\": {\"swap\": 0.15, \"replace\": 0.05}, "
        "\"elite_carry\": {\"policy\": \"count\", \"fraction\": 0.0}}";
    static const char *const bad_upgrades_ga =
        "{\"seed\": 77, \"population\": 30, \"generations\": 20, \"elite\": 2, "
        "\"tournament\": 2, \"pool_top_k\": 1600, \"weights\": {\"confusion\": 1.0, \"flow\": 0.5, "
        "\"complexity\": 0.1}, \"mutation\": {\"swap\": 0.15, \"replace\": 0.05}, \"islands\": "
        "{\"count\": 1, \"migrate_every\": 0, \"migrate_count\": -1}, \"adaptive_mutation\": "
        "{\"stagnation_threshold\": 0, \"swap_scale\": 1.0, \"replace_scale\": 1.0}, "
        "\"elite_carry\": {\"policy\": \"count\", \"fraction\": 0.0}, \"restart\": "
        "{\"diversity_threshold\": 0.0, \"inject_fraction\": 0.5}}";
    static const char *const no_top_k_ga =
        "{\"seed\": 77, \"population\": 30, \"generations\": 20, \"elite\": 2, "
        "\"tournament\": 2, \"weights\": {\"confusion\": 1.0, \"flow\": 0.5, "
        "\"complexity\": 0.1}, \"mutation\": {\"swap\": 0.15, \"replace\": 0.05}, "
        "\"elite_carry\": {\"policy\": \"count\", \"fraction\": 0.0}}";
    char wordlist_only_dir[BT_PATH_MAX];
    scratch_dir("wordlist-only", wordlist_only_dir);
    char wordlist_src[BT_PATH_MAX];
    snprintf(wordlist_src, sizeof wordlist_src, "%s/ref_doc/vietnamese/%s", root,
             BT_GR_WORDLIST_FILE);
    FILE *wordlist_file = fopen(wordlist_src, "rb");
    BT_CHECK(wordlist_file != NULL);
    if (wordlist_file != NULL) {
        char wordlist_dst_dir[BT_PATH_MAX];
        snprintf(wordlist_dst_dir, sizeof wordlist_dst_dir, "%s/vietnamese", wordlist_only_dir);
        _mkdir(wordlist_dst_dir);
        char wordlist_dst[BT_PATH_MAX];
        snprintf(wordlist_dst, sizeof wordlist_dst, "%s/%s", wordlist_dst_dir, BT_GR_WORDLIST_FILE);
        FILE *sink = fopen(wordlist_dst, "wb");
        int byte = 0;
        while ((byte = fgetc(wordlist_file)) != EOF) {
            fputc(byte, sink);
        }
        fclose(sink);
        fclose(wordlist_file);
    }
    char wordlist_only_paths[BT_PATH_MAX + 128];
    snprintf(wordlist_only_paths, sizeof wordlist_only_paths,
             "{\"ref_doc\": \"%s\", \"data_vendor\": \"data/vendor\", \"artifacts\": "
             "\"build/test-tmp/ga-run-inputs\", \"data_ga\": \"%s\"}",
             wordlist_only_dir, half_ga_dir);
    const load_case rows[] = {
        {
            "no-ref-doc",
            "{\"data_vendor\": \"data/vendor\", \"data_ga\": \"data/ga\"}",
            NULL,
            "config section missing: paths.ref_doc",
        },
        {"bad-ref-doc", bad_ref_doc_paths, NULL, "syllables-7184-abc.txt"},
        {"no-freq-json", empty_paths, NULL, "freq.json"},
        {"small-pool", half_paths, small_pool_ga, "smaller than"},
        {"no-tournament", half_paths, no_tournament_ga, "/ga/tournament"},
        {"bad-vendor", bad_vendor_paths, NULL, "cases.json"},
        {"bad-upgrades", half_paths, bad_upgrades_ga, "ga.islands.migrate_count"},
        {"no-top-k", half_paths, no_top_k_ga, "config section missing: ga.pool_top_k"},
        {"freqs-missing", wordlist_only_paths, NULL, BT_GR_FREQS_FILE},
    };
    for (size_t i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        run_load_case(&rows[i]);
    }
}

BT_TEST(ga_run_inputs, load_reports_a_missing_config_file) {
    char dir[BT_PATH_MAX];
    scratch_dir("absent-config", dir);
    char config_path[BT_PATH_MAX];
    snprintf(config_path, sizeof config_path, "%s/config.json", dir);
    const char *prior = getenv("BT_CONFIG");
    char saved[BT_PATH_MAX];
    snprintf(saved, sizeof saved, "%s", prior != NULL ? prior : "");
    _putenv_s("BT_CONFIG", config_path);
    bt_gr_run_inputs inputs;
    bt_gr_offenders offenders = {0};
    int rc = bt_gr_load_run_inputs(&inputs, &offenders);
    if (saved[0] != '\0') {
        _putenv_s("BT_CONFIG", saved);
    } else {
        _putenv("BT_CONFIG=");
    }
    BT_CHECK_EQ(rc, -1);
    BT_CHECK(offenders.count >= 1);
    if (offenders.count >= 1) {
        BT_CHECK(strstr(offenders.items[0], "load config failed") != NULL);
    }
    bt_gr_offenders_free(&offenders);
}

BT_TEST(ga_run_inputs, offenders_list_grows_past_its_first_capacity) {
    bt_gr_offenders offenders = {0};
    for (size_t i = 0; i < 12; i++) {
        BT_CHECK_EQ(bt_gr_offenders_push(&offenders, "offender %zu", i), 0);
    }
    BT_CHECK_EQ(offenders.count, (size_t)12);
    BT_CHECK(offenders.cap >= 12);
    BT_CHECK_EQ(strcmp(offenders.items[11], "offender 11"), 0);
    bt_gr_offenders_free(&offenders);
    BT_CHECK_EQ(offenders.count, (size_t)0);
    BT_CHECK_EQ(offenders.cap, (size_t)0);
}

BT_TEST(ga_run_inputs, build_report_handles_an_empty_per_generation_series) {
    static bt_ga_pool_entry pool[2];
    memset(pool, 0, sizeof pool);
    snprintf(pool[0].text, BT_GA_TEXT_MAX, "ma");
    pool[0].frequency = 9;
    snprintf(pool[1].text, BT_GA_TEXT_MAX, "nem");
    pool[1].frequency = 4;
    bt_gr_run_inputs inputs;
    memset(&inputs, 0, sizeof inputs);
    inputs.pool = pool;
    inputs.pool_count = 2;
    bt_ga_run_outcome outcome;
    memset(&outcome, 0, sizeof outcome);
    outcome.best_fitness = 0.5F;
    outcome.best_parts[0] = 0.1F;
    outcome.best_parts[1] = 0.2F;
    outcome.best_parts[2] = 0.2F;
    outcome.per_gen_best = NULL;
    outcome.per_gen_count = 0;
    bt_gr_syllable_assignment assignment;
    memset(&assignment, 0, sizeof assignment);
    bt_gr_ga_report report;
    bt_gr_build_report(&inputs, &outcome, &assignment, NULL, &report);
    BT_CHECK_EQ(report.pool_top_count, (size_t)2);
    BT_CHECK_EQ(strcmp(report.pool_top[0].text, "ma"), 0);
    BT_CHECK_EQ(report.per_gen_count, (size_t)0);
    BT_CHECK(report.per_gen_best != NULL);
    bt_gr_report_free(&report);
}
