#include "policy/lib.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "policy/comments.h"
#include "policy/constants.h"
#include "policy/sloc.h"

const char POLICY_DIRTY_FIXTURE[] = "int dirty(void) {\n"
                                    "    const char *host = \"https://example.org/a//b\";\n"
                                    "    int scale = 7;\n"
                                    "    int shift = 7;\n"
                                    "    return scale + shift;\n"
                                    "}\n"
                                    "\n"
                                    "// selftest line marker\n"
                                    "/* nested /* marker */ tail */\n";

const char POLICY_CLEAN_FIXTURE[] = "int clean(void) {\n"
                                    "    int one = 1;\n"
                                    "    int two = 2;\n"
                                    "    return one + two;\n"
                                    "}\n";

const char POLICY_BIG_FIXTURE[] = "int big(void) {\n"
                                  "    int a = 1;\n"
                                  "    int b = 2;\n"
                                  "    int c = 3;\n"
                                  "    int d = 4;\n"
                                  "    int e = 5;\n"
                                  "    int f = 6;\n"
                                  "    int g = 7;\n"
                                  "    return a + b + c + d + e + f + g;\n"
                                  "}\n";

const char POLICY_PY_DIRTY_FIXTURE[] = "x = 1\ny = 2\nz = 4\n# selftest hash marker\n";

const char POLICY_PY_CLEAN_FIXTURE[] = "x = 1\ny = 2\nz = 4\n";

const char POLICY_PAIR_C_FIXTURE[] = "int pair_a(void) {\n"
                                     "    int x = 42;\n"
                                     "    int y = 42;\n"
                                     "    return x + y;\n"
                                     "}\n";

const char POLICY_PAIR_PY_FIXTURE[] = "a = 42\nb = 42\n";

const char POLICY_SELFTEST_CONFIG[] =
    "{\"policy\":{\"literal_recurrence\":2,\"small_literal_allow\":[]},\"sloc\":{"
    "\"max_file_lines\":8}}";

void *policy_xalloc(size_t size) {
    void *block = malloc(size == 0 ? 1 : size);
    if (block == NULL) {
        BT_TRACE_FAIL("policy.xalloc", "out of memory for %zu bytes", size);
        fputs("bt-policy: out of memory\n", stderr);
        exit(2);
    }
    return block;
}

char *policy_xstrdup(const char *text) {
    size_t len = strlen(text);
    char *copy = policy_xalloc(len + 1);
    memcpy(copy, text, len + 1);
    return copy;
}

char *policy_xstrndup(const char *text, size_t len) {
    char *copy = policy_xalloc(len + 1);
    memcpy(copy, text, len);
    copy[len] = '\0';
    return copy;
}

void policy_finding_free(policy_finding *finding) {
    free(finding->path);
    free(finding->detail);
    finding->path = NULL;
    finding->detail = NULL;
}

void policy_outcome_free(policy_outcome *outcome) {
    policy_findings_free(outcome->findings, outcome->finding_count);
    outcome->findings = NULL;
    outcome->finding_count = 0;
    outcome->finding_cap = 0;
    for (size_t at = 0; at < outcome->stale_count; at++) {
        free(outcome->stale_allowlist[at]);
    }
    free(outcome->stale_allowlist);
    outcome->stale_allowlist = NULL;
    outcome->stale_count = 0;
    outcome->stale_cap = 0;
}

void policy_allowlist_free(policy_allowlist *allowlist) {
    for (size_t at = 0; at < allowlist->count; at++) {
        free(allowlist->entries[at].path);
        free(allowlist->entries[at].core);
        free(allowlist->entries[at].justification);
    }
    free(allowlist->entries);
    allowlist->entries = NULL;
    allowlist->count = 0;
}

void policy_workspace_root(char out[BT_PATH_MAX]) {
    BT_TRACE_ENTER("policy.workspace_root");
    bt_paths_workspace_root(out);
    BT_TRACE_EXIT("policy.workspace_root");
}

int policy_config_path_from(const char *env_value, const char *workspace_root, char *out,
                            size_t out_len) {
    BT_TRACE_ENTER("policy.config_path_from");
    bt_paths_config_path_from(env_value, workspace_root, out);
    (void)out_len;
    BT_TRACE_EXIT("policy.config_path_from");
    return 0;
}

bt_json_value *policy_load_config(char *err, size_t err_len) {
    BT_TRACE_ENTER("policy.load_config");
    char path[BT_PATH_MAX];
    bt_paths_config_path(path);
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        snprintf(err, err_len, "read %s: %s", path, strerror(errno));
        BT_TRACE_FAIL("policy.load_config", "cannot open %s", path);
        BT_TRACE_EXIT("policy.load_config");
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = policy_xalloc(size > 0 ? (size_t)size + 1 : 1);
    size_t got = fread(text, 1, size > 0 ? (size_t)size : 0, file);
    fclose(file);
    text[got] = '\0';
    bt_json_value *config = NULL;
    bt_json_error json_err;
    if (bt_json_parse(text, got, &config, &json_err) != 0) {
        snprintf(err, err_len, "parse %s: %s", path, json_err.message);
        BT_TRACE_FAIL("policy.load_config", "parse failed on %s", path);
        free(text);
        BT_TRACE_EXIT("policy.load_config");
        return NULL;
    }
    free(text);
    BT_TRACE_EXIT("policy.load_config");
    return config;
}

int policy_parse_allowlist(const char *text, const char *source, policy_allowlist *out, char *err,
                           size_t err_len) {
    (void)text;
    (void)source;
    (void)out;
    snprintf(err, err_len, "stub");
    return -1;
}

int policy_allowlist_covers(const policy_allowlist *allow, const char *path, const char *core) {
    (void)allow;
    (void)path;
    (void)core;
    return 0;
}

int policy_glob_match(const char *pattern, const char *name) {
    (void)pattern;
    (void)name;
    return 0;
}

char **policy_collect_sources(const char *root, size_t *count) {
    (void)root;
    *count = 0;
    return NULL;
}

void policy_free_sources(char **files, size_t count) {
    for (size_t at = 0; at < count; at++) {
        free(files[at]);
    }
    free(files);
}

int policy_run_checks(const char *root, policy_check_mode mode, const bt_json_value *config,
                      const char *allowlist_path, policy_outcome *outcome, char *err,
                      size_t err_len) {
    (void)root;
    (void)mode;
    (void)config;
    (void)allowlist_path;
    (void)outcome;
    snprintf(err, err_len, "stub");
    return -1;
}

int policy_selftest(const char *workdir, char *err, size_t err_len) {
    (void)workdir;
    snprintf(err, err_len, "stub");
    return -1;
}
