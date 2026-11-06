#include "common/rng.h"
#include "ga/eval.h"
#include "ga/features.h"
#include "ga/ga.h"
#include "ga/inventory.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pick_pool(const char **out, size_t want) {
    FILE *file = fopen("ref_doc/vietnamese/syllables-7184-abc.txt", "rb");
    if (file == NULL) {
        return 0;
    }
    char line[64];
    size_t kept = 0;
    while (kept < want && fgets(line, sizeof line, file) != NULL) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0 || line[0] == '#') {
            continue;
        }
        bt_ga_features feat;
        if (bt_ga_parse(line, &feat)) {
            out[kept++] = _strdup(line);
        }
    }
    fclose(file);
    return kept == want;
}

static void random_individual(bt_rng *rng, size_t pool_size, size_t case_count, uint32_t *scratch,
                              uint32_t *out) {
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

enum { PROBE_POOL = 19, PROBE_POP = 10, PROBE_CASES = 9 };

int main(void) {
    const char *pool_words[PROBE_POOL];
    if (!pick_pool(pool_words, PROBE_POOL)) {
        fprintf(stderr, "could not pick %d parseable words\n", PROBE_POOL);
        return 1;
    }
    for (int i = 0; i < PROBE_POOL; i++) {
        fprintf(stderr, "pool %d %s\n", i, pool_words[i]);
    }
    bt_ga_features *feats = calloc(PROBE_POOL, sizeof *feats);
    float *lengths = calloc(PROBE_POOL, sizeof *lengths);
    for (int i = 0; i < PROBE_POOL; i++) {
        if (!bt_ga_parse(pool_words[i], &feats[i])) {
            fprintf(stderr, "parse failed for %s\n", pool_words[i]);
            return 1;
        }
        lengths[i] = 3.0F;
    }
    bt_ga_pair_weight *cooc = calloc(6, sizeof *cooc);
    cooc[0].i = 0;
    cooc[0].j = 1;
    cooc[0].w = 0.9F;
    cooc[1].i = 1;
    cooc[1].j = 2;
    cooc[1].w = 0.7F;
    cooc[2].i = 3;
    cooc[2].j = 4;
    cooc[2].w = 0.6F;
    cooc[3].i = 5;
    cooc[3].j = 6;
    cooc[3].w = 0.5F;
    cooc[4].i = 7;
    cooc[4].j = 8;
    cooc[4].w = 0.4F;
    cooc[5].i = 2;
    cooc[5].j = 6;
    cooc[5].w = 0.3F;
    bt_ga_pair_weight *adj = calloc(3, sizeof *adj);
    adj[0].i = 0;
    adj[0].j = 1;
    adj[0].w = 0.8F;
    adj[1].i = 3;
    adj[1].j = 4;
    adj[1].w = 0.5F;
    adj[2].i = 5;
    adj[2].j = 6;
    adj[2].w = 0.2F;
    bt_ga_env env;
    char err[BT_GA_ERR_MAX];
    if (!bt_ga_build_env(PROBE_POOL, lengths, PROBE_POOL, feats, PROBE_POOL, PROBE_CASES, cooc, 6,
                         adj, 3, &env, err)) {
        fprintf(stderr, "build_env: %s\n", err);
        return 1;
    }
    fprintf(stderr, "stage: env built\n");
    bt_ga_params params;
    params.seed = 0;
    params.population = PROBE_POP;
    params.generations = 1;
    params.elite = 2;
    params.swap = 0.15;
    params.replace = 0.05;
    params.w_conf = 1.0F;
    params.w_flow = 0.5F;
    params.w_cplx = 0.1F;
    bt_ga_eval *eval = bt_ga_eval_create(&env, &params, PROBE_POP);
    fprintf(stderr, "stage: eval created\n");
    for (uint64_t seed = 1; seed <= 400; seed++) {
        bt_rng rng;
        bt_rng_seed(&rng, seed);
        float best[2] = {FLT_MAX, FLT_MAX};
        for (int isl = 0; isl < 2; isl++) {
            uint32_t *population[PROBE_POP];
            float scores[PROBE_POP];
            uint32_t *scratch = malloc(PROBE_POOL * sizeof *scratch);
            for (int i = 0; i < PROBE_POP; i++) {
                population[i] = malloc(PROBE_CASES * sizeof **population);
                random_individual(&rng, PROBE_POOL, PROBE_CASES, scratch, population[i]);
            }
            fprintf(stderr, "stage: seed %llu island %d built\n", (unsigned long long)seed, isl);
            bt_ga_eval_population(eval, population, PROBE_POP, scores);
            for (int i = 0; i < PROBE_POP; i++) {
                if (scores[i] < best[isl]) {
                    best[isl] = scores[i];
                }
            }
            for (int i = 0; i < PROBE_POP; i++) {
                free(population[i]);
            }
            free(scratch);
        }
        if (best[1] < best[0]) {
            printf("seed %llu island0 %.6f island1 %.6f\n", (unsigned long long)seed, best[0],
                   best[1]);
            return 0;
        }
    }
    puts("no seed found");
    return 2;
}
