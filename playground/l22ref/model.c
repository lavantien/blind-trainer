#include "common/rng.h"
#include "ga/features.h"
#include "ga/ga.h"
#include "ga/inventory.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
} ref_params;

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
} ref_upgrades;

typedef struct {
    uint32_t **population;
    float *scores;
    size_t *order;
    uint32_t stagnation;
    float island_best;
} ref_island;

typedef struct {
    uint32_t *best;
    size_t best_count;
    float best_fitness;
    size_t best_generation;
    float *per_gen_best;
    size_t per_gen_count;
    uint32_t stagnation_tail;
    uint32_t max_stagnation;
    uint64_t evaluations;
} ref_outcome;

static int ref_chrom_cmp(const uint32_t *a, const uint32_t *b, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (a[i] != b[i]) {
            return a[i] < b[i] ? -1 : 1;
        }
    }
    return 0;
}

static void ref_sorted_order(const float *fitness, uint32_t **population, size_t count,
                             size_t case_count, size_t *order) {
    for (size_t i = 0; i < count; i++) {
        order[i] = i;
    }
    int swapped = 1;
    while (swapped) {
        swapped = 0;
        for (size_t i = 0; i + 1 < count; i++) {
            size_t a = order[i];
            size_t b = order[i + 1];
            int cmp;
            if (fitness[a] < fitness[b]) {
                cmp = -1;
            } else if (fitness[b] < fitness[a]) {
                cmp = 1;
            } else {
                cmp = ref_chrom_cmp(population[a], population[b], case_count);
            }
            if (cmp > 0) {
                order[i] = b;
                order[i + 1] = a;
                swapped = 1;
            }
        }
    }
}

static void ref_random_individual(bt_rng *rng, size_t pool_size, size_t case_count,
                                  uint32_t *scratch, uint32_t *out) {
    for (size_t i = 0; i < pool_size; i++) {
        scratch[i] = (uint32_t)i;
    }
    for (size_t i = pool_size; i > 1; i--) {
        size_t j = (size_t)bt_rng_below(rng, (uint64_t)i);
        uint32_t tmp = scratch[i - 1];
        scratch[i - 1] = scratch[j];
        scratch[j] = tmp;
    }
    for (size_t g = 0; g < case_count; g++) {
        out[g] = scratch[g];
    }
}

static size_t ref_distinct_count(uint32_t **population, size_t pop, size_t case_count,
                                 size_t *scratch_order) {
    for (size_t i = 0; i < pop; i++) {
        scratch_order[i] = i;
    }
    int swapped = 1;
    while (swapped) {
        swapped = 0;
        for (size_t i = 0; i + 1 < pop; i++) {
            size_t a = scratch_order[i];
            size_t b = scratch_order[i + 1];
            if (ref_chrom_cmp(population[a], population[b], case_count) > 0) {
                scratch_order[i] = b;
                scratch_order[i + 1] = a;
                swapped = 1;
            }
        }
    }
    size_t distinct = pop > 0 ? 1 : 0;
    for (size_t i = 1; i < pop; i++) {
        if (ref_chrom_cmp(population[scratch_order[i - 1]], population[scratch_order[i]],
                          case_count) != 0) {
            distinct++;
        }
    }
    return distinct;
}

static ref_outcome ref_run(const bt_ga_env *env, const ref_params *params,
                           const ref_upgrades *up) {
    ref_outcome outcome;
    memset(&outcome, 0, sizeof outcome);
    size_t islands = up->island_count < 1 ? 1 : up->island_count;
    size_t pop = params->population;
    size_t elite = params->elite;
    if (up->elite_fraction_policy) {
        elite = (size_t)(up->elite_fraction * (double)pop);
    }
    size_t case_count = env->case_count;
    bt_rng rng;
    bt_rng_seed(&rng, params->seed);
    ref_island *island = calloc(islands, sizeof *island);
    uint32_t *scratch = malloc(env->pool_size * sizeof *scratch);
    for (size_t isl = 0; isl < islands; isl++) {
        island[isl].population = malloc(pop * sizeof *island[isl].population);
        for (size_t i = 0; i < pop; i++) {
            island[isl].population[i] = malloc(case_count * sizeof **island[isl].population);
            ref_random_individual(&rng, env->pool_size, case_count, scratch,
                                  island[isl].population[i]);
        }
        island[isl].scores = malloc(pop * sizeof *island[isl].scores);
        island[isl].order = malloc(pop * sizeof *island[isl].order);
    }
    uint64_t evaluations = islands * pop;
    for (size_t isl = 0; isl < islands; isl++) {
        for (size_t i = 0; i < pop; i++) {
            island[isl].scores[i] = bt_ga_fitness((const bt_ga_params *)params, env,
                                                  island[isl].population[i]);
        }
        ref_sorted_order(island[isl].scores, island[isl].population, pop, case_count,
                         island[isl].order);
        island[isl].island_best = island[isl].scores[island[isl].order[0]];
    }
    outcome.best = malloc(case_count * sizeof *outcome.best);
    outcome.best_count = case_count;
    outcome.best_fitness = island[0].scores[island[0].order[0]];
    memcpy(outcome.best, island[0].population[island[0].order[0]], case_count * sizeof *outcome.best);
    for (size_t isl = 1; isl < islands; isl++) {
        float candidate = island[isl].scores[island[isl].order[0]];
        if (candidate < outcome.best_fitness) {
            outcome.best_fitness = candidate;
            memcpy(outcome.best, island[isl].population[island[isl].order[0]],
                   case_count * sizeof *outcome.best);
        }
    }
    outcome.best_generation = 0;
    outcome.per_gen_best = malloc((params->generations + 1) * sizeof *outcome.per_gen_best);
    outcome.per_gen_count = params->generations + 1;
    outcome.per_gen_best[0] = outcome.best_fitness;
    outcome.stagnation_tail = 0;
    outcome.max_stagnation = 0;
    uint32_t **next = malloc(pop * sizeof *next);
    unsigned char *is_elite = malloc(pop);
    uint32_t **staging = NULL;
    float *staging_scores = NULL;
    if (islands >= 2 && up->migrate_every > 0) {
        staging = malloc(islands * up->migrate_count * sizeof *staging);
        staging_scores = malloc(islands * up->migrate_count * sizeof *staging_scores);
        for (size_t i = 0; i < islands * up->migrate_count; i++) {
            staging[i] = malloc(case_count * sizeof **staging);
        }
    }
    size_t *diversity_order = NULL;
    if (up->diversity_threshold > 0.0) {
        diversity_order = malloc(pop * sizeof *diversity_order);
    }
    for (size_t generation = 1; generation <= params->generations; generation++) {
        for (size_t isl = 0; isl < islands; isl++) {
            uint64_t swap_gate = (uint64_t)(params->swap * 1000000.0);
            uint64_t replace_gate = (uint64_t)(params->replace * 1000000.0);
            if (up->stagnation_threshold > 0 && island[isl].stagnation >= up->stagnation_threshold) {
                swap_gate = (uint64_t)(params->swap * up->swap_scale * 1000000.0);
                replace_gate = (uint64_t)(params->replace * up->replace_scale * 1000000.0);
            }
            memset(is_elite, 0, pop);
            size_t filled = 0;
            for (size_t e = 0; e < elite; e++) {
                next[filled] = island[isl].population[island[isl].order[e]];
                is_elite[island[isl].order[e]] = 1;
                filled++;
            }
            while (filled < pop) {
                size_t p1 = bt_ga_tournament(&rng, island[isl].scores, pop);
                size_t p2 = bt_ga_tournament(&rng, island[isl].scores, pop);
                size_t cut1 = 0;
                size_t cut2 = 0;
                bt_ga_draw_cuts(&rng, case_count, &cut1, &cut2);
                uint32_t *child = malloc(case_count * sizeof *child);
                bt_ga_ox(island[isl].population[p1], island[isl].population[p2], case_count, cut1,
                         cut2, child);
                if (bt_rng_below(&rng, 1000000) < swap_gate) {
                    bt_ga_mutate_swap(child, case_count, &rng);
                }
                if (bt_rng_below(&rng, 1000000) < replace_gate) {
                    bt_ga_mutate_replace(child, case_count, &rng, env->pool_size);
                }
                next[filled] = child;
                filled++;
            }
            evaluations += pop - elite;
            for (size_t i = 0; i < pop; i++) {
                island[isl].scores[i] = bt_ga_fitness((const bt_ga_params *)params, env, next[i]);
            }
            for (size_t i = 0; i < pop; i++) {
                if (!is_elite[i]) {
                    free(island[isl].population[i]);
                }
                island[isl].population[i] = next[i];
            }
            ref_sorted_order(island[isl].scores, island[isl].population, pop, case_count,
                             island[isl].order);
        }
        if (islands >= 2 && up->migrate_every > 0 && generation % up->migrate_every == 0) {
            for (size_t isl = 0; isl < islands; isl++) {
                for (size_t k = 0; k < up->migrate_count; k++) {
                    memcpy(staging[isl * up->migrate_count + k],
                           island[isl].population[island[isl].order[k]],
                           case_count * sizeof **staging);
                    staging_scores[isl * up->migrate_count + k] =
                        island[isl].scores[island[isl].order[k]];
                }
            }
            for (size_t isl = 0; isl < islands; isl++) {
                size_t dst = (isl + 1) % islands;
                for (size_t k = 0; k < up->migrate_count; k++) {
                    size_t victim = island[dst].order[pop - 1 - k];
                    memcpy(island[dst].population[victim],
                           staging[isl * up->migrate_count + k], case_count * sizeof **staging);
                    island[dst].scores[victim] = staging_scores[isl * up->migrate_count + k];
                }
                ref_sorted_order(island[dst].scores, island[dst].population, pop, case_count,
                                 island[dst].order);
            }
        }
        float gen_best = island[0].scores[island[0].order[0]];
        for (size_t isl = 1; isl < islands; isl++) {
            float candidate = island[isl].scores[island[isl].order[0]];
            if (candidate < gen_best) {
                gen_best = candidate;
            }
        }
        outcome.per_gen_best[generation] = gen_best;
        if (gen_best < outcome.best_fitness) {
            size_t owner = 0;
            for (size_t isl = 0; isl < islands; isl++) {
                if (island[isl].scores[island[isl].order[0]] == gen_best) {
                    owner = isl;
                    break;
                }
            }
            outcome.best_fitness = gen_best;
            memcpy(outcome.best, island[owner].population[island[owner].order[0]],
                   case_count * sizeof *outcome.best);
            outcome.best_generation = generation;
            outcome.stagnation_tail = 0;
        } else {
            outcome.stagnation_tail += 1;
            if (outcome.stagnation_tail > outcome.max_stagnation) {
                outcome.max_stagnation = outcome.stagnation_tail;
            }
        }
        for (size_t isl = 0; isl < islands; isl++) {
            float island_gen_best = island[isl].scores[island[isl].order[0]];
            if (island_gen_best < island[isl].island_best) {
                island[isl].island_best = island_gen_best;
                island[isl].stagnation = 0;
            } else {
                island[isl].stagnation += 1;
            }
        }
        if (up->diversity_threshold > 0.0) {
            for (size_t isl = 0; isl < islands; isl++) {
                size_t distinct =
                    ref_distinct_count(island[isl].population, pop, case_count, diversity_order);
                if ((double)distinct / (double)pop < up->diversity_threshold) {
                    size_t kept = (size_t)(up->inject_fraction * (double)pop);
                    for (size_t r = kept; r < pop; r++) {
                        size_t idx = island[isl].order[r];
                        ref_random_individual(&rng, env->pool_size, case_count, scratch,
                                              island[isl].population[idx]);
                        island[isl].scores[idx] =
                            bt_ga_fitness((const bt_ga_params *)params, env,
                                          island[isl].population[idx]);
                    }
                    evaluations += pop - kept;
                    ref_sorted_order(island[isl].scores, island[isl].population, pop, case_count,
                                     island[isl].order);
                    island[isl].stagnation = 0;
                }
            }
        }
    }
    outcome.evaluations = evaluations;
    for (size_t isl = 0; isl < islands; isl++) {
        for (size_t i = 0; i < pop; i++) {
            free(island[isl].population[i]);
        }
        free(island[isl].population);
        free(island[isl].scores);
        free(island[isl].order);
    }
    free(island);
    free(scratch);
    free(next);
    free(is_elite);
    for (size_t i = 0; staging != NULL && i < islands * up->migrate_count; i++) {
        free(staging[i]);
    }
    free(staging);
    free(staging_scores);
    free(diversity_order);
    return outcome;
}

static uint32_t bt_f32_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}

static size_t codepoint_count(const char *text) {
    size_t count = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++) {
        if ((*p & 0xC0U) != 0x80U) {
            count++;
        }
    }
    return count;
}

static bt_ga_features parse_or_die(const char *text) {
    bt_ga_features out;
    if (!bt_ga_parse(text, &out)) {
        fprintf(stderr, "parse failed for %s\n", text);
        exit(1);
    }
    return out;
}

static const char *const POOL8[] = {"ả", "ã", "không", "lông", "tía", "mút", "án", "đặt"};

static bt_ga_env *fixture_env(size_t case_count) {
    bt_ga_features *feats = calloc(8, sizeof *feats);
    float *lengths = calloc(8, sizeof *lengths);
    for (size_t i = 0; i < 8; i++) {
        feats[i] = parse_or_die(POOL8[i]);
        lengths[i] = (float)codepoint_count(POOL8[i]);
    }
    bt_ga_pair_weight *cooc = calloc(2, sizeof *cooc);
    cooc[0].i = 0;
    cooc[0].j = 1;
    cooc[0].w = 0.5F;
    cooc[1].i = 1;
    cooc[1].j = 2;
    cooc[1].w = 0.25F;
    bt_ga_pair_weight *adj = calloc(1, sizeof *adj);
    adj[0].i = 0;
    adj[0].j = 1;
    adj[0].w = 0.5F;
    bt_ga_env *env = malloc(sizeof *env);
    char err[BT_GA_ERR_MAX];
    if (!bt_ga_build_env(8, lengths, 8, feats, 8, case_count, cooc, 2, adj, 1, env, err)) {
        fprintf(stderr, "build_env: %s\n", err);
        exit(1);
    }
    return env;
}

static ref_params base_params(uint64_t seed, size_t pop, size_t gens, size_t elite) {
    ref_params params;
    params.seed = seed;
    params.population = pop;
    params.generations = gens;
    params.elite = elite;
    params.swap = 0.15;
    params.replace = 0.05;
    params.w_conf = 1.0F;
    params.w_flow = 0.5F;
    params.w_cplx = 0.1F;
    return params;
}

static ref_upgrades zero_upgrades(void) {
    ref_upgrades up;
    memset(&up, 0, sizeof up);
    up.swap_scale = 0.0;
    up.replace_scale = 0.0;
    return up;
}

static void print_outcome(const char *label, const ref_outcome *outcome) {
    printf("%s genes", label);
    for (size_t i = 0; i < outcome->best_count; i++) {
        printf(" %u", outcome->best[i]);
    }
    printf("\n%s best_fitness 0x%08llx best_generation %zu stagnation_tail %u max_stagnation %u "
           "evaluations %llu\n",
           label, (unsigned long long)bt_f32_bits(outcome->best_fitness), outcome->best_generation,
           outcome->stagnation_tail, outcome->max_stagnation,
           (unsigned long long)outcome->evaluations);
    printf("%s per_gen", label);
    for (size_t i = 0; i < outcome->per_gen_count; i++) {
        printf(" 0x%08llx", (unsigned long long)bt_f32_bits(outcome->per_gen_best[i]));
    }
    printf("\n");
}

static int outcomes_equal(const ref_outcome *a, const bt_ga_run_outcome *b) {
    if (a->best_count != b->best_count || a->per_gen_count != b->per_gen_count ||
        a->best_generation != b->best_generation || a->stagnation_tail != b->stagnation_tail ||
        a->max_stagnation != b->max_stagnation || a->evaluations != b->evaluations) {
        return 0;
    }
    if (bt_f32_bits(a->best_fitness) != bt_f32_bits(b->best_fitness)) {
        return 0;
    }
    for (size_t i = 0; i < a->best_count; i++) {
        if (a->best[i] != b->best[i]) {
            return 0;
        }
    }
    for (size_t i = 0; i < a->per_gen_count; i++) {
        if (bt_f32_bits(a->per_gen_best[i]) != bt_f32_bits(b->per_gen_best[i])) {
            return 0;
        }
    }
    return 1;
}

static int ref_outcomes_equal(const ref_outcome *a, const ref_outcome *b) {
    if (a->best_count != b->best_count || a->per_gen_count != b->per_gen_count ||
        a->best_generation != b->best_generation || a->stagnation_tail != b->stagnation_tail ||
        a->max_stagnation != b->max_stagnation || a->evaluations != b->evaluations) {
        return 0;
    }
    if (bt_f32_bits(a->best_fitness) != bt_f32_bits(b->best_fitness)) {
        return 0;
    }
    for (size_t i = 0; i < a->best_count; i++) {
        if (a->best[i] != b->best[i]) {
            return 0;
        }
    }
    for (size_t i = 0; i < a->per_gen_count; i++) {
        if (bt_f32_bits(a->per_gen_best[i]) != bt_f32_bits(b->per_gen_best[i])) {
            return 0;
        }
    }
    return 1;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    bt_ga_env *env = fixture_env(6);
    bt_ga_env *env3 = fixture_env(3);
    ref_params legacy = base_params(424242, 8, 6, 2);
    ref_upgrades zero = zero_upgrades();
    ref_outcome s0 = ref_run(env, &legacy, &zero);
    print_outcome("s0", &s0);
    bt_ga_params real;
    real.seed = legacy.seed;
    real.population = legacy.population;
    real.generations = legacy.generations;
    real.elite = legacy.elite;
    real.swap = legacy.swap;
    real.replace = legacy.replace;
    real.w_conf = legacy.w_conf;
    real.w_flow = legacy.w_flow;
    real.w_cplx = legacy.w_cplx;
    bt_ga_run_outcome *engine = bt_ga_run(env, &real);
    printf("s0 model_vs_engine %s\n", outcomes_equal(&s0, engine) ? "MATCH" : "MISMATCH");
    bt_ga_run_outcome_free(engine);

    ref_upgrades adaptive = zero_upgrades();
    adaptive.stagnation_threshold = 1;
    adaptive.swap_scale = 0.5;
    adaptive.replace_scale = 0.5;
    ref_outcome s1 = ref_run(env, &legacy, &adaptive);
    print_outcome("s1", &s1);

    ref_upgrades islands3 = zero_upgrades();
    islands3.island_count = 3;
    islands3.migrate_every = 2;
    islands3.migrate_count = 1;
    ref_outcome s2 = ref_run(env, &legacy, &islands3);
    print_outcome("s2", &s2);

    ref_params collapse = base_params(2, 4, 6, 3);
    collapse.swap = 0.0;
    collapse.replace = 0.0;
    ref_upgrades restart = zero_upgrades();
    restart.diversity_threshold = 0.9;
    restart.inject_fraction = 0.5;
    ref_outcome s3 = ref_run(env, &collapse, &restart);
    print_outcome("s3", &s3);

    ref_params flat = base_params(14, 2, 6, 1);
    flat.swap = 0.0;
    flat.replace = 0.0;
    flat.w_conf = 0.0F;
    flat.w_flow = 0.0F;
    flat.w_cplx = 0.0F;
    ref_upgrades flat_restart = zero_upgrades();
    flat_restart.diversity_threshold = 0.75;
    flat_restart.inject_fraction = 0.5;
    ref_outcome s3b = ref_run(env3, &flat, &flat_restart);
    print_outcome("s3b", &s3b);

    ref_upgrades fraction = zero_upgrades();
    fraction.elite_fraction_policy = 1;
    fraction.elite_fraction = 0.25;
    ref_outcome s4 = ref_run(env, &legacy, &fraction);
    print_outcome("s4", &s4);
    printf("s4 equals s0 %s\n", ref_outcomes_equal(&s0, &s4) ? "MATCH" : "MISMATCH");
    return 0;
}
