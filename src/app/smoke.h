#ifndef BT_APP_SMOKE_H
#define BT_APP_SMOKE_H

#include "app/logic.h"
#include "app/states.h"

#include <stddef.h>
#include <stdint.h>

#define BT_SMOKE_DWELL_SECS 0.5F
#define BT_SMOKE_STUCK_LIMIT_SECS 10.0F
#define BT_SMOKE_FATAL_REASON "smoke tour complete"
#define BT_SMOKE_SHOT_MAX BT_APP_TOUR_COUNT

typedef enum {
    BT_SMOKE_WAIT_FOR_BOOT = 0,
    BT_SMOKE_DWELL,
    BT_SMOKE_WAIT_FOR_SHOT,
    BT_SMOKE_DONE,
} bt_smoke_phase_kind;

typedef struct {
    bt_smoke_phase_kind kind;
    bt_app_state state;
    float waited;
    float stuck;
    char path[BT_PATH_MAX];
} bt_smoke_phase;

typedef struct {
    bt_app_state state;
    char path[BT_PATH_MAX];
} bt_smoke_shot;

typedef struct {
    char dir[BT_PATH_MAX];
    bt_smoke_phase phase;
    bt_app_state entered[BT_SMOKE_SHOT_MAX];
    size_t entered_count;
    bt_smoke_shot shots[BT_SMOKE_SHOT_MAX];
    size_t shot_count;
    int exit_code;
} bt_smoke_tour;

void bt_smoke_seeds(uint64_t seed, uint64_t out[3]);
void bt_smoke_tour_init(bt_smoke_tour *tour, const char *dir);
void bt_smoke_shot_path(const bt_smoke_tour *tour, bt_app_state state, char out[BT_PATH_MAX]);
void bt_smoke_shoot(const char *path);
int bt_smoke_finish(const bt_smoke_tour *tour);
int bt_smoke_step(bt_smoke_tour *tour, bt_app_ctx *ctx, int fonts_ready, float dt,
                  char shot_request[BT_PATH_MAX], int *has_shot_request);

#endif
