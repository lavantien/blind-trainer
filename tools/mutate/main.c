#include "mutate/mutate.h"
#include "mutate/tests.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/config.h"
#include "common/paths.h"

#ifdef _WIN32
#include <windows.h>
#endif

static void usage(void) {
    fprintf(stderr, "usage: mutate list [--config P] <glob>...\n");
    fprintf(stderr, "       mutate run [--config P] [--clang P] [--scratch D] [--ledger P] "
                    "[--timeout S] [--only ids] [--limit N] --exe NAME <glob>...\n");
    fprintf(stderr, "       mutate report [--config P] --ledger P --out P\n");
    fprintf(stderr, "       mutate selftest\n");
}

static void root_from_config(const char *config_path, char out[BT_MUT_PATH_MAX]) {
    snprintf(out, BT_MUT_PATH_MAX, "%s", config_path);
    for (int i = 0; i < 2; i++) {
        char *slash = strrchr(out, '/');
        char *backslash = strrchr(out, '\\');
        char *cut = slash > backslash ? slash : backslash;
        if (cut == NULL) {
            break;
        }
        *cut = '\0';
    }
}

static int resolve_config(const char *override, char path_out[BT_MUT_PATH_MAX],
                          char root_out[BT_MUT_PATH_MAX]) {
    const char *config = override;
    if (config == NULL || config[0] == '\0') {
        config = getenv("BT_CONFIG");
    }
    if (config == NULL || config[0] == '\0') {
        bt_paths_workspace_root(root_out);
        snprintf(path_out, BT_MUT_PATH_MAX, "%s/config/config.json", root_out);
        return 0;
    }
    snprintf(path_out, BT_MUT_PATH_MAX, "%s", config);
    root_from_config(config, root_out);
    return 0;
}

static int glob_segment_match(const char *pattern, size_t pattern_len, const char *name) {
    size_t name_len = strlen(name);
    size_t pattern_index = 0;
    size_t name_index = 0;
    size_t star_pattern = (size_t)-1;
    size_t star_name = 0;
    while (name_index < name_len) {
        if (pattern_index < pattern_len &&
            (pattern[pattern_index] == '?' || pattern[pattern_index] == name[name_index])) {
            pattern_index++;
            name_index++;
        } else if (pattern_index < pattern_len && pattern[pattern_index] == '*') {
            star_pattern = pattern_index++;
            star_name = name_index;
        } else if (star_pattern != (size_t)-1) {
            pattern_index = star_pattern + 1;
            name_index = ++star_name;
        } else {
            return 0;
        }
    }
    while (pattern_index < pattern_len && pattern[pattern_index] == '*') {
        pattern_index++;
    }
    return pattern_index == pattern_len;
}

static int paths_contains(const bt_mut_paths *paths, const char *text) {
    for (size_t i = 0; i < paths->count; i++) {
        if (strcmp(paths->items[i], text) == 0) {
            return 1;
        }
    }
    return 0;
}

static int push_unique(bt_mut_paths *paths, const char *text) {
    if (paths_contains(paths, text)) {
        return 0;
    }
    return bt_mut_paths_push(paths, text);
}

static int expand_segment(const char *base, const char *rel, const char *pattern,
                          bt_mut_paths *out) {
    const char *slash = strchr(pattern, '/');
    size_t segment_len = slash == NULL ? strlen(pattern) : (size_t)(slash - pattern);
    const char *rest = slash == NULL ? NULL : slash + 1;
    char search[BT_MUT_PATH_MAX];
    if (rel[0] == '\0') {
        snprintf(search, sizeof search, "%s", base);
    } else {
        snprintf(search, sizeof search, "%s/%s", base, rel);
    }
#ifdef _WIN32
    char pattern_all[BT_MUT_PATH_MAX];
    snprintf(pattern_all, sizeof pattern_all, "%s/*", search);
    WIN32_FIND_DATAA entry;
    HANDLE find = FindFirstFileA(pattern_all, &entry);
    if (find == INVALID_HANDLE_VALUE) {
        return 0;
    }
    int rc = 0;
    do {
        if (strcmp(entry.cFileName, ".") == 0 || strcmp(entry.cFileName, "..") == 0) {
            continue;
        }
        if (!glob_segment_match(pattern, segment_len, entry.cFileName)) {
            continue;
        }
        char child_rel[BT_MUT_PATH_MAX];
        if (rel[0] == '\0') {
            snprintf(child_rel, sizeof child_rel, "%s", entry.cFileName);
        } else {
            snprintf(child_rel, sizeof child_rel, "%s/%s", rel, entry.cFileName);
        }
        if (rest == NULL) {
            if (!(entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                rc = push_unique(out, child_rel);
            }
        } else if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            rc = expand_segment(base, child_rel, rest, out);
        }
    } while (rc == 0 && FindNextFileA(find, &entry));
    FindClose(find);
    return rc;
#else
    return -1;
#endif
}

static int path_is_c_source(const char *path) {
    size_t len = strlen(path);
    return len > 2 && strcmp(path + len - 2, ".c") == 0;
}

static int expand_globs(const char *repo_root, char **patterns, int pattern_count,
                        bt_mut_paths *out) {
    for (int i = 0; i < pattern_count; i++) {
        const char *pattern = patterns[i];
        if (strchr(pattern, '*') == NULL && strchr(pattern, '?') == NULL) {
            char trimmed[BT_MUT_PATH_MAX];
            snprintf(trimmed, sizeof trimmed, "%s", pattern);
            size_t len = strlen(trimmed);
            while (len > 1 && (trimmed[len - 1] == '/' || trimmed[len - 1] == '\\')) {
                trimmed[--len] = '\0';
            }
            for (char *cursor = trimmed; *cursor != '\0'; cursor++) {
                if (*cursor == '\\') {
                    *cursor = '/';
                }
            }
            if (!paths_contains(out, trimmed)) {
                int rc = push_unique(out, trimmed);
                if (rc != 0) {
                    return rc;
                }
            }
            continue;
        }
        int rc = expand_segment(repo_root, "", pattern, out);
        if (rc != 0) {
            return rc;
        }
    }
    for (size_t i = 0; i < out->count; i++) {
        if (!path_is_c_source(out->items[i])) {
            fprintf(stderr, "mutate: target is not a .c file: %s\n", out->items[i]);
            return 2;
        }
    }
    if (out->count > 1) {
        for (size_t i = 0; i < out->count; i++) {
            for (size_t j = i + 1; j < out->count; j++) {
                if (strcmp(out->items[i], out->items[j]) > 0) {
                    char *swap = out->items[i];
                    out->items[i] = out->items[j];
                    out->items[j] = swap;
                }
            }
        }
    }
    return 0;
}

static int config_mutants(const bt_json_value *config, const char *key, long long *out,
                          long long fallback) {
    *out = fallback;
    return bt_config_int(config, key, out);
}

int run_list(int argc, char **argv) {
    char config_path[BT_MUT_PATH_MAX];
    char repo_root[BT_MUT_PATH_MAX];
    const char *config_override = NULL;
    char **patterns = NULL;
    int pattern_count = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_override = argv[++i];
        } else {
            char **grown = realloc(patterns, (size_t)(pattern_count + 1) * sizeof(char *));
            if (grown == NULL) {
                free(patterns);
                usage();
                return 2;
            }
            patterns = grown;
            patterns[pattern_count++] = argv[i];
        }
    }
    if (pattern_count == 0) {
        usage();
        free(patterns);
        return 2;
    }
    resolve_config(config_override, config_path, repo_root);
    bt_mut_paths files = {0};
    if (expand_globs(repo_root, patterns, pattern_count, &files) != 0) {
        free(patterns);
        return 2;
    }
    int rc = 0;
    for (size_t f = 0; f < files.count && rc == 0; f++) {
        char abs_path[BT_MUT_PATH_MAX];
        snprintf(abs_path, sizeof abs_path, "%s/%s", repo_root, files.items[f]);
        char *text = NULL;
        size_t len = 0;
        if (bt_mut_read_file(abs_path, &text, &len) != 0) {
            fprintf(stderr, "mutate list: cannot read %s\n", abs_path);
            rc = 2;
            break;
        }
        bt_mut_mutant_list list;
        if (bt_mut_enumerate(files.items[f], text, len, &list) != 0) {
            free(text);
            fprintf(stderr, "mutate list: cannot enumerate %s\n", abs_path);
            rc = 2;
            break;
        }
        for (size_t i = 0; i < list.count; i++) {
            bt_mut_mutant *mutant = &list.items[i];
            printf("%s\t%s\t%zu\t%zu\t%s\t%s\t%s\n", mutant->id, mutant->file, mutant->line,
                   mutant->col, bt_mut_op_name(mutant->op), mutant->from, mutant->to);
        }
        bt_mut_mutants_free(&list);
        free(text);
    }
    bt_mut_paths_free(&files);
    free(patterns);
    return rc;
}

int run_run(int argc, char **argv) {
    const char *config_override = NULL;
    const char *clang_override = NULL;
    const char *scratch_override = NULL;
    const char *ledger_override = NULL;
    const char *test_exe = NULL;
    const char *only = NULL;
    long long timeout_secs = -1;
    size_t limit = 0;
    char **patterns = NULL;
    int pattern_count = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_override = argv[++i];
        } else if (strcmp(argv[i], "--clang") == 0 && i + 1 < argc) {
            clang_override = argv[++i];
        } else if (strcmp(argv[i], "--scratch") == 0 && i + 1 < argc) {
            scratch_override = argv[++i];
        } else if (strcmp(argv[i], "--ledger") == 0 && i + 1 < argc) {
            ledger_override = argv[++i];
        } else if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) {
            timeout_secs = strtoll(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--only") == 0 && i + 1 < argc) {
            only = argv[++i];
        } else if (strcmp(argv[i], "--limit") == 0 && i + 1 < argc) {
            limit = (size_t)strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--exe") == 0 && i + 1 < argc) {
            test_exe = argv[++i];
        } else {
            char **grown = realloc(patterns, (size_t)(pattern_count + 1) * sizeof(char *));
            if (grown == NULL) {
                free(patterns);
                usage();
                return 2;
            }
            patterns = grown;
            patterns[pattern_count++] = argv[i];
        }
    }
    if (test_exe == NULL || pattern_count == 0) {
        usage();
        free(patterns);
        return 2;
    }
    char config_path[BT_MUT_PATH_MAX];
    char repo_root[BT_MUT_PATH_MAX];
    resolve_config(config_override, config_path, repo_root);
    bt_json_value *config = NULL;
    if (bt_config_try_load(config_path, &config) != 0) {
        fprintf(stderr, "mutate run: cannot load config %s\n", config_path);
        free(patterns);
        return 2;
    }
    bt_mut_engine engine;
    memset(&engine, 0, sizeof engine);
    snprintf(engine.repo_root, BT_MUT_PATH_MAX, "%s", repo_root);
    if (bt_mut_build_load(repo_root, config, clang_override, &engine.build) != 0) {
        bt_json_free(config);
        free(patterns);
        return 2;
    }
    if (bt_mut_area_map_load(config, &engine.area_map) != 0) {
        fprintf(stderr, "mutate run: %s\n", bt_mut_engine_last_error());
        bt_mut_build_free(&engine.build);
        bt_json_free(config);
        free(patterns);
        return 2;
    }
    long long workers = 1;
    long long timeout_config = 120;
    config_mutants(config, "mutants.workers", &workers, 1);
    config_mutants(config, "mutants.timeout_secs", &timeout_config, 120);
    engine.workers = (unsigned long long)workers;
    engine.timeout_ms =
        (unsigned long long)(timeout_secs >= 0 ? timeout_secs : timeout_config) * 1000ULL;
    const char *scratch_config = NULL;
    bt_config_str(config, "mutants.scratch_root", &scratch_config);
    bt_mut_scratch_root(scratch_override != NULL ? scratch_override : scratch_config,
                        engine.scratch_root);
    const char *ledger_config = NULL;
    bt_config_str(config, "mutants.ledger", &ledger_config);
    if (ledger_override != NULL) {
        snprintf(engine.ledger_path, BT_MUT_PATH_MAX, "%s", ledger_override);
    } else if (ledger_config != NULL && ledger_config[0] != '\0') {
        snprintf(engine.ledger_path, BT_MUT_PATH_MAX, "%s/%s", repo_root, ledger_config);
    } else {
        snprintf(engine.ledger_path, BT_MUT_PATH_MAX, "%s/artifacts/mutants/ledger.tsv", repo_root);
    }
    snprintf(engine.test_exe, BT_MUT_PATH_MAX, "%s", test_exe);
    bt_mut_paths files = {0};
    int rc = expand_globs(repo_root, patterns, pattern_count, &files);
    free(patterns);
    if (rc != 0 || files.count == 0) {
        fprintf(stderr, "mutate run: no files matched\n");
        bt_mut_build_free(&engine.build);
        bt_json_free(config);
        return 2;
    }
    bt_mut_target *targets = calloc(files.count, sizeof(bt_mut_target));
    if (targets == NULL) {
        bt_mut_build_free(&engine.build);
        bt_json_free(config);
        return 2;
    }
    size_t target_count = 0;
    for (size_t i = 0; i < files.count; i++) {
        if (bt_mut_target_from_repo(&engine, files.items[i], test_exe, &targets[target_count]) !=
            0) {
            fprintf(stderr, "mutate run: %s\n", bt_mut_engine_last_error());
            rc = 2;
            break;
        }
        target_count++;
    }
    bt_mut_summary summary;
    if (rc == 0) {
        rc = bt_mut_engine_run(&engine, targets, target_count, only, limit, &summary);
        if (rc == 0) {
            printf("mutants: total %llu, killed %llu, survived %llu, timeout %llu, builderr %llu, "
                   "pending %llu\n",
                   summary.total, summary.killed, summary.survived, summary.timeout,
                   summary.builderr, summary.pending);
        } else {
            fprintf(stderr, "mutate run: %s\n", bt_mut_engine_last_error());
        }
    }
    for (size_t i = 0; i < target_count; i++) {
        bt_mut_target_free(&targets[i]);
    }
    free(targets);
    bt_mut_paths_free(&files);
    bt_mut_build_free(&engine.build);
    bt_json_free(config);
    return rc;
}

int run_report(int argc, char **argv) {
    const char *config_override = NULL;
    const char *ledger_override = NULL;
    const char *out_path = NULL;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_override = argv[++i];
        } else if (strcmp(argv[i], "--ledger") == 0 && i + 1 < argc) {
            ledger_override = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        }
    }
    if (ledger_override == NULL || out_path == NULL) {
        usage();
        return 2;
    }
    char config_path[BT_MUT_PATH_MAX];
    char repo_root[BT_MUT_PATH_MAX];
    resolve_config(config_override, config_path, repo_root);
    int rc = bt_mut_report(ledger_override, repo_root, out_path);
    if (rc != 0) {
        fprintf(stderr, "mutate report: cannot produce %s\n", out_path);
    }
    return rc;
}

static void trace_from_config(const bt_json_value *config) {
    long long ring_bytes = 65536;
    long long max_threads = 8;
    long long line_max = 256;
    bt_config_int(config, "trace.ring_bytes", &ring_bytes);
    bt_config_int(config, "trace.max_threads", &max_threads);
    bt_config_int(config, "trace.line_max", &line_max);
    bt_trace_init((size_t)ring_bytes, (size_t)max_threads, (size_t)line_max);
    bt_trace_install_crash_handler();
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage();
        return 2;
    }
    if (strcmp(argv[1], "selftest") == 0) {
        bt_trace_init(65536, 8, 256);
        bt_trace_install_crash_handler();
        return bt_mutate_selftest();
    }
    if (strcmp(argv[1], "selftest-probe") == 0) {
        printf("%llu\n", (unsigned long long)bt_mut_process_start());
        return 0;
    }
    if (strcmp(argv[1], "selftest-flood") == 0) {
        for (;;) {
            printf("flood\n");
            fflush(stdout);
        }
        return 0;
    }
    char config_path[BT_MUT_PATH_MAX];
    char repo_root[BT_MUT_PATH_MAX];
    const char *config_override = NULL;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_override = argv[i + 1];
        }
    }
    resolve_config(config_override, config_path, repo_root);
    bt_json_value *config = NULL;
    if (bt_config_try_load(config_path, &config) == 0) {
        trace_from_config(config);
    } else {
        bt_trace_init(65536, 8, 256);
    }
    int rc;
    if (strcmp(argv[1], "list") == 0) {
        rc = run_list(argc, argv);
    } else if (strcmp(argv[1], "run") == 0) {
        rc = run_run(argc, argv);
    } else if (strcmp(argv[1], "report") == 0) {
        rc = run_report(argc, argv);
    } else {
        usage();
        rc = 2;
    }
    if (rc != 0) {
        bt_trace_dump();
    }
    bt_trace_shutdown();
    if (config != NULL) {
        bt_json_free(config);
    }
    return rc;
}
