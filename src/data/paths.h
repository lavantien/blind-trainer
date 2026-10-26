#ifndef BT_DATA_PATHS_H
#define BT_DATA_PATHS_H

#include "common/json.h"
#include "common/paths.h"

#include <stddef.h>

#define BT_DATA_USER_ALGS_FILE "user-algs.json"
#define BT_DATA_SETTINGS_FILE "settings.json"
#define BT_DATA_SRS_FILE "srs.json"
#define BT_DATA_SOLVES_FILE "solves.json"
#define BT_DATA_DB_FILE "blind-trainer.db"

typedef enum {
    BT_PATHS_ERR_READ,
    BT_PATHS_ERR_WRITE,
    BT_PATHS_ERR_SERIALIZE,
    BT_PATHS_ERR_DESERIALIZE,
    BT_PATHS_ERR_MISSING,
} bt_paths_error_kind;

typedef struct {
    bt_paths_error_kind kind;
    char path[BT_PATH_MAX];
    char detail[160];
    char section[48];
} bt_paths_error;

typedef enum {
    BT_DATA_READ_OK,
    BT_DATA_READ_MISSING,
    BT_DATA_READ_ERROR,
} bt_data_read_status;

typedef struct {
    char *key;
    char *alg;
} bt_user_alg_entry;

typedef struct {
    bt_user_alg_entry *items;
    size_t count;
    size_t cap;
} bt_user_algs;

void bt_paths_error_string(const bt_paths_error *err, char *out, size_t out_len);
void bt_data_workspace_root(char out[BT_PATH_MAX]);
void bt_data_config_path_from(const char *env_value, char out[BT_PATH_MAX]);
void bt_data_config_path(char out[BT_PATH_MAX]);
bt_json_value *bt_data_load_config(void);
int bt_data_vendor_root_from(const char *env_value, const bt_json_value *config,
                             char out[BT_PATH_MAX], bt_paths_error *err);
bt_json_value *bt_data_vendor_root_or_null(char out[BT_PATH_MAX]);
int bt_data_packs_root_from(const char *env_value, const bt_json_value *config,
                            char out[BT_PATH_MAX], bt_paths_error *err);
bt_json_value *bt_data_packs_root_or_null(char out[BT_PATH_MAX]);
void bt_data_user_root_from(const char *appdata, const char *xdg, const char *home,
                            char out[BT_PATH_MAX]);
void bt_data_user_root(char out[BT_PATH_MAX]);
void bt_data_user_dir_path_at(const char *root, const char *kind, char out[BT_PATH_MAX]);
void bt_data_user_algs_path_at(const char *root, char out[BT_PATH_MAX]);
void bt_data_settings_path_at(const char *root, char out[BT_PATH_MAX]);
void bt_data_srs_path_at(const char *root, char out[BT_PATH_MAX]);
void bt_data_solves_path_at(const char *root, char out[BT_PATH_MAX]);
void bt_data_db_file_name(char out[BT_PATH_MAX]);
void bt_data_db_path_at(const char *root, char out[BT_PATH_MAX]);
bt_data_read_status bt_data_read_json(const char *path, bt_json_value **out);
int bt_data_write_json_atomic(const char *path, const bt_json_value *value, bt_paths_error *err);

void bt_user_algs_init(bt_user_algs *algs);
void bt_user_algs_free(bt_user_algs *algs);
int bt_user_algs_clone(const bt_user_algs *src, bt_user_algs *dst);
int bt_user_algs_eq(const bt_user_algs *a, const bt_user_algs *b);
void bt_user_algs_set(bt_user_algs *algs, const char *sheet_file, const char *position,
                      const char *alg);
int bt_user_algs_set_entry(bt_user_algs *algs, const char *key, const char *alg);
const char *bt_user_algs_alg_for(const bt_user_algs *algs, const char *sheet_file,
                                 const char *position);
int bt_user_algs_remove(bt_user_algs *algs, const char *sheet_file, const char *position);
size_t bt_user_algs_len(const bt_user_algs *algs);
int bt_user_algs_is_empty(const bt_user_algs *algs);
int bt_user_algs_save_at(const bt_user_algs *algs, const char *user_root, bt_paths_error *err);
bt_data_read_status bt_user_algs_load_at(bt_user_algs *algs, const char *user_root,
                                         bt_paths_error *err);
int bt_user_algs_from_json(const bt_json_value *root, bt_user_algs *out, bt_paths_error *err);

#endif
