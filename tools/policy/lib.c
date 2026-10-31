#include "policy/lib.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

#include "common/bt_trace.h"
#include "policy/comments.h"
#include "policy/constants.h"
#include "policy/fsutil.h"
#include "policy/sloc.h"

enum {
    POLICY_SCAN_PATTERN_COUNT = 12,
    POLICY_SKIP_DIR_COUNT = 7,
    POLICY_MAX_PATTERN_SEGS = 8,
    POLICY_DETAIL_MAX = 64,
};

static const char *const POLICY_SCAN_PATTERNS[POLICY_SCAN_PATTERN_COUNT] = {
    "src/**/*.c",    "src/**/*.h",     "src/**/*.py",    "tools/**/*.c",
    "tools/**/*.h",  "tools/**/*.py",  "tests/**/*.c",   "tests/**/*.h",
    "tests/**/*.py", "scripts/**/*.c", "scripts/**/*.h", "scripts/**/*.py",
};

static const char *const POLICY_SKIP_DIRS[POLICY_SKIP_DIR_COUNT] = {
    "vendor",  "build",      "ref_ground", "ref_doc",
    "playground", "artifacts", "__pycache__",
};

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
    "{\"policy\":{\"literal_recurrence\":2,\"small_literal_allow\":[],\"exempt_ext\":[]},"
    "\"sloc\":{\"max_file_lines\":8}}";

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
    BT_TRACE_ENTER("policy.parse_allowlist");
    memset(out, 0, sizeof *out);
    size_t cap = 0;
    size_t line_no = 0;
    const char *cursor = text;
    while (*cursor != '\0') {
        line_no++;
        const char *nl = strchr(cursor, '\n');
        size_t raw_len = nl != NULL ? (size_t)(nl - cursor) : strlen(cursor);
        if (raw_len > 0 && cursor[raw_len - 1] == '\r') {
            raw_len--;
        }
        if (raw_len > 0 && cursor[0] != '#') {
            const char *tab1 = memchr(cursor, '\t', raw_len);
            const char *tab2 =
                tab1 == NULL ? NULL : memchr(tab1 + 1, '\t', (size_t)(cursor + raw_len - tab1 - 1));
            if (tab1 == NULL || tab2 == NULL) {
                snprintf(err, err_len,
                         "allowlist %s line %zu needs path TAB core TAB justification", source,
                         line_no);
                BT_TRACE_FAIL("policy.parse_allowlist", "short line %zu in %s", line_no, source);
                policy_allowlist_free(out);
                BT_TRACE_EXIT("policy.parse_allowlist");
                return -1;
            }
            if (out->count == cap) {
                cap = cap == 0 ? 16 : cap * 2;
                policy_allow_entry *grown = realloc(out->entries, cap * sizeof *grown);
                if (grown == NULL) {
                    BT_TRACE_FAIL("policy.parse_allowlist", "entry growth failed at %zu", cap);
                    fputs("bt-policy: out of memory\n", stderr);
                    exit(2);
                }
                out->entries = grown;
            }
            policy_allow_entry *entry = &out->entries[out->count];
            entry->path = policy_xstrndup(cursor, (size_t)(tab1 - cursor));
            entry->core = policy_xstrndup(tab1 + 1, (size_t)(tab2 - tab1 - 1));
            entry->justification = policy_xstrndup(tab2 + 1, (size_t)(cursor + raw_len - tab2 - 1));
            out->count++;
        }
        if (nl == NULL) {
            break;
        }
        cursor = nl + 1;
    }
    BT_TRACE_EXIT("policy.parse_allowlist");
    return 0;
}

int policy_allowlist_covers(const policy_allowlist *allow, const char *path, const char *core) {
    for (size_t at = 0; at < allow->count; at++) {
        if (strcmp(allow->entries[at].path, path) == 0 &&
            strcmp(allow->entries[at].core, core) == 0) {
            return 1;
        }
    }
    return 0;
}

int policy_glob_match(const char *pattern, const char *name) {
    if (*pattern == '\0') {
        return *name == '\0';
    }
    if (*pattern == '*') {
        if (policy_glob_match(pattern + 1, name)) {
            return 1;
        }
        return *name != '\0' && policy_glob_match(pattern, name + 1);
    }
    return *name != '\0' && *pattern == *name && policy_glob_match(pattern + 1, name + 1);
}

static int skip_dir_name(const char *name) {
    for (size_t at = 0; at < POLICY_SKIP_DIR_COUNT; at++) {
        if (strcmp(POLICY_SKIP_DIRS[at], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int walk(const char *dir, const char *const *segs, size_t depth, policy_strvec *out,
                char *err, size_t err_len) {
    if (depth == 0) {
        if (policy_path_is_file(dir) && !policy_path_is_reparse_or_link(dir)) {
            policy_strvec_push(out, policy_xstrdup(dir));
        }
        return 0;
    }
    policy_strvec names = {0};
    if (policy_list_dir(dir, &names, err, err_len) != 0) {
        goto fail;
    }
    if (strcmp(segs[0], "**") == 0) {
        if (walk(dir, segs + 1, depth - 1, out, err, err_len) != 0) {
            goto fail;
        }
        for (size_t at = 0; at < names.count; at++) {
            char child[BT_PATH_MAX];
            if (policy_join_checked(child, dir, names.items[at], err, err_len) != 0) {
                goto fail;
            }
            if (policy_path_is_dir(child) && !skip_dir_name(names.items[at]) &&
                !policy_path_is_reparse_or_link(child) &&
                walk(child, segs, depth, out, err, err_len) != 0) {
                goto fail;
            }
        }
    } else {
        for (size_t at = 0; at < names.count; at++) {
            if (policy_glob_match(segs[0], names.items[at])) {
                char child[BT_PATH_MAX];
                if (policy_join_checked(child, dir, names.items[at], err, err_len) != 0) {
                    goto fail;
                }
                if (walk(child, segs + 1, depth - 1, out, err, err_len) != 0) {
                    goto fail;
                }
            }
        }
    }
    for (size_t at = 0; at < names.count; at++) {
        free(names.items[at]);
    }
    free(names.items);
    return 0;
fail:
    for (size_t at = 0; at < names.count; at++) {
        free(names.items[at]);
    }
    free(names.items);
    return -1;
}

char **policy_collect_sources(const char *root, size_t *count, char *err, size_t err_len) {
    BT_TRACE_ENTER("policy.collect_sources");
    policy_strvec files = {0};
    for (size_t at = 0; at < POLICY_SCAN_PATTERN_COUNT; at++) {
        const char *pattern = POLICY_SCAN_PATTERNS[at];
        char pattern_copy[BT_PATH_MAX];
        snprintf(pattern_copy, sizeof pattern_copy, "%s", pattern);
        const char *segs[POLICY_MAX_PATTERN_SEGS];
        size_t seg_count = 0;
        char *cursor = pattern_copy;
        while (cursor != NULL && seg_count < POLICY_MAX_PATTERN_SEGS) {
            char *slash = strchr(cursor, '/');
            if (slash != NULL) {
                *slash = '\0';
            }
            segs[seg_count++] = cursor;
            cursor = slash != NULL ? slash + 1 : NULL;
        }
        if (walk(root, segs, seg_count, &files, err, err_len) != 0) {
            for (size_t hit = 0; hit < files.count; hit++) {
                free(files.items[hit]);
            }
            free(files.items);
            BT_TRACE_EXIT("policy.collect_sources");
            return NULL;
        }
    }
    policy_strvec_sort(&files);
    size_t used = 0;
    for (size_t at = 0; at < files.count; at++) {
        if (used > 0 && strcmp(files.items[used - 1], files.items[at]) == 0) {
            free(files.items[at]);
            continue;
        }
        files.items[used++] = files.items[at];
    }
    *count = used;
    if (used == 0) {
        snprintf(err, err_len, "no sources under %s", root);
        BT_TRACE_FAIL("policy.collect_sources", "no sources under %s", root);
        BT_TRACE_EXIT("policy.collect_sources");
        return NULL;
    }
    BT_TRACE_EXIT("policy.collect_sources");
    return files.items;
}

void policy_free_sources(char **files, size_t count) {
    for (size_t at = 0; at < count; at++) {
        free(files[at]);
    }
    free(files);
}

static const char *rel_path(const char *path, const char *root) {
    size_t root_len = strlen(root);
    if (strncmp(path, root, root_len) == 0 && path[root_len] == '/') {
        return path + root_len + 1;
    }
    return path;
}

static int has_tests_component(const char *rel) {
    const char *at = rel;
    while (*at != '\0') {
        const char *seg = at;
        while (*at != '\0' && *at != '/') {
            at++;
        }
        if ((size_t)(at - seg) == 5 && memcmp(seg, "tests", 5) == 0) {
            return 1;
        }
        if (*at == '/') {
            at++;
        }
    }
    return 0;
}

static int policy_read_source(const char *path, char **text_out, size_t *len_out, char *err,
                              size_t err_len) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        snprintf(err, err_len, "read %s: %s", path, strerror(errno));
        return -1;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = policy_xalloc(size > 0 ? (size_t)size + 1 : 1);
    size_t got = fread(text, 1, size > 0 ? (size_t)size : 0, file);
    int read_error = ferror(file);
    fclose(file);
    if (read_error || got != (size > 0 ? (size_t)size : 0)) {
        snprintf(err, err_len, "read %s: short read", path);
        BT_TRACE_FAIL("policy.read_source", "short read on %s", path);
        free(text);
        return -1;
    }
    text[got] = '\0';
    *text_out = text;
    *len_out = got;
    return 0;
}

static void outcome_push_finding(policy_outcome *outcome, policy_finding finding) {
    if (outcome->finding_count == outcome->finding_cap) {
        outcome->finding_cap = outcome->finding_cap == 0 ? 16 : outcome->finding_cap * 2;
    }
    policy_finding *grown = realloc(outcome->findings, outcome->finding_cap * sizeof *grown);
    if (grown == NULL) {
        BT_TRACE_FAIL("policy.run_checks", "finding growth failed at %zu", outcome->finding_cap);
        fputs("bt-policy: out of memory\n", stderr);
        exit(2);
    }
    outcome->findings = grown;
    outcome->findings[outcome->finding_count++] = finding;
}

static void outcome_push_stale(policy_outcome *outcome, char *stale) {
    if (outcome->stale_count == outcome->stale_cap) {
        outcome->stale_cap = outcome->stale_cap == 0 ? 8 : outcome->stale_cap * 2;
    }
    char **grown = realloc(outcome->stale_allowlist, outcome->stale_cap * sizeof *grown);
    if (grown == NULL) {
        BT_TRACE_FAIL("policy.run_checks", "stale growth failed at %zu", outcome->stale_cap);
        fputs("bt-policy: out of memory\n", stderr);
        exit(2);
    }
    outcome->stale_allowlist = grown;
    outcome->stale_allowlist[outcome->stale_count++] = stale;
}

typedef struct {
    char *path;
    char *core;
} policy_covered_pair;

static const char *const POLICY_SOURCE_EXTS[] = {
    ".cpp", ".cc",  ".cxx", ".hpp", ".hh",  ".hxx", ".rs",   ".go",
    ".js",  ".mjs", ".cjs", ".ts",  ".tsx", ".jsx", ".java",
};

static int ext_matches_ci(const char *left, const char *right) {
    if (strlen(left) != strlen(right)) {
        return 0;
    }
    for (size_t at = 0; left[at] != '\0'; at++) {
        char a = left[at];
        char b = right[at];
        if (a >= 'A' && a <= 'Z') {
            a = (char)(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = (char)(b - 'A' + 'a');
        }
        if (a != b) {
            return 0;
        }
    }
    return 1;
}

static int source_ext_finding(const char *name, const policy_policy_config *pcfg,
                              const char **ext_out) {
    const char *dot = strrchr(name, '.');
    if (dot == NULL || dot == name || dot[1] == '\0') {
        return 0;
    }
    for (size_t at = 0; at < sizeof POLICY_SOURCE_EXTS / sizeof POLICY_SOURCE_EXTS[0]; at++) {
        if (ext_matches_ci(dot, POLICY_SOURCE_EXTS[at])) {
            for (size_t ex = 0; ex < pcfg->exempt_count; ex++) {
                if (ext_matches_ci(dot, pcfg->exempt_ext[ex])) {
                    return 0;
                }
            }
            *ext_out = dot;
            return 1;
        }
    }
    return 0;
}

static int sweep_unexpected_extensions(const char *dir, const char *root,
                                       const policy_policy_config *pcfg, policy_outcome *outcome,
                                       char *err, size_t err_len) {
    policy_strvec names = {0};
    if (policy_list_dir(dir, &names, err, err_len) != 0) {
        return -1;
    }
    size_t at = 0;
    for (; at < names.count; at++) {
        char child[BT_PATH_MAX];
        if (policy_join_checked(child, dir, names.items[at], err, err_len) != 0) {
            goto fail;
        }
        if (policy_path_is_reparse_or_link(child)) {
            continue;
        }
        if (policy_path_is_dir(child) && !skip_dir_name(names.items[at]) &&
            sweep_unexpected_extensions(child, root, pcfg, outcome, err, err_len) != 0) {
            goto fail;
        }
        if (!policy_path_is_dir(child)) {
            const char *ext = NULL;
            if (source_ext_finding(names.items[at], pcfg, &ext)) {
                policy_finding finding;
                memset(&finding, 0, sizeof finding);
                finding.path = policy_xstrdup(rel_path(child, root));
                finding.line = 1;
                finding.kind = "extension";
                finding.detail = policy_xalloc(POLICY_DETAIL_MAX);
                snprintf(finding.detail, POLICY_DETAIL_MAX, "unexpected %s file", ext);
                outcome_push_finding(outcome, finding);
            }
        }
        free(names.items[at]);
    }
    free(names.items);
    return 0;
fail:
    for (; at < names.count; at++) {
        free(names.items[at]);
    }
    free(names.items);
    return -1;
}

static const char *const POLICY_SCAN_ROOTS[] = {"src", "tools", "tests", "scripts"};

int policy_run_checks(const char *root, policy_check_mode mode, const bt_json_value *config,
                      const char *allowlist_path, policy_outcome *outcome, char *err,
                      size_t err_len) {
    BT_TRACE_ENTER("policy.run_checks");
    memset(outcome, 0, sizeof *outcome);
    size_t file_count = 0;
    char **files = policy_collect_sources(root, &file_count, err, err_len);
    if (files == NULL) {
        BT_TRACE_EXIT("policy.run_checks");
        return -1;
    }
    int rc = 0;
    policy_policy_config pcfg;
    int have_pcfg = 0;
    if (mode == POLICY_MODE_CONSTANTS || mode == POLICY_MODE_BOTH) {
        rc = policy_constants_from_config(config, &pcfg, err, err_len);
        if (rc != 0) {
            policy_free_sources(files, file_count);
            BT_TRACE_EXIT("policy.run_checks");
            return -1;
        }
        have_pcfg = 1;
    }
    if (mode == POLICY_MODE_BOTH) {
        for (size_t at = 0; at < sizeof POLICY_SCAN_ROOTS / sizeof POLICY_SCAN_ROOTS[0] && rc == 0;
             at++) {
            char dir[BT_PATH_MAX];
            if (policy_join_checked(dir, root, POLICY_SCAN_ROOTS[at], err, err_len) != 0) {
                rc = -1;
                break;
            }
            rc = sweep_unexpected_extensions(dir, root, &pcfg, outcome, err, err_len);
        }
    }
    if (mode == POLICY_MODE_COMMENTS || mode == POLICY_MODE_BOTH) {
        for (size_t at = 0; at < file_count && rc == 0; at++) {
            char *text = NULL;
            size_t len = 0;
            rc = policy_read_source(files[at], &text, &len, err, err_len);
            if (rc != 0) {
                break;
            }
            policy_finding *more = NULL;
            size_t more_count = 0;
            policy_comment_findings(text, len, rel_path(files[at], root), &more, &more_count);
            for (size_t hit = 0; hit < more_count; hit++) {
                outcome_push_finding(outcome, more[hit]);
            }
            free(more);
            free(text);
        }
    }
    if (rc == 0 && (mode == POLICY_MODE_CONSTANTS || mode == POLICY_MODE_BOTH)) {
        char *allow_text = NULL;
        policy_allowlist allow;
        memset(&allow, 0, sizeof allow);
        size_t allow_len = 0;
        rc = policy_read_source(allowlist_path, &allow_text, &allow_len, err, err_len);
        if (rc == 0) {
            rc = policy_parse_allowlist(allow_text, allowlist_path, &allow, err, err_len);
        }
        policy_covered_pair *covered = NULL;
        size_t covered_count = 0;
        size_t covered_cap = 0;
        for (size_t at = 0; at < file_count && rc == 0; at++) {
            const char *rel_probe = rel_path(files[at], root);
            if (has_tests_component(rel_probe)) {
                continue;
            }
            char *text = NULL;
            size_t len = 0;
            rc = policy_read_source(files[at], &text, &len, err, err_len);
            if (rc != 0) {
                break;
            }
            policy_lang lang = policy_lang_of_path(files[at]);
            policy_masked masked;
            policy_mask_source(text, len, lang, &masked);
            const char *rel = rel_probe;
            size_t hit_count = 0;
            policy_recurring *hits = policy_recurring_literals(masked.text, len, lang,
                                                               pcfg.literal_recurrence, &hit_count);
            for (size_t hit = 0; hit < hit_count; hit++) {
                int small_allowed = 0;
                for (size_t allow_at = 0; allow_at < pcfg.allow_count; allow_at++) {
                    if (strcmp(pcfg.small_literal_allow[allow_at], hits[hit].core) == 0) {
                        small_allowed = 1;
                        break;
                    }
                }
                if (small_allowed) {
                    continue;
                }
                if (covered_count == covered_cap) {
                    covered_cap = covered_cap == 0 ? 16 : covered_cap * 2;
                    policy_covered_pair *grown = realloc(covered, covered_cap * sizeof *grown);
                    if (grown == NULL) {
                        BT_TRACE_FAIL("policy.run_checks", "covered growth failed at %zu",
                                      covered_cap);
                        fputs("bt-policy: out of memory\n", stderr);
                        exit(2);
                    }
                    covered = grown;
                }
                covered[covered_count].path = policy_xstrdup(rel);
                covered[covered_count].core = policy_xstrdup(hits[hit].core);
                covered_count++;
                if (!policy_allowlist_covers(&allow, rel, hits[hit].core)) {
                    policy_finding finding;
                    memset(&finding, 0, sizeof finding);
                    finding.path = policy_xstrdup(rel);
                    finding.line = hits[hit].line;
                    finding.kind = "constant";
                    finding.detail = policy_xalloc(96);
                    snprintf(finding.detail, 96, "literal %s x%llu", hits[hit].core,
                             (unsigned long long)hits[hit].count);
                    outcome_push_finding(outcome, finding);
                }
            }
            policy_recurring_free(hits, hit_count);
            policy_masked_free(&masked);
            free(text);
        }
        if (rc == 0) {
            for (size_t at = 0; at < allow.count; at++) {
                int seen = 0;
                for (size_t pair = 0; pair < covered_count; pair++) {
                    if (strcmp(covered[pair].path, allow.entries[at].path) == 0 &&
                        strcmp(covered[pair].core, allow.entries[at].core) == 0) {
                        seen = 1;
                        break;
                    }
                }
                if (!seen) {
                    char *stale = policy_xalloc(BT_PATH_MAX + POLICY_DETAIL_MAX);
                    snprintf(stale, BT_PATH_MAX + POLICY_DETAIL_MAX, "%s TAB %s",
                             allow.entries[at].path, allow.entries[at].core);
                    outcome_push_stale(outcome, stale);
                }
            }
        }
        for (size_t pair = 0; pair < covered_count; pair++) {
            free(covered[pair].path);
            free(covered[pair].core);
        }
        free(covered);
        policy_allowlist_free(&allow);
        free(allow_text);
    }
    if (rc == 0 && (mode == POLICY_MODE_SLOC || mode == POLICY_MODE_BOTH)) {
        policy_sloc_config scfg;
        rc = policy_sloc_from_config(config, &scfg, err, err_len);
        for (size_t at = 0; at < file_count && rc == 0; at++) {
            char *text = NULL;
            size_t len = 0;
            rc = policy_read_source(files[at], &text, &len, err, err_len);
            if (rc != 0) {
                break;
            }
            policy_finding one;
            int fired =
                policy_cap_finding(text, len, rel_path(files[at], root), scfg.max_file_lines, &one);
            if (fired == 1) {
                outcome_push_finding(outcome, one);
            }
            free(text);
        }
    }
    if (have_pcfg) {
        policy_policy_config_free(&pcfg);
    }
    policy_free_sources(files, file_count);
    BT_TRACE_EXIT("policy.run_checks");
    return rc;
}
