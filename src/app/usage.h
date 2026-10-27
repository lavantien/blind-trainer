#ifndef BT_APP_USAGE_H
#define BT_APP_USAGE_H

#include "app/logic.h"
#include "app/usage_script.h"
#include "scheme/pairs.h"

#include <stddef.h>
#include <stdint.h>

#define BT_USAGE_DWELL_SECS 0.4F
#define BT_USAGE_STUCK_LIMIT_SECS 30.0F
#define BT_USAGE_SOLVE_SEED_ATTEMPTS 400
#define BT_USAGE_SHOTS_MAX BT_USAGE_SHOT_MAX

typedef enum {
    BT_USAGE_WAIT_APP = 0,
    BT_USAGE_WAIT_FONTS,
    BT_USAGE_WAIT_DRILL,
    BT_USAGE_WAIT_MEMO,
    BT_USAGE_WAIT_SOLVE,
    BT_USAGE_WAIT_CASE_DRILL,
} bt_usage_wait_kind;

typedef enum {
    BT_USAGE_MODE_ACT = 0,
    BT_USAGE_MODE_WAIT_UNTIL,
    BT_USAGE_MODE_ELAPSE,
    BT_USAGE_MODE_SHOT,
    BT_USAGE_MODE_DONE,
} bt_usage_mode_kind;

typedef struct {
    bt_usage_wait_kind kind;
    bt_app_state app;
    bt_drill_phase drill;
    bt_memo_phase memo;
    bt_solve_phase solve;
    bt_case_drill_phase case_drill;
    float waited;
} bt_usage_wait;

typedef struct {
    char path[BT_PATH_MAX];
    float clock;
    int taken;
} bt_usage_shot_state;

typedef struct {
    int key;
    char ch;
    int has_key;
} bt_usage_key_request;

typedef struct {
    char dir[BT_PATH_MAX];
    bt_usage_action *actions;
    size_t action_count;
    size_t action_cap;
    size_t index;
    bt_usage_mode_kind mode;
    bt_usage_wait wait;
    float elapse_left;
    bt_usage_shot_state shot;
    char shots[BT_USAGE_SHOTS_MAX][BT_PATH_MAX];
    size_t shot_count;
    size_t expected_shots;
    int exit_code;
} bt_usage_tour;

int bt_usage_tour_init(bt_usage_tour *tour, const char *dir, uint64_t seed,
                       unsigned int cadence_ms);
int bt_usage_fatal_init(bt_usage_tour *tour, const char *dir);
void bt_usage_tour_set_actions(bt_usage_tour *tour, const bt_usage_action *actions,
                               size_t count);
void bt_usage_tour_free(bt_usage_tour *tour);
bt_case bt_usage_pinned_case(void);
const char *bt_usage_covered_alg_text(const bt_corpus *corpus, const bt_case *case_id);
int bt_usage_fully_covered(const bt_corpus *corpus, const bt_cube *scramble);
int bt_usage_covered_seed(const bt_corpus *corpus, uint64_t base, uint64_t attempts,
                          uint64_t *out);
int bt_usage_finish(const bt_usage_tour *tour);
int bt_usage_step(bt_usage_tour *tour, bt_app_ctx *ctx, int fonts_ready, float dt,
                  bt_usage_key_request *key_request, char shot_request[BT_PATH_MAX],
                  int *has_shot_request);

#endif
