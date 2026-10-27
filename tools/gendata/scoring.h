#ifndef GD_SCORING_H
#define GD_SCORING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/json.h"
#include "core/moves.h"
#include "scheme/scheme.h"

typedef struct {
    bool preferred[9];
    bool preferred_edge[9];
    float penalty_other;
    float weight_len;
    float weight_subset;
    float weight_regrip;
    float weight_finger;
    float finger_cost[6][2];
} bt_gd_scoring_cfg;

typedef struct {
    uint32_t qtm;
    float subset;
    uint32_t regrips;
    float finger;
    float total;
} bt_gd_score_parts;

bool bt_gd_count_regrips(const bt_move *moves, size_t count, uint32_t *out);
bool bt_gd_score(const bt_move *moves, size_t count, bt_orbit orbit, const bt_gd_scoring_cfg *cfg,
                 bt_gd_score_parts *out);
int bt_gd_rank_cmp(const bt_move *left, size_t left_len, const bt_move *right, size_t right_len,
                   bt_orbit orbit, const bt_gd_scoring_cfg *cfg);
int bt_gd_scoring_from_config(const bt_json_value *config, bt_gd_scoring_cfg *out, char *err,
                              size_t err_len);

#endif
