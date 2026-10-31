#include "ga/alloc.h"
#include "ga/ga.h"
#include "ga/inventory.h"

#include "common/bt_harness.h"
#include "common/json.h"

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const POOL8[] = {"ả", "ã", "không", "lông", "tía", "mút", "án", "đặt"};

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

static bt_ga_env *make_env(size_t case_count) {
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

static uint32_t bits(float value) {
    uint32_t out;
    memcpy(&out, &value, sizeof out);
    return out;
}

static void print_line(const char *tag, uint64_t seed, size_t pop, size_t elite, size_t gens,
                       double swap, double replace, size_t case_count,
                       const bt_ga_run_outcome *o) {
    printf("%s %" PRIu64 " %zu %zu %zu %g %g %zu genes", tag, seed, pop, elite, gens, swap, replace,
           case_count);
    for (size_t i = 0; i < o->best_count; i++) {
        printf(" %u", o->best[i]);
    }
    printf(" fit %08" PRIx32, bits(o->best_fitness));
    for (size_t i = 0; i < 3; i++) {
        printf(" %08" PRIx32, bits(o->best_parts[i]));
    }
    printf(" pergen");
    for (size_t i = 0; i < o->per_gen_count; i++) {
        printf(" %08" PRIx32, bits(o->per_gen_best[i]));
    }
    printf(" gen %zu tail %u max %u evals %" PRIu64 "\n", o->best_generation, o->stagnation_tail,
           o->max_stagnation, o->evaluations);
}

static bt_ga_upgrades zero_upgrades(void) {
    bt_ga_upgrades up;
    memset(&up, 0, sizeof up);
    return up;
}

static int outcomes_identical(const bt_ga_run_outcome *a, const bt_ga_run_outcome *b) {
    if (a->best_count != b->best_count || a->per_gen_count != b->per_gen_count ||
        a->best_generation != b->best_generation || a->stagnation_tail != b->stagnation_tail ||
        a->max_stagnation != b->max_stagnation || a->evaluations != b->evaluations) {
        return 0;
    }
    if (bits(a->best_fitness) != bits(b->best_fitness)) {
        return 0;
    }
    for (size_t i = 0; i < 3; i++) {
        if (bits(a->best_parts[i]) != bits(b->best_parts[i])) {
            return 0;
        }
    }
    for (size_t i = 0; i < a->best_count; i++) {
        if (a->best[i] != b->best[i]) {
            return 0;
        }
    }
    for (size_t i = 0; i < a->per_gen_count; i++) {
        if (bits(a->per_gen_best[i]) != bits(b->per_gen_best[i])) {
            return 0;
        }
    }
    return 1;
}

static void mode_sweep(void) {
    static const uint64_t seeds[] = {1, 2, 424242, 999983};
    static const size_t pops[] = {2, 3, 4, 5, 8};
    static const double swaps[] = {0.15, 1.0};
    static const double replaces[] = {0.05, 1.0};
    size_t cases[] = {6, 3};
    size_t mismatch = 0;
    size_t total = 0;
    for (size_t c = 0; c < 2; c++) {
        bt_ga_env *env = make_env(cases[c]);
        for (size_t si = 0; si < 4; si++) {
            for (size_t pi = 0; pi < 5; pi++) {
                for (size_t ei = 1; ei < pops[pi]; ei++) {
                    for (size_t wi = 0; wi < 2; wi++) {
                        for (size_t ri = 0; ri < 2; ri++) {
                            bt_ga_params params;
                            params.seed = seeds[si];
                            params.population = pops[pi];
                            params.generations = 4;
                            params.elite = ei;
                            params.swap = swaps[wi];
                            params.replace = replaces[ri];
                            params.w_conf = 1.0F;
                            params.w_flow = 0.5F;
                            params.w_cplx = 0.1F;
                            bt_ga_upgrades zero = zero_upgrades();
                            bt_ga_run_outcome *a = bt_ga_run(env, &params);
                            bt_ga_run_outcome *b = bt_ga_run_ex(env, &params, &zero);
                            total++;
                            if (!outcomes_identical(a, b)) {
                                mismatch++;
                                print_line("MISMATCH-run-vs-runex", seeds[si], pops[pi], ei, 4,
                                           swaps[wi], replaces[ri], cases[c], a);
                                print_line("MISMATCH-run-vs-runex", seeds[si], pops[pi], ei, 4,
                                           swaps[wi], replaces[ri], cases[c], b);
                            }
                            print_line("L", seeds[si], pops[pi], ei, 4, swaps[wi], replaces[ri],
                                       cases[c], a);
                            bt_ga_run_outcome_free(b);
                            bt_ga_run_outcome_free(a);
                        }
                    }
                }
            }
        }
        bt_ga_env_free(env);
    }
    fprintf(stderr, "sweep total %zu mismatch %zu\n", total, mismatch);
}

static void mode_restart0(void) {
    size_t found = 0;
    for (uint64_t seed = 1; seed < 60 && found < 4; seed++) {
        bt_ga_env *env = make_env(6);
        bt_ga_params params;
        params.seed = seed;
        params.population = 8;
        params.generations = 6;
        params.elite = 2;
        params.swap = 0.15;
        params.replace = 0.05;
        params.w_conf = 1.0F;
        params.w_flow = 0.5F;
        params.w_cplx = 0.1F;
        bt_ga_upgrades restart = zero_upgrades();
        restart.diversity_threshold = 0.9;
        restart.inject_fraction = 0.1;
        bt_ga_run_outcome *o = bt_ga_run_ex(env, &params, &restart);
        int rose = 0;
        for (size_t g = 1; g < o->per_gen_count; g++) {
            if (o->per_gen_best[g] > o->per_gen_best[g - 1]) {
                rose = 1;
            }
        }
        uint64_t plain_evals;
        {
            bt_ga_upgrades zero = zero_upgrades();
            bt_ga_run_outcome *p = bt_ga_run_ex(env, &params, &zero);
            plain_evals = p->evaluations;
            bt_ga_run_outcome_free(p);
        }
        if (rose) {
            found++;
            printf("seed %" PRIu64 " pergen rose, inject 0.1 of pop 8 keeps 0\n", seed);
            print_line("R", seed, 8, 2, 6, 0.15, 0.05, 6, o);
            printf("evals %" PRIu64 " vs plain %" PRIu64 " delta %" PRIu64 "\n", o->evaluations,
                   plain_evals, o->evaluations - plain_evals);
        }
        bt_ga_run_outcome_free(o);
        bt_ga_env_free(env);
    }
    if (found == 0) {
        printf("no rise found in 60 seeds\n");
    }
}

static void mode_ringoob(void) {
    size_t pop = 4;
    size_t case_count = 3;
    size_t island_count = 2;
    size_t order_slots = 8;
    size_t *backing = malloc(order_slots * 2 * sizeof *backing);
    memset(backing, 0xAA, order_slots * 2 * sizeof *backing);
    bt_ga_island *islands = calloc(island_count, sizeof *islands);
    uint32_t **staging = calloc(pop + 2, sizeof *staging);
    float *staging_scores = calloc(pop + 2, sizeof *staging_scores);
    for (size_t isl = 0; isl < island_count; isl++) {
        islands[isl].population = calloc(pop, sizeof *islands[isl].population);
        islands[isl].scores = calloc(pop, sizeof *islands[isl].scores);
        islands[isl].order = backing + isl * order_slots;
        for (size_t i = 0; i < pop; i++) {
            islands[isl].population[i] = calloc(case_count, sizeof(uint32_t));
            for (size_t g = 0; g < case_count; g++) {
                islands[isl].population[i][g] = (uint32_t)(isl * pop + i);
            }
            islands[isl].scores[i] = (float)(10 + i);
            islands[isl].order[i] = i;
        }
    }
    for (size_t i = 0; i < pop + 2; i++) {
        staging[i] = calloc(case_count, sizeof(uint32_t));
    }
    printf("calling exported bt_ga_ring_migrate with migrate_count %zu over pop %zu\n", pop + 2,
           pop);
    fflush(stdout);
    bt_ga_ring_migrate(islands, island_count, pop, pop + 2, case_count, staging, staging_scores);
    printf("returned without crash, island 0 slot 3 genes now:");
    for (size_t g = 0; g < case_count; g++) {
        printf(" %u", islands[0].population[3][g]);
    }
    printf("\n");
}

static void mode_gate(void) {
    double product = 0.15 * 1.0e18 * 1000000.0;
    printf("swap*swap_scale*gate_ppm = %g\n", product);
    printf("(uint64_t)cast of it = %" PRIu64 "\n", (uint64_t)product);
    bt_ga_env *env = make_env(6);
    bt_ga_params params;
    params.seed = 424242;
    params.population = 8;
    params.generations = 4;
    params.elite = 2;
    params.swap = 0.15;
    params.replace = 0.05;
    params.w_conf = 1.0F;
    params.w_flow = 0.5F;
    params.w_cplx = 0.1F;
    bt_ga_upgrades adaptive = zero_upgrades();
    adaptive.stagnation_threshold = 1;
    adaptive.swap_scale = 1.0;
    adaptive.replace_scale = 1.0;
    bt_ga_run_outcome *plain = bt_ga_run_ex(env, &params, &adaptive);
    print_line("G1", 424242, 8, 2, 4, 0.15, 0.05, 6, plain);
    bt_ga_run_outcome_free(plain);
    adaptive.swap_scale = 1.0e18;
    bt_ga_run_outcome *huge = bt_ga_run_ex(env, &params, &adaptive);
    print_line("G2", 424242, 8, 2, 4, 0.15, 0.05, 6, huge);
    printf("streams %s\n", outcomes_identical(plain, huge) ? "same" : "diverged");
    bt_ga_run_outcome_free(huge);
    bt_ga_run_outcome_free(plain);
    bt_ga_env_free(env);
}

static void mode_cfg(void) {
    const char *literal =
        "{\"ga\":{\"elite_carry\":{\"policy\":\"fraction\",\"fraction\":0.04},"
        "\"islands\":{\"count\":2,\"migrate_every\":2,\"migrate_count\":5}}}";
    bt_json_value *config = NULL;
    bt_json_error error;
    if (bt_json_parse(literal, strlen(literal), &config, &error) != 0) {
        printf("literal does not parse\n");
        return;
    }
    bt_ga_upgrades upgrades;
    char err[BT_GA_ERR_MAX];
    int ok = bt_ga_config_upgrades(config, &upgrades, err);
    printf("config fraction 0.04 migrate_count 5: validation %s err %s\n", ok ? "PASS" : "FAIL",
           ok ? "-" : err);
    bt_json_free(config);
}

static void mode_cfgdie(void) {
    bt_ga_env *env = make_env(6);
    bt_ga_params params;
    params.seed = 424242;
    params.population = 20;
    params.generations = 4;
    params.elite = 2;
    params.swap = 0.15;
    params.replace = 0.05;
    params.w_conf = 1.0F;
    params.w_flow = 0.5F;
    params.w_cplx = 0.1F;
    bt_ga_upgrades fraction = zero_upgrades();
    fraction.elite_fraction_policy = 1;
    fraction.elite_fraction = 0.04;
    printf("calling run_ex with validated config fraction 0.04 pop 20\n");
    fflush(stdout);
    bt_ga_run_outcome *o = bt_ga_run_ex(env, &params, &fraction);
    printf("survived, evaluations %" PRIu64 "\n", o->evaluations);
    bt_ga_run_outcome_free(o);
    bt_ga_env_free(env);
}

static void mode_starnve(void) {
    bt_ga_env *env = make_env(6);
    bt_ga_params params;
    params.seed = 424242;
    params.population = 8;
    params.generations = 6;
    params.elite = 2;
    params.swap = 0.15;
    params.replace = 0.05;
    params.w_conf = 1.0F;
    params.w_flow = 0.5F;
    params.w_cplx = 0.1F;
    bt_ga_upgrades edge = zero_upgrades();
    edge.island_count = 2;
    edge.migrate_every = 1;
    edge.migrate_count = 6;
    bt_ga_run_outcome *o = bt_ga_run_ex(env, &params, &edge);
    print_line("E", 424242, 8, 2, 6, 0.15, 0.05, 6, o);
    uint64_t want = 2ULL * 8ULL + 6ULL * 2ULL * (8ULL - 2ULL);
    printf("evals %" PRIu64 " want %" PRIu64 " %s\n", o->evaluations, want,
           o->evaluations == want ? "OK" : "BAD");
    bt_ga_run_outcome_free(o);
    bt_ga_env_free(env);
}

static void mode_ringoverlap(void) {
    size_t pop = 4;
    size_t case_count = 3;
    size_t island_count = 2;
    size_t migrate_count = 3;
    bt_ga_island *islands = calloc(island_count, sizeof *islands);
    uint32_t **staging = calloc(migrate_count * island_count, sizeof *staging);
    float *staging_scores = calloc(migrate_count * island_count, sizeof *staging_scores);
    for (size_t i = 0; i < migrate_count * island_count; i++) {
        staging[i] = calloc(case_count, sizeof(uint32_t));
    }
    for (size_t isl = 0; isl < island_count; isl++) {
        islands[isl].population = calloc(pop, sizeof *islands[isl].population);
        islands[isl].scores = calloc(pop, sizeof *islands[isl].scores);
        islands[isl].order = calloc(pop, sizeof *islands[isl].order);
        for (size_t i = 0; i < pop; i++) {
            islands[isl].population[i] = calloc(case_count, sizeof(uint32_t));
            for (size_t g = 0; g < case_count; g++) {
                islands[isl].population[i][g] = (uint32_t)(isl * 100 + i * 10 + g);
            }
            islands[isl].scores[i] = (float)(40 - i * 10 + isl);
            islands[isl].order[i] = i;
        }
    }
    printf("before: island1 slots by rank:");
    for (size_t r = 0; r < pop; r++) {
        printf(" [%zu]", islands[1].order[r]);
    }
    printf(" genes");
    for (size_t r = 0; r < pop; r++) {
        printf(" %u-%u-%u", islands[1].population[islands[1].order[r]][0],
               islands[1].population[islands[1].order[r]][1],
               islands[1].population[islands[1].order[r]][2]);
    }
    printf("\n");
    bt_ga_ring_migrate(islands, island_count, pop, migrate_count, case_count, staging,
                       staging_scores);
    printf("after: island1 slots by new rank:");
    for (size_t r = 0; r < pop; r++) {
        printf(" [%zu]", islands[1].order[r]);
    }
    printf(" genes");
    for (size_t r = 0; r < pop; r++) {
        printf(" %u-%u-%u", islands[1].population[islands[1].order[r]][0],
               islands[1].population[islands[1].order[r]][1],
               islands[1].population[islands[1].order[r]][2]);
    }
    printf("\nexpect island0 originals 0-1-2 10-11-12 20-21-22 landed in island1, snapshot order\n");
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 2) {
        fprintf(stderr, "usage: probe_cur <sweep|restart0|ringoob|ringoverlap|gate|cfg|cfgdie|starnve>\n");
        return 2;
    }
    if (strcmp(argv[1], "sweep") == 0) {
        mode_sweep();
    } else if (strcmp(argv[1], "restart0") == 0) {
        mode_restart0();
    } else if (strcmp(argv[1], "ringoob") == 0) {
        mode_ringoob();
    } else if (strcmp(argv[1], "ringoverlap") == 0) {
        mode_ringoverlap();
    } else if (strcmp(argv[1], "gate") == 0) {
        mode_gate();
    } else if (strcmp(argv[1], "cfg") == 0) {
        mode_cfg();
    } else if (strcmp(argv[1], "cfgdie") == 0) {
        mode_cfgdie();
    } else if (strcmp(argv[1], "starnve") == 0) {
        mode_starnve();
    } else {
        fprintf(stderr, "unknown mode %s\n", argv[1]);
        return 2;
    }
    return 0;
}
