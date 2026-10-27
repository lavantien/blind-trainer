#include "common/bt_harness.h"

#include <stdio.h>
#include <string.h>

#include "policy/constants.h"
#include "policy/lexer.h"
#include "policy/lib.h"

static void check_cores(const char *text, policy_lang lang, const char *want) {
    size_t count = 0;
    policy_occurrence *hits = policy_scan_literals(text, lang, &count);
    char got[512];
    size_t used = 0;
    got[0] = '\0';
    for (size_t at = 0; at < count; at++) {
        if (used > 0) {
            used += (size_t)snprintf(got + used, sizeof got - used, ",");
            if (used >= sizeof got) {
                break;
            }
        }
        used += (size_t)snprintf(got + used, sizeof got - used, "%s", hits[at].core);
        if (used >= sizeof got) {
            break;
        }
    }
    if (strcmp(got, want) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "cores of \"%s\": got \"%s\" want \"%s\"", text, got,
                        want);
    }
    policy_occurrences_free(hits, count);
}

static void check_masked_cores(const char *text, policy_lang lang, const char *want) {
    policy_masked masked;
    policy_mask_source(text, strlen(text), lang, &masked);
    check_cores(masked.text, lang, want);
    policy_masked_free(&masked);
}

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
    policy_policy_config cfg = {0};
    char err[POLICY_ERR_MAX];
    int rc = policy_constants_from_config(config, &cfg, err, sizeof err);
    BT_CHECK_EQ(rc, -1);
    BT_CHECK(strcmp(err, want) == 0);
    bt_json_free(config);
}

BT_TEST(policy_constants, loop_bounds_yield_both_ends) {
    check_cores("for (int i = 0; i < 8; i++) {}", POLICY_LANG_C, "0,8");
}

BT_TEST(policy_constants, python_slice_bounds_yield_both_ends) {
    check_cores("x = a[0:9]", POLICY_LANG_PY, "0,9");
}

BT_TEST(policy_constants, trailing_dot_float_stays_one_literal) {
    check_cores("double x = 1.;", POLICY_LANG_C, "1.");
    check_cores("double x = 1.5;", POLICY_LANG_C, "1.5");
}

BT_TEST(policy_constants, separators_normalize_to_the_same_core) {
    check_cores("int sum = 1'000 + 1000;", POLICY_LANG_C, "1000,1000");
    check_cores("s = 1_000 + 1000", POLICY_LANG_PY, "1000,1000");
}

BT_TEST(policy_constants, integer_cores_normalize_to_decimal) {
    check_cores("0x1F", POLICY_LANG_C, "31");
    check_cores("0b1010", POLICY_LANG_C, "10");
    check_cores("0o17", POLICY_LANG_PY, "15");
    check_cores("0x1F + 31 + 037", POLICY_LANG_C, "31,31,31");
}

BT_TEST(policy_constants, exponents_stay_in_the_core) {
    check_cores("1.5e3", POLICY_LANG_C, "1.5e3");
    check_cores("2e10", POLICY_LANG_C, "2e10");
    check_cores("7E-2", POLICY_LANG_C, "7E-2");
}

BT_TEST(policy_constants, hex_float_literals_form_one_core) {
    check_cores("double x = 0x1.8p3;", POLICY_LANG_C, "0x1.8p3");
    check_cores("double y = 0x1p3;", POLICY_LANG_C, "0x1p3");
}

BT_TEST(policy_constants, type_suffixes_are_stripped) {
    check_cores("1u", POLICY_LANG_C, "1");
    check_cores("2ull", POLICY_LANG_C, "2");
    check_cores("1.5f", POLICY_LANG_C, "1.5");
    check_cores("0xFFul", POLICY_LANG_C, "255");
    check_cores("2j", POLICY_LANG_PY, "2");
}

BT_TEST(policy_constants, leading_dot_floats_count_but_member_access_stays_excluded) {
    check_cores("x = .5 + .25;", POLICY_LANG_PY, ".5,.25");
    check_cores("y = obj.field", POLICY_LANG_PY, "");
    check_cores("int a = point.0;", POLICY_LANG_C, "");
    check_cores("int b = nested.0.1;", POLICY_LANG_C, "");
}

BT_TEST(policy_constants, digits_after_utf8_continuation_bytes_stay_excluded) {
    check_cores("v = a\xc3\xa9"
                "2 + 1;",
                POLICY_LANG_C, "1");
}

BT_TEST(policy_constants, identifier_adjacent_digits_are_excluded) {
    check_cores("sha256 of md5 at v2", POLICY_LANG_C, "");
}

BT_TEST(policy_constants, literals_inside_strings_and_comments_are_invisible) {
    check_masked_cores("const char *s = \"7 7 7\";", POLICY_LANG_C, "");
    check_masked_cores("int a = 7; // 9 9\nint b = 7;", POLICY_LANG_C, "7,7");
    check_masked_cores("s = \"7 7 7\"", POLICY_LANG_PY, "");
}

BT_TEST(policy_constants, char_literals_hide_their_digits) {
    check_masked_cores("char c = '7';", POLICY_LANG_C, "");
}

BT_TEST(policy_constants, occurrences_carry_one_based_lines) {
    const char *text = "int a = 1;\nint b = 2;\nint c = 1;";
    size_t count = 0;
    policy_occurrence *hits = policy_scan_literals(text, POLICY_LANG_C, &count);
    BT_CHECK_EQ(count, 3);
    BT_CHECK_EQ(hits[0].line, 1);
    BT_CHECK_EQ(hits[1].line, 2);
    BT_CHECK_EQ(hits[2].line, 3);
    policy_occurrences_free(hits, count);
}

BT_TEST(policy_constants, recurring_counts_per_core_within_one_file) {
    const char *text = "int a = 7;\nint b = 7;\nint c = 3;\nint d = 3;\nint e = 3;";
    size_t count = 0;
    policy_recurring *hits = policy_recurring_literals(text, POLICY_LANG_C, 2, &count);
    BT_CHECK_EQ(count, 2);
    const policy_recurring *seven = NULL;
    const policy_recurring *three = NULL;
    for (size_t at = 0; at < count; at++) {
        if (strcmp(hits[at].core, "7") == 0) {
            seven = &hits[at];
        }
        if (strcmp(hits[at].core, "3") == 0) {
            three = &hits[at];
        }
    }
    BT_CHECK(seven != NULL);
    BT_CHECK(three != NULL);
    BT_CHECK_EQ(seven->count, 2);
    BT_CHECK_EQ(seven->line, 1);
    BT_CHECK_EQ(three->count, 3);
    BT_CHECK_EQ(three->line, 3);
    policy_recurring_free(hits, count);
    count = 0;
    hits = policy_recurring_literals(text, POLICY_LANG_C, 3, &count);
    BT_CHECK_EQ(count, 1);
    policy_recurring_free(hits, count);
    count = 0;
    hits = policy_recurring_literals(text, POLICY_LANG_C, 4, &count);
    BT_CHECK_EQ(count, 0);
    policy_recurring_free(hits, count);
}

BT_TEST(policy_constants, policy_section_parses_when_present) {
    bt_json_value *config =
        parse_cfg("{\"policy\": {\"literal_recurrence\": 2, \"small_literal_allow\": [\"1\", "
                  "\"2\"]}}");
    policy_policy_config cfg = {0};
    char err[POLICY_ERR_MAX];
    int rc = policy_constants_from_config(config, &cfg, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(cfg.literal_recurrence, 2);
    BT_CHECK_EQ(cfg.allow_count, 2);
    BT_CHECK(strcmp(cfg.small_literal_allow[0], "1") == 0);
    BT_CHECK(strcmp(cfg.small_literal_allow[1], "2") == 0);
    policy_policy_config_free(&cfg);
    bt_json_free(config);
}

BT_TEST(policy_constants, missing_policy_section_is_an_error_naming_the_key) {
    config_err_is("{}", "config section missing: policy");
}

BT_TEST(policy_constants, missing_policy_keys_are_errors_naming_the_keys) {
    config_err_is("{\"policy\": {\"small_literal_allow\": []}}",
                  "config section missing: policy.literal_recurrence");
    config_err_is("{\"policy\": {\"literal_recurrence\": 2}}",
                  "config section missing: policy.small_literal_allow");
}

BT_TEST(policy_constants, malformed_or_zero_recurrence_is_an_error) {
    config_err_is("{\"policy\": {\"literal_recurrence\": \"2\", \"small_literal_allow\": []}}",
                  "config value malformed: policy.literal_recurrence");
    config_err_is("{\"policy\": {\"literal_recurrence\": 2.0, \"small_literal_allow\": []}}",
                  "config value malformed: policy.literal_recurrence");
    config_err_is("{\"policy\": {\"literal_recurrence\": 2.5, \"small_literal_allow\": []}}",
                  "config value malformed: policy.literal_recurrence");
    config_err_is("{\"policy\": {\"literal_recurrence\": -1, \"small_literal_allow\": []}}",
                  "config value malformed: policy.literal_recurrence");
    config_err_is("{\"policy\": {\"literal_recurrence\": 0, \"small_literal_allow\": []}}",
                  "policy.literal_recurrence must be >= 1");
    config_err_is("{\"policy\": {\"literal_recurrence\": 2, \"small_literal_allow\": [1]}}",
                  "config value malformed: policy.small_literal_allow");
    config_err_is("{\"policy\": {\"literal_recurrence\": 2, \"small_literal_allow\": \"1\"}}",
                  "config value malformed: policy.small_literal_allow");
}
