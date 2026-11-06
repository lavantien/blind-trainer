#include "ga-run/lib.h"
#include "ga-run/out.h"

#include "common/bt_harness.h"
#include "common/json.h"
#include "common/paths.h"
#include "common/strbuf.h"
#include "data/paths.h"
#include "ga/hash.h"

#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void scratch_dir(const char *name, char out[BT_PATH_MAX]) {
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    snprintf(out, BT_PATH_MAX, "%s/build/test-tmp/ga-run-flow/%s", root, name);
    for (char *cursor = out; *cursor != '\0'; cursor++) {
        if (*cursor == '\\') {
            *cursor = '/';
        }
    }
    _mkdir("build");
    _mkdir("build/test-tmp");
    _mkdir("build/test-tmp/ga-run-flow");
    _mkdir(out);
}

static void scratch_file(const char *dir, const char *name, const char *bytes, size_t len) {
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        BT_ABORT("cannot write %s", path);
    }
    fwrite(bytes, 1, len, file);
    fclose(file);
}

static const char *GA_TINY =
    "{\"seed\": 77, \"population\": 30, \"generations\": 20, \"elite\": 2, \"tournament\": 2, "
    "\"pool_top_k\": 1600, \"weights\": {\"confusion\": 1.0, \"flow\": 0.5, \"complexity\": 0.1}, "
    "\"mutation\": {\"swap\": 0.15, \"replace\": 0.05}, \"islands\": {\"count\": 1, "
    "\"migrate_every\": 0, \"migrate_count\": 1}, \"adaptive_mutation\": {\"stagnation_threshold\": "
    "0, \"swap_scale\": 1.0, \"replace_scale\": 1.0}, \"elite_carry\": {\"policy\": \"count\", "
    "\"fraction\": 0.0}, \"restart\": {\"diversity_threshold\": 0.0, \"inject_fraction\": 0.5}}";

static void write_config(const char *dir, const char *paths_json) {
    char text[8192];
    snprintf(text, sizeof text, "{\"paths\": %s, \"ga\": %s}", paths_json, GA_TINY);
    scratch_file(dir, "config.json", text, strlen(text));
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

static void with_config_env(const char *path, void (*fn)(void *user), void *user) {
    const char *prior = getenv("BT_CONFIG");
    char saved[BT_PATH_MAX];
    snprintf(saved, sizeof saved, "%s", prior != NULL ? prior : "");
    _putenv_s("BT_CONFIG", path);
    fn(user);
    if (saved[0] != '\0') {
        _putenv_s("BT_CONFIG", saved);
    } else {
        _putenv("BT_CONFIG=");
    }
}

typedef struct {
    int rc;
    size_t offenders;
    char first[256];
} run_all_result;

static void run_all_once(void *user) {
    run_all_result *result = user;
    bt_gr_offenders offenders = {0};
    result->rc = bt_gr_run_all(&offenders);
    result->offenders = offenders.count;
    if (offenders.count > 0) {
        snprintf(result->first, sizeof result->first, "%s", offenders.items[0]);
    }
    bt_gr_offenders_free(&offenders);
}

static void copy_repo_freq_json(const char *data_ga) {
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    char freq[BT_PATH_MAX];
    snprintf(freq, sizeof freq, "%s/data/ga/freq.json", root);
    size_t len = 0;
    char *bytes = read_bytes(freq, &len);
    if (bytes == NULL) {
        BT_ABORT("cannot read %s, run python bt.py memofreq first", freq);
    }
    scratch_file(data_ga, "freq.json", bytes, len);
    free(bytes);
}

BT_TEST(ga_run_flow, run_all_writes_the_three_artifacts_from_a_tiny_config) {
    char data_ga[BT_PATH_MAX];
    char artifacts[BT_PATH_MAX];
    scratch_dir("success-data", data_ga);
    scratch_dir("success-artifacts", artifacts);
    char paths[BT_PATH_MAX * 2 + 128];
    snprintf(paths, sizeof paths,
             "{\"ref_doc\": \"ref_doc\", \"data_vendor\": \"data/vendor\", \"data_ga\": \"%s\", "
             "\"artifacts\": \"%s\"}",
             data_ga, artifacts);
    write_config(data_ga, paths);
    copy_repo_freq_json(data_ga);
    char config_path[BT_PATH_MAX];
    snprintf(config_path, sizeof config_path, "%s/config.json", data_ga);
    run_all_result result;
    memset(&result, 0, sizeof result);
    with_config_env(config_path, run_all_once, &result);
    BT_CHECK_EQ(result.rc, 0);
    BT_CHECK_EQ(result.offenders, (size_t)0);
    size_t freq_len = 0;
    char freq_path[BT_PATH_MAX];
    snprintf(freq_path, sizeof freq_path, "%s/freq.json", data_ga);
    char *freq_bytes = read_bytes(freq_path, &freq_len);
    BT_CHECK(freq_bytes != NULL);
    char want_sha[BT_GA_SHA256_HEX_LEN];
    bt_ga_sha256_hex(freq_bytes, freq_len, want_sha);
    free(freq_bytes);
    bt_json_error error;
    bt_json_value *report = NULL;
    char report_path[BT_PATH_MAX];
    snprintf(report_path, sizeof report_path, "%s/report.json", data_ga);
    size_t report_len = 0;
    char *report_text = read_bytes(report_path, &report_len);
    BT_CHECK(report_text != NULL);
    if (report_text != NULL) {
        if (bt_json_parse(report_text, report_len, &report, &error) != 0) {
            bt_harness_fail(__FILE__, __LINE__, "report.json does not parse: %s", error.message);
        }
        free(report_text);
    }
    const bt_json_value *sha = bt_json_get(report, "freq_sha256");
    BT_CHECK(sha != NULL && sha->type == BT_JSON_STRING);
    if (sha != NULL) {
        BT_CHECK_EQ(sha->as.string, want_sha);
    }
    const bt_json_value *lane = bt_json_get(report, "eval_used_cuda");
    BT_CHECK(lane != NULL && bt_json_is_integer(lane) && lane->integer == 0);
    bt_json_free(report);
    bt_json_value *syllables = NULL;
    char syllables_path[BT_PATH_MAX];
    snprintf(syllables_path, sizeof syllables_path, "%s/syllables.json", data_ga);
    size_t syllables_len = 0;
    char *syllables_text = read_bytes(syllables_path, &syllables_len);
    BT_CHECK(syllables_text != NULL);
    if (syllables_text != NULL) {
        if (bt_json_parse(syllables_text, syllables_len, &syllables, &error) != 0) {
            bt_harness_fail(__FILE__, __LINE__, "syllables.json does not parse: %s", error.message);
        }
        free(syllables_text);
    }
    const bt_json_value *cases = bt_json_get(syllables, "cases");
    BT_CHECK(cases != NULL && cases->type == BT_JSON_ARRAY &&
             cases->as.array.count == bt_ga_case_count());
    const bt_json_value *seed = bt_json_get(syllables, "seed");
    BT_CHECK(seed != NULL && bt_json_is_integer(seed) && seed->integer == 77);
    bt_json_free(syllables);
    char markdown[BT_PATH_MAX];
    snprintf(markdown, sizeof markdown, "%s/ga/report.md", artifacts);
    FILE *file = fopen(markdown, "rb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        char head[32] = {0};
        fread(head, 1, sizeof head - 1, file);
        fclose(file);
        BT_CHECK(strncmp(head, "# ga run report", 15) == 0);
    }
}

BT_TEST(ga_run_flow, run_all_reports_a_missing_config) {
    run_all_result result;
    memset(&result, 0, sizeof result);
    with_config_env("build/test-tmp/ga-run-flow/absent/config.json", run_all_once, &result);
    BT_CHECK_EQ(result.rc, -1);
    BT_CHECK_EQ(result.offenders, (size_t)1);
    BT_CHECK_EQ(result.first, "load config failed");
}

static void run_missing_key_case(const char *case_name, const char *drop_key, const char *want) {
    char data_ga[BT_PATH_MAX];
    scratch_dir(case_name, data_ga);
    char paths[BT_PATH_MAX * 2 + 128];
    snprintf(paths, sizeof paths,
             "{\"ref_doc\": \"ref_doc\", \"data_vendor\": \"data/vendor\", \"artifacts\": "
             "\"build/test-tmp/ga-run-flow\", \"data_ga\": \"%s\", \"baselines\": \"baselines\"}",
             data_ga);
    char reduced[BT_PATH_MAX * 2 + 128];
    snprintf(reduced, sizeof reduced, "%s", paths);
    char key[96];
    snprintf(key, sizeof key, "\"%s\": ", drop_key);
    char *at = strstr(reduced, key);
    if (at == NULL) {
        BT_ABORT("key %s not in the scratch paths", drop_key);
    }
    char *value_end = strchr(at + strlen(key), ',');
    if (value_end == NULL) {
        BT_ABORT("cannot splice key %s", drop_key);
    }
    memmove(at, value_end + 1, strlen(value_end + 1) + 1);
    write_config(data_ga, reduced);
    copy_repo_freq_json(data_ga);
    char config_path[BT_PATH_MAX];
    snprintf(config_path, sizeof config_path, "%s/config.json", data_ga);
    run_all_result result;
    memset(&result, 0, sizeof result);
    with_config_env(config_path, run_all_once, &result);
    BT_CHECK_EQ(result.rc, -1);
    BT_CHECK(result.offenders >= 1);
    BT_CHECK(strstr(result.first, want) != NULL);
}

BT_TEST(ga_run_flow, run_all_reports_each_missing_path_key) {
    run_missing_key_case("no-ref-doc", "ref_doc", "config section missing: paths.ref_doc");
    run_missing_key_case("no-data-ga", "data_ga", "config section missing: paths.data_ga");
    run_missing_key_case("no-artifacts", "artifacts", "config section missing: paths.artifacts");
}

BT_TEST(ga_run_flow, run_all_fails_the_syllables_write_when_it_is_a_directory) {
    char data_ga[BT_PATH_MAX];
    scratch_dir("syllables-dir", data_ga);
    char paths[BT_PATH_MAX * 2 + 128];
    snprintf(paths, sizeof paths,
             "{\"ref_doc\": \"ref_doc\", \"data_vendor\": \"data/vendor\", \"data_ga\": \"%s\", "
             "\"artifacts\": \"build/test-tmp/ga-run-flow\"}",
             data_ga);
    write_config(data_ga, paths);
    copy_repo_freq_json(data_ga);
    char blocked[BT_PATH_MAX];
    snprintf(blocked, sizeof blocked, "%s/syllables.json", data_ga);
    _mkdir(blocked);
    char config_path[BT_PATH_MAX];
    snprintf(config_path, sizeof config_path, "%s/config.json", data_ga);
    run_all_result result;
    memset(&result, 0, sizeof result);
    with_config_env(config_path, run_all_once, &result);
    BT_CHECK_EQ(result.rc, -1);
    BT_CHECK(strstr(result.first, "write ") == result.first);
}

BT_TEST(ga_run_flow, run_all_fails_the_report_write_when_it_is_a_directory) {
    char data_ga[BT_PATH_MAX];
    scratch_dir("report-dir", data_ga);
    char paths[BT_PATH_MAX * 2 + 128];
    snprintf(paths, sizeof paths,
             "{\"ref_doc\": \"ref_doc\", \"data_vendor\": \"data/vendor\", \"data_ga\": \"%s\", "
             "\"artifacts\": \"build/test-tmp/ga-run-flow\"}",
             data_ga);
    write_config(data_ga, paths);
    copy_repo_freq_json(data_ga);
    char blocked[BT_PATH_MAX];
    snprintf(blocked, sizeof blocked, "%s/report.json", data_ga);
    _mkdir(blocked);
    char config_path[BT_PATH_MAX];
    snprintf(config_path, sizeof config_path, "%s/config.json", data_ga);
    run_all_result result;
    memset(&result, 0, sizeof result);
    with_config_env(config_path, run_all_once, &result);
    BT_CHECK_EQ(result.rc, -1);
    BT_CHECK(strstr(result.first, "write ") == result.first);
    BT_CHECK(strstr(result.first, "report.json") != NULL);
}

BT_TEST(ga_run_flow, run_all_fails_the_markdown_write_under_a_file_prefix) {
    char data_ga[BT_PATH_MAX];
    char artifacts[BT_PATH_MAX];
    scratch_dir("markdown-data", data_ga);
    scratch_dir("markdown-artifacts", artifacts);
    char blocked[BT_PATH_MAX];
    snprintf(blocked, sizeof blocked, "%s/ga", artifacts);
    FILE *file = fopen(blocked, "wb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        fwrite("x", 1, 1, file);
        fclose(file);
    }
    char paths[BT_PATH_MAX * 2 + 128];
    snprintf(paths, sizeof paths,
             "{\"ref_doc\": \"ref_doc\", \"data_vendor\": \"data/vendor\", \"data_ga\": \"%s\", "
             "\"artifacts\": \"%s\"}",
             data_ga, artifacts);
    write_config(data_ga, paths);
    copy_repo_freq_json(data_ga);
    char config_path[BT_PATH_MAX];
    snprintf(config_path, sizeof config_path, "%s/config.json", data_ga);
    run_all_result result;
    memset(&result, 0, sizeof result);
    with_config_env(config_path, run_all_once, &result);
    BT_CHECK_EQ(result.rc, -1);
    BT_CHECK(strstr(result.first, "write ") == result.first);
    BT_CHECK(strstr(result.first, "report.md") != NULL);
}
