#include "e2e/lib.h"

#include "common/bt_trace.h"
#include "common/config.h"
#include "common/paths.h"
#include "e2e/proc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static void trace_from_config(void) {
    char path[BT_PATH_MAX];
    bt_paths_config_path(path);
    bt_json_value *config = NULL;
    if (bt_config_try_load(path, &config) != 0) {
        config = NULL;
    }
    long long ring_bytes = 65536;
    long long max_threads = 8;
    long long line_max = 256;
    if (config != NULL) {
        bt_config_int(config, "trace.ring_bytes", &ring_bytes);
        bt_config_int(config, "trace.max_threads", &max_threads);
        bt_config_int(config, "trace.line_max", &line_max);
        bt_json_free(config);
    }
    bt_trace_init((size_t)ring_bytes, (size_t)max_threads, (size_t)line_max);
    bt_trace_install_crash_handler();
}

static void parent_dir(const char *child_path, char result[BT_PATH_MAX]) {
    snprintf(result, BT_PATH_MAX, "%s", child_path);
    char *slash = strrchr(result, '/');
    char *back = strrchr(result, '\\');
    char *cut = back > slash ? back : slash;
    if (cut != NULL) {
        *cut = '\0';
    }
}

static int smoke_failed(const bt_e2e_smoke_run *run, char *error, size_t error_cap) {
    char *stdout_tail = bt_e2e_tail(run->stdout_bytes, run->stdout_len);
    char *stderr_tail = bt_e2e_tail(run->stderr_bytes, run->stderr_len);
    if (run->has_code) {
        snprintf(error, error_cap,
                 "smoke run failed with exit code %ld\nstdout tail:\n%s\nstderr tail:\n%s",
                 run->code, stdout_tail != NULL ? stdout_tail : "",
                 stderr_tail != NULL ? stderr_tail : "");
    } else {
        snprintf(error, error_cap,
                 "smoke process terminated without exit code\nstdout tail:\n%s\nstderr tail:\n%s",
                 stdout_tail != NULL ? stdout_tail : "",
                 stderr_tail != NULL ? stderr_tail : "");
    }
    free(stdout_tail);
    free(stderr_tail);
    return -1;
}

static int bt_e2e_real_main(size_t argc, const char *const *argv, char *error,
                            size_t error_cap) {
    BT_TRACE_ENTER("e2e.real_main");
    bt_e2e_args args;
    if (bt_e2e_parse_args(argc, argv, &args) != 0) {
        snprintf(error, error_cap, "usage: bt-e2e [blind-trainer.exe] [out-dir]");
        BT_TRACE_FAIL("e2e.real_main", "usage");
        BT_TRACE_EXIT("e2e.real_main");
        return -1;
    }
    char root[BT_PATH_MAX];
    bt_paths_workspace_root(root);
    char config_path[BT_PATH_MAX];
    bt_e2e_config_path_from(getenv("BT_CONFIG"), root, config_path);
    bt_json_value *config = NULL;
    if (bt_e2e_read_config(config_path, &config, error, error_cap) != 0) {
        BT_TRACE_FAIL("e2e.real_main", "config unreadable");
        BT_TRACE_EXIT("e2e.real_main");
        return -1;
    }
    bt_e2e_window_dims dims;
    if (bt_e2e_window_dims_from(config, &dims, error, error_cap) != 0) {
        bt_json_free(config);
        BT_TRACE_FAIL("e2e.real_main", "%s", error);
        BT_TRACE_EXIT("e2e.real_main");
        return -1;
    }
    bt_e2e_settings settings;
    if (bt_e2e_settings_from(config, &settings, error, error_cap) != 0) {
        bt_json_free(config);
        BT_TRACE_FAIL("e2e.real_main", "%s", error);
        BT_TRACE_EXIT("e2e.real_main");
        return -1;
    }
    unsigned long long deadline = 0;
    if (bt_e2e_deadline_from(settings.timeout_secs, bt_e2e_now_ms(), &deadline) != 0) {
        bt_json_free(config);
        bt_e2e_free_settings(&settings);
        snprintf(error, error_cap, "config e2e.timeout_secs overflows the deadline");
        BT_TRACE_FAIL("e2e.real_main", "%s", error);
        BT_TRACE_EXIT("e2e.real_main");
        return -1;
    }
    char out_dir[BT_PATH_MAX];
    if (args.has_out_dir) {
        snprintf(out_dir, BT_PATH_MAX, "%s", args.out_dir);
    } else {
        bt_e2e_out_dir_from(getenv("BT_ARTIFACTS"), config, root, out_dir);
    }
    char app_exe[BT_PATH_MAX];
    const bt_json_value *app_exe_value =
        bt_json_get(bt_json_get(config, "build"), "app_exe");
    if (app_exe_value != NULL && app_exe_value->type == BT_JSON_STRING) {
        snprintf(app_exe, BT_PATH_MAX, "%s", app_exe_value->as.string);
    } else {
        snprintf(app_exe, BT_PATH_MAX, "bt-app.exe");
    }
    char binary[BT_PATH_MAX];
    if (args.has_binary) {
        snprintf(binary, BT_PATH_MAX, "%s", args.binary);
    } else {
        char bin_dir[BT_PATH_MAX];
        bt_paths_join(root, "build/bin", bin_dir);
        bt_paths_join(bin_dir, app_exe, binary);
    }
    if (!bt_e2e_file_exists(binary)) {
        bt_json_free(config);
        bt_e2e_free_settings(&settings);
        snprintf(error, error_cap, "release binary missing: %s", binary);
        BT_TRACE_FAIL("e2e.real_main", "%s", error);
        BT_TRACE_EXIT("e2e.real_main");
        return -1;
    }
    printf("binary: %s\n", binary);
    printf("out dir: %s\n", out_dir);
    printf("window: %ux%u\n", dims.width, dims.height);
    if (bt_e2e_clean_pngs(out_dir, error, error_cap) != 0) {
        bt_json_free(config);
        bt_e2e_free_settings(&settings);
        BT_TRACE_FAIL("e2e.real_main", "%s", error);
        BT_TRACE_EXIT("e2e.real_main");
        return -1;
    }
    bt_e2e_smoke_run run;
    if (bt_e2e_run_smoke(binary, out_dir, NULL, 0, deadline, &run, error, error_cap) != 0) {
        bt_json_free(config);
        bt_e2e_free_settings(&settings);
        BT_TRACE_FAIL("e2e.real_main", "%s", error);
        BT_TRACE_EXIT("e2e.real_main");
        return -1;
    }
    if (!run.has_code || run.code != 0) {
        smoke_failed(&run, error, error_cap);
        bt_e2e_smoke_run_free(&run);
        bt_json_free(config);
        bt_e2e_free_settings(&settings);
        BT_TRACE_FAIL("e2e.real_main", "%s", error);
        BT_TRACE_EXIT("e2e.real_main");
        return -1;
    }
    bt_e2e_smoke_run_free(&run);
    char **found = NULL;
    size_t found_count = 0;
    if (bt_e2e_png_names(out_dir, &found, &found_count, error, error_cap) != 0) {
        bt_json_free(config);
        bt_e2e_free_settings(&settings);
        BT_TRACE_FAIL("e2e.real_main", "%s", error);
        BT_TRACE_EXIT("e2e.real_main");
        return -1;
    }
    char **expected = NULL;
    size_t expected_count = 0;
    if (bt_e2e_expected_names((const char *const *)settings.shots, settings.shots_count,
                              &expected, &expected_count) != 0) {
        bt_json_free(config);
        bt_e2e_free_names(found, found_count);
        bt_e2e_free_settings(&settings);
        BT_TRACE_FAIL("e2e.real_main", "expected names failed");
        BT_TRACE_EXIT("e2e.real_main");
        return -1;
    }
    char **missing = NULL;
    char **unexpected = NULL;
    size_t missing_count = 0;
    size_t unexpected_count = 0;
    bt_e2e_classify((const char *const *)found, found_count, (const char *const *)expected,
                    expected_count, &missing, &missing_count, &unexpected, &unexpected_count);
    size_t failures = 0;
    for (size_t i = 0; i < missing_count; i++) {
        printf("FAIL %s: missing screenshot\n", missing[i]);
        failures += 1;
    }
    for (size_t i = 0; i < unexpected_count; i++) {
        printf("FAIL %s: unexpected screenshot\n", unexpected[i]);
        failures += 1;
    }
    const char *shots_root = out_dir;
    for (size_t i = 0; i < expected_count; i++) {
        int is_missing = 0;
        for (size_t j = 0; j < missing_count; j++) {
            if (strcmp(missing[j], expected[i]) == 0) {
                is_missing = 1;
            }
        }
        if (is_missing) {
            continue;
        }
        char dest[BT_PATH_MAX];
        bt_paths_join(shots_root, expected[i], dest);
        char reason[BT_E2E_ERROR_MAX];
        if (bt_e2e_check_shot(dest, dims, reason, sizeof reason) == BT_E2E_SHOT_OK) {
            printf("ok %s\n", expected[i]);
        } else {
            printf("FAIL %s: %s\n", expected[i], reason);
            failures += 1;
        }
    }
    printf("%zu shots expected, %zu found, %zu failed\n", expected_count, found_count,
           failures);
    bt_e2e_free_names(missing, missing_count);
    bt_e2e_free_names(unexpected, unexpected_count);
    bt_e2e_free_names(expected, expected_count);
    bt_e2e_free_names(found, found_count);
    bt_e2e_free_settings(&settings);
    if (failures > 0) {
        bt_json_free(config);
        BT_TRACE_FAIL("e2e.real_main", "%zu shot failures", failures);
        BT_TRACE_EXIT("e2e.real_main");
        return 1;
    }
    long neg_exit = 0;
    if (bt_e2e_neg_config_exit_from(config, &neg_exit, error, error_cap) != 0) {
        bt_json_free(config);
        BT_TRACE_FAIL("e2e.real_main", "%s", error);
        BT_TRACE_EXIT("e2e.real_main");
        return -1;
    }
    bt_json_free(config);
    char parent[BT_PATH_MAX];
    parent_dir(out_dir, parent);
    char neg_root[BT_PATH_MAX];
    bt_paths_join(parent, "e2e-neg", neg_root);
    char neg_dir[BT_PATH_MAX];
    bt_paths_join(neg_root, "shots", neg_dir);
    if (bt_e2e_clean_pngs(neg_dir, error, error_cap) != 0) {
        BT_TRACE_FAIL("e2e.real_main", "%s", error);
        BT_TRACE_EXIT("e2e.real_main");
        return -1;
    }
    char artifacts_dir[BT_PATH_MAX];
    bt_paths_join(root, "artifacts", artifacts_dir);
    char neg_config[BT_PATH_MAX];
    bt_paths_join(artifacts_dir, "e2e-neg-config.json", neg_config);
    wchar_t neg_config_w[BT_E2E_PATH_WMAX];
    if (bt_e2e_utf8_to_wide(neg_config, neg_config_w, BT_E2E_PATH_WMAX) == 0) {
        _wremove(neg_config_w);
    }
    char env_pair[BT_PATH_MAX * 2];
    snprintf(env_pair, sizeof env_pair, "BT_CONFIG=%s", neg_config);
    const char *env_pairs[] = {env_pair, NULL};
    bt_e2e_smoke_run neg_run;
    if (bt_e2e_run_smoke(binary, neg_dir, env_pairs, 1, deadline, &neg_run, error,
                         error_cap) != 0) {
        BT_TRACE_FAIL("e2e.real_main", "%s", error);
        BT_TRACE_EXIT("e2e.real_main");
        return -1;
    }
    int verdict = 0;
    if (neg_run.has_code) {
        if (neg_run.code == neg_exit) {
            printf("ok missing-config smoke exit %ld\n", neg_run.code);
        } else {
            char *stdout_tail = bt_e2e_tail(neg_run.stdout_bytes, neg_run.stdout_len);
            char *stderr_tail = bt_e2e_tail(neg_run.stderr_bytes, neg_run.stderr_len);
            snprintf(error, error_cap,
                     "smoke with a missing config exited %ld, expected %ld\nstdout tail:\n"
                     "%s\nstderr tail:\n%s",
                     neg_run.code, neg_exit, stdout_tail != NULL ? stdout_tail : "",
                     stderr_tail != NULL ? stderr_tail : "");
            free(stdout_tail);
            free(stderr_tail);
            verdict = -1;
        }
    } else {
        snprintf(error, error_cap, "missing-config smoke terminated without an exit code");
        verdict = -1;
    }
    bt_e2e_smoke_run_free(&neg_run);
    if (verdict == 0) {
        BT_TRACE_EXIT("e2e.real_main");
    } else {
        BT_TRACE_FAIL("e2e.real_main", "%s", error);
        BT_TRACE_EXIT("e2e.real_main");
    }
    return verdict;
}

int wmain(int argc, wchar_t **argv) {
    trace_from_config();
    char **utf8_argv = calloc((size_t)argc, sizeof(char *));
    if (utf8_argv == NULL && argc > 0) {
        fprintf(stderr, "bt-e2e: argument conversion failed\n");
        bt_trace_shutdown();
        return 2;
    }
    for (int i = 0; i < argc; i++) {
        utf8_argv[i] = calloc((size_t)BT_E2E_PATH_WMAX * 3, 1);
        if (utf8_argv[i] == NULL ||
            bt_e2e_wide_to_utf8(argv[i], utf8_argv[i], (size_t)BT_E2E_PATH_WMAX * 3) != 0) {
            fprintf(stderr, "bt-e2e: argument conversion failed\n");
            bt_trace_shutdown();
            return 2;
        }
    }
    char error[BT_E2E_ERROR_MAX];
    int code = bt_e2e_real_main(argc > 0 ? (size_t)argc - 1 : 0,
                                argc > 0 ? (const char *const *)utf8_argv + 1 : NULL, error,
                                sizeof error);
    for (int i = 0; i < argc; i++) {
        free(utf8_argv[i]);
    }
    free(utf8_argv);
    if (code == -1) {
        fprintf(stderr, "bt-e2e: %s\n", error);
        bt_trace_dump();
        bt_trace_shutdown();
        return 2;
    }
    bt_trace_shutdown();
    return code;
}
