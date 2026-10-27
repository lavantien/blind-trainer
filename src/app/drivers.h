#ifndef BT_APP_DRIVERS_H
#define BT_APP_DRIVERS_H

#include "app/logic.h"

#include <stdint.h>

unsigned int bt_app_driver_vk_of(int raylib_key);
int bt_app_smoke_main(bt_app_ctx *ctx, const char *dir, uint64_t seed);
int bt_app_usage_main(bt_app_ctx *ctx, const char *dir);
int bt_app_fatal_main(bt_app_ctx *ctx, const char *dir);

#endif
