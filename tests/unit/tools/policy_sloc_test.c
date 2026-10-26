#include "common/bt_harness.h"

#include <stdio.h>
#include <string.h>

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

static void config_err_is(const char *text, const char *want) {
    bt_json_value *config = parse_cfg(text);
    policy_sloc_config cfg = {0};
    char err[POLICY_ERR_MAX];
    int rc = policy_sloc_from_config(config, &cfg, err, sizeof err);
    BT_CHECK_EQ(rc, -1);
    BT_CHECK(strcmp(err, want) == 0);
    bt_json_free(config);
}

BT_TEST(policy_sloc, config_section_parses_when_present) {
    bt_json_value *config = parse_cfg("{\"sloc\": {\"max_file_lines\": 1000}}");
    policy_sloc_config cfg = {0};
    char err[POLICY_ERR_MAX];
    int rc = policy_sloc_from_config(config, &cfg, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(cfg.max_file_lines, 1000);
    bt_json_free(config);
}

BT_TEST(policy_sloc, missing_or_malformed_sections_are_errors_naming_the_key) {
    config_err_is("{}", "config section missing: sloc.max_file_lines");
    config_err_is("{\"sloc\": {}}", "config section missing: sloc.max_file_lines");
    config_err_is("{\"sloc\": {\"max_file_lines\": \"1000\"}}",
                  "config value malformed: sloc.max_file_lines");
    config_err_is("{\"sloc\": {\"max_file_lines\": 1000.0}}",
                  "config value malformed: sloc.max_file_lines");
    config_err_is("{\"sloc\": {\"max_file_lines\": 1000.5}}",
                  "config value malformed: sloc.max_file_lines");
    config_err_is("{\"sloc\": {\"max_file_lines\": -1}}",
                  "config value malformed: sloc.max_file_lines");
    config_err_is("{\"sloc\": {\"max_file_lines\": 0}}", "sloc.max_file_lines must be >= 1");
}

BT_TEST(policy_sloc, blank_lines_do_not_count_and_the_cap_is_inclusive) {
    const char *at_cap =
        "int a(void) {}\n\nint b(void) {}\n\n\nint x = 1;\nint y = 2;\nint z = 3;\n";
    policy_finding finding = {0};
    int rc = policy_cap_finding(at_cap, strlen(at_cap), "src/a.c", 5, &finding);
    BT_CHECK_EQ(rc, 0);
}

BT_TEST(policy_sloc, one_line_over_cap_fires_with_the_crossing_line) {
    const char *over = "int a(void) {}\nint b(void) {}\nint c(void) {}\nint d(void) {}\nint "
                       "e(void) {}\nint f(void) {}\n";
    policy_finding finding = {0};
    int rc = policy_cap_finding(over, strlen(over), "src/big.c", 5, &finding);
    BT_CHECK_EQ(rc, 1);
    BT_CHECK(strcmp(finding.kind, "sloc") == 0);
    BT_CHECK(strcmp(finding.path, "src/big.c") == 0);
    BT_CHECK_EQ(finding.line, 6);
    BT_CHECK(strcmp(finding.detail, "6 non-blank lines over cap 5") == 0);
    policy_finding_free(&finding);
}

BT_TEST(policy_sloc, the_crossing_line_skips_blank_lines) {
    const char *over = "a\n\nb\nc\nd\ne\n\nf\n";
    policy_finding finding = {0};
    int rc = policy_cap_finding(over, strlen(over), "src/x.c", 5, &finding);
    BT_CHECK_EQ(rc, 1);
    BT_CHECK_EQ(finding.line, 8);
    BT_CHECK(strcmp(finding.detail, "6 non-blank lines over cap 5") == 0);
    policy_finding_free(&finding);
}

BT_TEST(policy_sloc, an_empty_file_stays_silent) {
    policy_finding finding = {0};
    BT_CHECK_EQ(policy_cap_finding("", 0, "src/e.c", 1, &finding), 0);
    BT_CHECK_EQ(policy_cap_finding("\n\n  \n", 4, "src/e.c", 1, &finding), 0);
}
