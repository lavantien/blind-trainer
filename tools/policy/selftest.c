#include "policy/selftest.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "common/bt_trace.h"
#include "policy/fsutil.h"
#include "policy/lib.h"

static int selftest_write(const char *path, const char *text, char *err, size_t err_len) {
    char partial[BT_PATH_MAX];
    int wrote = snprintf(partial, sizeof partial, "%s", path);
    if (wrote < 0 || (size_t)wrote >= sizeof partial) {
        snprintf(err, err_len, "write %s: path too long", path);
        return -1;
    }
    for (char *cut = partial + 1; *cut != '\0'; cut++) {
        if (*cut != '/' && *cut != '\\') {
            continue;
        }
        char keep = *cut;
        *cut = '\0';
#ifdef _WIN32
        _mkdir(partial);
#else
        mkdir(partial, 0755);
#endif
        *cut = keep;
    }
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        snprintf(err, err_len, "write %s: %s", path, strerror(errno));
        return -1;
    }
    size_t len = strlen(text);
    if (fwrite(text, 1, len, file) != len) {
        fclose(file);
        snprintf(err, err_len, "write %s: short write", path);
        return -1;
    }
    fclose(file);
    return 0;
}

static int selftest_plant(const char *workdir, char *err, size_t err_len) {
    struct plant_row {
        const char *rel;
        const char *fixture;
    };
    static const struct plant_row rows[] = {
        {"src/dirty.c", POLICY_DIRTY_FIXTURE},
        {"src/clean.c", POLICY_CLEAN_FIXTURE},
        {"src/pair_a.c", POLICY_PAIR_C_FIXTURE},
        {"tools/dirty.py", POLICY_PY_DIRTY_FIXTURE},
        {"tools/pair_b.py", POLICY_PAIR_PY_FIXTURE},
        {"tests/big.c", POLICY_BIG_FIXTURE},
        {"clean_only/src/clean.c", POLICY_CLEAN_FIXTURE},
        {"clean_only/scripts/clean.py", POLICY_PY_CLEAN_FIXTURE},
        {"config.json", POLICY_SELFTEST_CONFIG},
        {"allow.txt", "# selftest allowlist\n"},
    };
    for (size_t at = 0; at < sizeof rows / sizeof rows[0]; at++) {
        char path[BT_PATH_MAX];
        snprintf(path, sizeof path, "%s/%s", workdir, rows[at].rel);
        if (selftest_write(path, rows[at].fixture, err, err_len) != 0) {
            return -1;
        }
    }
    return 0;
}

static int finding_is(const policy_finding *finding, const char *path, const char *detail) {
    return strcmp(finding->path, path) == 0 && strcmp(finding->detail, detail) == 0;
}

static unsigned long policy_process_id(void) {
#ifdef _WIN32
    return (unsigned long)GetCurrentProcessId();
#else
    return (unsigned long)getpid();
#endif
}

static int selftest_run_dir(const char *workdir, char out[BT_PATH_MAX], char *err, size_t err_len) {
#ifdef _WIN32
    _mkdir(workdir);
#else
    mkdir(workdir, 0755);
#endif
    policy_strvec names = {0};
    if (policy_list_dir(workdir, &names, err, err_len) != 0) {
        return -1;
    }
    for (size_t at = 0; at < names.count; at++) {
        if (strncmp(names.items[at], "pol-", 4) == 0) {
            char child[BT_PATH_MAX];
            if (policy_join_checked(child, workdir, names.items[at], err, err_len) != 0) {
                for (size_t left = at; left < names.count; left++) {
                    free(names.items[left]);
                }
                free(names.items);
                return -1;
            }
            policy_rm_rf(child);
        }
        free(names.items[at]);
    }
    free(names.items);
    static unsigned long long run_counter;
    run_counter++;
    int wrote =
        snprintf(out, BT_PATH_MAX, "%s/pol-%lu-%llu", workdir, policy_process_id(), run_counter);
    if (wrote < 0 || (size_t)wrote >= BT_PATH_MAX) {
        snprintf(err, err_len, "selftest run dir too long under %s", workdir);
        return -1;
    }
    return 0;
}

int policy_selftest(const char *workdir, char *err, size_t err_len) {
    BT_TRACE_ENTER("policy.selftest");
    char run_dir[BT_PATH_MAX];
    if (selftest_run_dir(workdir, run_dir, err, err_len) != 0) {
        BT_TRACE_FAIL("policy.selftest", "fresh run dir failed under %s", workdir);
        BT_TRACE_EXIT("policy.selftest");
        return -1;
    }
    if (selftest_plant(run_dir, err, err_len) != 0) {
        BT_TRACE_FAIL("policy.selftest", "planting fixtures failed in %s", run_dir);
        BT_TRACE_EXIT("policy.selftest");
        return -1;
    }
    char config_path[BT_PATH_MAX];
    char allow_path[BT_PATH_MAX];
    snprintf(config_path, sizeof config_path, "%s/config.json", run_dir);
    snprintf(allow_path, sizeof allow_path, "%s/allow.txt", run_dir);
    char *config_text = NULL;
    size_t config_len = 0;
    if (policy_read_source(config_path, &config_text, &config_len, err, err_len) != 0) {
        BT_TRACE_FAIL("policy.selftest", "reading selftest config failed");
        BT_TRACE_EXIT("policy.selftest");
        return -1;
    }
    bt_json_value *config = NULL;
    bt_json_error json_err;
    if (bt_json_parse(config_text, config_len, &config, &json_err) != 0) {
        snprintf(err, err_len, "parse %s: %s", config_path, json_err.message);
        free(config_text);
        BT_TRACE_FAIL("policy.selftest", "selftest config parse failed");
        BT_TRACE_EXIT("policy.selftest");
        return -1;
    }
    free(config_text);
    policy_outcome dirty_run;
    int rc =
        policy_run_checks(run_dir, POLICY_MODE_BOTH, config, allow_path, &dirty_run, err, err_len);
    if (rc != 0) {
        bt_json_free(config);
        BT_TRACE_FAIL("policy.selftest", "dirty run failed: %s", err);
        BT_TRACE_EXIT("policy.selftest");
        return -1;
    }
    size_t line_hits = 0;
    size_t block_hits = 0;
    size_t hash_hits = 0;
    size_t literal_hits = 0;
    size_t pair_hits = 0;
    size_t sloc_hits = 0;
    int others_silent = 1;
    for (size_t at = 0; at < dirty_run.finding_count; at++) {
        policy_finding *finding = &dirty_run.findings[at];
        if (finding_is(finding, "src/dirty.c", "line comment")) {
            line_hits++;
        } else if (finding_is(finding, "src/dirty.c", "block comment")) {
            block_hits++;
        } else if (finding_is(finding, "tools/dirty.py", "hash comment")) {
            hash_hits++;
        } else if (finding_is(finding, "src/dirty.c", "literal 7 x2")) {
            literal_hits++;
        } else if (finding_is(finding, "src/pair_a.c", "literal 42 x2") ||
                   finding_is(finding, "tools/pair_b.py", "literal 42 x2")) {
            pair_hits++;
        } else if (strcmp(finding->path, "tests/big.c") == 0 &&
                   strcmp(finding->kind, "sloc") == 0) {
            sloc_hits++;
        } else {
            others_silent = 0;
        }
    }
    size_t dirty_stale = dirty_run.stale_count;
    char clean_root[BT_PATH_MAX];
    snprintf(clean_root, sizeof clean_root, "%s/clean_only", run_dir);
    policy_outcome clean_run;
    int clean_rc = policy_run_checks(clean_root, POLICY_MODE_BOTH, config, allow_path, &clean_run,
                                     err, err_len);
    bt_json_free(config);
    int clean_empty = clean_rc == 0 && clean_run.finding_count == 0 && clean_run.stale_count == 0;
    if (clean_rc == 0) {
        policy_outcome_free(&clean_run);
    }
    policy_outcome_free(&dirty_run);
    int pass = line_hits == 1 && block_hits == 1 && hash_hits == 1 && literal_hits == 1 &&
               pair_hits == 2 && sloc_hits == 1 && others_silent && dirty_stale == 0 && clean_empty;
    if (!pass) {
        BT_TRACE_FAIL("policy.selftest",
                      "counts line=%llu block=%llu hash=%llu literal=%llu pair=%llu sloc=%llu "
                      "silent=%d stale=%llu clean_rc=%d",
                      (unsigned long long)line_hits, (unsigned long long)block_hits,
                      (unsigned long long)hash_hits, (unsigned long long)literal_hits,
                      (unsigned long long)pair_hits, (unsigned long long)sloc_hits, others_silent,
                      (unsigned long long)dirty_stale, clean_rc);
    }
    BT_TRACE_EXIT("policy.selftest");
    return pass ? 1 : 0;
}
