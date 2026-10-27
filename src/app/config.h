#ifndef BT_APP_CONFIG_H
#define BT_APP_CONFIG_H

#include "common/json.h"
#include "common/paths.h"

#include <stddef.h>

#define BT_APP_TITLE_MAX 64
#define BT_APP_MEMO_MODE_MAX 32

typedef struct {
    unsigned int width;
    unsigned int height;
    char title[BT_APP_TITLE_MAX];
} bt_app_window_cfg;

typedef struct {
    unsigned int cadence_ms;
    char mode[BT_APP_MEMO_MODE_MAX];
} bt_app_memo_cfg;

typedef struct {
    unsigned int reps;
} bt_app_drill_cfg;

typedef struct {
    unsigned int target_fps;
    int vsync;
    unsigned int capture_dwell_frames;
} bt_app_render_cfg;

typedef struct {
    bt_app_window_cfg window;
    bt_app_memo_cfg memo;
    bt_app_drill_cfg drill;
    bt_app_render_cfg render;
    unsigned long long smoke_seed;
} bt_app_config;

void bt_app_config_defaults(bt_app_config *config);
int bt_app_config_from_json(const bt_json_value *value, bt_app_config *out, char *error,
                            size_t error_len);
const char *bt_app_display_path(const char *path, char *out, size_t out_len);
int bt_app_load_config_value(const char *path, bt_json_value **out, char *error, size_t error_len);

#endif
