#include "legacy_ga.c"

#include "common/json.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const POOL8_LEG[] = {"ả", "ã", "không", "lông", "tía", "mút", "án", "đặt"};

static size_t codepoint_count_leg(const char *text) {
    size_t count = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++) {
        if ((*p & 0xC0U) != 0x80U) {
            count++;
        }
    }
    return count;
}

static bt_ga_features parse_or_die_leg(const char *text) {
    bt_ga_features out;
    if (!bt_ga_parse(text, &out)) {
        fprintf(stderr, "parse failed for %s\n", text);
        exit(1);
    }
    return out;
}

static bt_ga_env *make_env_leg(size_t case_count) {
    bt_ga_features *feats = calloc(8, sizeof *feats);
    float *lengths = calloc(8, sizeof *lengths);
    for (size_t i = 0; i < 8; i++) {
        feats[i] = parse_or_die_leg(POOL8_LEG[i]);
        lengths[i] = (float)codepoint_count_leg(POOL8_LEG[i]);
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

static uint32_t bits_leg(float value) {
    uint32_t out;
    memcpy(&out, &value, sizeof out);
    return out;
}

static void print_line_leg(uint64_t seed, size_t pop, size_t elite, size_t gens, double swap,
                           double replace, size_t case_count, const bt_ga_run_outcome *o) {
    printf("L %" PRIu64 " %zu %zu %zu %g %g %zu genes", seed, pop, elite, gens, swap, replace,
           case_count);
    for (size_t i = 0; i < o->best_count; i++) {
        printf(" %u", o->best[i]);
    }
    printf(" fit %08" PRIx32, bits_leg(o->best_fitness));
    for (size_t i = 0; i < 3; i++) {
        printf(" %08" PRIx32, bits_leg(o->best_parts[i]));
    }
    printf(" pergen");
    for (size_t i = 0; i < o->per_gen_count; i++) {
        printf(" %08" PRIx32, bits_leg(o->per_gen_best[i]));
    }
    printf(" gen %zu tail %u max %u evals %" PRIu64 "\n", o->best_generation, o->stagnation_tail,
           o->max_stagnation, o->evaluations);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    static const uint64_t seeds[] = {1, 2, 424242, 999983};
    static const size_t pops[] = {2, 3, 4, 5, 8};
    static const double swaps[] = {0.15, 1.0};
    static const double replaces[] = {0.05, 1.0};
    size_t cases[] = {6, 3};
    for (size_t c = 0; c < 2; c++) {
        bt_ga_env *env = make_env_leg(cases[c]);
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
                            bt_ga_run_outcome *o = bt_ga_run(env, &params);
                            print_line_leg(seeds[si], pops[pi], ei, 4, swaps[wi], replaces[ri],
                                           cases[c], o);
                            bt_ga_run_outcome_free(o);
                        }
                    }
                }
            }
        }
        bt_ga_env_free(env);
    }
    return 0;
}
