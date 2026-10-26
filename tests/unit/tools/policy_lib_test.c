#include "policy_test_util.h"

#include <string.h>

#include "policy/comments.h"
#include "policy/constants.h"
#include "policy/lexer.h"
#include "policy/lib.h"
#include "policy/sloc.h"

static bt_json_value *parse_cfg(const char *text) {
    bt_json_value *config = NULL;
    bt_json_error error;
    int rc = bt_json_parse(text, strlen(text), &config, &error);
    if (rc != 0) {
        bt_harness_fail(__FILE__, __LINE__, "config parse failed: %s", error.message);
        return NULL;
    }
    return config;
}

static bt_json_value *selftest_cfg(void) {
    return parse_cfg(POLICY_SELFTEST_CONFIG);
}

static void plant(char root[POLICY_UTIL_PATH_MAX], const char *rel, const char *text) {
    char path[POLICY_UTIL_PATH_MAX];
    policy_util_join(path, root, rel);
    policy_util_write(path, text);
}

static int run(const char *root, policy_check_mode mode, const bt_json_value *config,
               const char *allow_rel, policy_outcome *outcome, char err[POLICY_ERR_MAX]) {
    char allow[POLICY_UTIL_PATH_MAX];
    policy_util_join(allow, root, allow_rel);
    return policy_run_checks(root, mode, config, allow, outcome, err, POLICY_ERR_MAX);
}

BT_TEST(policy_lib, oversized_files_fail_both_mode_with_a_sloc_finding) {
    char root[POLICY_UTIL_PATH_MAX];
    policy_util_scratch("run-sloc-both", root);
    plant(root, "src/big.c", POLICY_BIG_FIXTURE);
    plant(root, "allow.txt", "# allow\n");
    policy_outcome outcome = {0};
    char err[POLICY_ERR_MAX];
    int rc = run(root, POLICY_MODE_BOTH, selftest_cfg(), "allow.txt", &outcome, err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(outcome.finding_count, 1);
    BT_CHECK(strcmp(outcome.findings[0].kind, "sloc") == 0);
    BT_CHECK(strcmp(outcome.findings[0].path, "src/big.c") == 0);
    BT_CHECK_EQ(outcome.findings[0].line, 9);
    policy_outcome_free(&outcome);
    policy_util_rm_rf(root);
}

BT_TEST(policy_lib, sloc_mode_reports_only_cap_findings_without_the_allowlist) {
    char root[POLICY_UTIL_PATH_MAX];
    policy_util_scratch("run-sloc-only", root);
    plant(root, "src/big.c", POLICY_BIG_FIXTURE);
    policy_outcome outcome = {0};
    char err[POLICY_ERR_MAX];
    int rc = run(root, POLICY_MODE_SLOC, selftest_cfg(), "absent.txt", &outcome, err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(outcome.finding_count, 1);
    BT_CHECK(strcmp(outcome.findings[0].detail, "10 non-blank lines over cap 8") == 0);
    BT_CHECK_EQ(outcome.stale_count, 0);
    policy_outcome_free(&outcome);
    policy_util_rm_rf(root);
}

BT_TEST(policy_lib, sloc_mode_fails_closed_without_the_hub_key) {
    char root[POLICY_UTIL_PATH_MAX];
    policy_util_scratch("run-sloc-missing", root);
    plant(root, "src/clean.c", POLICY_CLEAN_FIXTURE);
    bt_json_value *config =
        parse_cfg("{\"policy\": {\"literal_recurrence\": 2, \"small_literal_allow\": []}}");
    policy_outcome outcome = {0};
    char err[POLICY_ERR_MAX];
    int rc = run(root, POLICY_MODE_SLOC, config, "absent.txt", &outcome, err);
    BT_CHECK_EQ(rc, -1);
    BT_CHECK(strcmp(err, "config section missing: sloc.max_file_lines") == 0);
    bt_json_free(config);
    policy_util_rm_rf(root);
}

BT_TEST(policy_lib, workspace_root_is_the_repo_root) {
    char root[BT_PATH_MAX];
    policy_workspace_root(root);
    char path[BT_PATH_MAX + 32];
    snprintf(path, sizeof path, "%s/config/config.json", root);
    FILE *file = fopen(path, "rb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        fclose(file);
    }
    snprintf(path, sizeof path, "%s/bt.py", root);
    file = fopen(path, "rb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        fclose(file);
    }
}

BT_TEST(policy_lib, config_path_honors_env_and_default) {
    char root[BT_PATH_MAX];
    char out[BT_PATH_MAX];
    char want[BT_PATH_MAX + 32];
    snprintf(root, sizeof root, "build/test-tmp/ws");
    policy_config_path_from(NULL, root, out, sizeof out);
    snprintf(want, sizeof want, "%s/config/config.json", root);
    BT_CHECK(strcmp(out, want) == 0);
    policy_config_path_from("other/conf.json", root, out, sizeof out);
    snprintf(want, sizeof want, "%s/other/conf.json", root);
    BT_CHECK(strcmp(out, want) == 0);
    const char *absolute = "C:/definitely/absolute/x.json";
    policy_config_path_from(absolute, root, out, sizeof out);
    BT_CHECK(strcmp(out, absolute) == 0);
}

BT_TEST(policy_lib, glob_match_handles_star_names) {
    BT_CHECK(policy_glob_match("*.c", "main.c") == 1);
    BT_CHECK(policy_glob_match("*.c", "main.txt") == 0);
    BT_CHECK(policy_glob_match("*.c", "") == 0);
}

BT_TEST(policy_lib, collect_sources_covers_every_scan_root) {
    char root[BT_PATH_MAX];
    policy_workspace_root(root);
    size_t count = 0;
    char **files = policy_collect_sources(root, &count);
    BT_CHECK(count > 0);
    size_t root_len = strlen(root);
    int has_common = 0;
    int has_policy_tool = 0;
    int has_tests = 0;
    int has_scripts = 0;
    for (size_t at = 0; at < count; at++) {
        const char *rel = files[at] + root_len;
        BT_CHECK(files[at][root_len] == '/');
        rel++;
        if (strcmp(rel, "src/common/json.c") == 0) {
            has_common = 1;
        }
        if (strcmp(rel, "tools/policy/lib.c") == 0) {
            has_policy_tool = 1;
        }
        if (strncmp(rel, "tests/", 6) == 0) {
            has_tests = 1;
        }
        if (strncmp(rel, "scripts/", 8) == 0) {
            has_scripts = 1;
        }
    }
    BT_CHECK_EQ(has_common, 1);
    BT_CHECK_EQ(has_policy_tool, 1);
    BT_CHECK_EQ(has_tests, 1);
    BT_CHECK_EQ(has_scripts, 1);
    policy_free_sources(files, count);
}

BT_TEST(policy_lib, real_repo_sources_have_no_comment_hits) {
    char root[BT_PATH_MAX];
    policy_workspace_root(root);
    policy_outcome outcome = {0};
    char err[POLICY_ERR_MAX];
    int rc = run(root, POLICY_MODE_COMMENTS, NULL, "unused-in-comments-mode", &outcome, err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(outcome.finding_count, 0);
    policy_outcome_free(&outcome);
}

BT_TEST(policy_lib, parse_allowlist_skips_headers_and_reads_fields) {
    const char *text = "# header\n\nsrc/a.c\t42\tlegacy table\nsrc/b.c\t7\twhy\tdetail continues\n";
    policy_allowlist allow = {0};
    char err[POLICY_ERR_MAX];
    int rc = policy_parse_allowlist(text, "allow.txt", &allow, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(allow.count, 2);
    BT_CHECK(strcmp(allow.entries[0].path, "src/a.c") == 0);
    BT_CHECK(strcmp(allow.entries[0].core, "42") == 0);
    BT_CHECK(strcmp(allow.entries[0].justification, "legacy table") == 0);
    BT_CHECK(strcmp(allow.entries[1].justification, "why\tdetail continues") == 0);
    BT_CHECK(policy_allowlist_covers(&allow, "src/a.c", "42") == 1);
    BT_CHECK(policy_allowlist_covers(&allow, "src/a.c", "7") == 0);
    BT_CHECK(policy_allowlist_covers(&allow, "src/b.c", "42") == 0);
    policy_allowlist_free(&allow);
}

BT_TEST(policy_lib, parse_allowlist_rejects_short_lines_naming_the_line) {
    policy_allowlist allow = {0};
    char err[POLICY_ERR_MAX];
    int rc = policy_parse_allowlist("src/a.c\t42\n", "allow.txt", &allow, err, sizeof err);
    BT_CHECK_EQ(rc, -1);
    BT_CHECK(strcmp(err, "allowlist allow.txt line 1 needs path TAB core TAB justification") == 0);
}

BT_TEST(policy_lib, run_checks_reports_relative_paths_and_both_kinds) {
    char root[POLICY_UTIL_PATH_MAX];
    policy_util_scratch("run-basic", root);
    plant(root, "src/dirty.c", POLICY_DIRTY_FIXTURE);
    plant(root, "src/clean.c", POLICY_CLEAN_FIXTURE);
    plant(root, "allow.txt", "# allow\n");
    policy_outcome outcome = {0};
    char err[POLICY_ERR_MAX];
    int rc = run(root, POLICY_MODE_BOTH, selftest_cfg(), "allow.txt", &outcome, err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(outcome.stale_count, 0);
    BT_CHECK_EQ(outcome.finding_count, 3);
    BT_CHECK(strcmp(outcome.findings[0].path, "src/dirty.c") == 0);
    BT_CHECK(strcmp(outcome.findings[0].detail, "line comment") == 0);
    BT_CHECK(strcmp(outcome.findings[1].detail, "block comment") == 0);
    BT_CHECK(strcmp(outcome.findings[2].detail, "literal 7 x2") == 0);
    BT_CHECK(strcmp(outcome.findings[2].kind, "constant") == 0);
    for (size_t at = 0; at < outcome.finding_count; at++) {
        BT_CHECK(strcmp(outcome.findings[at].path, "src/dirty.c") == 0);
    }
    policy_outcome_free(&outcome);
    policy_util_rm_rf(root);
}

BT_TEST(policy_lib, constants_scope_covers_impl_roots_and_exempts_tests) {
    char root[POLICY_UTIL_PATH_MAX];
    policy_util_scratch("run-scope", root);
    plant(root, "src/a.c", POLICY_DIRTY_FIXTURE);
    plant(root, "tools/b.c", POLICY_DIRTY_FIXTURE);
    plant(root, "tests/c.c", POLICY_DIRTY_FIXTURE);
    plant(root, "allow.txt", "# allow\n");
    policy_outcome outcome = {0};
    char err[POLICY_ERR_MAX];
    int rc = run(root, POLICY_MODE_CONSTANTS, selftest_cfg(), "allow.txt", &outcome, err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(outcome.finding_count, 2);
    BT_CHECK(strcmp(outcome.findings[0].path, "src/a.c") == 0);
    BT_CHECK(strcmp(outcome.findings[1].path, "tools/b.c") == 0);
    policy_outcome_free(&outcome);
    policy_util_rm_rf(root);
}

BT_TEST(policy_lib, allowlist_covers_recurring_pairs_and_flags_stale_entries) {
    char root[POLICY_UTIL_PATH_MAX];
    policy_util_scratch("run-allow", root);
    plant(root, "src/dirty.c", POLICY_DIRTY_FIXTURE);
    plant(root, "allow.txt", "src/dirty.c\t7\tlegacy\nsrc/gone.c\t9\tnone\n");
    policy_outcome outcome = {0};
    char err[POLICY_ERR_MAX];
    int rc = run(root, POLICY_MODE_CONSTANTS, selftest_cfg(), "allow.txt", &outcome, err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(outcome.finding_count, 0);
    BT_CHECK_EQ(outcome.stale_count, 1);
    BT_CHECK(strcmp(outcome.stale_allowlist[0], "src/gone.c TAB 9") == 0);
    policy_outcome_free(&outcome);
    policy_util_rm_rf(root);
}

BT_TEST(policy_lib, small_literal_allow_silences_a_core_everywhere) {
    char root[POLICY_UTIL_PATH_MAX];
    policy_util_scratch("run-small", root);
    plant(root, "src/dirty.c", POLICY_DIRTY_FIXTURE);
    plant(root, "allow.txt", "# allow\n");
    bt_json_value *config =
        parse_cfg("{\"policy\":{\"literal_recurrence\":2,\"small_literal_allow\":[\"7\"]}}");
    policy_outcome outcome = {0};
    char err[POLICY_ERR_MAX];
    int rc = run(root, POLICY_MODE_CONSTANTS, config, "allow.txt", &outcome, err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(outcome.finding_count, 0);
    policy_outcome_free(&outcome);
    bt_json_free(config);
    policy_util_rm_rf(root);
}

BT_TEST(policy_lib, missing_allowlist_file_is_a_harness_error) {
    char root[POLICY_UTIL_PATH_MAX];
    policy_util_scratch("run-miss", root);
    plant(root, "src/clean.c", POLICY_CLEAN_FIXTURE);
    policy_outcome outcome = {0};
    char err[POLICY_ERR_MAX];
    int rc = run(root, POLICY_MODE_CONSTANTS, selftest_cfg(), "absent.txt", &outcome, err);
    BT_CHECK_EQ(rc, -1);
    BT_CHECK(strncmp(err, "read ", 5) == 0);
    BT_CHECK(strstr(err, "absent.txt") != NULL);
    policy_util_rm_rf(root);
}

BT_TEST(policy_lib, comments_mode_ignores_the_allowlist_file) {
    char root[POLICY_UTIL_PATH_MAX];
    policy_util_scratch("run-comments", root);
    plant(root, "src/clean.c", POLICY_CLEAN_FIXTURE);
    policy_outcome outcome = {0};
    char err[POLICY_ERR_MAX];
    int rc = run(root, POLICY_MODE_COMMENTS, NULL, "absent.txt", &outcome, err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(outcome.finding_count, 0);
    policy_outcome_free(&outcome);
    policy_util_rm_rf(root);
}

BT_TEST(policy_lib, empty_scan_root_is_a_harness_error) {
    char root[POLICY_UTIL_PATH_MAX];
    policy_util_scratch("run-empty", root);
    policy_outcome outcome = {0};
    char err[POLICY_ERR_MAX];
    int rc = run(root, POLICY_MODE_BOTH, NULL, "allow.txt", &outcome, err);
    BT_CHECK_EQ(rc, -1);
    BT_CHECK(strncmp(err, "no sources under", 16) == 0);
    policy_util_rm_rf(root);
}

BT_TEST(policy_lib, walk_skips_excluded_directories) {
    char root[POLICY_UTIL_PATH_MAX];
    policy_util_scratch("walk-skip", root);
    plant(root, "src/real.c", POLICY_CLEAN_FIXTURE);
    plant(root, "src/target/gen.c", POLICY_DIRTY_FIXTURE);
    plant(root, "src/vendor/gen.c", POLICY_DIRTY_FIXTURE);
    plant(root, "src/build/gen.c", POLICY_DIRTY_FIXTURE);
    size_t count = 0;
    char **files = policy_collect_sources(root, &count);
    BT_CHECK_EQ(count, 1);
    if (count == 1) {
        size_t root_len = strlen(root);
        BT_CHECK(strcmp(files[0] + root_len + 1, "src/real.c") == 0);
    }
    policy_free_sources(files, count);
    policy_util_rm_rf(root);
}

BT_TEST(policy_lib, selftest_proves_both_detection_directions) {
    char workdir[POLICY_UTIL_PATH_MAX];
    policy_util_scratch("selftest", workdir);
    char err[POLICY_ERR_MAX];
    int rc = policy_selftest(workdir, err, sizeof err);
    BT_CHECK_EQ(rc, 1);
    char probe[POLICY_UTIL_PATH_MAX];
    policy_util_join(probe, workdir, "src/dirty.c");
    FILE *file = fopen(probe, "rb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        fclose(file);
    }
    policy_util_rm_rf(workdir);
}
