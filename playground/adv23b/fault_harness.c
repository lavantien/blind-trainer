#include "ga-run/lib.h"
#include "ga/eval.h"
#include "common/bt_trace.h"
#include "common/config.h"
#include "common/rng.h"
#include "data/paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static size_t hook_hits;
static char hook_site[64];
static int hook_fail = 1;

static int hook(const char *site, void *user) {
    (void)user;
    snprintf(hook_site, sizeof hook_site, "%s", site);
    hook_hits++;
    return hook_fail;
}

static void trace_setup(void) {
    bt_json_value *config = bt_data_load_config();
    long long ring_bytes = 0;
    long long max_threads = 0;
    long long line_max = 0;
    if (config != NULL && bt_config_int(config, "trace.ring_bytes", &ring_bytes) == 0 &&
        bt_config_int(config, "trace.max_threads", &max_threads) == 0 &&
        bt_config_int(config, "trace.line_max", &line_max) == 0 &&
        bt_trace_init((size_t)ring_bytes, (size_t)max_threads, (size_t)line_max) == 0) {
        bt_trace_install_crash_handler();
    }
    bt_json_free(config);
}

static void ring_dump_marked(const char *mark) {
    fprintf(stderr, "---- ring dump %s ----\n", mark);
    bt_trace_dump_to(stderr);
    fprintf(stderr, "---- end ring dump %s ----\n", mark);
}

static double now_secs(void) {
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

static void fingerprint(const char *label, const bt_ga_run_outcome *o) {
    unsigned long long per_hash = 1469598103934665603ULL;
    for (size_t i = 0; i < o->per_gen_count; i++) {
        unsigned bits;
        memcpy(&bits, &o->per_gen_best[i], sizeof bits);
        per_hash = (per_hash ^ bits) * 1099511628211ULL;
    }
    unsigned long long best_hash = 1469598103934665603ULL;
    for (size_t i = 0; i < o->best_count; i++) {
        per_hash = (per_hash ^ o->best[i]) * 1099511628211ULL;
        best_hash = (best_hash ^ o->best[i]) * 1099511628211ULL;
    }
    unsigned fit_bits;
    unsigned part_bits[3];
    memcpy(&fit_bits, &o->best_fitness, sizeof fit_bits);
    for (int k = 0; k < 3; k++) {
        memcpy(&part_bits[k], &o->best_parts[k], sizeof part_bits[k]);
    }
    printf("fp %s fit_bits=%08x parts_bits=%08x,%08x,%08x best_gen=%zu per_gen_count=%zu "
           "stag_tail=%u max_stag=%u evals=%llu best_count=%zu per_hash=%016llx "
           "best_hash=%016llx\n",
           label, fit_bits, part_bits[0], part_bits[1], part_bits[2], o->best_generation,
           o->per_gen_count, o->stagnation_tail, o->max_stagnation,
           (unsigned long long)o->evaluations, o->best_count, per_hash, best_hash);
}

static int outcomes_identical(const bt_ga_run_outcome *a, const bt_ga_run_outcome *b) {
    if (a->best_generation != b->best_generation || a->per_gen_count != b->per_gen_count ||
        a->stagnation_tail != b->stagnation_tail || a->max_stagnation != b->max_stagnation ||
        a->evaluations != b->evaluations || a->best_count != b->best_count) {
        return 0;
    }
    if (memcmp(&a->best_fitness, &b->best_fitness, sizeof a->best_fitness) != 0 ||
        memcmp(a->best_parts, b->best_parts, sizeof a->best_parts) != 0) {
        return 0;
    }
    for (size_t i = 0; i < a->per_gen_count; i++) {
        if (memcmp(&a->per_gen_best[i], &b->per_gen_best[i], sizeof(float)) != 0) {
            printf("per_gen_best differs at %zu: %a vs %a\n", i, (double)a->per_gen_best[i],
                   (double)b->per_gen_best[i]);
            return 0;
        }
    }
    return memcmp(a->best, b->best, a->best_count * sizeof *a->best) == 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: fault_harness <seam|ctxfail> <cpu-config-path>\n");
        return 2;
    }
    const char *mode = argv[1];
    const char *cpu_config = argv[2];
    trace_setup();
    printf("harness mode=%s cuda_compiled=%d\n", mode, bt_ga_eval_cuda_compiled());
    bt_gr_offenders offenders = {0};
    bt_gr_run_inputs inputs;
    if (bt_gr_load_run_inputs(&inputs, &offenders) != 0) {
        for (size_t i = 0; i < offenders.count; i++) {
            fprintf(stderr, "inputs: %s\n", offenders.items[i]);
        }
        return 2;
    }
    bt_gr_offenders_free(&offenders);
    printf("inputs pool=%zu cases=%zu cooc=%zu adj=%zu pop=%zu gens=%zu\n", inputs.pool_count,
           inputs.env.case_count, inputs.env.cooc_count, inputs.env.adj_count,
           inputs.params.population, inputs.params.generations);
    int pass = 1;
    if (strcmp(mode, "seam") == 0) {
        bt_ga_eval_fault_install(hook, NULL);
    }
    bt_ga_eval *session = bt_ga_eval_create(&inputs.env, &inputs.params, inputs.params.population);
    printf("phaseA uses_cuda=%d hook_hits=%zu hook_site=%s\n", bt_ga_eval_uses_cuda(session),
           hook_hits, hook_site);
    ring_dump_marked("after phaseA create");
    if (strcmp(mode, "seam") == 0) {
        pass = pass && bt_ga_eval_uses_cuda(session) == 0;
        pass = pass && hook_hits >= 1 && strcmp(hook_site, "ga.eval.cuda") == 0;
    }
    bt_ga_eval_destroy(session);
    bt_ga_eval_fault_reset();
    size_t case_count = inputs.env.case_count;
    size_t probe_pop = inputs.params.population < 32 ? inputs.params.population : 32;
    uint32_t *chroms = malloc(probe_pop * case_count * sizeof *chroms);
    uint32_t **owners = malloc(probe_pop * sizeof *owners);
    bt_rng rng;
    bt_rng_seed(&rng, inputs.params.seed ^ 0x5eed5eedULL);
    uint32_t *scratch = malloc(inputs.pool_count * sizeof *scratch);
    for (size_t i = 0; i < probe_pop; i++) {
        for (size_t g = 0; g < inputs.pool_count; g++) {
            scratch[g] = (uint32_t)g;
        }
        for (size_t g = case_count; g < inputs.pool_count; g++) {
            size_t j = (size_t)bt_rng_below(&rng, (uint64_t)g + 1);
            uint32_t tmp = scratch[g];
            scratch[g] = scratch[j];
            scratch[j] = tmp;
        }
        memcpy(chroms + i * case_count, scratch, case_count * sizeof *scratch);
        owners[i] = chroms + i * case_count;
    }
    free(scratch);
    if (strcmp(mode, "seam") == 0) {
        bt_ga_eval_fault_install(hook, NULL);
    }
    bt_ga_eval *scorer = bt_ga_eval_create(&inputs.env, &inputs.params, probe_pop);
    float *scores = malloc(probe_pop * sizeof *scores);
    bt_ga_eval_population(scorer, owners, probe_pop, scores);
    size_t mismatches = 0;
    for (size_t i = 0; i < probe_pop; i++) {
        float want = bt_ga_fitness(&inputs.params, &inputs.env, owners[i]);
        if (memcmp(&scores[i], &want, sizeof want) != 0) {
            mismatches++;
        }
    }
    printf("phaseA2 uses_cuda=%d probe_pop=%zu score_mismatches=%zu\n",
           bt_ga_eval_uses_cuda(scorer), probe_pop, mismatches);
    pass = pass && mismatches == 0;
    if (strcmp(mode, "seam") == 0) {
        pass = pass && bt_ga_eval_uses_cuda(scorer) == 0;
    }
    bt_ga_eval_destroy(scorer);
    bt_ga_eval_fault_reset();
    free(scores);
    free(chroms);
    free(owners);
    if (strcmp(mode, "seam") == 0) {
        bt_ga_eval_fault_install(hook, NULL);
    }
    double t0 = now_secs();
    bt_ga_run_outcome *lane = bt_ga_run_ex(&inputs.env, &inputs.params, &inputs.upgrades);
    double t1 = now_secs();
    bt_ga_eval_fault_reset();
    _putenv_s("BT_CONFIG", cpu_config);
    bt_ga_run_outcome *cpu = bt_ga_run_ex(&inputs.env, &inputs.params, &inputs.upgrades);
    double t2 = now_secs();
    fingerprint("lane", lane);
    fingerprint("cpu", cpu);
    int identical = outcomes_identical(lane, cpu);
    printf("phaseB identical=%d evals_lane=%llu evals_cpu=%llu dt_lane=%.3fs dt_cpu=%.3fs\n",
           identical, (unsigned long long)lane->evaluations, (unsigned long long)cpu->evaluations,
           t1 - t0, t2 - t1);
    pass = pass && identical;
    bt_ga_run_outcome_free(lane);
    bt_ga_run_outcome_free(cpu);
    bt_gr_run_inputs_free(&inputs);
    ring_dump_marked("final");
    bt_trace_shutdown();
    printf("harness verdict pass=%d\n", pass);
    return pass ? 0 : 1;
}
