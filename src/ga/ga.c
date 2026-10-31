#include "ga/ga.h"

#include "common/bt_harness.h"
#include "common/bt_trace.h"
#include "common/config.h"
#include "ga/alloc.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define BT_GA_GENE_MAX UINT32_MAX
#define BT_GA_GATE_PPM 1000000.0

BT_NORETURN static void bt_ga_abort(const char *trace_unit, const char *label, const char *why,
                                    ...);

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
    out->conf_table =
        bt_ga_alloc(pool_size * pool_size * sizeof *out->conf_table, "ga.build_env.conf_table");
    out->flow_table =
        bt_ga_alloc(pool_size * pool_size * sizeof *out->flow_table, "ga.build_env.flow_table");
    if (out->conf_table == NULL || out->flow_table == NULL) {
        int failed_conf = out->conf_table == NULL;
        bt_ga_free(out->conf_table);
        bt_ga_free(out->flow_table);
        out->conf_table = NULL;
        out->flow_table = NULL;
        snprintf(err, BT_GA_ERR_MAX, "%s table allocation failed", failed_conf ? "conf" : "flow");
        BT_TRACE_FAIL("ga.build_env", "%s table allocation failed", failed_conf ? "conf" : "flow");
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
    bt_ga_free(env->conf_table);
    bt_ga_free(env->flow_table);
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
    uint32_t *fill = bt_ga_alloc(n * sizeof *fill, "ga.ox.fill");
    if (fill == NULL) {
        bt_ga_abort("ga.ox", "ga ox", "allocation failed at ga.ox.fill");
    }
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
    bt_ga_free(fill);
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
    size_t used_bytes = pool_size > 0 ? pool_size : 1;
    uint8_t *used = bt_ga_alloc(used_bytes, "ga.mutate_replace.used");
    if (used == NULL) {
        bt_ga_abort("ga.mutate_replace", "ga mutate_replace",
                    "allocation failed at ga.mutate_replace.used");
    }
    memset(used, 0, used_bytes);
    for (size_t g = 0; g < n; g++) {
        used[chrom[g]] = 1;
    }
    size_t pos = (size_t)bt_rng_below(rng, (uint64_t)n);
    size_t candidate = (size_t)bt_rng_below(rng, (uint64_t)pool_size);
    while (used[candidate]) {
        candidate = (candidate + 1) % pool_size;
    }
    chrom[pos] = (uint32_t)candidate;
    bt_ga_free(used);
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

BT_NORETURN static void bt_ga_abort(const char *trace_unit, const char *label, const char *why,
                                    ...) {
    char text[128];
    va_list args;
    va_start(args, why);
    vsnprintf(text, sizeof text, why, args);
    va_end(args);
    BT_TRACE_FAIL(trace_unit, "ga abort: %s", text);
    bt_harness_fail("src/ga/ga.c", 0, "abort: %s: %s", label, text);
    bt_harness_exit_failure();
}

static void *bt_ga_run_alloc(size_t bytes, const char *site) {
    void *block = bt_ga_alloc(bytes, site);
    if (block == NULL) {
        bt_ga_abort("ga.run", "ga run", "allocation failed at %s", site);
    }
    return block;
}

static void bt_ga_eval_population(const bt_ga_params *params, const bt_ga_env *env,
                                  uint32_t **population, size_t count, float *scores) {
    for (size_t i = 0; i < count; i++) {
        scores[i] = bt_ga_fitness(params, env, population[i]);
    }
}

static size_t bt_ga_effective_elite(const bt_ga_params *params, const bt_ga_upgrades *upgrades) {
    if (upgrades->elite_fraction_policy) {
        return (size_t)(upgrades->elite_fraction * (double)params->population);
    }
    return params->elite;
}

typedef struct {
    uint32_t **population;
    size_t case_count;
} bt_ga_distinct_ctx;

static int bt_ga_distinct_cmp(void *context, const void *left, const void *right) {
    const bt_ga_distinct_ctx *ctx = context;
    size_t a = *(const size_t *)left;
    size_t b = *(const size_t *)right;
    return bt_ga_chrom_cmp(ctx->population[a], ctx->population[b], ctx->case_count);
}

static size_t bt_ga_distinct_count(uint32_t **population, size_t pop, size_t case_count,
                                   size_t *scratch_order) {
    for (size_t i = 0; i < pop; i++) {
        scratch_order[i] = i;
    }
    bt_ga_distinct_ctx ctx;
    ctx.population = population;
    ctx.case_count = case_count;
    qsort_s(scratch_order, pop, sizeof *scratch_order, bt_ga_distinct_cmp, &ctx);
    size_t distinct = pop > 0 ? 1 : 0;
    for (size_t i = 1; i < pop; i++) {
        if (bt_ga_chrom_cmp(population[scratch_order[i - 1]], population[scratch_order[i]],
                            case_count) != 0) {
            distinct++;
        }
    }
    return distinct;
}

void bt_ga_ring_migrate(bt_ga_island *islands, size_t island_count, size_t pop,
                        size_t migrate_count, size_t case_count, uint32_t **staging,
                        float *staging_scores) {
    BT_TRACE_ENTER("ga.ring_migrate");
    for (size_t isl = 0; isl < island_count; isl++) {
        for (size_t k = 0; k < migrate_count; k++) {
            memcpy(staging[isl * migrate_count + k], islands[isl].population[islands[isl].order[k]],
                   case_count * sizeof **staging);
            staging_scores[isl * migrate_count + k] = islands[isl].scores[islands[isl].order[k]];
        }
    }
    for (size_t isl = 0; isl < island_count; isl++) {
        size_t dst = (isl + 1) % island_count;
        for (size_t k = 0; k < migrate_count; k++) {
            size_t victim = islands[dst].order[pop - 1 - k];
            memcpy(islands[dst].population[victim], staging[isl * migrate_count + k],
                   case_count * sizeof **staging);
            islands[dst].scores[victim] = staging_scores[isl * migrate_count + k];
        }
        bt_ga_sorted_order(islands[dst].scores, islands[dst].population, pop, case_count,
                           islands[dst].order);
    }
    BT_TRACE_EXIT("ga.ring_migrate");
}

bt_ga_run_outcome *bt_ga_run_ex(const bt_ga_env *env, const bt_ga_params *params,
                                const bt_ga_upgrades *upgrades) {
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
    if (upgrades->elite_fraction_policy &&
        (!(upgrades->elite_fraction >= 0.0) || upgrades->elite_fraction >= 1.0)) {
        bt_ga_abort("ga.run", "ga run", "elite fraction must stay within 0 to 1");
    }
    if (!isfinite(upgrades->swap_scale) || upgrades->swap_scale < 0.0 ||
        !isfinite(upgrades->replace_scale) || upgrades->replace_scale < 0.0) {
        bt_ga_abort("ga.run", "ga run", "adaptive scales must be finite and non negative");
    }
    if (upgrades->diversity_threshold < 0.0 || upgrades->diversity_threshold > 1.0 ||
        upgrades->inject_fraction < 0.0 || upgrades->inject_fraction > 1.0) {
        bt_ga_abort("ga.run", "ga run", "restart thresholds must stay within 0 to 1");
    }
    size_t islands = upgrades->island_count < 1 ? 1 : upgrades->island_count;
    size_t elite = bt_ga_effective_elite(params, upgrades);
    if (!(params->population > elite)) {
        bt_ga_abort("ga.run", "ga run", "population must exceed elite");
    }
    if (!(elite >= 1)) {
        bt_ga_abort("ga.run", "ga run", "elite must be at least 1");
    }
    if (upgrades->migrate_every > 0) {
        if (islands < 2) {
            bt_ga_abort("ga.run", "ga run", "migration requires at least 2 islands");
        }
        if (upgrades->migrate_count < 1) {
            bt_ga_abort("ga.run", "ga run", "migration requires a positive migrate count");
        }
        if (upgrades->migrate_count + elite > params->population) {
            bt_ga_abort("ga.run", "ga run",
                        "migrate count plus elite must stay at or below the population");
        }
    }
    bt_rng rng;
    bt_rng_seed(&rng, params->seed);
    size_t pop = params->population;
    size_t case_count = env->case_count;
    uint32_t *scratch = bt_ga_run_alloc(env->pool_size * sizeof *scratch, "ga.run.scratch");
    bt_ga_island *island = bt_ga_run_alloc(islands * sizeof *island, "ga.run.islands");
    uint32_t *stagnation = bt_ga_run_alloc(islands * sizeof *stagnation, "ga.run.stagnation");
    float *island_best = bt_ga_run_alloc(islands * sizeof *island_best, "ga.run.island_best");
    for (size_t isl = 0; isl < islands; isl++) {
        island[isl].population =
            bt_ga_run_alloc(pop * sizeof *island[isl].population, "ga.run.population");
        for (size_t i = 0; i < pop; i++) {
            island[isl].population[i] =
                bt_ga_run_alloc(case_count * sizeof **island[isl].population, "ga.run.individual");
            bt_ga_random_individual(&rng, env->pool_size, case_count, scratch,
                                    island[isl].population[i]);
        }
        island[isl].scores = bt_ga_run_alloc(pop * sizeof *island[isl].scores, "ga.run.scores");
        bt_ga_eval_population(params, env, island[isl].population, pop, island[isl].scores);
        island[isl].order = bt_ga_run_alloc(pop * sizeof *island[isl].order, "ga.run.order");
        bt_ga_sorted_order(island[isl].scores, island[isl].population, pop, case_count,
                           island[isl].order);
        island_best[isl] = island[isl].scores[island[isl].order[0]];
        stagnation[isl] = 0;
    }
    uint64_t evaluations = islands * pop;
    uint32_t *best = bt_ga_run_alloc(case_count * sizeof *best, "ga.run.best");
    memcpy(best, island[0].population[island[0].order[0]], case_count * sizeof *best);
    float best_fitness = island[0].scores[island[0].order[0]];
    for (size_t isl = 1; isl < islands; isl++) {
        float candidate = island[isl].scores[island[isl].order[0]];
        if (candidate < best_fitness) {
            best_fitness = candidate;
            memcpy(best, island[isl].population[island[isl].order[0]], case_count * sizeof *best);
        }
    }
    size_t best_generation = 0;
    float *per_gen_best =
        bt_ga_run_alloc((params->generations + 1) * sizeof *per_gen_best, "ga.run.per_gen");
    per_gen_best[0] = best_fitness;
    uint32_t stagnation_tail = 0;
    uint32_t max_stagnation = 0;
    uint32_t **next = bt_ga_run_alloc(pop * sizeof *next, "ga.run.next");
    unsigned char *is_elite = bt_ga_run_alloc(pop, "ga.run.is_elite");
    uint64_t swap_gate = (uint64_t)(params->swap * BT_GA_GATE_PPM);
    uint64_t replace_gate = (uint64_t)(params->replace * BT_GA_GATE_PPM);
    uint32_t **staging = NULL;
    float *staging_scores = NULL;
    if (islands >= 2 && upgrades->migrate_every > 0) {
        staging =
            bt_ga_run_alloc(islands * upgrades->migrate_count * sizeof *staging, "ga.run.migrants");
        staging_scores = bt_ga_run_alloc(islands * upgrades->migrate_count * sizeof *staging_scores,
                                         "ga.run.migrant_scores");
        for (size_t i = 0; i < islands * upgrades->migrate_count; i++) {
            staging[i] = bt_ga_run_alloc(case_count * sizeof **staging, "ga.run.migrant");
        }
    }
    size_t *diversity_order = NULL;
    if (upgrades->diversity_threshold > 0.0) {
        diversity_order = bt_ga_run_alloc(pop * sizeof *diversity_order, "ga.run.diversity_order");
    }
    for (size_t generation = 1; generation <= params->generations; generation++) {
        for (size_t isl = 0; isl < islands; isl++) {
            uint64_t island_swap_gate = swap_gate;
            uint64_t island_replace_gate = replace_gate;
            if (upgrades->stagnation_threshold > 0 &&
                stagnation[isl] >= upgrades->stagnation_threshold) {
                island_swap_gate = (uint64_t)(params->swap * upgrades->swap_scale * BT_GA_GATE_PPM);
                island_replace_gate =
                    (uint64_t)(params->replace * upgrades->replace_scale * BT_GA_GATE_PPM);
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
                uint32_t *child = bt_ga_run_alloc(case_count * sizeof *child, "ga.run.child");
                bt_ga_ox(island[isl].population[p1], island[isl].population[p2], case_count, cut1,
                         cut2, child);
                if (bt_rng_below(&rng, 1000000) < island_swap_gate) {
                    bt_ga_mutate_swap(child, case_count, &rng);
                }
                if (bt_rng_below(&rng, 1000000) < island_replace_gate) {
                    bt_ga_mutate_replace(child, case_count, &rng, env->pool_size);
                }
                next[filled] = child;
                filled++;
            }
            evaluations += pop - elite;
            bt_ga_eval_population(params, env, next, pop, island[isl].scores);
            for (size_t i = 0; i < pop; i++) {
                if (!is_elite[i]) {
                    bt_ga_free(island[isl].population[i]);
                }
                island[isl].population[i] = next[i];
            }
            bt_ga_sorted_order(island[isl].scores, island[isl].population, pop, case_count,
                               island[isl].order);
        }
        if (islands >= 2 && upgrades->migrate_every > 0 &&
            generation % upgrades->migrate_every == 0) {
            bt_ga_ring_migrate(island, islands, pop, upgrades->migrate_count, case_count, staging,
                               staging_scores);
        }
        float gen_best = island[0].scores[island[0].order[0]];
        size_t owner = 0;
        for (size_t isl = 1; isl < islands; isl++) {
            float candidate = island[isl].scores[island[isl].order[0]];
            if (candidate < gen_best) {
                gen_best = candidate;
                owner = isl;
            }
        }
        per_gen_best[generation] = gen_best;
        if (gen_best < best_fitness) {
            best_fitness = gen_best;
            memcpy(best, island[owner].population[island[owner].order[0]],
                   case_count * sizeof *best);
            best_generation = generation;
            stagnation_tail = 0;
        } else {
            stagnation_tail += 1;
            if (stagnation_tail > max_stagnation) {
                max_stagnation = stagnation_tail;
            }
        }
        for (size_t isl = 0; isl < islands; isl++) {
            float isl_best = island[isl].scores[island[isl].order[0]];
            if (isl_best < island_best[isl]) {
                island_best[isl] = isl_best;
                stagnation[isl] = 0;
            } else {
                stagnation[isl] += 1;
            }
        }
        if (upgrades->diversity_threshold > 0.0) {
            for (size_t isl = 0; isl < islands; isl++) {
                size_t distinct =
                    bt_ga_distinct_count(island[isl].population, pop, case_count, diversity_order);
                if ((double)distinct / (double)pop < upgrades->diversity_threshold) {
                    size_t kept = (size_t)(upgrades->inject_fraction * (double)pop);
                    for (size_t r = kept; r < pop; r++) {
                        size_t idx = island[isl].order[r];
                        bt_ga_random_individual(&rng, env->pool_size, case_count, scratch,
                                                island[isl].population[idx]);
                        island[isl].scores[idx] =
                            bt_ga_fitness(params, env, island[isl].population[idx]);
                    }
                    evaluations += pop - kept;
                    bt_ga_sorted_order(island[isl].scores, island[isl].population, pop, case_count,
                                       island[isl].order);
                    stagnation[isl] = 0;
                }
            }
        }
    }
    bt_ga_free(scratch);
    bt_ga_free(is_elite);
    for (size_t isl = 0; isl < islands; isl++) {
        for (size_t i = 0; i < pop; i++) {
            bt_ga_free(island[isl].population[i]);
        }
        bt_ga_free(island[isl].population);
        bt_ga_free(island[isl].scores);
        bt_ga_free(island[isl].order);
    }
    bt_ga_free(island);
    bt_ga_free(stagnation);
    bt_ga_free(island_best);
    bt_ga_free(next);
    for (size_t i = 0; staging != NULL && i < islands * upgrades->migrate_count; i++) {
        bt_ga_free(staging[i]);
    }
    bt_ga_free(staging);
    bt_ga_free(staging_scores);
    bt_ga_free(diversity_order);

    bt_ga_run_outcome *outcome = bt_ga_run_alloc(sizeof *outcome, "ga.run.outcome");
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

bt_ga_run_outcome *bt_ga_run(const bt_ga_env *env, const bt_ga_params *params) {
    bt_ga_upgrades upgrades;
    memset(&upgrades, 0, sizeof upgrades);
    return bt_ga_run_ex(env, params, &upgrades);
}

void bt_ga_run_outcome_free(bt_ga_run_outcome *outcome) {
    BT_TRACE_ENTER("ga.run_outcome_free");
    bt_ga_free(outcome->best);
    bt_ga_free(outcome->per_gen_best);
    bt_ga_free(outcome);
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

static int bt_ga_upgrade_int(const bt_json_value *config, const char *dotted, long long floor,
                             long long *out, char err[BT_GA_ERR_MAX]) {
    const bt_json_value *value = bt_config_value(config, dotted);
    if (value == NULL) {
        return 1;
    }
    if (!bt_json_is_integer(value) || value->integer < floor) {
        snprintf(err, BT_GA_ERR_MAX, "config value out of range: %s", dotted);
        BT_TRACE_FAIL("ga.config_upgrades", "%s out of range", dotted);
        return 0;
    }
    *out = value->integer;
    return 1;
}

static int bt_ga_upgrade_number(const bt_json_value *config, const char *dotted, double low,
                                double high, double *out, char err[BT_GA_ERR_MAX]) {
    const bt_json_value *value = bt_config_value(config, dotted);
    if (value == NULL) {
        return 1;
    }
    if (value->type != BT_JSON_NUMBER || !(value->as.number >= low) ||
        !(value->as.number <= high) || !isfinite(value->as.number)) {
        snprintf(err, BT_GA_ERR_MAX, "config value out of range: %s", dotted);
        BT_TRACE_FAIL("ga.config_upgrades", "%s out of range", dotted);
        return 0;
    }
    *out = value->as.number;
    return 1;
}

int bt_ga_config_upgrades(const bt_json_value *config, bt_ga_upgrades *out,
                          char err[BT_GA_ERR_MAX]) {
    BT_TRACE_ENTER("ga.config_upgrades");
    memset(out, 0, sizeof *out);
    out->island_count = 1;
    out->migrate_count = 1;
    out->swap_scale = 1.0;
    out->replace_scale = 1.0;
    out->inject_fraction = 0.5;
    long long number = 0;
    if (!bt_ga_upgrade_int(config, "ga.islands.count", 1, &number, err)) {
        return 0;
    }
    if (bt_config_value(config, "ga.islands.count") != NULL) {
        out->island_count = (size_t)number;
    }
    if (!bt_ga_upgrade_int(config, "ga.islands.migrate_every", 0, &number, err)) {
        return 0;
    }
    if (bt_config_value(config, "ga.islands.migrate_every") != NULL) {
        out->migrate_every = (size_t)number;
    }
    if (!bt_ga_upgrade_int(config, "ga.islands.migrate_count", 0, &number, err)) {
        return 0;
    }
    if (bt_config_value(config, "ga.islands.migrate_count") != NULL) {
        out->migrate_count = (size_t)number;
    }
    if (!bt_ga_upgrade_int(config, "ga.adaptive_mutation.stagnation_threshold", 0, &number, err)) {
        return 0;
    }
    if (bt_config_value(config, "ga.adaptive_mutation.stagnation_threshold") != NULL) {
        out->stagnation_threshold = (uint32_t)number;
    }
    if (!bt_ga_upgrade_number(config, "ga.adaptive_mutation.swap_scale", 0.0, 1.0e18,
                              &out->swap_scale, err)) {
        return 0;
    }
    if (!bt_ga_upgrade_number(config, "ga.adaptive_mutation.replace_scale", 0.0, 1.0e18,
                              &out->replace_scale, err)) {
        return 0;
    }
    const bt_json_value *policy = bt_config_value(config, "ga.elite_carry.policy");
    if (policy != NULL) {
        if (policy->type != BT_JSON_STRING) {
            snprintf(err, BT_GA_ERR_MAX, "config value out of range: ga.elite_carry.policy");
            BT_TRACE_FAIL("ga.config_upgrades", "ga.elite_carry.policy not a string");
            return 0;
        }
        if (strcmp(policy->as.string, "count") == 0) {
            out->elite_fraction_policy = 0;
        } else if (strcmp(policy->as.string, "fraction") == 0) {
            out->elite_fraction_policy = 1;
        } else {
            snprintf(err, BT_GA_ERR_MAX, "config value out of range: ga.elite_carry.policy");
            BT_TRACE_FAIL("ga.config_upgrades", "ga.elite_carry.policy not count or fraction");
            return 0;
        }
    }
    if (!bt_ga_upgrade_number(config, "ga.elite_carry.fraction", 0.0, 1.0, &out->elite_fraction,
                              err)) {
        return 0;
    }
    if (out->elite_fraction >= 1.0) {
        snprintf(err, BT_GA_ERR_MAX, "config value out of range: ga.elite_carry.fraction");
        BT_TRACE_FAIL("ga.config_upgrades", "ga.elite_carry.fraction must stay below 1");
        return 0;
    }
    if (!bt_ga_upgrade_number(config, "ga.restart.diversity_threshold", 0.0, 1.0,
                              &out->diversity_threshold, err)) {
        return 0;
    }
    if (!bt_ga_upgrade_number(config, "ga.restart.inject_fraction", 0.0, 1.0, &out->inject_fraction,
                              err)) {
        return 0;
    }
    if (out->migrate_every > 0) {
        if (out->island_count < 2) {
            snprintf(err, BT_GA_ERR_MAX, "config value out of range: ga.islands.count");
            BT_TRACE_FAIL("ga.config_upgrades", "migration requires at least 2 islands");
            return 0;
        }
        if (out->migrate_count < 1) {
            snprintf(err, BT_GA_ERR_MAX, "config value out of range: ga.islands.migrate_count");
            BT_TRACE_FAIL("ga.config_upgrades", "migration requires a positive migrate count");
            return 0;
        }
    }
    BT_TRACE_EXIT("ga.config_upgrades");
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
