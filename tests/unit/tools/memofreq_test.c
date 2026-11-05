#include "unit/tools/spawn_util.h"

#include "ga/dist.h"
#include "ga/hash.h"
#include "memofreq/lib.h"

#include "common/bt_harness.h"
#include "common/paths.h"
#include "data/paths.h"

#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void scratch_dir(const char *name, char out[BT_PATH_MAX]) {
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    snprintf(out, BT_PATH_MAX, "%s/build/test-tmp/memofreq-main/%s", root, name);
    for (char *cursor = out; *cursor != '\0'; cursor++) {
        if (*cursor == '\\') {
            *cursor = '/';
        }
    }
    bt_test_mkdirs(out);
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

static char *read_bytes(const char *path, size_t *len) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *bytes = malloc((size_t)size + 1);
    if (bytes != NULL) {
        *len = fread(bytes, 1, (size_t)size, file);
        bytes[*len] = '\0';
    }
    fclose(file);
    return bytes;
}

static int spawn_memofreq(const char *config_path, int *code, char *out, size_t out_cap, char *err,
                          size_t err_cap) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "memofreq.exe");
    const char *prior = getenv("BT_CONFIG");
    char saved[BT_PATH_MAX];
    snprintf(saved, sizeof saved, "%s", prior != NULL ? prior : "");
    if (config_path != NULL) {
        _putenv_s("BT_CONFIG", config_path);
    } else {
        _putenv("BT_CONFIG=");
    }
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    const char *argv[] = {NULL};
    int rc = bt_spawn_capture(exe, argv, root, 120000, code, out, out_cap, err, err_cap);
    if (saved[0] != '\0') {
        _putenv_s("BT_CONFIG", saved);
    } else {
        _putenv("BT_CONFIG=");
    }
    return rc;
}

static void write_memofreq_config(const char *dir, const char *paths_json,
                                  const char *memofreq_json, const char *trace_json) {
    char text[BT_PATH_MAX * 2 + 512];
    snprintf(text, sizeof text, "{\"paths\": %s, \"memofreq\": %s, \"trace\": %s}", paths_json,
             memofreq_json, trace_json);
    write_text(dir, "config.json", text);
}

BT_TEST(memofreq, a_small_full_run_writes_freq_json_and_reports_the_sha) {
    char data_ga[BT_PATH_MAX];
    scratch_dir("success", data_ga);
    char paths[BT_PATH_MAX + 64];
    snprintf(paths, sizeof paths, "{\"data_ga\": \"%s\"}", data_ga);
    write_memofreq_config(data_ga, paths, "{\"samples\": 30000, \"seed\": 20261012}",
                          "{\"ring_bytes\": 65536, \"max_threads\": 8, \"line_max\": 256}");
    char config_path[BT_PATH_MAX];
    snprintf(config_path, sizeof config_path, "%s/config.json", data_ga);
    int code = -1;
    char out[4096];
    char err[2048];
    BT_CHECK_EQ(spawn_memofreq(config_path, &code, out, sizeof out, err, sizeof err), 0);
    BT_CHECK_EQ(code, 0);
    BT_CHECK(strstr(out, "memofreq: 30000 draws seed 20261012") != NULL);
    BT_CHECK(strstr(out, "cases all hit") != NULL);
    size_t len = 0;
    char freq_path[BT_PATH_MAX];
    snprintf(freq_path, sizeof freq_path, "%s/freq.json", data_ga);
    char *bytes = read_bytes(freq_path, &len);
    BT_CHECK(bytes != NULL);
    if (bytes != NULL) {
        char sha[BT_GA_SHA256_HEX_LEN];
        bt_ga_sha256_hex(bytes, len, sha);
        char want[BT_GA_SHA256_HEX_LEN + 32];
        snprintf(want, sizeof want, "sha256 %s", sha);
        BT_CHECK(strstr(out, want) != NULL);
        bt_ga_freq_data data;
        char parse_err[128];
        if (bt_ga_load_freq_data(freq_path, &data, parse_err)) {
            BT_CHECK_EQ(data.total_draws, (unsigned long long)30000);
            BT_CHECK_EQ(data.seed, (unsigned long long)20261012);
            bt_ga_freq_data_free(&data);
        } else {
            bt_harness_fail(__FILE__, __LINE__, "written freq.json does not parse: %s", parse_err);
        }
        free(bytes);
    }
}

BT_TEST(memofreq, a_short_run_reports_the_never_hit_cases) {
    char data_ga[BT_PATH_MAX];
    scratch_dir("zero-hit", data_ga);
    char paths[BT_PATH_MAX + 64];
    snprintf(paths, sizeof paths, "{\"data_ga\": \"%s\"}", data_ga);
    write_memofreq_config(data_ga, paths, "{\"samples\": 250, \"seed\": 20261012}", "{}");
    char config_path[BT_PATH_MAX];
    snprintf(config_path, sizeof config_path, "%s/config.json", data_ga);
    int code = -1;
    char out[2048];
    char err[16384];
    BT_CHECK_EQ(spawn_memofreq(config_path, &code, out, sizeof out, err, sizeof err), 0);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "cases never appeared over 250 draws") != NULL);
}

BT_TEST(memofreq, a_config_without_the_memofreq_section_exits_1) {
    char data_ga[BT_PATH_MAX];
    scratch_dir("no-section", data_ga);
    write_text(data_ga, "config.json", "{\"paths\": {\"data_ga\": \"build\"}}");
    char config_path[BT_PATH_MAX];
    snprintf(config_path, sizeof config_path, "%s/config.json", data_ga);
    int code = -1;
    char out[2048];
    char err[2048];
    BT_CHECK_EQ(spawn_memofreq(config_path, &code, out, sizeof out, err, sizeof err), 0);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "memofreq: config section missing: memofreq.samples") != NULL);
}

BT_TEST(memofreq, a_config_without_paths_data_ga_exits_1) {
    char data_ga[BT_PATH_MAX];
    scratch_dir("no-path", data_ga);
    write_text(data_ga, "config.json", "{\"memofreq\": {\"samples\": 30000, \"seed\": 20261012}}");
    char config_path[BT_PATH_MAX];
    snprintf(config_path, sizeof config_path, "%s/config.json", data_ga);
    int code = -1;
    char out[2048];
    char err[2048];
    BT_CHECK_EQ(spawn_memofreq(config_path, &code, out, sizeof out, err, sizeof err), 0);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "memofreq: config section missing: paths.data_ga") != NULL);
}

BT_TEST(memofreq, a_missing_config_exits_1) {
    int code = -1;
    char out[2048];
    char err[2048];
    BT_CHECK_EQ(spawn_memofreq("build/test-tmp/memofreq-main/absent/config.json", &code, out,
                               sizeof out, err, sizeof err),
                0);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "memofreq: load config failed") != NULL);
}

BT_TEST(memofreq, a_freq_json_destination_that_is_a_directory_exits_1) {
    char data_ga[BT_PATH_MAX];
    scratch_dir("blocked", data_ga);
    char blocked[BT_PATH_MAX];
    snprintf(blocked, sizeof blocked, "%s/freq.json", data_ga);
    _mkdir(blocked);
    char paths[BT_PATH_MAX + 64];
    snprintf(paths, sizeof paths, "{\"data_ga\": \"%s\"}", data_ga);
    write_memofreq_config(data_ga, paths, "{\"samples\": 30000, \"seed\": 20261012}", "{}");
    char config_path[BT_PATH_MAX];
    snprintf(config_path, sizeof config_path, "%s/config.json", data_ga);
    int code = -1;
    char out[2048];
    char err[2048];
    BT_CHECK_EQ(spawn_memofreq(config_path, &code, out, sizeof out, err, sizeof err), 0);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "memofreq: write ") != NULL);
}

BT_TEST(memofreq, freq_data_text_prints_the_schema_with_empty_and_filled_sections) {
    bt_ga_case_row cases[1];
    snprintf(cases[0].orbit, sizeof cases[0].orbit, "edge");
    snprintf(cases[0].letters, sizeof cases[0].letters, "AB");
    cases[0].count = 120;
    bt_ga_pair_count adjacent[1] = {{0, 17, 3}};
    bt_ga_freq_data data = {0};
    data.cases = cases;
    data.cases_count = 1;
    data.adjacent = adjacent;
    data.adjacent_count = 1;
    data.total_draws = 1000;
    data.seed = 20261012;
    bt_strbuf text;
    bt_strbuf_init(&text, 512);
    BT_CHECK_EQ(bt_mf_freq_data_text(&data, &text), 0);
    const char *want = "{\n"
                       "  \"cases\": [\n"
                       "    {\n"
                       "      \"orbit\": \"edge\",\n"
                       "      \"letters\": \"AB\",\n"
                       "      \"count\": 120\n"
                       "    }\n"
                       "  ],\n"
                       "  \"adjacent\": [\n"
                       "    {\n"
                       "      \"from\": 0,\n"
                       "      \"to\": 17,\n"
                       "      \"count\": 3\n"
                       "    }\n"
                       "  ],\n"
                       "  \"cooccurring\": [],\n"
                       "  \"total_draws\": 1000,\n"
                       "  \"seed\": 20261012\n"
                       "}";
    BT_CHECK_EQ(text.data, want);
    bt_strbuf_free(&text);
    bt_ga_freq_data empty = {0};
    bt_strbuf_init(&text, 512);
    BT_CHECK_EQ(bt_mf_freq_data_text(&empty, &text), 0);
    BT_CHECK(strstr(text.data, "\"cases\": [],") != NULL);
    BT_CHECK(strstr(text.data, "\"adjacent\": [],") != NULL);
    BT_CHECK(strstr(text.data, "\"cooccurring\": [],") != NULL);
    bt_strbuf_free(&text);
}

BT_TEST(memofreq, freq_data_text_escapes_and_separates_multiple_rows) {
    bt_ga_case_row cases[3];
    snprintf(cases[0].orbit, sizeof cases[0].orbit, "edge");
    snprintf(cases[0].letters, sizeof cases[0].letters, "AB");
    cases[0].count = 1;
    snprintf(cases[1].orbit, sizeof cases[1].orbit, "e\"d\\g");
    snprintf(cases[1].letters, sizeof cases[1].letters, "a\\b");
    cases[1].count = 2;
    snprintf(cases[2].orbit, sizeof cases[2].orbit,
             "c\x01"
             "f\x1f");
    snprintf(cases[2].letters, sizeof cases[2].letters,
             "\xE9"
             "t");
    cases[2].count = 3;
    bt_ga_pair_count adjacent[2] = {{0, 1, 5}, {1, 2, 6}};
    bt_ga_pair_count cooccurring[2] = {{3, 4, 7}, {5, 6, 8}};
    bt_ga_freq_data data = {0};
    data.cases = cases;
    data.cases_count = 3;
    data.adjacent = adjacent;
    data.adjacent_count = 2;
    data.cooccurring = cooccurring;
    data.cooccurring_count = 2;
    data.total_draws = 9;
    data.seed = 424242;
    bt_strbuf text;
    bt_strbuf_init(&text, 512);
    BT_CHECK_EQ(bt_mf_freq_data_text(&data, &text), 0);
    const char *want = "{\n"
                       "  \"cases\": [\n"
                       "    {\n"
                       "      \"orbit\": \"edge\",\n"
                       "      \"letters\": \"AB\",\n"
                       "      \"count\": 1\n"
                       "    },\n"
                       "    {\n"
                       "      \"orbit\": \"e\\\"d\\\\g\",\n"
                       "      \"letters\": \"a\\\\b\",\n"
                       "      \"count\": 2\n"
                       "    },\n"
                       "    {\n"
                       "      \"orbit\": \"c\\u0001f\\u001f\",\n"
                       "      \"letters\": \"\xE9t\",\n"
                       "      \"count\": 3\n"
                       "    }\n"
                       "  ],\n"
                       "  \"adjacent\": [\n"
                       "    {\n"
                       "      \"from\": 0,\n"
                       "      \"to\": 1,\n"
                       "      \"count\": 5\n"
                       "    },\n"
                       "    {\n"
                       "      \"from\": 1,\n"
                       "      \"to\": 2,\n"
                       "      \"count\": 6\n"
                       "    }\n"
                       "  ],\n"
                       "  \"cooccurring\": [\n"
                       "    {\n"
                       "      \"from\": 3,\n"
                       "      \"to\": 4,\n"
                       "      \"count\": 7\n"
                       "    },\n"
                       "    {\n"
                       "      \"from\": 5,\n"
                       "      \"to\": 6,\n"
                       "      \"count\": 8\n"
                       "    }\n"
                       "  ],\n"
                       "  \"total_draws\": 9,\n"
                       "  \"seed\": 424242\n"
                       "}";
    BT_CHECK_EQ(text.data, want);
    bt_strbuf_free(&text);
}
