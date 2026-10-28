#ifndef BT_RV_EXEC_H
#define BT_RV_EXEC_H

#include <stddef.h>

#include "common/paths.h"

#define BT_RV_SPAWN_DETAIL_MAX 256

typedef enum {
    BT_RV_OUTCOME_EXIT = 0,
    BT_RV_OUTCOME_TIMEOUT = 1,
    BT_RV_OUTCOME_SPAWN = 2,
} bt_rv_outcome_kind;

typedef struct {
    bt_rv_outcome_kind kind;
    int exit_code;
    char detail[BT_RV_SPAWN_DETAIL_MAX];
} bt_rv_outcome;

typedef struct {
    const char *key;
    const char *value;
} bt_rv_env_pair;

typedef struct {
    char program[BT_PATH_MAX];
    char **args;
    size_t arg_count;
    char cwd[BT_PATH_MAX];
    char **env;
    size_t env_count;
} bt_rv_child_spec;

typedef struct {
    bt_rv_outcome outcome;
    char *stdout_text;
    char *stderr_text;
    unsigned long long elapsed_ms;
    unsigned long pid;
} bt_rv_run_result;

const char *bt_rv_env_get(const bt_rv_child_spec *spec, const char *key);
size_t bt_rv_scrub_env(const char *const vars[], char ***out);
void bt_rv_str_list_free(char **list, size_t count);
void bt_rv_child_spec_free(bt_rv_child_spec *spec);
void bt_rv_stage_spec(const char *program, const char *const args[], const char *cwd,
                      const char *const vars[], const bt_rv_env_pair *extras, size_t extra_count,
                      bt_rv_child_spec *out);
int bt_rv_run_child(const bt_rv_child_spec *spec, unsigned long long deadline_ms,
                    bt_rv_run_result *out);
void bt_rv_run_result_free(bt_rv_run_result *result);

#endif
