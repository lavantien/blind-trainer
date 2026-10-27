#include "jobwrap/lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_harness.h"
#include "common/json.h"
#include "common/paths.h"

static int parse_ok(char **argv, int argc, bt_jobwrap_invocation *out) {
    char err[BT_JOBWRAP_ERR_MAX];
    err[0] = '\0';
    int rc = bt_jobwrap_parse_invocation(argc, argv, out, err, sizeof err);
    if (rc != 0) {
        fprintf(stderr, "parse_invocation: %s\n", err);
    }
    return rc;
}

static int parse_err(char **argv, int argc) {
    bt_jobwrap_invocation invocation;
    char err[BT_JOBWRAP_ERR_MAX];
    err[0] = '\0';
    int rc = bt_jobwrap_parse_invocation(argc, argv, &invocation, err, sizeof err);
    if (rc == 0) {
        fprintf(stderr, "parse_invocation unexpectedly ok\n");
    }
    return rc != 0 && err[0] != '\0';
}

static bt_json_value *config_of(const char *body) {
    bt_json_value *config = NULL;
    bt_json_error error;
    if (bt_json_parse(body, strlen(body), &config, &error) != 0) {
        BT_ABORT("bad test json: %s", error.message);
    }
    return config;
}

BT_TEST(jobwrap, parses_anchor_and_payload) {
    bt_jobwrap_invocation invocation;
    char *with_anchor[] = {
        (char *)"--anchor", (char *)"mutation", (char *)"--", (char *)"cargo", (char *)"test",
    };
    BT_CHECK_EQ(parse_ok(with_anchor, 5, &invocation), 0);
    BT_CHECK_EQ(invocation.kind, BT_JOBWRAP_INVOCATION_WRAP);
    BT_CHECK(invocation.anchor != NULL && strcmp(invocation.anchor, "mutation") == 0);
    BT_CHECK_EQ(invocation.payload_count, 2U);
    BT_CHECK_EQ(strcmp(invocation.payload[0], "cargo"), 0);
    BT_CHECK_EQ(strcmp(invocation.payload[1], "test"), 0);
    char *plain[] = {(char *)"--", (char *)"make", (char *)"chain"};
    BT_CHECK_EQ(parse_ok(plain, 3, &invocation), 0);
    BT_CHECK(invocation.anchor == NULL);
    BT_CHECK_EQ(invocation.payload_count, 2U);
    BT_CHECK_EQ(strcmp(invocation.payload[0], "make"), 0);
    BT_CHECK_EQ(strcmp(invocation.payload[1], "chain"), 0);
}

BT_TEST(jobwrap, rejects_missing_payload_markers) {
    BT_CHECK(parse_err(NULL, 0));
    char *marker_only[] = {(char *)"--"};
    BT_CHECK(parse_err(marker_only, 1));
    char *no_marker[] = {(char *)"cargo", (char *)"test"};
    BT_CHECK(parse_err(no_marker, 2));
    char *dangling_anchor[] = {(char *)"--anchor", (char *)"x"};
    BT_CHECK(parse_err(dangling_anchor, 2));
    char *anchor_no_payload[] = {(char *)"--anchor", (char *)"x", (char *)"--"};
    BT_CHECK(parse_err(anchor_no_payload, 3));
}

BT_TEST(jobwrap, rejects_anchor_names_outside_the_memguard_alphabet) {
    BT_CHECK_EQ(bt_jobwrap_is_anchor_name("mutation-pkg-bt-core"), 1);
    BT_CHECK_EQ(bt_jobwrap_is_anchor_name("a.b_c"), 1);
    BT_CHECK_EQ(bt_jobwrap_is_anchor_name(""), 0);
    BT_CHECK_EQ(bt_jobwrap_is_anchor_name("bad name"), 0);
    BT_CHECK_EQ(bt_jobwrap_is_anchor_name("a/b"), 0);
    char *bad[] = {(char *)"--anchor", (char *)"bad name", (char *)"--", (char *)"cargo"};
    BT_CHECK(parse_err(bad, 4));
}

BT_TEST(jobwrap, parses_selftest_modes) {
    bt_jobwrap_invocation invocation;
    char *alloc[] = {(char *)"--selftest-alloc-mb", (char *)"640"};
    BT_CHECK_EQ(parse_ok(alloc, 2, &invocation), 0);
    BT_CHECK_EQ(invocation.kind, BT_JOBWRAP_INVOCATION_ALLOC_MB);
    BT_CHECK_EQ(invocation.value, 640ULL);
    char *hold[] = {(char *)"--selftest-hold-secs", (char *)"0"};
    BT_CHECK_EQ(parse_ok(hold, 2, &invocation), 0);
    BT_CHECK_EQ(invocation.kind, BT_JOBWRAP_INVOCATION_HOLD_SECS);
    BT_CHECK_EQ(invocation.value, 0ULL);
    char *zero[] = {(char *)"--selftest-alloc-mb", (char *)"0"};
    BT_CHECK(parse_err(zero, 2));
    char *abc[] = {(char *)"--selftest-alloc-mb", (char *)"abc"};
    BT_CHECK(parse_err(abc, 2));
    char *missing[] = {(char *)"--selftest-alloc-mb"};
    BT_CHECK(parse_err(missing, 1));
    char *extra[] = {(char *)"--selftest-hold-secs", (char *)"3", (char *)"4"};
    BT_CHECK(parse_err(extra, 3));
}

BT_TEST(jobwrap, quotes_plain_and_whitespace_arguments) {
    char out[64];
    BT_CHECK_EQ(bt_jobwrap_quote_argument("cargo", out, sizeof out), 0);
    BT_CHECK_EQ(strcmp(out, "cargo"), 0);
    BT_CHECK_EQ(bt_jobwrap_quote_argument("a b", out, sizeof out), 0);
    BT_CHECK_EQ(strcmp(out, "\"a b\""), 0);
    BT_CHECK_EQ(bt_jobwrap_quote_argument("", out, sizeof out), 0);
    BT_CHECK_EQ(strcmp(out, "\"\""), 0);
    char *payload[] = {(char *)"a b", (char *)"c"};
    char line[64];
    BT_CHECK_EQ(bt_jobwrap_command_line(payload, 2, line, sizeof line), 0);
    BT_CHECK_EQ(strcmp(line, "\"a b\" c"), 0);
}

BT_TEST(jobwrap, doubles_trailing_backslashes_inside_quotes) {
    char out[64];
    BT_CHECK_EQ(bt_jobwrap_quote_argument("C:\\work dir\\", out, sizeof out), 0);
    BT_CHECK_EQ(strcmp(out, "\"C:\\work dir\\\\\""), 0);
    char *payload[] = {(char *)"C:\\work dir\\", (char *)"plain"};
    char line[64];
    BT_CHECK_EQ(bt_jobwrap_command_line(payload, 2, line, sizeof line), 0);
    BT_CHECK_EQ(strcmp(line, "\"C:\\work dir\\\\\" plain"), 0);
}

BT_TEST(jobwrap, escapes_embedded_quotes_even_without_whitespace) {
    char out[64];
    BT_CHECK_EQ(bt_jobwrap_quote_argument("he said \"hi\"", out, sizeof out), 0);
    BT_CHECK_EQ(strcmp(out, "\"he said \\\"hi\\\"\""), 0);
    BT_CHECK_EQ(bt_jobwrap_quote_argument("a\\\"b", out, sizeof out), 0);
    BT_CHECK_EQ(strcmp(out, "\"a\\\\\\\"b\""), 0);
}

BT_TEST(jobwrap, dotnet_ticks_anchor_at_known_epochs) {
    BT_CHECK_EQ(bt_jobwrap_dotnet_ticks(0), 504911232000000000ULL);
    BT_CHECK_EQ(bt_jobwrap_dotnet_ticks(116444736000000000ULL), 621355968000000000ULL);
}

BT_TEST(jobwrap, per_process_bytes_validates_the_hub_section) {
    unsigned long long bytes = 0;
    char err[BT_JOBWRAP_ERR_MAX];
    bt_json_value *config = config_of("{\"job\":{\"per_process_bytes\":4294967296}}");
    BT_CHECK_EQ(bt_jobwrap_per_process_bytes(config, &bytes, err, sizeof err), 0);
    BT_CHECK_EQ(bytes, 4294967296ULL);
    bt_json_free(config);

    config = config_of("{}");
    BT_CHECK(bt_jobwrap_per_process_bytes(config, &bytes, err, sizeof err) != 0);
    BT_CHECK_EQ(strcmp(err, "config section missing: job.per_process_bytes"), 0);
    bt_json_free(config);

    config = config_of("{\"job\":{\"per_process_bytes\":\"big\"}}");
    BT_CHECK(bt_jobwrap_per_process_bytes(config, &bytes, err, sizeof err) != 0);
    BT_CHECK_EQ(strcmp(err, "config value malformed: job.per_process_bytes"), 0);
    bt_json_free(config);

    config = config_of("{\"job\":{\"per_process_bytes\":0}}");
    BT_CHECK(bt_jobwrap_per_process_bytes(config, &bytes, err, sizeof err) != 0);
    BT_CHECK_EQ(strcmp(err, "config value invalid: job.per_process_bytes must be > 0"), 0);
    bt_json_free(config);
}

BT_TEST(jobwrap, pid_dir_resolves_absolute_and_repo_relative) {
    char out[BT_PATH_MAX];
    char err[BT_JOBWRAP_ERR_MAX];
    const char *temp = getenv("TEMP");
    BT_CHECK(temp != NULL);
    char absolute[BT_PATH_MAX];
    snprintf(absolute, sizeof absolute, "%s/pids", temp);
    for (char *cursor = absolute; *cursor != '\0'; cursor++) {
        if (*cursor == '\\') {
            *cursor = '/';
        }
    }
    char body[BT_PATH_MAX * 2];
    snprintf(body, sizeof body, "{\"memguard\":{\"pid_dir\":\"%s\"}}", absolute);
    bt_json_value *config = config_of(body);
    BT_CHECK_EQ(bt_jobwrap_pid_dir_from(config, out, err, sizeof err), 0);
    BT_CHECK_EQ(strcmp(out, absolute), 0);
    bt_json_free(config);

    config = config_of("{\"memguard\":{\"pid_dir\":\"artifacts/progress/pids\"}}");
    char root[BT_PATH_MAX];
    bt_jobwrap_workspace_root(root);
    char expected[BT_PATH_MAX];
    snprintf(expected, sizeof expected, "%s/artifacts/progress/pids", root);
    BT_CHECK_EQ(bt_jobwrap_pid_dir_from(config, out, err, sizeof err), 0);
    BT_CHECK_EQ(strcmp(out, expected), 0);
    bt_json_free(config);

    config = config_of("{}");
    BT_CHECK(bt_jobwrap_pid_dir_from(config, out, err, sizeof err) != 0);
    BT_CHECK_EQ(strcmp(err, "config section missing: memguard.pid_dir"), 0);
    bt_json_free(config);

    config = config_of("{\"memguard\":{\"pid_dir\":\"  \"}}");
    BT_CHECK(bt_jobwrap_pid_dir_from(config, out, err, sizeof err) != 0);
    BT_CHECK_EQ(strcmp(err, "config value invalid: memguard.pid_dir"), 0);
    bt_json_free(config);
}

BT_TEST(jobwrap, config_path_honors_env_and_default) {
    char out[BT_PATH_MAX];
    char root[BT_PATH_MAX];
    bt_jobwrap_workspace_root(root);
    char expected[BT_PATH_MAX];
    bt_jobwrap_config_path_from(NULL, out);
    snprintf(expected, sizeof expected, "%s/config/config.json", root);
    BT_CHECK_EQ(strcmp(out, expected), 0);
    bt_jobwrap_config_path_from("other/conf.json", out);
    snprintf(expected, sizeof expected, "%s/other/conf.json", root);
    BT_CHECK_EQ(strcmp(out, expected), 0);
    snprintf(expected, sizeof expected, "%s/x.json", root);
    bt_jobwrap_config_path_from(expected, out);
    BT_CHECK_EQ(strcmp(out, expected), 0);
}
