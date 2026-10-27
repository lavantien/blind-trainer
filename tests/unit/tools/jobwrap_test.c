#include "jobwrap/lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "common/bt_harness.h"
#include "common/json.h"
#include "common/paths.h"

static int parse_ok(const wchar_t *const argv[], int argc, bt_jobwrap_invocation *out) {
    char err[BT_JOBWRAP_ERR_MAX];
    err[0] = '\0';
    int rc = bt_jobwrap_parse_invocation(argc, argv, out, err, sizeof err);
    if (rc != 0) {
        fprintf(stderr, "parse_invocation: %s\n", err);
    }
    return rc;
}

static int parse_err(const wchar_t *const argv[], int argc) {
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
    const wchar_t *with_anchor[] = {
        L"--anchor", L"mutation", L"--", L"cargo", L"test",
    };
    BT_CHECK_EQ(parse_ok(with_anchor, 5, &invocation), 0);
    BT_CHECK_EQ(invocation.kind, BT_JOBWRAP_INVOCATION_WRAP);
    BT_CHECK(invocation.anchor != NULL && wcscmp(invocation.anchor, L"mutation") == 0);
    BT_CHECK_EQ(invocation.payload_count, 2U);
    BT_CHECK_EQ(wcscmp(invocation.payload[0], L"cargo"), 0);
    BT_CHECK_EQ(wcscmp(invocation.payload[1], L"test"), 0);
    const wchar_t *plain[] = {
        L"--",
        L"make",
        L"chain",
    };
    BT_CHECK_EQ(parse_ok(plain, 3, &invocation), 0);
    BT_CHECK(invocation.anchor == NULL);
    BT_CHECK_EQ(invocation.payload_count, 2U);
    BT_CHECK_EQ(wcscmp(invocation.payload[0], L"make"), 0);
    BT_CHECK_EQ(wcscmp(invocation.payload[1], L"chain"), 0);
}

BT_TEST(jobwrap, rejects_missing_payload_markers) {
    BT_CHECK(parse_err(NULL, 0));
    const wchar_t *marker_only[] = {
        L"--",
    };
    BT_CHECK(parse_err(marker_only, 1));
    const wchar_t *no_marker[] = {
        L"cargo",
        L"test",
    };
    BT_CHECK(parse_err(no_marker, 2));
    const wchar_t *dangling_anchor[] = {
        L"--anchor",
        L"x",
    };
    BT_CHECK(parse_err(dangling_anchor, 2));
    const wchar_t *anchor_no_payload[] = {
        L"--anchor",
        L"x",
        L"--",
    };
    BT_CHECK(parse_err(anchor_no_payload, 3));
}

BT_TEST(jobwrap, rejects_anchor_names_outside_the_memguard_alphabet) {
    BT_CHECK_EQ(bt_jobwrap_is_anchor_name(L"mutation-pkg-bt-core"), 1);
    BT_CHECK_EQ(bt_jobwrap_is_anchor_name(L"a.b_c"), 1);
    BT_CHECK_EQ(bt_jobwrap_is_anchor_name(L""), 0);
    BT_CHECK_EQ(bt_jobwrap_is_anchor_name(L"bad name"), 0);
    BT_CHECK_EQ(bt_jobwrap_is_anchor_name(L"a/b"), 0);
    BT_CHECK_EQ(bt_jobwrap_is_anchor_name(L"café"), 0);
    BT_CHECK_EQ(bt_jobwrap_is_anchor_name(L"愛"), 0);
    const wchar_t *bad[] = {
        L"--anchor",
        L"bad name",
        L"--",
        L"cargo",
    };
    BT_CHECK(parse_err(bad, 4));
}

BT_TEST(jobwrap, parses_selftest_modes) {
    bt_jobwrap_invocation invocation;
    const wchar_t *alloc[] = {
        L"--selftest-alloc-mb",
        L"640",
    };
    BT_CHECK_EQ(parse_ok(alloc, 2, &invocation), 0);
    BT_CHECK_EQ(invocation.kind, BT_JOBWRAP_INVOCATION_ALLOC_MB);
    BT_CHECK_EQ(invocation.value, 640ULL);
    const wchar_t *plus[] = {
        L"--selftest-alloc-mb",
        L"+640",
    };
    BT_CHECK_EQ(parse_ok(plus, 2, &invocation), 0);
    BT_CHECK_EQ(invocation.kind, BT_JOBWRAP_INVOCATION_ALLOC_MB);
    BT_CHECK_EQ(invocation.value, 640ULL);
    const wchar_t *double_plus[] = {
        L"--selftest-alloc-mb",
        L"++640",
    };
    BT_CHECK(parse_err(double_plus, 2));
    const wchar_t *hold[] = {
        L"--selftest-hold-secs",
        L"0",
    };
    BT_CHECK_EQ(parse_ok(hold, 2, &invocation), 0);
    BT_CHECK_EQ(invocation.kind, BT_JOBWRAP_INVOCATION_HOLD_SECS);
    BT_CHECK_EQ(invocation.value, 0ULL);
    const wchar_t *zero[] = {
        L"--selftest-alloc-mb",
        L"0",
    };
    BT_CHECK(parse_err(zero, 2));
    const wchar_t *abc[] = {
        L"--selftest-alloc-mb",
        L"abc",
    };
    BT_CHECK(parse_err(abc, 2));
    const wchar_t *missing[] = {
        L"--selftest-alloc-mb",
    };
    BT_CHECK(parse_err(missing, 1));
    const wchar_t *extra[] = {
        L"--selftest-hold-secs",
        L"3",
        L"4",
    };
    BT_CHECK(parse_err(extra, 3));
}

BT_TEST(jobwrap, parses_long_argument_lists_without_a_count_cap) {
    const wchar_t *argv[110];
    argv[0] = L"--";
    for (size_t at = 1; at < 109; at++) {
        argv[at] = L"arg";
    }
    argv[109] = L"tail";
    bt_jobwrap_invocation invocation;
    BT_CHECK_EQ(parse_ok(argv, 110, &invocation), 0);
    BT_CHECK_EQ(invocation.payload_count, 109U);
    BT_CHECK_EQ(wcscmp(invocation.payload[108], L"tail"), 0);
}

BT_TEST(jobwrap, quotes_plain_and_whitespace_arguments) {
    wchar_t out[64];
    BT_CHECK_EQ(bt_jobwrap_quote_argument(L"cargo", out, 64), 0);
    BT_CHECK_EQ(wcscmp(out, L"cargo"), 0);
    BT_CHECK_EQ(bt_jobwrap_quote_argument(L"a b", out, 64), 0);
    BT_CHECK_EQ(wcscmp(out, L"\"a b\""), 0);
    BT_CHECK_EQ(bt_jobwrap_quote_argument(L"", out, 64), 0);
    BT_CHECK_EQ(wcscmp(out, L"\"\""), 0);
    const wchar_t *payload[] = {
        L"a b",
        L"c",
    };
    wchar_t line[64];
    BT_CHECK_EQ(bt_jobwrap_command_line(payload, 2, line, 64), 0);
    BT_CHECK_EQ(wcscmp(line, L"\"a b\" c"), 0);
}

BT_TEST(jobwrap, doubles_trailing_backslashes_inside_quotes) {
    wchar_t out[64];
    BT_CHECK_EQ(bt_jobwrap_quote_argument(L"C:\\work dir\\", out, 64), 0);
    BT_CHECK_EQ(wcscmp(out, L"\"C:\\work dir\\\\\""), 0);
    const wchar_t *payload[] = {
        L"C:\\work dir\\",
        L"plain",
    };
    wchar_t line[64];
    BT_CHECK_EQ(bt_jobwrap_command_line(payload, 2, line, 64), 0);
    BT_CHECK_EQ(wcscmp(line, L"\"C:\\work dir\\\\\" plain"), 0);
}

BT_TEST(jobwrap, escapes_embedded_quotes_even_without_whitespace) {
    wchar_t out[64];
    BT_CHECK_EQ(bt_jobwrap_quote_argument(L"he said \"hi\"", out, 64), 0);
    BT_CHECK_EQ(wcscmp(out, L"\"he said \\\"hi\\\"\""), 0);
    BT_CHECK_EQ(bt_jobwrap_quote_argument(L"a\\\"b", out, 64), 0);
    BT_CHECK_EQ(wcscmp(out, L"\"a\\\\\\\"b\""), 0);
}

BT_TEST(jobwrap, passes_non_ascii_arguments_through_untouched) {
    wchar_t out[64];
    BT_CHECK_EQ(bt_jobwrap_quote_argument(L"café", out, 64), 0);
    BT_CHECK_EQ(wcscmp(out, L"café"), 0);
    BT_CHECK_EQ(wcslen(out), 4U);
    BT_CHECK_EQ(bt_jobwrap_quote_argument(L"愛 pwsh", out, 64), 0);
    BT_CHECK_EQ(wcslen(out), 8U);
    BT_CHECK_EQ(out[0], L'"');
    BT_CHECK_EQ(out[1], 0x611b);
    BT_CHECK_EQ(bt_jobwrap_quote_argument(L"café 愛", out, 64), 0);
    BT_CHECK_EQ(out[0], L'"');
    BT_CHECK_EQ(out[1], L'c');
    BT_CHECK_EQ(out[4], 0x00e9);
    BT_CHECK_EQ(out[5], L' ');
    BT_CHECK_EQ(out[6], 0x611b);
    const wchar_t *payload[] = {
        L"café",
        L"plain",
    };
    wchar_t line[64];
    BT_CHECK_EQ(bt_jobwrap_command_line(payload, 2, line, 64), 0);
    BT_CHECK_EQ(wcslen(line), 10U);
    BT_CHECK_EQ(wcscmp(line, L"café plain"), 0);
}

BT_TEST(jobwrap, command_line_rejects_over_budget_payloads) {
    wchar_t huge[BT_JOBWRAP_CMDLINE_MAX + 4];
    for (size_t at = 0; at < BT_JOBWRAP_CMDLINE_MAX + 3; at++) {
        huge[at] = L'a';
    }
    huge[BT_JOBWRAP_CMDLINE_MAX + 3] = L'\0';
    const wchar_t *payload[] = {
        huge,
    };
    wchar_t line[BT_JOBWRAP_CMDLINE_MAX];
    BT_CHECK(bt_jobwrap_command_line(payload, 1, line, BT_JOBWRAP_CMDLINE_MAX) != 0);
    BT_CHECK(bt_jobwrap_quote_argument(huge, line, BT_JOBWRAP_CMDLINE_MAX) != 0);
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
