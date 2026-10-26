#ifndef BT_PATHS_H
#define BT_PATHS_H

#include <stddef.h>

#define BT_PATH_MAX 1024
#define BT_APP_DIR_NAME "blind-trainer"

void bt_paths_workspace_root_from(const char *start_dir, char out[BT_PATH_MAX]);
void bt_paths_workspace_root(char out[BT_PATH_MAX]);
void bt_paths_user_root_from(const char *appdata, const char *xdg, const char *home,
                             char out[BT_PATH_MAX]);
void bt_paths_user_root(char out[BT_PATH_MAX]);
void bt_paths_config_path_from(const char *env_value, const char *workspace_root,
                               char out[BT_PATH_MAX]);
void bt_paths_config_path(char out[BT_PATH_MAX]);
void bt_paths_exe_dir(char out[BT_PATH_MAX]);
void bt_paths_resolve_root(char out[BT_PATH_MAX], const char *primary,
                           const char *const *candidates, size_t count);
int bt_paths_write_atomic(const char *path, const void *data, size_t len);

#endif
