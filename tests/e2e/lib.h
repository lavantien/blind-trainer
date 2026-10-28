#ifndef BT_E2E_LIB_H
#define BT_E2E_LIB_H

#include "common/json.h"
#include "common/paths.h"

#include <stddef.h>

#define BT_E2E_ERROR_MAX 1024
#define BT_E2E_NAME_MAX 256
#define BT_E2E_PARSE_USAGE 1

typedef struct {
    unsigned int width;
    unsigned int height;
} bt_e2e_window_dims;

typedef struct {
    char **shots;
    size_t shots_count;
    unsigned long long timeout_secs;
} bt_e2e_settings;

typedef struct {
    char binary[BT_PATH_MAX];
    char out_dir[BT_PATH_MAX];
    int has_binary;
    int has_out_dir;
} bt_e2e_args;

typedef enum {
    BT_E2E_SHOT_OK = 0,
    BT_E2E_SHOT_FAIL = 1,
} bt_e2e_shot_verdict;

int bt_e2e_parse_args(size_t argc, const char *const *argv, bt_e2e_args *out);
void bt_e2e_free_settings(bt_e2e_settings *settings);
int bt_e2e_read_config(const char *path, bt_json_value **out, char *error, size_t error_cap);
int bt_e2e_neg_config_exit_from(const bt_json_value *value, long *out, char *error,
                                size_t error_cap);
int bt_e2e_window_dims_from(const bt_json_value *value, bt_e2e_window_dims *out, char *error,
                            size_t error_cap);
int bt_e2e_settings_from(const bt_json_value *value, bt_e2e_settings *out, char *error,
                         size_t error_cap);
void bt_e2e_config_path_from(const char *env_value, const char *root, char out[BT_PATH_MAX]);
void bt_e2e_out_dir_from(const char *env_value, const bt_json_value *value, const char *root,
                         char out[BT_PATH_MAX]);
int bt_e2e_clean_pngs(const char *dir, char *error, size_t error_cap);
int bt_e2e_png_names(const char *dir, char ***out_names, size_t *out_count, char *error,
                     size_t error_cap);
void bt_e2e_free_names(char **names, size_t count);
int bt_e2e_expected_names(const char *const *shots, size_t shots_count,
                          char names[][BT_E2E_NAME_MAX], size_t *out_count);
void bt_e2e_classify(const char *const *found, size_t found_count, const char *const *expected,
                     size_t expected_count, char missing[][BT_E2E_NAME_MAX],
                     size_t *missing_count, char unexpected[][BT_E2E_NAME_MAX],
                     size_t *unexpected_count);
bt_e2e_shot_verdict bt_e2e_check_shot(const char *path, bt_e2e_window_dims dims, char *reason,
                                      size_t reason_cap);
void bt_e2e_tail(const unsigned char *bytes, size_t len, char *out, size_t out_cap);

#endif
