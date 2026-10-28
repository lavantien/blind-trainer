#ifndef BT_RV_LIB_H
#define BT_RV_LIB_H

#include <stddef.h>

#include "common/json.h"
#include "common/paths.h"
#include "release-verify/exec.h"

#define BT_RV_ERR_MAX 512
#define BT_RV_USAGE_LINE "usage: bt-release-verify <staging-dir> <zip> [--run]"
#define BT_RV_DB_FILE_FALLBACK "blind-trainer.db"
#define BT_RV_LATEST_USER_VERSION 3ULL
#define BT_RV_NAME_MAX 128
#define BT_RV_SHOT_NAME_MAX 64
#define BT_RV_SHOTS_MAX 32

enum {
    BT_RV_EXIT_OK = 0,
    BT_RV_EXIT_FAIL = 1,
    BT_RV_EXIT_USAGE = 2,
};

typedef struct {
    char staging_dir[BT_PATH_MAX];
    char exe[BT_RV_NAME_MAX];
    char config_file[BT_RV_NAME_MAX];
    char fonts_dir[BT_RV_NAME_MAX];
    char db_file[BT_RV_NAME_MAX];
    unsigned long long run_timeout_secs;
    int neg_config_exit;
    int usage_exit;
    char shots[BT_RV_SHOTS_MAX][BT_RV_SHOT_NAME_MAX];
    size_t shot_count;
} bt_rv_release_cfg;

typedef struct {
    char staging[BT_PATH_MAX];
    char zip[BT_PATH_MAX];
    int run;
} bt_rv_parsed_args;

typedef struct {
    const char *const *argv;
    const char *config;
    const char *const *vars;
    const char *root;
} bt_rv_env;

int bt_rv_parse_args(int argc, const char *const argv[], bt_rv_parsed_args *out, char *err,
                     size_t err_cap);
int bt_rv_release_cfg_from(const bt_json_value *value, bt_rv_release_cfg *out, char *err,
                           size_t err_cap);
int bt_rv_read_db_user_version(const unsigned char *bytes, size_t len, unsigned int *version,
                               char *err, size_t err_cap);
int bt_rv_db_stage(const char *sandbox_root, const bt_rv_release_cfg *cfg, char *err,
                   size_t err_cap);
int bt_rv_packaged_window_dims(const char *staging, const char *config_file, unsigned int *width,
                               unsigned int *height, char *err, size_t err_cap);
int bt_rv_zip_root_from(const char *staging, char out[BT_PATH_MAX], char *err, size_t err_cap);
int bt_rv_check_exit(const bt_rv_outcome *outcome, int expected, char *err, size_t err_cap);
int bt_rv_check_shot_dir(const char *dir, const bt_rv_release_cfg *cfg, unsigned int width,
                         unsigned int height, size_t *count, char *err, size_t err_cap);
int bt_rv_smoke_stage(const char *exe, const char *sandbox_root, const bt_rv_release_cfg *cfg,
                      unsigned int width, unsigned int height, const char *const vars[],
                      const char *bt_config, size_t *shot_count, char *err, size_t err_cap);
int bt_rv_n1_spec(const char *exe, const char *extract_dir, const char *sandbox_root,
                  const bt_rv_release_cfg *cfg, const char *const vars[], bt_rv_child_spec *out,
                  char *err, size_t err_cap);
int bt_rv_n1_stage(const char *exe, const char *extract_dir, const char *sandbox_root,
                   const bt_rv_release_cfg *cfg, const char *const vars[], char *err,
                   size_t err_cap);
int bt_rv_n2_stage(const char *exe, const char *sandbox_root, const bt_rv_release_cfg *cfg,
                   const char *const vars[], char *err, size_t err_cap);
int bt_rv_run(const bt_rv_env *env, int *exit_code, char *err, size_t err_cap);

#endif
