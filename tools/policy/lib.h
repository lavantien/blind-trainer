#ifndef POLICY_LIB_H
#define POLICY_LIB_H

#include <stddef.h>

#include "common/json.h"
#include "common/paths.h"
#include "policy/lexer.h"

#define POLICY_ERR_MAX 256

typedef struct {
    char *path;
    size_t line;
    const char *kind;
    char *detail;
} policy_finding;

typedef enum {
    POLICY_MODE_COMMENTS,
    POLICY_MODE_CONSTANTS,
    POLICY_MODE_SLOC,
    POLICY_MODE_BOTH,
} policy_check_mode;

typedef struct {
    policy_finding *findings;
    size_t finding_count;
    size_t finding_cap;
    char **stale_allowlist;
    size_t stale_count;
    size_t stale_cap;
} policy_outcome;

typedef struct {
    char *path;
    char *core;
    char *justification;
} policy_allow_entry;

typedef struct {
    policy_allow_entry *entries;
    size_t count;
} policy_allowlist;

void *policy_xalloc(size_t size);
char *policy_xstrdup(const char *text);
char *policy_xstrndup(const char *text, size_t len);

void policy_finding_free(policy_finding *finding);
void policy_outcome_free(policy_outcome *outcome);
void policy_allowlist_free(policy_allowlist *allowlist);

void policy_workspace_root(char out[BT_PATH_MAX]);
int policy_config_path_from(const char *env_value, const char *workspace_root, char *out,
                            size_t out_len);
bt_json_value *policy_load_config(char *err, size_t err_len);

int policy_parse_allowlist(const char *text, const char *source, policy_allowlist *out, char *err,
                           size_t err_len);
int policy_allowlist_covers(const policy_allowlist *allow, const char *path, const char *core);

int policy_glob_match(const char *pattern, const char *name);
char **policy_collect_sources(const char *root, size_t *count, char *err, size_t err_len);
void policy_free_sources(char **files, size_t count);

int policy_run_checks(const char *root, policy_check_mode mode, const bt_json_value *config,
                      const char *allowlist_path, policy_outcome *outcome, char *err,
                      size_t err_len);
int policy_selftest(const char *workdir, char *err, size_t err_len);

extern const char POLICY_DIRTY_FIXTURE[];
extern const char POLICY_CLEAN_FIXTURE[];
extern const char POLICY_BIG_FIXTURE[];
extern const char POLICY_PY_DIRTY_FIXTURE[];
extern const char POLICY_PY_CLEAN_FIXTURE[];
extern const char POLICY_PAIR_C_FIXTURE[];
extern const char POLICY_PAIR_PY_FIXTURE[];
extern const char POLICY_SELFTEST_CONFIG[];

#endif
