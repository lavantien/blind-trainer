#ifndef BT_APP_BOOT_H
#define BT_APP_BOOT_H

#include "app/logic.h"
#include "app/states.h"

#include <stddef.h>

typedef enum {
    BT_APP_SHOT_NONE = 0,
    BT_APP_SHOT_SMOKE,
    BT_APP_SHOT_USAGE,
    BT_APP_SHOT_FATAL_SHOT,
} bt_app_shot_mode;

typedef void (*bt_app_state_fn)(bt_app_ctx *ctx);

typedef struct {
    bt_app_state_fn enter;
    bt_app_state_fn update;
    bt_app_state_fn draw;
} bt_app_state_row;

extern const bt_app_state_row BT_APP_STATE_TABLE[BT_APP_STATE_COUNT];

int bt_app_parse_shot_mode(int argc, char **argv, bt_app_shot_mode *mode, char *dir_out,
                           size_t dir_len, char *error, size_t error_len);
int bt_app_init(int argc, char **argv, bt_app_ctx *ctx, bt_app_shot_mode *mode, char *dir,
                size_t dir_len, char *error, size_t error_len);

#endif
