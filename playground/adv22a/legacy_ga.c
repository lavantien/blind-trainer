#include "ga/ga.h"

#include "common/bt_harness.h"
#include "common/bt_trace.h"
#include "common/config.h"

#include <stdlib.h>
#include <string.h>

#define BT_GA_GENE_MAX UINT32_MAX

BT_NORETURN static void bt_ga_abort(const char *trace_unit, const char *label, const char *why);

int bt_ga_build_env(size_t pool_size, float *lengths, size_t lengths_count, bt_ga_features *feats,
                    size_t feats_count, size_t case_count, bt_ga_pair_weight *cooc,
                    size_t cooc_count, bt_ga_pair_weight *adj, size_t adj_count, bt_ga_env *out,
                    char err[BT_GA_ERR_MAX]) {
    BT_TRACE_ENTER("ga.build_env");
    out->pool_size = pool_size;
    out->case_count = case_count;
    out->lengths = lengths;
    out->feats = feats;
    out->cooc = cooc;
    out->cooc_count = cooc_count;
    out->adj = adj;
    out->adj_count = adj_count;
    out->conf_table = NULL;
    out->flow_table = NULL;
    if (lengths_count != pool_size || feats_count != pool_size) {
        snprintf(err, BT_GA_ERR_MAX, "pool feature or length table size mismatch");
        BT_TRACE_FAIL("ga.build_env", "table counts %zu and %zu do not match pool %zu",
                      lengths_count, feats_count, pool_size);
        return 0;
    }
    if (pool_size < case_count) {
        snprintf(err, BT_GA_ERR_MAX, "pool %zu smaller than case count %zu", pool_size, case_count);
        BT_TRACE_FAIL("ga.build_env", "pool %zu smaller than case count %zu", pool_size,
                      case_count);
        return 0;
    }
    if (pool_size != 0 && (pool_size > SIZE_MAX / pool_size ||
                           pool_size * pool_size > SIZE_MAX / sizeof *out->conf_table)) {
        snprintf(err, BT_GA_ERR_MAX, "pool table allocation failed");
        BT_TRACE_FAIL("ga.build_env", "table bytes overflow for pool %zu", pool_size);
        return 0;
    }
    out->conf_table = malloc(pool_size * pool_size * sizeof *out->conf_table);
    out->flow_table = malloc(pool_size * pool_size * sizeof *out->flow_table);
    if (out->conf_table == NULL || out->flow_table == NULL) {
        free(out->conf_table);
        free(out->flow_table);
        out->conf_table = NULL;
        out->flow_table = NULL;
        snprintf(err, BT_GA_ERR_MAX, "pool table allocation failed");
        BT_TRACE_FAIL("ga.build_env", "table allocation failed");
        return 0;
    }
    for (size_t a = 0; a < pool_size; a++) {
        for (size_t b = 0; b < pool_size; b++) {
            out->conf_table[a * pool_size + b] = bt_ga_confusion(&feats[a], &feats[b]);
            out->flow_table[a * pool_size + b] =
                bt_ga_flow_penalty(&feats[a], (size_t)lengths[a], &feats[b], (size_t)lengths[b]);
        }
    }
    BT_TRACE_EXIT("ga.build_env");
    return 1;
}

void bt_ga_env_free(bt_ga_env *env) {
    BT_TRACE_ENTER("ga.env_free");
    free(env->conf_table);
    free(env->flow_table);
    env->conf_table = NULL;
    env->flow_table = NULL;
    BT_TRACE_EXIT("ga.env_free");
}

void bt_ga_fitness_parts(const bt_ga_env *env, const uint32_t *chrom, float parts[3]) {
    BT_TRACE_ENTER("ga.fitness_parts");
    float conf = 0.0F;
    for (size_t k = 0; k < env->cooc_count; k++) {
        conf += env->cooc[k].w * env->conf_table[(size_t)chrom[env->cooc[k].i] * env->pool_size +
                                                 (size_t)chrom[env->cooc[k].j]];
    }
    float flow = 0.0F;
    for (size_t k = 0; k < env->adj_count; k++) {
        flow += env->adj[k].w * env->flow_table[(size_t)chrom[env->adj[k].i] * env->pool_size +
                                                (size_t)chrom[env->adj[k].j]];
    }
    float cplx = 0.0F;
    for (size_t g = 0; g < env->case_count; g++) {
        cplx += env->lengths[chrom[g]];
    }
    cplx /= (float)env->case_count;
    parts[0] = conf;
    parts[1] = flow;
    parts[2] = cplx;
    BT_TRACE_EXIT("ga.fitness_parts");
}

float bt_ga_fitness(const bt_ga_params *params, const bt_ga_env *env, const uint32_t *chrom) {
    BT_TRACE_ENTER("ga.fitness");
    float parts[3];
    bt_ga_fitness_parts(env, chrom, parts);
    float value = params->w_conf * parts[0] + params->w_flow * parts[1] + params->w_cplx * parts[2];
    BT_TRACE_EXIT("ga.fitness");
    return value;
}

static int bt_ga_gene_in(const uint32_t *genes, size_t count, uint32_t gene) {
    for (size_t i = 0; i < count; i++) {
        if (genes[i] == gene) {
            return 1;
        }
    }
    return 0;
}

void bt_ga_ox(const uint32_t *p1, const uint32_t *p2, size_t n, size_t cut1, size_t cut2,
              uint32_t *out) {
    BT_TRACE_ENTER("ga.ox");
    for (size_t i = 0; i < n; i++) {
        out[i] = BT_GA_GENE_MAX;
    }
    for (size_t i = cut1; i < cut2; i++) {
        out[i] = p1[i];
    }
    uint32_t *fill = malloc(n * sizeof *fill);
    size_t fill_count = 0;
    for (size_t i = 0; i < n; i++) {
        if (!bt_ga_gene_in(p1 + cut1, cut2 - cut1, p2[i])) {
            fill[fill_count++] = p2[i];
        }
    }
    size_t free_slots = n - (cut2 - cut1);
    size_t slot = cut2 % n;
    for (size_t f = 0; f < fill_count && f < free_slots; f++) {
        while (out[slot] != BT_GA_GENE_MAX) {
            slot = (slot + 1) % n;
        }
        out[slot] = fill[f];
    }
    free(fill);
    BT_TRACE_EXIT("ga.ox");
}

void bt_ga_draw_cuts(bt_rng *rng, size_t n, size_t *cut1, size_t *cut2) {
    BT_TRACE_ENTER("ga.draw_cuts");
    *cut1 = (size_t)bt_rng_below(rng, (uint64_t)n - 1);
    *cut2 = *cut1 + 1 + (size_t)bt_rng_below(rng, (uint64_t)n - (uint64_t)*cut1 - 1);
    BT_TRACE_EXIT("ga.draw_cuts");
}

void bt_ga_mutate_swap(uint32_t *chrom, size_t n, bt_rng *rng) {
    BT_TRACE_ENTER("ga.mutate_swap");
    if (n < 2) {
        bt_ga_abort("ga.mutate_swap", "ga mutate_swap", "n below 2");
    }
    size_t i = (size_t)bt_rng_below(rng, (uint64_t)n);
    size_t j = (size_t)bt_rng_below(rng, (uint64_t)n - 1);
    if (j >= i) {
        j += 1;
    }
    uint32_t tmp = chrom[i];
    chrom[i] = chrom[j];
    chrom[j] = tmp;
    BT_TRACE_EXIT("ga.mutate_swap");
}

void bt_ga_mutate_replace(uint32_t *chrom, size_t n, bt_rng *rng, size_t pool_size) {
    BT_TRACE_ENTER("ga.mutate_replace");
    uint8_t *used = calloc(pool_size > 0 ? pool_size : 1, 1);
    for (size_t g = 0; g < n; g++) {
        used[chrom[g]] = 1;
    }
    size_t pos = (size_t)bt_rng_below(rng, (uint64_t)n);
    size_t candidate = (size_t)bt_rng_below(rng, (uint64_t)pool_size);
    while (used[candidate]) {
        candidate = (candidate + 1) % pool_size;
    }
    chrom[pos] = (uint32_t)candidate;
    free(used);
    BT_TRACE_EXIT("ga.mutate_replace");
}

size_t bt_ga_tournament(bt_rng *rng, const float *fitness, size_t count) {
    BT_TRACE_ENTER("ga.tournament");
    size_t a = (size_t)bt_rng_below(rng, (uint64_t)count);
    size_t b = (size_t)bt_rng_below(rng, (uint64_t)count);
    size_t winner = fitness[b] < fitness[a] ? b : a;
    BT_TRACE_EXIT("ga.tournament");
    return winner;
}

static void bt_ga_random_individual(bt_rng *rng, size_t pool_size, size_t case_count,
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

static int bt_ga_chrom_cmp(const uint32_t *a, const uint32_t *b, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (a[i] != b[i]) {
            return a[i] < b[i] ? -1 : 1;
        }
    }
    return 0;
}

static void bt_ga_sorted_order(const float *fitness, uint32_t *const *population, size_t count,
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
                cmp = bt_ga_chrom_cmp(population[a], population[b], case_count);
            }
            if (cmp > 0) {
                order[i] = b;
                order[i + 1] = a;
                swapped = 1;
            }
        }
    }
}

BT_NORETURN static void bt_ga_abort(const char *trace_unit, const char *label, const char *why) {
    BT_TRACE_FAIL(trace_unit, "ga abort: %s", why);
    bt_harness_fail("src/ga/ga.c", 0, "abort: %s: %s", label, why);
    bt_harness_exit_failure();
}

static void *bt_ga_run_alloc(size_t bytes) {
    void *block = malloc(bytes);
    if (block == NULL) {
        bt_ga_abort("ga.run", "ga run", "allocation failed");
    }
    return block;
}

bt_ga_run_outcome *bt_ga_run(const bt_ga_env *env, const bt_ga_params *params) {
    BT_TRACE_ENTER("ga.run");
    if (!(params->population > params->elite)) {
        bt_ga_abort("ga.run", "ga run", "population must exceed elite");
    }
    if (!(params->population >= 2)) {
        bt_ga_abort("ga.run", "ga run", "population must be at least 2");
    }
    if (!(params->elite >= 1)) {
        bt_ga_abort("ga.run", "ga run", "elite must be at least 1");
    }
    bt_rng rng;
    bt_rng_seed(&rng, params->seed);
    size_t pop = params->population;
    size_t case_count = env->case_count;
    uint32_t *scratch = bt_ga_run_alloc(env->pool_size * sizeof *scratch);
    uint32_t **population = bt_ga_run_alloc(pop * sizeof *population);
    for (size_t i = 0; i < pop; i++) {
        population[i] = bt_ga_run_alloc(case_count * sizeof **population);
        bt_ga_random_individual(&rng, env->pool_size, case_count, scratch, population[i]);
    }
    uint64_t evaluations = pop;
    float *scores = bt_ga_run_alloc(pop * sizeof *scores);
    for (size_t i = 0; i < pop; i++) {
        scores[i] = bt_ga_fitness(params, env, population[i]);
    }
    size_t *order = bt_ga_run_alloc(pop * sizeof *order);
    bt_ga_sorted_order(scores, population, pop, case_count, order);
    uint32_t *best = bt_ga_run_alloc(case_count * sizeof *best);
    memcpy(best, population[order[0]], case_count * sizeof *best);
    float best_fitness = scores[order[0]];
    size_t best_generation = 0;
    float *per_gen_best = bt_ga_run_alloc((params->generations + 1) * sizeof *per_gen_best);
    per_gen_best[0] = best_fitness;
    uint32_t stagnation_tail = 0;
    uint32_t max_stagnation = 0;

    uint32_t **next = bt_ga_run_alloc(pop * sizeof *next);
    unsigned char *is_elite = bt_ga_run_alloc(pop);
    uint64_t swap_gate = (uint64_t)(params->swap * 1000000.0);
    uint64_t replace_gate = (uint64_t)(params->replace * 1000000.0);
    for (size_t generation = 1; generation <= params->generations; generation++) {
        memset(is_elite, 0, pop);
        size_t filled = 0;
        for (size_t e = 0; e < params->elite; e++) {
            next[filled] = population[order[e]];
            is_elite[order[e]] = 1;
            filled++;
        }
        while (filled < pop) {
            size_t p1 = bt_ga_tournament(&rng, scores, pop);
            size_t p2 = bt_ga_tournament(&rng, scores, pop);
            size_t cut1 = 0;
            size_t cut2 = 0;
            bt_ga_draw_cuts(&rng, case_count, &cut1, &cut2);
            uint32_t *child = bt_ga_run_alloc(case_count * sizeof *child);
            bt_ga_ox(population[p1], population[p2], case_count, cut1, cut2, child);
            if (bt_rng_below(&rng, 1000000) < swap_gate) {
                bt_ga_mutate_swap(child, case_count, &rng);
            }
            if (bt_rng_below(&rng, 1000000) < replace_gate) {
                bt_ga_mutate_replace(child, case_count, &rng, env->pool_size);
            }
            next[filled] = child;
            filled++;
        }
        evaluations += pop - params->elite;
        for (size_t i = 0; i < pop; i++) {
            scores[i] = bt_ga_fitness(params, env, next[i]);
        }
        for (size_t i = 0; i < pop; i++) {
            if (!is_elite[i]) {
                free(population[i]);
            }
            population[i] = next[i];
        }
        bt_ga_sorted_order(scores, population, pop, case_count, order);
        float gen_best = scores[order[0]];
        per_gen_best[generation] = gen_best;
        if (gen_best < best_fitness) {
            best_fitness = gen_best;
            memcpy(best, population[order[0]], case_count * sizeof *best);
            best_generation = generation;
            stagnation_tail = 0;
        } else {
            stagnation_tail += 1;
            if (stagnation_tail > max_stagnation) {
                max_stagnation = stagnation_tail;
            }
        }
    }
    free(scratch);
    free(is_elite);

    for (size_t i = 0; i < pop; i++) {
        free(population[i]);
    }
    free(population);
    free(next);
    free(scores);
    free(order);

    bt_ga_run_outcome *outcome = bt_ga_run_alloc(sizeof *outcome);
    float parts[3];
    bt_ga_fitness_parts(env, best, parts);
    outcome->best = best;
    outcome->best_count = case_count;
    outcome->best_fitness = best_fitness;
    outcome->best_parts[0] = parts[0];
    outcome->best_parts[1] = parts[1];
    outcome->best_parts[2] = parts[2];
    outcome->per_gen_best = per_gen_best;
    outcome->per_gen_count = params->generations + 1;
    outcome->best_generation = best_generation;
    outcome->stagnation_tail = stagnation_tail;
    outcome->max_stagnation = max_stagnation;
    outcome->evaluations = evaluations;
    BT_TRACE_EXIT("ga.run");
    return outcome;
}

void bt_ga_run_outcome_free(bt_ga_run_outcome *outcome) {
    BT_TRACE_ENTER("ga.run_outcome_free");
    free(outcome->best);
    free(outcome->per_gen_best);
    free(outcome);
    BT_TRACE_EXIT("ga.run_outcome_free");
}

static int bt_ga_config_number(const bt_json_value *config, const char *dotted, const char *slash,
                               double *out, char err[BT_GA_ERR_MAX]) {
    const bt_json_value *value = bt_config_value(config, dotted);
    if (value == NULL || value->type != BT_JSON_NUMBER) {
        snprintf(err, BT_GA_ERR_MAX, "config section missing: %s", slash);
        return 0;
    }
    *out = value->as.number;
    return 1;
}

int bt_ga_config_params(const bt_json_value *config, bt_ga_params *out, char err[BT_GA_ERR_MAX]) {
    BT_TRACE_ENTER("ga.config_params");
    double number = 0.0;
    if (!bt_ga_config_number(config, "ga.tournament", "/ga/tournament", &number, err)) {
        BT_TRACE_FAIL("ga.config_params", "tournament missing");
        return 0;
    }
    uint64_t tournament = (uint64_t)number;
    if (tournament != 2) {
        snprintf(err, BT_GA_ERR_MAX,
                 "ga.tournament %llu unsupported, engine implements tournament-of-2 only",
                 (unsigned long long)tournament);
        BT_TRACE_FAIL("ga.config_params", "tournament %llu unsupported",
                      (unsigned long long)tournament);
        return 0;
    }
    if (!bt_ga_config_number(config, "ga.seed", "/ga/seed", &number, err)) {
        BT_TRACE_FAIL("ga.config_params", "seed missing");
        return 0;
    }
    out->seed = (uint64_t)number;
    if (!bt_ga_config_number(config, "ga.population", "/ga/population", &number, err)) {
        BT_TRACE_FAIL("ga.config_params", "population missing");
        return 0;
    }
    out->population = (size_t)number;
    if (!bt_ga_config_number(config, "ga.generations", "/ga/generations", &number, err)) {
        BT_TRACE_FAIL("ga.config_params", "generations missing");
        return 0;
    }
    out->generations = (size_t)number;
    if (!bt_ga_config_number(config, "ga.elite", "/ga/elite", &number, err)) {
        BT_TRACE_FAIL("ga.config_params", "elite missing");
        return 0;
    }
    out->elite = (size_t)number;
    if (!bt_ga_config_number(config, "ga.mutation.swap", "/ga/mutation/swap", &number, err)) {
        BT_TRACE_FAIL("ga.config_params", "swap missing");
        return 0;
    }
    out->swap = number;
    if (!bt_ga_config_number(config, "ga.mutation.replace", "/ga/mutation/replace", &number, err)) {
        BT_TRACE_FAIL("ga.config_params", "replace missing");
        return 0;
    }
    out->replace = number;
    if (!bt_ga_config_number(config, "ga.weights.confusion", "/ga/weights/confusion", &number,
                             err)) {
        BT_TRACE_FAIL("ga.config_params", "w_conf missing");
        return 0;
    }
    out->w_conf = (float)number;
    if (!bt_ga_config_number(config, "ga.weights.flow", "/ga/weights/flow", &number, err)) {
        BT_TRACE_FAIL("ga.config_params", "w_flow missing");
        return 0;
    }
    out->w_flow = (float)number;
    if (!bt_ga_config_number(config, "ga.weights.complexity", "/ga/weights/complexity", &number,
                             err)) {
        BT_TRACE_FAIL("ga.config_params", "w_cplx missing");
        return 0;
    }
    out->w_cplx = (float)number;
    BT_TRACE_EXIT("ga.config_params");
    return 1;
}
