#include "jobwrap/lib.h"

#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

#include "common/bt_harness.h"
#include "common/json.h"
#include "common/paths.h"
#include "jobwrap/ffi.h"
#include "unit/tools/spawn_util.h"

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

BT_TEST(jobwrap, command_line_joins_at_the_exact_cap_without_overflow) {
    wchar_t tail[64];
    tail[0] = L'a';
    for (size_t at = 1; at < 62; at++) {
        tail[at] = L'b';
    }
    tail[62] = L'\0';
    const wchar_t *exact[] = {
        tail,
    };
    wchar_t out[64];
    BT_CHECK_EQ(bt_jobwrap_command_line(exact, 1, out, 64), 0);
    BT_CHECK_EQ(wcslen(out), 62U);
    wchar_t second[64];
    second[0] = L'c';
    for (size_t at = 1; at < 61; at++) {
        second[at] = L'b';
    }
    second[61] = L'\0';
    const wchar_t *joined[] = {
        L"a",
        second,
    };
    wchar_t guard_out[65];
    guard_out[64] = L'Z';
    BT_CHECK_EQ(bt_jobwrap_command_line(joined, 2, guard_out, 64), 0);
    BT_CHECK_EQ(wcslen(guard_out), 63U);
    BT_CHECK_EQ(guard_out[63], L'\0');
    BT_CHECK_EQ(guard_out[64], L'Z');
}

BT_TEST(jobwrap, command_line_rejects_one_over_the_cap_with_multiple_arguments) {
    wchar_t second[64];
    second[0] = L'c';
    for (size_t at = 1; at < 62; at++) {
        second[at] = L'b';
    }
    second[62] = L'\0';
    const wchar_t *joined[] = {
        L"a",
        second,
    };
    wchar_t out[65];
    out[64] = L'Z';
    BT_CHECK(bt_jobwrap_command_line(joined, 2, out, 64) != 0);
    BT_CHECK_EQ(out[64], L'Z');
    wchar_t wide_tail[BT_JOBWRAP_CMDLINE_MAX];
    for (size_t at = 0; at < BT_JOBWRAP_CMDLINE_MAX - 3; at++) {
        wide_tail[at] = L'b';
    }
    wide_tail[BT_JOBWRAP_CMDLINE_MAX - 3] = L'\0';
    const wchar_t *wide_joined[] = {
        L"a",
        wide_tail,
    };
    wchar_t wide_out[BT_JOBWRAP_CMDLINE_MAX];
    BT_CHECK_EQ(bt_jobwrap_command_line(wide_joined, 2, wide_out, BT_JOBWRAP_CMDLINE_MAX), 0);
    BT_CHECK_EQ(wcslen(wide_out), BT_JOBWRAP_CMDLINE_MAX - 1U);
    wchar_t over_tail[BT_JOBWRAP_CMDLINE_MAX];
    for (size_t at = 0; at < BT_JOBWRAP_CMDLINE_MAX - 2; at++) {
        over_tail[at] = L'b';
    }
    over_tail[BT_JOBWRAP_CMDLINE_MAX - 2] = L'\0';
    const wchar_t *over_joined[] = {
        L"a",
        over_tail,
    };
    BT_CHECK(bt_jobwrap_command_line(over_joined, 2, wide_out, BT_JOBWRAP_CMDLINE_MAX) != 0);
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

BT_TEST(jobwrap, pid_dir_blank_check_covers_unicode_whitespace) {
    char out[BT_PATH_MAX];
    char err[BT_JOBWRAP_ERR_MAX];
    bt_json_value *config = config_of("{\"memguard\":{\"pid_dir\":\"\xc2\xa0\"}}");
    BT_CHECK(bt_jobwrap_pid_dir_from(config, out, err, sizeof err) != 0);
    BT_CHECK_EQ(strcmp(err, "config value invalid: memguard.pid_dir"), 0);
    bt_json_free(config);
    config = config_of("{\"memguard\":{\"pid_dir\":\"\xe3\x80\x80\"}}");
    BT_CHECK(bt_jobwrap_pid_dir_from(config, out, err, sizeof err) != 0);
    BT_CHECK_EQ(strcmp(err, "config value invalid: memguard.pid_dir"), 0);
    bt_json_free(config);
    config = config_of("{\"memguard\":{\"pid_dir\":\"\xc2\xa0x\xe2\x80\x8a\"}}");
    BT_CHECK_EQ(bt_jobwrap_pid_dir_from(config, out, err, sizeof err), 0);
    bt_json_free(config);
}

BT_TEST(jobwrap, pid_dir_joins_like_a_rust_path_push) {
    char out[BT_PATH_MAX];
    char err[BT_JOBWRAP_ERR_MAX];
    char root[BT_PATH_MAX];
    bt_jobwrap_workspace_root(root);
    char expected[BT_PATH_MAX];
    bt_json_value *config = config_of("{\"memguard\":{\"pid_dir\":\"C:temp\"}}");
    BT_CHECK_EQ(bt_jobwrap_pid_dir_from(config, out, err, sizeof err), 0);
    BT_CHECK_EQ(strcmp(out, "C:temp"), 0);
    bt_json_free(config);
    config = config_of("{\"memguard\":{\"pid_dir\":\"/tmp/pids\"}}");
    snprintf(expected, sizeof expected, "%2.2s/tmp/pids", root);
    BT_CHECK_EQ(bt_jobwrap_pid_dir_from(config, out, err, sizeof err), 0);
    BT_CHECK_EQ(strcmp(out, expected), 0);
    bt_json_free(config);
    config = config_of("{\"memguard\":{\"pid_dir\":\"\\\\\\\\server\\\\\\\\share\\\\\\\\pids\"}}");
    BT_CHECK_EQ(bt_jobwrap_pid_dir_from(config, out, err, sizeof err), 0);
    BT_CHECK_EQ(strcmp(out, "\\\\server\\\\share\\\\pids"), 0);
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

static char g_config_saved[BT_PATH_MAX];
static int g_config_had = -1;

static void set_config_env(const char *value) {
    if (g_config_had < 0) {
        const char *raw = getenv("BT_CONFIG");
        g_config_had = raw != NULL && raw[0] != '\0';
        snprintf(g_config_saved, sizeof g_config_saved, "%s", raw != NULL ? raw : "");
    }
    if (value != NULL) {
        _putenv_s("BT_CONFIG", value);
    } else {
        _putenv("BT_CONFIG=");
    }
}

static void restore_config_env(void) {
    if (g_config_had > 0) {
        _putenv_s("BT_CONFIG", g_config_saved);
    } else {
        _putenv("BT_CONFIG=");
    }
}

static void write_config_pair(char dir[BT_PATH_MAX], const char *tag, const char *body) {
    snprintf(dir, BT_PATH_MAX, "%s\\bt-jobwrap-cfg-%s-%d", getenv("TEMP"), tag, _getpid());
    CreateDirectoryA(dir, NULL);
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s\\config.json", dir);
    FILE *file = fopen(path, "wb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        fputs(body, file);
        fclose(file);
    }
}

static void drop_config_dir(const char *dir) {
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s\\config.json", dir);
    DeleteFileA(path);
    RemoveDirectoryA(dir);
}

BT_TEST(jobwrap, load_config_reports_missing_and_broken_files) {
    char err[BT_JOBWRAP_ERR_MAX];
    set_config_env("Z:/no-such/jobwrap-missing.json");
    bt_json_value *config = bt_jobwrap_load_config(err, sizeof err);
    BT_CHECK(config == NULL);
    BT_CHECK(strstr(err, "read") == err);
    char dir[BT_PATH_MAX];
    write_config_pair(dir, "broken", "{\"job\": ");
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s\\config.json", dir);
    set_config_env(path);
    config = bt_jobwrap_load_config(err, sizeof err);
    BT_CHECK(config == NULL);
    BT_CHECK(strstr(err, "parse") == err);
    restore_config_env();
    drop_config_dir(dir);
}

BT_TEST(jobwrap, pid_dir_rejects_an_empty_value) {
    char out[BT_PATH_MAX];
    char err[BT_JOBWRAP_ERR_MAX];
    bt_json_value *config = config_of("{\"memguard\":{\"pid_dir\":\"\"}}");
    BT_CHECK(bt_jobwrap_pid_dir_from(config, out, err, sizeof err) != 0);
    BT_CHECK_EQ(strcmp(err, "config value invalid: memguard.pid_dir"), 0);
    bt_json_free(config);
}

BT_TEST(jobwrap, run_rejects_non_wrap_kinds_and_bad_configs) {
    unsigned int code = 0;
    char err[BT_JOBWRAP_ERR_MAX];
    bt_jobwrap_invocation plain = {BT_JOBWRAP_INVOCATION_HOLD_SECS, NULL, NULL, 0, 0};
    BT_CHECK_EQ(bt_jobwrap_run(&plain, &code, err, sizeof err), BT_JOBWRAP_RUN_USAGE);
    BT_CHECK(strstr(err, "run needs a wrap invocation") != NULL);

    const wchar_t *payload[] = {L"cmd", L"/c", L"exit", L"0"};
    bt_jobwrap_invocation wrap = {BT_JOBWRAP_INVOCATION_WRAP, NULL, payload, 4, 0};
    char dir[BT_PATH_MAX];
    write_config_pair(dir, "nojob", "{}");
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s\\config.json", dir);
    set_config_env(path);
    int verdict = bt_jobwrap_run(&wrap, &code, err, sizeof err);
    restore_config_env();
    BT_CHECK_EQ(verdict, BT_JOBWRAP_RUN_FAILURE);
    BT_CHECK(strstr(err, "job.per_process_bytes") != NULL);
    drop_config_dir(dir);

    char base[BT_PATH_MAX];
    char piddir[BT_PATH_MAX];
    snprintf(base, sizeof base, "%s\\bt-jobwrap-block-%d", getenv("TEMP"), _getpid());
    CreateDirectoryA(base, NULL);
    char blocker[BT_PATH_MAX];
    snprintf(blocker, sizeof blocker, "%s\\blocker.txt", base);
    FILE *file = fopen(blocker, "wb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        fputs("in the way", file);
        fclose(file);
    }
    snprintf(piddir, sizeof piddir, "%s\\blocker.txt\\pids", base);
    char body[BT_PATH_MAX * 2];
    char piddir_fwd[BT_PATH_MAX];
    snprintf(piddir_fwd, sizeof piddir_fwd, "%s", piddir);
    for (char *cursor = piddir_fwd; *cursor != '\0'; cursor++) {
        if (*cursor == '\\') {
            *cursor = '/';
        }
    }
    snprintf(body, sizeof body,
             "{\"job\":{\"per_process_bytes\":4294967296},\"memguard\":{\"pid_dir\":\"%s\"}}",
             piddir_fwd);
    write_config_pair(dir, "blocked", body);
    bt_jobwrap_invocation anchored = {BT_JOBWRAP_INVOCATION_WRAP, L"anchor", payload, 4, 0};
    char blocked_path[BT_PATH_MAX];
    snprintf(blocked_path, sizeof blocked_path, "%s\\config.json", dir);
    set_config_env(blocked_path);
    verdict = bt_jobwrap_run(&anchored, &code, err, sizeof err);
    restore_config_env();
    BT_CHECK_EQ(verdict, BT_JOBWRAP_RUN_FAILURE);
    BT_CHECK(strstr(err, "anchor") == err);
    drop_config_dir(dir);
    DeleteFileA(blocker);
    RemoveDirectoryA(base);
}

BT_TEST(jobwrap, run_spawns_the_payload_and_passes_the_exit_code_through) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "sleeper.exe");
    wchar_t payload0[BT_PATH_MAX];
    size_t exelen = strlen(exe);
    for (size_t at = 0; at <= exelen; at++) {
        payload0[at] = (wchar_t)exe[at];
    }
    const wchar_t *payload[] = {payload0, L"0.1"};
    char dir[BT_PATH_MAX];
    write_config_pair(dir, "ok",
                      "{\"job\":{\"per_process_bytes\":4294967296},"
                      "\"memguard\":{\"pid_dir\":\"artifacts/progress/pids\"}}");
    bt_jobwrap_invocation anchored = {BT_JOBWRAP_INVOCATION_WRAP, L"suitewrap", payload, 2, 0};
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s\\config.json", dir);
    set_config_env(path);
    unsigned int code = 99;
    char err[BT_JOBWRAP_ERR_MAX];
    int verdict = bt_jobwrap_run(&anchored, &code, err, sizeof err);
    BT_CHECK_EQ(verdict, BT_JOBWRAP_RUN_OK);
    BT_CHECK_EQ(code, 0U);
    bt_jobwrap_invocation bare = {BT_JOBWRAP_INVOCATION_WRAP, NULL, payload, 2, 0};
    code = 99;
    verdict = bt_jobwrap_run(&bare, &code, err, sizeof err);
    BT_CHECK_EQ(verdict, BT_JOBWRAP_RUN_OK);
    BT_CHECK_EQ(code, 0U);
    const wchar_t *usage_error[] = {payload0, L"abc"};
    bt_jobwrap_invocation failing = {BT_JOBWRAP_INVOCATION_WRAP, NULL, usage_error, 2, 0};
    code = 0;
    verdict = bt_jobwrap_run(&failing, &code, err, sizeof err);
    restore_config_env();
    BT_CHECK_EQ(verdict, BT_JOBWRAP_RUN_OK);
    BT_CHECK_EQ(code, 2U);
    drop_config_dir(dir);
}

BT_TEST(jobwrap, run_reports_a_missing_payload_program) {
    const wchar_t *payload[] = {L"bt-missing-program-xyz", L"arg"};
    char dir[BT_PATH_MAX];
    write_config_pair(dir, "miss",
                      "{\"job\":{\"per_process_bytes\":4294967296},"
                      "\"memguard\":{\"pid_dir\":\"artifacts/progress/pids\"}}");
    bt_jobwrap_invocation wrap = {BT_JOBWRAP_INVOCATION_WRAP, NULL, payload, 2, 0};
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s\\config.json", dir);
    set_config_env(path);
    unsigned int code = 0;
    char err[BT_JOBWRAP_ERR_MAX];
    int verdict = bt_jobwrap_run(&wrap, &code, err, sizeof err);
    restore_config_env();
    BT_CHECK_EQ(verdict, BT_JOBWRAP_RUN_FAILURE);
    BT_CHECK(strstr(err, "spawn failed") != NULL);
    drop_config_dir(dir);
}

BT_TEST(jobwrap, run_rejects_an_over_budget_command_line) {
    wchar_t huge[BT_JOBWRAP_CMDLINE_MAX + 8];
    for (size_t at = 0; at < BT_JOBWRAP_CMDLINE_MAX + 4; at++) {
        huge[at] = L'a';
    }
    huge[BT_JOBWRAP_CMDLINE_MAX + 4] = L'\0';
    const wchar_t *payload[] = {huge};
    char dir[BT_PATH_MAX];
    write_config_pair(dir, "long",
                      "{\"job\":{\"per_process_bytes\":4294967296},"
                      "\"memguard\":{\"pid_dir\":\"artifacts/progress/pids\"}}");
    bt_jobwrap_invocation wrap = {BT_JOBWRAP_INVOCATION_WRAP, NULL, payload, 1, 0};
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s\\config.json", dir);
    set_config_env(path);
    unsigned int code = 0;
    char err[BT_JOBWRAP_ERR_MAX];
    int verdict = bt_jobwrap_run(&wrap, &code, err, sizeof err);
    restore_config_env();
    BT_CHECK_EQ(verdict, BT_JOBWRAP_RUN_FAILURE);
    BT_CHECK(strstr(err, "payload command line too long") != NULL);
    drop_config_dir(dir);
}

BT_TEST(jobwrap, ffi_rejects_broken_and_oversized_command_lines) {
    bt_jobwrap_spawned spawned;
    unsigned long code = bt_jobwrap_spawn_suspended(L"bt-missing-program-xyz", &spawned);
    BT_CHECK(code != 0);
    wchar_t over[BT_JOBWRAP_CMDLINE_MAX + 2];
    for (size_t at = 0; at < BT_JOBWRAP_CMDLINE_MAX + 1; at++) {
        over[at] = L'x';
    }
    over[BT_JOBWRAP_CMDLINE_MAX + 1] = L'\0';
    code = bt_jobwrap_spawn_suspended(over, &spawned);
    BT_CHECK_EQ(code, ERROR_INVALID_PARAMETER);
}

BT_TEST(jobwrap, ffi_assign_resume_and_wait_fail_on_dead_handles) {
    bt_jobwrap_spawned dead = {NULL, NULL, 0};
    HANDLE job = NULL;
    BT_CHECK(bt_jobwrap_create_ceiling_job(4294967296ULL, &job) == 0);
    BT_CHECK(job != NULL);
    unsigned long code = bt_jobwrap_assign_and_resume(job, &dead);
    BT_CHECK(code != 0);
    bt_jobwrap_spawned child;
    BT_CHECK_EQ(bt_jobwrap_spawn_suspended(L"cmd /c exit 0", &child), 0UL);
    BT_CHECK_EQ(bt_jobwrap_assign_and_resume(job, &child), 0UL);
    bt_jobwrap_spawned resumed = {child.process, NULL, child.process_id};
    code = bt_jobwrap_assign_and_resume(job, &resumed);
    BT_CHECK(code != 0);
    unsigned int exit_code = 0;
    BT_CHECK_EQ(bt_jobwrap_wait_exit_code(child.process, &exit_code), 0UL);
    BT_CHECK_EQ(exit_code, 0U);
    code = bt_jobwrap_wait_exit_code(NULL, &exit_code);
    BT_CHECK(code != 0);
    code = bt_jobwrap_wait_exit_code(child.thread, &exit_code);
    BT_CHECK(code != 0);
    bt_jobwrap_close_handle(child.process);
    bt_jobwrap_close_handle(child.thread);
    bt_jobwrap_close_handle(job);
    bt_jobwrap_close_handle(NULL);
    unsigned long long ticks = 0;
    BT_CHECK_EQ(bt_jobwrap_dotnet_start_ticks(&ticks), 0UL);
    BT_CHECK(ticks > BT_JOBWRAP_DOTNET_TICKS_AT_FILETIME_ZERO);
}

BT_TEST(jobwrap, quote_argument_hits_cap_limits_mid_string) {
    wchar_t out[8];
    BT_CHECK(bt_jobwrap_quote_argument(L"plain", out, 2) != 0);
    BT_CHECK(bt_jobwrap_quote_argument(L"", out, 2) != 0);
    BT_CHECK(bt_jobwrap_quote_argument(L"a \"quoted\" tail", out, 8) != 0);
    BT_CHECK(bt_jobwrap_quote_argument(L"back\\slash tail", out, 8) != 0);
    BT_CHECK(bt_jobwrap_quote_argument(L"a b\\", out, 6) != 0);
    wchar_t tight[6];
    BT_CHECK_EQ(bt_jobwrap_quote_argument(L"ab", tight, 6), 0);
    BT_CHECK_EQ(wcscmp(tight, L"ab"), 0);
}

BT_TEST(jobwrap, run_with_anchor_removes_the_pidfile_when_the_command_line_overflows) {
    wchar_t huge[BT_JOBWRAP_CMDLINE_MAX + 8];
    for (size_t at = 0; at < BT_JOBWRAP_CMDLINE_MAX + 4; at++) {
        huge[at] = L'a';
    }
    huge[BT_JOBWRAP_CMDLINE_MAX + 4] = L'\0';
    const wchar_t *payload[] = {huge};
    char dir[BT_PATH_MAX];
    write_config_pair(dir, "longanchored",
                      "{\"job\":{\"per_process_bytes\":4294967296},"
                      "\"memguard\":{\"pid_dir\":\"artifacts/progress/pids\"}}");
    bt_jobwrap_invocation wrap = {BT_JOBWRAP_INVOCATION_WRAP, L"longwrap", payload, 1, 0};
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s\\config.json", dir);
    set_config_env(path);
    unsigned int code = 0;
    char err[BT_JOBWRAP_ERR_MAX];
    int verdict = bt_jobwrap_run(&wrap, &code, err, sizeof err);
    restore_config_env();
    BT_CHECK_EQ(verdict, BT_JOBWRAP_RUN_FAILURE);
    BT_CHECK(strstr(err, "payload command line too long") != NULL);
    char root[BT_PATH_MAX];
    bt_jobwrap_workspace_root(root);
    char piddir[BT_PATH_MAX];
    snprintf(piddir, sizeof piddir, "%s/artifacts/progress/pids", root);
    char pattern[BT_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/longwrap-*.pid", piddir);
    WIN32_FIND_DATAA found;
    HANDLE scan = FindFirstFileA(pattern, &found);
    BT_CHECK(scan == INVALID_HANDLE_VALUE);
    if (scan != INVALID_HANDLE_VALUE) {
        FindClose(scan);
    }
    drop_config_dir(dir);
}

BT_TEST(jobwrap, run_reports_a_pid_file_that_cannot_be_created) {
    const wchar_t *payload[] = {L"cmd", L"/c", L"exit", L"0"};
    char dir[BT_PATH_MAX];
    char piddir[BT_PATH_MAX];
    char base[BT_PATH_MAX];
    DWORD n = GetTempPathA(BT_PATH_MAX, base);
    BT_CHECK(n > 0 && n < BT_PATH_MAX);
    snprintf(piddir, sizeof piddir, "%sbt-jobwrap-pidblock-%lu", base, GetCurrentProcessId());
    CreateDirectoryA(piddir, NULL);
    char blocked[BT_PATH_MAX];
    snprintf(blocked, sizeof blocked, "%s\\blocked-%lu-%lu.pid", piddir, GetCurrentProcessId(),
             GetCurrentProcessId());
    CreateDirectoryA(blocked, NULL);
    char piddir_fwd[BT_PATH_MAX];
    snprintf(piddir_fwd, sizeof piddir_fwd, "%s", piddir);
    for (char *cursor = piddir_fwd; *cursor != '\0'; cursor++) {
        if (*cursor == '\\') {
            *cursor = '/';
        }
    }
    char body[BT_PATH_MAX * 2];
    snprintf(body, sizeof body,
             "{\"job\":{\"per_process_bytes\":4294967296},\"memguard\":{\"pid_dir\":\"%s\"}}",
             piddir_fwd);
    write_config_pair(dir, "pidblocked", body);
    wchar_t anchor[64];
    swprintf(anchor, 64, L"blocked-%lu", GetCurrentProcessId());
    bt_jobwrap_invocation wrap = {BT_JOBWRAP_INVOCATION_WRAP, anchor, payload, 4, 0};
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s\\config.json", dir);
    set_config_env(path);
    unsigned int code = 0;
    char err[BT_JOBWRAP_ERR_MAX];
    int verdict = bt_jobwrap_run(&wrap, &code, err, sizeof err);
    restore_config_env();
    BT_CHECK_EQ(verdict, BT_JOBWRAP_RUN_FAILURE);
    BT_CHECK(strstr(err, "anchor") == err);
    drop_config_dir(dir);
    RemoveDirectoryA(blocked);
    RemoveDirectoryA(piddir);
}

BT_TEST(jobwrap, run_rejects_a_blank_pid_dir_with_an_anchor) {
    const wchar_t *payload[] = {L"cmd", L"/c", L"exit", L"0"};
    char dir[BT_PATH_MAX];
    write_config_pair(dir, "blankdir",
                      "{\"job\":{\"per_process_bytes\":4294967296},"
                      "\"memguard\":{\"pid_dir\":\"\"}}");
    bt_jobwrap_invocation wrap = {BT_JOBWRAP_INVOCATION_WRAP, L"blank", payload, 4, 0};
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s\\config.json", dir);
    set_config_env(path);
    unsigned int code = 0;
    char err[BT_JOBWRAP_ERR_MAX];
    int verdict = bt_jobwrap_run(&wrap, &code, err, sizeof err);
    restore_config_env();
    BT_CHECK_EQ(verdict, BT_JOBWRAP_RUN_FAILURE);
    BT_CHECK(strstr(err, "anchor blank") == err);
    drop_config_dir(dir);
}

BT_TEST(jobwrap, exe_wrap_path_passes_the_child_exit_code) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "jobwrap.exe");
    char sleeper[BT_PATH_MAX];
    bt_test_bin_path(sleeper, "sleeper.exe");
    wchar_t sleeper_wide[BT_PATH_MAX];
    for (size_t at = 0; at <= strlen(sleeper); at++) {
        sleeper_wide[at] = (wchar_t)sleeper[at];
    }
    char command[BT_PATH_MAX * 3];
    snprintf(command, sizeof command, "\"%s\" -- \"%s\" 0.1", exe, sleeper);
    wchar_t wide[BT_PATH_MAX * 3];
    MultiByteToWideChar(CP_UTF8, 0, command, -1, wide, BT_PATH_MAX * 3);
    STARTUPINFOW startup = {0};
    startup.cb = sizeof startup;
    PROCESS_INFORMATION info = {0};
    BT_CHECK(CreateProcessW(NULL, wide, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &info) != 0);
    WaitForSingleObject(info.hProcess, 30000);
    DWORD code = 0;
    GetExitCodeProcess(info.hProcess, &code);
    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);
    BT_CHECK_EQ(code, 0U);
    snprintf(command, sizeof command, "\"%s\" --anchor bad!name -- \"%s\" 0.1", exe, sleeper);
    MultiByteToWideChar(CP_UTF8, 0, command, -1, wide, BT_PATH_MAX * 3);
    BT_CHECK(CreateProcessW(NULL, wide, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &info) != 0);
    WaitForSingleObject(info.hProcess, 30000);
    GetExitCodeProcess(info.hProcess, &code);
    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);
    BT_CHECK_EQ(code, 2U);
}

static DWORD spawn_jobwrap_wait(char *command, DWORD timeout_ms, DWORD *code) {
    wchar_t wide[BT_PATH_MAX * 3];
    MultiByteToWideChar(CP_UTF8, 0, command, -1, wide, BT_PATH_MAX * 3);
    STARTUPINFOW startup = {0};
    startup.cb = sizeof startup;
    PROCESS_INFORMATION info = {0};
    if (CreateProcessW(NULL, wide, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &info) == 0) {
        return (DWORD)-1;
    }
    DWORD wait = WaitForSingleObject(info.hProcess, timeout_ms);
    if (wait == WAIT_TIMEOUT) {
        TerminateProcess(info.hProcess, 124);
        WaitForSingleObject(info.hProcess, 5000);
        GetExitCodeProcess(info.hProcess, code);
        CloseHandle(info.hProcess);
        CloseHandle(info.hThread);
        return WAIT_TIMEOUT;
    }
    GetExitCodeProcess(info.hProcess, code);
    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);
    return wait;
}

BT_TEST(jobwrap, exe_max_hold_seconds_saturate_and_never_finish) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "jobwrap.exe");
    char command[BT_PATH_MAX * 3];
    snprintf(command, sizeof command, "\"%s\" --selftest-hold-secs 18446744073709551615", exe);
    DWORD code = 0;
    DWORD wait = spawn_jobwrap_wait(command, 3000, &code);
    BT_CHECK_EQ(wait, WAIT_TIMEOUT);
}

static int spawn_alloc_capped(char *command, DWORD cap_bytes, DWORD *code) {
    wchar_t wide[BT_PATH_MAX * 3];
    MultiByteToWideChar(CP_UTF8, 0, command, -1, wide, BT_PATH_MAX * 3);
    STARTUPINFOW startup = {0};
    startup.cb = sizeof startup;
    PROCESS_INFORMATION info = {0};
    if (CreateProcessW(NULL, wide, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &startup,
                       &info) == 0) {
        return -1;
    }
    HANDLE job = CreateJobObjectA(NULL, NULL);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {0};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    limits.ProcessMemoryLimit = cap_bytes;
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof limits);
    AssignProcessToJobObject(job, info.hProcess);
    ResumeThread(info.hThread);
    WaitForSingleObject(info.hProcess, 60000);
    GetExitCodeProcess(info.hProcess, code);
    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);
    CloseHandle(job);
    return 0;
}

BT_TEST(jobwrap, exe_alloc_fails_inside_a_capped_job_and_aborts) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "jobwrap.exe");
    char command[BT_PATH_MAX * 3];
    snprintf(command, sizeof command, "\"%s\" --selftest-alloc-mb 512", exe);
    DWORD code = 0;
    int rc = spawn_alloc_capped(command, 64U * 1024U * 1024U, &code);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK(code == 3 || code == 0xC0000409);
}

BT_TEST(jobwrap, exe_run_failure_verdict_exits_3_with_the_trace_dump) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "jobwrap.exe");
    char sleeper[BT_PATH_MAX];
    bt_test_bin_path(sleeper, "sleeper.exe");
    char dir[BT_PATH_MAX];
    write_config_pair(dir, "nojob", "{}");
    char config[BT_PATH_MAX];
    snprintf(config, sizeof config, "%s\\config.json", dir);
    char saved[BT_PATH_MAX];
    DWORD had = GetEnvironmentVariableA("BT_CONFIG", saved, BT_PATH_MAX);
    SetEnvironmentVariableA("BT_CONFIG", config);
    char command[BT_PATH_MAX * 3];
    snprintf(command, sizeof command, "\"%s\" --anchor blank -- \"%s\" 0.1", exe, sleeper);
    DWORD code = 0;
    DWORD wait = spawn_jobwrap_wait(command, 30000, &code);
    SetEnvironmentVariableA("BT_CONFIG", had > 0 && had < BT_PATH_MAX ? saved : NULL);
    drop_config_dir(dir);
    BT_CHECK_EQ(wait, WAIT_OBJECT_0);
    BT_CHECK_EQ(code, 3U);
}
