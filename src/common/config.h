#ifndef BT_CONFIG_H
#define BT_CONFIG_H

#include "json.h"

int bt_config_try_load(const char *path, bt_json_value **out);
const bt_json_value *bt_config_load(void);
void bt_config_reset(void);
const bt_json_value *bt_config_value(const bt_json_value *config, const char *dotted_path);
int bt_config_str(const bt_json_value *config, const char *dotted_path, const char **out);
int bt_config_int(const bt_json_value *config, const char *dotted_path, long long *out);
int bt_config_f64(const bt_json_value *config, const char *dotted_path, double *out);
int bt_config_bool(const bt_json_value *config, const char *dotted_path, int *out);

#endif
