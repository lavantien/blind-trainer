#ifndef BT_GA_GA_H
#define BT_GA_GA_H

#include <stddef.h>
#include <stdint.h>

#include "common/json.h"
#include "common/rng.h"
#include "ga/features.h"

typedef struct {
    uint64_t seed;
    size_t population;
    size_t generations;
    size_t elite;
    double swap;
    double replace;
    float w_conf;
    float w_flow;
    float w_cplx;
} bt_ga_params;

typedef struct {
    uint32_t i;
    uint32_t j;
    float w;
} bt_ga_pair_weight;

typedef struct {
    size_t pool_size;
    size_t case_count;
    float *lengths;
    bt_ga_features *feats;
    float *conf_table;
    float *flow_table;
    bt_ga_pair_weight *cooc;
    size_t cooc_count;
    bt_ga_pair_weight *adj;
    size_t adj_count;
} bt_ga_env;

int bt_ga_build_env(size_t pool_size, float *lengths, size_t lengths_count, bt_ga_features *feats,
                    size_t feats_count, size_t case_count, bt_ga_pair_weight *cooc,
                    size_t cooc_count, bt_ga_pair_weight *adj, size_t adj_count, bt_ga_env *out,
                    char err[BT_GA_ERR_MAX]);
void bt_ga_env_free(bt_ga_env *env);

void bt_ga_fitness_parts(const bt_ga_env *env, const uint32_t *chrom, float parts[3]);
float bt_ga_fitness(const bt_ga_params *params, const bt_ga_env *env, const uint32_t *chrom);

void bt_ga_ox(const uint32_t *p1, const uint32_t *p2, size_t n, size_t cut1, size_t cut2,
              uint32_t *out);
void bt_ga_draw_cuts(bt_rng *rng, size_t n, size_t *cut1, size_t *cut2);
void bt_ga_mutate_swap(uint32_t *chrom, size_t n, bt_rng *rng);
void bt_ga_mutate_replace(uint32_t *chrom, size_t n, bt_rng *rng, size_t pool_size);
size_t bt_ga_tournament(bt_rng *rng, const float *fitness, size_t count);

typedef struct {
    uint32_t *best;
    size_t best_count;
    float best_fitness;
    float best_parts[3];
    float *per_gen_best;
    size_t per_gen_count;
    size_t best_generation;
    uint32_t stagnation_tail;
    uint32_t max_stagnation;
    uint64_t evaluations;
} bt_ga_run_outcome;

typedef struct {
    size_t island_count;
    size_t migrate_every;
    size_t migrate_count;
    uint32_t stagnation_threshold;
    double swap_scale;
    double replace_scale;
    int elite_fraction_policy;
    double elite_fraction;
    double diversity_threshold;
    double inject_fraction;
} bt_ga_upgrades;

typedef struct {
    uint32_t **population;
    float *scores;
    size_t *order;
} bt_ga_island;

bt_ga_run_outcome *bt_ga_run(const bt_ga_env *env, const bt_ga_params *params);
bt_ga_run_outcome *bt_ga_run_ex(const bt_ga_env *env, const bt_ga_params *params,
                                const bt_ga_upgrades *upgrades);
void bt_ga_run_outcome_free(bt_ga_run_outcome *outcome);

void bt_ga_ring_migrate(bt_ga_island *islands, size_t island_count, size_t pop,
                        size_t migrate_count, size_t case_count, uint32_t **staging,
                        float *staging_scores);

int bt_ga_config_params(const bt_json_value *config, bt_ga_params *out, char err[BT_GA_ERR_MAX]);
int bt_ga_config_upgrades(const bt_json_value *config, bt_ga_upgrades *out,
                          char err[BT_GA_ERR_MAX]);
int bt_ga_upgrades_check(const bt_ga_upgrades *upgrades, const bt_ga_params *params,
                         char err[BT_GA_ERR_MAX]);

#endif
