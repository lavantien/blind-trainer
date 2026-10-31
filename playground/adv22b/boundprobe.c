#include "common/json.h"
#include "ga/features.h"
#include "ga/ga.h"

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

static bt_ga_env *bound_env(size_t case_count) {
    bt_ga_features *feats = calloc(8, sizeof *feats);
    float *lengths = calloc(8, sizeof *lengths);
    for (size_t i = 0; i < 8; i++) {
        if (!bt_ga_parse(POOL8[i], &feats[i])) {
            fprintf(stderr, "parse failed for %s\n", POOL8[i]);
            exit(1);
        }
        lengths[i] = (float)codepoint_count(POOL8[i]);
    }
    bt_ga_env *env = malloc(sizeof *env);
    char err[BT_GA_ERR_MAX];
    if (!bt_ga_build_env(8, lengths, 8, feats, 8, case_count, NULL, 0, NULL, 0, env, err)) {
        fprintf(stderr, "build_env failed: %s\n", err);
        exit(1);
    }
    return env;
}

static bt_ga_params bound_params(uint64_t seed, size_t pop, size_t gens, size_t elite,
                                 double swap, double replace) {
    bt_ga_params params;
    params.seed = seed;
    params.population = pop;
    params.generations = gens;
    params.elite = elite;
    params.swap = swap;
    params.replace = replace;
    params.w_conf = 1.0F;
    params.w_flow = 0.5F;
    params.w_cplx = 0.1F;
    return params;
}

static bt_ga_upgrades zero_upgrades(void) {
    bt_ga_upgrades upgrades;
    memset(&upgrades, 0, sizeof upgrades);
    return upgrades;
}

static int outcomes_equal(const bt_ga_run_outcome *a, const bt_ga_run_outcome *b) {
    if (a->best_count != b->best_count || a->per_gen_count != b->per_gen_count ||
        a->best_generation != b->best_generation || a->stagnation_tail != b->stagnation_tail ||
        a->max_stagnation != b->max_stagnation || a->evaluations != b->evaluations) {
        return 0;
    }
    uint32_t abits;
    uint32_t bbits;
    memcpy(&abits, &a->best_fitness, sizeof abits);
    memcpy(&bbits, &b->best_fitness, sizeof bbits);
    if (abits != bbits) {
        return 0;
    }
    for (size_t i = 0; i < a->best_count; i++) {
        if (a->best[i] != b->best[i]) {
            return 0;
        }
    }
    for (size_t i = 0; i < a->per_gen_count; i++) {
        uint32_t x;
        uint32_t y;
        memcpy(&x, &a->per_gen_best[i], sizeof x);
        memcpy(&y, &b->per_gen_best[i], sizeof y);
        if (x != y) {
            return 0;
        }
    }
    return 1;
}

static int per_gen_monotone(const bt_ga_run_outcome *o) {
    for (size_t i = 1; i < o->per_gen_count; i++) {
        if (o->per_gen_best[i] > o->per_gen_best[i - 1] + 1.19209290e-07F) {
            return 0;
        }
    }
    return 1;
}

static void report(const char *label, const bt_ga_run_outcome *o) {
    uint32_t bits;
    memcpy(&bits, &o->best_fitness, sizeof bits);
    printf("%s evals=%llu best_gen=%zu best_fit=0x%08x monotone=%d\n", label,
           (unsigned long long)o->evaluations, o->best_generation, bits, per_gen_monotone(o));
}

static void case_islands1_degenerates(void) {
    bt_ga_env *env = bound_env(5);
    bt_ga_params params = bound_params(424242, 6, 3, 2, 0.15, 0.05);
    bt_ga_upgrades one = zero_upgrades();
    one.island_count = 1;
    bt_ga_run_outcome *legacy = bt_ga_run(env, &params);
    bt_ga_run_outcome *upgraded = bt_ga_run_ex(env, &params, &one);
    printf("islands1_degenerates %s\n", outcomes_equal(legacy, upgraded) ? "MATCH" : "MISMATCH");
    bt_ga_run_outcome_free(upgraded);
    bt_ga_run_outcome_free(legacy);
    bt_ga_env_free(env);
}

static void case_stagnation0_scales0(void) {
    bt_ga_env *env = bound_env(5);
    bt_ga_params params = bound_params(424242, 6, 3, 2, 0.15, 0.05);
    bt_ga_upgrades gated = zero_upgrades();
    gated.stagnation_threshold = 0;
    gated.swap_scale = 0.0;
    gated.replace_scale = 0.0;
    bt_ga_run_outcome *legacy = bt_ga_run(env, &params);
    bt_ga_run_outcome *upgraded = bt_ga_run_ex(env, &params, &gated);
    printf("stagnation0_scales0 %s\n", outcomes_equal(legacy, upgraded) ? "MATCH" : "MISMATCH");
    bt_ga_run_outcome_free(upgraded);
    bt_ga_run_outcome_free(legacy);
    bt_ga_env_free(env);
}

static void case_migrate_every0(void) {
    bt_ga_env *env = bound_env(5);
    bt_ga_params params = bound_params(424242, 6, 3, 2, 0.15, 0.05);
    bt_ga_upgrades islands = zero_upgrades();
    islands.island_count = 3;
    islands.migrate_every = 0;
    islands.migrate_count = 0;
    bt_ga_run_outcome *o = bt_ga_run_ex(env, &params, &islands);
    report("migrate_every0", o);
    bt_ga_run_outcome_free(o);
    bt_ga_env_free(env);
}

static void case_migrate_count_max(void) {
    bt_ga_env *env = bound_env(5);
    bt_ga_params params = bound_params(424242, 6, 3, 2, 0.15, 0.05);
    bt_ga_upgrades islands = zero_upgrades();
    islands.island_count = 2;
    islands.migrate_every = 1;
    islands.migrate_count = 4;
    bt_ga_run_outcome *o = bt_ga_run_ex(env, &params, &islands);
    report("migrate_count_max", o);
    bt_ga_run_outcome_free(o);
    bt_ga_env_free(env);
}

static void case_restart_inject_edges(void) {
    bt_ga_env *env = bound_env(3);
    bt_ga_params params = bound_params(2, 4, 6, 3, 0.0, 0.0);
    bt_ga_upgrades zero = zero_upgrades();
    bt_ga_upgrades inject0 = zero_upgrades();
    inject0.diversity_threshold = 1.0;
    inject0.inject_fraction = 0.0;
    bt_ga_upgrades inject1 = zero_upgrades();
    inject1.diversity_threshold = 1.0;
    inject1.inject_fraction = 1.0;
    bt_ga_run_outcome *plain = bt_ga_run_ex(env, &params, &zero);
    bt_ga_run_outcome *r0 = bt_ga_run_ex(env, &params, &inject0);
    bt_ga_run_outcome *r1 = bt_ga_run_ex(env, &params, &inject1);
    report("restart_plain", plain);
    report("restart_inject0", r0);
    report("restart_inject1", r1);
    bt_ga_run_outcome_free(r1);
    bt_ga_run_outcome_free(r0);
    bt_ga_run_outcome_free(plain);
    bt_ga_env_free(env);
}

static void case_scale_edges(void) {
    bt_ga_env *env = bound_env(5);
    bt_ga_params params = bound_params(424242, 6, 3, 2, 0.15, 0.05);
    bt_ga_upgrades huge = zero_upgrades();
    huge.stagnation_threshold = 1;
    huge.swap_scale = 1.0e18;
    huge.replace_scale = 1.0e18;
    bt_ga_run_outcome *o = bt_ga_run_ex(env, &params, &huge);
    report("scale_1e18", o);
    bt_ga_run_outcome_free(o);
    bt_ga_upgrades absurd = zero_upgrades();
    absurd.stagnation_threshold = 1;
    absurd.swap_scale = 1.0e300;
    absurd.replace_scale = 1.0e300;
    bt_ga_run_outcome *p = bt_ga_run_ex(env, &params, &absurd);
    report("scale_1e300", p);
    bt_ga_run_outcome_free(p);
    bt_ga_env_free(env);
}

static void case_elite_fraction_exact(void) {
    bt_ga_env *env = bound_env(5);
    bt_ga_params params = bound_params(424242, 6, 3, 2, 0.15, 0.05);
    bt_ga_upgrades fraction = zero_upgrades();
    fraction.elite_fraction_policy = 1;
    fraction.elite_fraction = 0.5;
    bt_ga_run_outcome *o = bt_ga_run_ex(env, &params, &fraction);
    report("elite_fraction_exact", o);
    bt_ga_run_outcome_free(o);
    bt_ga_env_free(env);
}

static void die_migrate_over(void) {
    bt_ga_env *env = bound_env(5);
    bt_ga_params params = bound_params(424242, 6, 3, 2, 0.15, 0.05);
    bt_ga_upgrades islands = zero_upgrades();
    islands.island_count = 2;
    islands.migrate_every = 1;
    islands.migrate_count = 5;
    bt_ga_run_outcome *o = bt_ga_run_ex(env, &params, &islands);
    bt_ga_run_outcome_free(o);
    bt_ga_env_free(env);
}

static void die_fraction_zero(void) {
    bt_ga_env *env = bound_env(5);
    bt_ga_params params = bound_params(424242, 6, 3, 2, 0.15, 0.05);
    bt_ga_upgrades fraction = zero_upgrades();
    fraction.elite_fraction_policy = 1;
    fraction.elite_fraction = 0.0;
    bt_ga_run_outcome *o = bt_ga_run_ex(env, &params, &fraction);
    bt_ga_run_outcome_free(o);
    bt_ga_env_free(env);
}

static void die_fraction_one(void) {
    bt_ga_env *env = bound_env(5);
    bt_ga_params params = bound_params(424242, 6, 3, 2, 0.15, 0.05);
    bt_ga_upgrades fraction = zero_upgrades();
    fraction.elite_fraction_policy = 1;
    fraction.elite_fraction = 1.0;
    bt_ga_run_outcome *o = bt_ga_run_ex(env, &params, &fraction);
    bt_ga_run_outcome_free(o);
    bt_ga_env_free(env);
}

static void die_islands_wrap(void) {
    bt_ga_env *env = bound_env(5);
    bt_ga_params params = bound_params(424242, 6, 3, 2, 0.15, 0.05);
    bt_ga_upgrades wrap = zero_upgrades();
    wrap.island_count = 4611686018427387904ULL;
    bt_ga_run_outcome *o = bt_ga_run_ex(env, &params, &wrap);
    bt_ga_run_outcome_free(o);
    bt_ga_env_free(env);
}

static void case_config_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        printf("cfg open failed\n");
        exit(1);
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = malloc((size_t)size + 1);
    if (fread(text, 1, (size_t)size, file) != (size_t)size) {
        printf("cfg read failed\n");
        exit(1);
    }
    text[size] = '\0';
    fclose(file);
    bt_json_value *config = NULL;
    bt_json_error error;
    if (bt_json_parse(text, (size_t)size, &config, &error) != 0) {
        printf("cfg parse failed: %s\n", error.message);
        exit(1);
    }
    bt_ga_upgrades upgrades;
    char err[BT_GA_ERR_MAX];
    int ok = bt_ga_config_upgrades(config, &upgrades, err);
    printf("cfg accept=%d err=%s islands=%zu every=%zu count=%zu stag=%u swap_scale=%.17g "
           "replace_scale=%.17g policy=%d fraction=%.17g diversity=%.17g inject=%.17g\n",
           ok, ok ? "-" : err, upgrades.island_count, upgrades.migrate_every,
           upgrades.migrate_count, upgrades.stagnation_threshold, upgrades.swap_scale,
           upgrades.replace_scale, upgrades.elite_fraction_policy, upgrades.elite_fraction,
           upgrades.diversity_threshold, upgrades.inject_fraction);
    bt_json_free(config);
    free(text);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: boundprobe <case> [path]\n");
        return 2;
    }
    const char *case_name = argv[1];
    if (strcmp(case_name, "islands1") == 0) {
        case_islands1_degenerates();
    } else if (strcmp(case_name, "stagnation0") == 0) {
        case_stagnation0_scales0();
    } else if (strcmp(case_name, "migrate0") == 0) {
        case_migrate_every0();
    } else if (strcmp(case_name, "migratemax") == 0) {
        case_migrate_count_max();
    } else if (strcmp(case_name, "restart") == 0) {
        case_restart_inject_edges();
    } else if (strcmp(case_name, "scales") == 0) {
        case_scale_edges();
    } else if (strcmp(case_name, "fraction") == 0) {
        case_elite_fraction_exact();
    } else if (strcmp(case_name, "die_over") == 0) {
        die_migrate_over();
    } else if (strcmp(case_name, "die_frac0") == 0) {
        die_fraction_zero();
    } else if (strcmp(case_name, "die_frac1") == 0) {
        die_fraction_one();
    } else if (strcmp(case_name, "die_wrap") == 0) {
        die_islands_wrap();
    } else if (strcmp(case_name, "cfg") == 0 && argc >= 3) {
        case_config_file(argv[2]);
    } else {
        fprintf(stderr, "unknown case %s\n", case_name);
        return 2;
    }
    return 0;
}
