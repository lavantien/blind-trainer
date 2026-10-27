#include "ga/ga.h"
#include "ga/inventory.h"

#include "common/bt_harness.h"
#include "common/json.h"

#include "death_test.h"
#include "ga_test_util.h"

static const char *const POOL8[] = {"ả", "ã", "không", "lông", "tía", "mút", "án", "đặt"};

static size_t codepoint_count(const char *text) {
    size_t count = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++) {
        if ((*p & 0xC0) != 0x80) {
            count++;
        }
    }
    return count;
}

static bt_ga_env *env_fixture_texts(size_t case_count, const char *const *texts,
                                    size_t text_count) {
    bt_ga_features *feats = calloc(text_count, sizeof *feats);
    float *lengths = calloc(text_count, sizeof *lengths);
    for (size_t i = 0; i < text_count; i++) {
        feats[i] = bt_ga_parse_or_abort(texts[i]);
        lengths[i] = (float)codepoint_count(texts[i]);
    }
    bt_ga_pair_weight *cooc = calloc(2, sizeof *cooc);
    cooc[0].i = 0;
    cooc[0].j = 1;
    cooc[0].w = 0.5f;
    cooc[1].i = 1;
    cooc[1].j = 2;
    cooc[1].w = 0.25f;
    bt_ga_pair_weight *adj = calloc(1, sizeof *adj);
    adj[0].i = 0;
    adj[0].j = 1;
    adj[0].w = 0.5f;
    bt_ga_env *env = malloc(sizeof *env);
    char err[BT_GA_ERR_MAX];
    if (!bt_ga_build_env(text_count, lengths, feats, text_count, case_count, cooc, 2, adj, 1, env,
                         err)) {
        BT_ABORT("build_env: %s", err);
    }
    return env;
}

static bt_ga_env *env_fixture(size_t case_count) {
    return env_fixture_texts(case_count, POOL8, 8);
}

static bt_ga_params small_params(uint64_t seed) {
    bt_ga_params params;
    params.seed = seed;
    params.population = 24;
    params.generations = 40;
    params.elite = 2;
    params.swap = 0.15;
    params.replace = 0.05;
    params.w_conf = 1.0f;
    params.w_flow = 0.5f;
    params.w_cplx = 0.1f;
    return params;
}

BT_TEST(ga, hang_watchdog_arms_for_this_binary) {
    bt_ga_arm_hang_watchdog();
}

BT_TEST(ga, ox_reproduces_the_chapter_pins) {
    uint32_t child[5];
    bt_ga_ox((uint32_t[]){1, 2, 3, 4, 5}, (uint32_t[]){3, 1, 2, 5, 4}, 5, 1, 3, child);
    uint32_t want1[5] = {4, 2, 3, 1, 5};
    BT_CHECK(memcmp(child, want1, sizeof want1) == 0);
    bt_ga_ox((uint32_t[]){5, 4, 3, 2, 1}, (uint32_t[]){2, 1, 4, 3, 5}, 5, 1, 4, child);
    uint32_t want2[5] = {5, 4, 3, 2, 1};
    BT_CHECK(memcmp(child, want2, sizeof want2) == 0);
}

BT_TEST(ga, ox_keeps_kept_segment_and_permutation_shape) {
    uint32_t p1[5] = {1, 2, 3, 4, 5};
    uint32_t p2[5] = {3, 1, 2, 5, 4};
    uint32_t child[5];
    bt_ga_ox(p1, p2, 5, 1, 3, child);
    BT_CHECK(memcmp(&child[1], &p1[1], 2 * sizeof(uint32_t)) == 0);
    uint32_t sorted[5];
    memcpy(sorted, child, sizeof sorted);
    for (int i = 0; i < 5; i++) {
        for (int j = i + 1; j < 5; j++) {
            if (sorted[j] < sorted[i]) {
                uint32_t tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }
        }
    }
    uint32_t identity[5] = {1, 2, 3, 4, 5};
    BT_CHECK(memcmp(sorted, identity, sizeof identity) == 0);
    bt_ga_ox(p1, p2, 5, 0, 5, child);
    BT_CHECK(memcmp(child, p1, sizeof p1) == 0);
}

BT_TEST(ga, ox_terminates_on_disjoint_parent_universes) {
    uint32_t p1[4] = {10, 11, 12, 13};
    uint32_t p2[4] = {20, 21, 22, 23};
    uint32_t child[4];
    bt_ga_ox(p1, p2, 4, 1, 3, child);
    uint32_t want[4] = {21, 11, 12, 20};
    BT_CHECK(memcmp(child, want, sizeof want) == 0);
    BT_CHECK(bt_ga_injective(child, 4));
}

BT_TEST(ga, ox_is_closed_over_injective_parents) {
    bt_rng rng;
    bt_rng_seed(&rng, 99);
    for (int iter = 0; iter < 200; iter++) {
        size_t n = 12;
        uint32_t base[40];
        for (uint32_t i = 0; i < 40; i++) {
            base[i] = i;
        }
        bt_ga_shuffle_u32(&rng, base, 40);
        uint32_t p1[12];
        memcpy(p1, base, sizeof p1);
        uint32_t base2[40];
        for (uint32_t i = 0; i < 40; i++) {
            base2[i] = i;
        }
        bt_ga_shuffle_u32(&rng, base2, 40);
        uint32_t p2[12];
        memcpy(p2, base2, sizeof p2);
        size_t cut1 = 0;
        size_t cut2 = 0;
        bt_ga_draw_cuts(&rng, n, &cut1, &cut2);
        uint32_t child[12];
        bt_ga_ox(p1, p2, n, cut1, cut2, child);
        BT_CHECK(bt_ga_injective(child, n));
        BT_CHECK(memcmp(&child[cut1], &p1[cut1], (cut2 - cut1) * sizeof(uint32_t)) == 0);
    }
}

BT_TEST(ga, mutations_preserve_injectivity) {
    bt_rng rng;
    bt_rng_seed(&rng, 7);
    for (int iter = 0; iter < 200; iter++) {
        uint32_t chrom_full[30];
        for (uint32_t i = 0; i < 30; i++) {
            chrom_full[i] = i;
        }
        bt_ga_shuffle_u32(&rng, chrom_full, 30);
        uint32_t chrom[10];
        memcpy(chrom, chrom_full, sizeof chrom);
        uint32_t swapped[10];
        memcpy(swapped, chrom, sizeof swapped);
        bt_ga_mutate_swap(swapped, 10, &rng);
        BT_CHECK(memcmp(swapped, chrom, sizeof chrom) != 0);
        BT_CHECK(bt_ga_injective(swapped, 10));
        uint32_t replaced[10];
        memcpy(replaced, chrom, sizeof replaced);
        bt_ga_mutate_replace(replaced, 10, &rng, 30);
        BT_CHECK(bt_ga_injective(replaced, 10));
        for (int i = 0; i < 10; i++) {
            BT_CHECK(replaced[i] < 30);
        }
        int changed = 0;
        for (int i = 0; i < 10; i++) {
            changed += replaced[i] != chrom[i];
        }
        BT_CHECK_EQ(changed, 1);
        int fresh_found = 0;
        for (int i = 0; i < 10; i++) {
            int present = 0;
            for (int j = 0; j < 10; j++) {
                present = present || replaced[i] == chrom[j];
            }
            if (!present) {
                fresh_found = 1;
                BT_CHECK(replaced[i] < 30);
            }
        }
        BT_CHECK(fresh_found);
    }
}

BT_TEST(ga, tournament_is_deterministic_and_picks_the_lower_fitness) {
    float fitness[8] = {5.0f, 3.0f, 8.0f, 1.0f, 9.0f, 2.0f, 7.0f, 4.0f};
    bt_rng rng;
    bt_rng_seed(&rng, 1);
    size_t winner = bt_ga_tournament(&rng, fitness, 8);
    bt_rng replay;
    bt_rng_seed(&replay, 1);
    BT_CHECK_EQ((long long)bt_ga_tournament(&replay, fitness, 8), (long long)winner);
    bt_rng all_equal;
    bt_rng_seed(&all_equal, 2);
    float flat[8] = {0};
    for (int i = 0; i < 50; i++) {
        size_t pick = bt_ga_tournament(&all_equal, flat, 8);
        BT_CHECK(pick < 8);
    }
}

BT_TEST(ga, tournament_wins_track_the_drawn_pair) {
    float fitness[8] = {5.0f, 3.0f, 8.0f, 1.0f, 9.0f, 2.0f, 7.0f, 4.0f};
    for (uint64_t seed = 0; seed < 64; seed++) {
        bt_rng rng;
        bt_rng_seed(&rng, seed);
        size_t a = (size_t)bt_rng_below(&rng, 8);
        bt_rng replay;
        bt_rng_seed(&replay, seed);
        bt_rng_next(&replay);
        size_t b = (size_t)bt_rng_below(&replay, 8);
        bt_rng draw;
        bt_rng_seed(&draw, seed);
        size_t winner = bt_ga_tournament(&draw, fitness, 8);
        size_t expected = fitness[b] < fitness[a] ? b : a;
        BT_CHECK_EQ((long long)winner, (long long)expected);
    }
}

BT_TEST(ga, fitness_parts_pin_the_documented_constants) {
    bt_ga_env *env = env_fixture_texts(3, (const char *[]){"ả", "ã", "mút"}, 3);
    uint32_t chrom[3] = {0, 1, 2};
    float parts[3];
    bt_ga_fitness_parts(env, chrom, parts);
    float hoi_nga = 0.70f;
    float expected = 0.5f * hoi_nga + 0.25f * bt_ga_confusion(&env->feats[1], &env->feats[2]);
    BT_CHECK_F64_NEAR(parts[0], expected, 1e-6);
    BT_CHECK_F64_NEAR(parts[2], 5.0 / 3.0, 1e-6);
    bt_ga_env_free(env);
}

BT_TEST(ga, flow_part_matches_the_feature_function) {
    bt_ga_env *env = env_fixture_texts(3, (const char *[]){"không", "lông", "tía"}, 3);
    uint32_t chrom[3] = {0, 1, 2};
    float parts[3];
    bt_ga_fitness_parts(env, chrom, parts);
    float expected = 0.5f * bt_ga_flow_penalty(&env->feats[0], (size_t)env->lengths[0],
                                               &env->feats[1], (size_t)env->lengths[1]);
    BT_CHECK_F64_NEAR(parts[1], expected, 1e-6);
    bt_ga_env_free(env);
}

BT_TEST(ga, run_is_deterministic_for_a_fixed_seed) {
    bt_ga_env *env = env_fixture(6);
    bt_ga_params params = small_params(20261012);
    bt_ga_run_outcome *a = bt_ga_run(env, &params);
    bt_ga_run_outcome *b = bt_ga_run(env, &params);
    BT_CHECK(a->best_count == b->best_count);
    BT_CHECK(memcmp(a->best, b->best, a->best_count * sizeof(uint32_t)) == 0);
    BT_CHECK(a->per_gen_count == b->per_gen_count);
    BT_CHECK(memcmp(a->per_gen_best, b->per_gen_best, a->per_gen_count * sizeof(float)) == 0);
    BT_CHECK(a->best_fitness == b->best_fitness);
    BT_CHECK(a->evaluations == b->evaluations);
    bt_ga_run_outcome_free(a);
    bt_ga_run_outcome_free(b);
    bt_ga_env_free(env);
}

BT_TEST(ga, run_keeps_elitism_monotone_and_valid_chromosomes) {
    bt_ga_env *env = env_fixture(6);
    bt_ga_params params = small_params(5);
    bt_ga_run_outcome *outcome = bt_ga_run(env, &params);
    BT_CHECK_EQ((long long)outcome->per_gen_count, (long long)(params.generations + 1));
    for (size_t i = 1; i < outcome->per_gen_count; i++) {
        BT_CHECK(outcome->per_gen_best[i] <= outcome->per_gen_best[i - 1] + 1.19209290e-07f);
    }
    BT_CHECK_EQ((long long)outcome->best_count, (long long)env->case_count);
    BT_CHECK(bt_ga_injective(outcome->best, outcome->best_count));
    for (size_t i = 0; i < outcome->best_count; i++) {
        BT_CHECK(outcome->best[i] < env->pool_size);
    }
    BT_CHECK_EQ((unsigned long long)outcome->evaluations,
                (unsigned long long)params.population +
                    (unsigned long long)params.generations *
                        ((unsigned long long)params.population - (unsigned long long)params.elite));
    BT_CHECK(outcome->stagnation_tail <= outcome->max_stagnation);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

BT_TEST(ga, config_params_read_the_hub) {
    const bt_json_value *config = bt_config_load();
    bt_ga_params params;
    char err[BT_GA_ERR_MAX];
    if (config == NULL || !bt_ga_config_params(config, &params, err)) {
        BT_ABORT("ga section: %s", err);
    }
    BT_CHECK_EQ((unsigned long long)params.seed, 20261012ULL);
    BT_CHECK_EQ((long long)params.population, 200LL);
    BT_CHECK_EQ((long long)params.generations, 500LL);
    BT_CHECK_EQ((long long)params.elite, 2LL);
    BT_CHECK_F64_NEAR(params.swap, 0.15, 1e-9);
    BT_CHECK_F64_NEAR(params.replace, 0.05, 1e-9);
    BT_CHECK_F64_NEAR(params.w_conf, 1.0, 1e-6);
    BT_CHECK_F64_NEAR(params.w_flow, 0.5, 1e-6);
    BT_CHECK_F64_NEAR(params.w_cplx, 0.1, 1e-6);
}

BT_TEST(ga, build_env_rejects_undersized_pools) {
    bt_ga_features feats[2];
    feats[0] = bt_ga_parse_or_abort("ma");
    feats[1] = bt_ga_parse_or_abort("má");
    float *lengths = calloc(2, sizeof *lengths);
    lengths[0] = 2.0f;
    lengths[1] = 2.0f;
    bt_ga_env env;
    char err[BT_GA_ERR_MAX];
    BT_CHECK(
        !bt_ga_build_env(2, lengths, feats, 2, bt_ga_case_count(), NULL, 0, NULL, 0, &env, err));
    free(lengths);
}

static void run_with_elite_equal_to_population(void) {
    bt_ga_env *env = env_fixture_texts(2, (const char *[]){"ma", "má", "mù"}, 3);
    bt_ga_params params = small_params(1);
    params.elite = params.population;
    bt_ga_run_outcome *outcome = bt_ga_run(env, &params);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

static void run_with_population_below_two(void) {
    bt_ga_env *env = env_fixture_texts(2, (const char *[]){"ma", "má", "mù"}, 3);
    bt_ga_params params = small_params(1);
    params.population = 1;
    params.elite = 0;
    bt_ga_run_outcome *outcome = bt_ga_run(env, &params);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

static void run_with_zero_elite(void) {
    bt_ga_env *env = env_fixture_texts(2, (const char *[]){"ma", "má", "mù"}, 3);
    bt_ga_params params = small_params(1);
    params.elite = 0;
    bt_ga_run_outcome *outcome = bt_ga_run(env, &params);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

BT_TEST(ga, run_rejects_elite_equal_to_population) {
    bt_ga_death_expect("run_rejects_elite_equal_to_population", "ga run",
                       run_with_elite_equal_to_population);
}

BT_TEST(ga, run_rejects_population_below_two) {
    bt_ga_death_expect("run_rejects_population_below_two", "ga run", run_with_population_below_two);
}

BT_TEST(ga, run_rejects_zero_elite) {
    bt_ga_death_expect("run_rejects_zero_elite", "ga run", run_with_zero_elite);
}

BT_TEST(ga, run_with_zero_generations_returns_the_initial_best) {
    bt_ga_env *env = env_fixture_texts(3, (const char *[]){"ma", "má", "mù", "mút"}, 4);
    bt_ga_params params = small_params(9);
    params.generations = 0;
    bt_ga_run_outcome *outcome = bt_ga_run(env, &params);
    BT_CHECK_EQ((long long)outcome->per_gen_count, 1LL);
    BT_CHECK_EQ((long long)outcome->best_generation, 0LL);
    BT_CHECK_EQ((long long)outcome->stagnation_tail, 0LL);
    BT_CHECK_EQ((long long)outcome->max_stagnation, 0LL);
    BT_CHECK_EQ((unsigned long long)outcome->evaluations, (unsigned long long)params.population);
    BT_CHECK(bt_ga_injective(outcome->best, outcome->best_count));
    for (size_t i = 0; i < outcome->best_count; i++) {
        BT_CHECK(outcome->best[i] < env->pool_size);
    }
    BT_CHECK(outcome->best_fitness == outcome->per_gen_best[0]);
    float recomposed = params.w_conf * outcome->best_parts[0] +
                       params.w_flow * outcome->best_parts[1] +
                       params.w_cplx * outcome->best_parts[2];
    BT_CHECK_F64_NEAR(outcome->best_fitness, recomposed, 1e-5);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

BT_TEST(ga, run_survives_extreme_mutation_gates) {
    bt_ga_env *env =
        env_fixture_texts(4, (const char *[]){"ma", "má", "mù", "mút", "mơ", "không"}, 6);
    bt_ga_params always = small_params(21);
    always.swap = 1.0;
    always.replace = 1.0;
    bt_ga_run_outcome *outcome = bt_ga_run(env, &always);
    BT_CHECK(bt_ga_injective(outcome->best, outcome->best_count));
    for (size_t i = 0; i < outcome->best_count; i++) {
        BT_CHECK(outcome->best[i] < env->pool_size);
    }
    BT_CHECK_EQ((long long)outcome->per_gen_count, (long long)(always.generations + 1));
    bt_ga_run_outcome_free(outcome);
    bt_ga_params never = small_params(21);
    never.swap = 0.0;
    never.replace = 0.0;
    outcome = bt_ga_run(env, &never);
    BT_CHECK(bt_ga_injective(outcome->best, outcome->best_count));
    BT_CHECK_EQ((unsigned long long)outcome->evaluations,
                (unsigned long long)(never.population +
                                     never.generations * (never.population - never.elite)));
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

BT_TEST(ga, run_reports_the_best_generation_consistently) {
    bt_ga_env *env = env_fixture_texts(
        5, (const char *[]){"ma", "má", "mù", "mút", "mơ", "không", "lông", "tía"}, 8);
    bt_ga_params params = small_params(77);
    bt_ga_run_outcome *outcome = bt_ga_run(env, &params);
    BT_CHECK(outcome->best_generation < outcome->per_gen_count);
    BT_CHECK(outcome->per_gen_best[outcome->best_generation] == outcome->best_fitness);
    float direct = bt_ga_fitness(&params, env, outcome->best);
    BT_CHECK_F64_NEAR(direct, outcome->best_fitness, 1e-5);
    bt_ga_run_outcome *replay = bt_ga_run(env, &params);
    BT_CHECK(memcmp(replay->best, outcome->best, outcome->best_count * sizeof(uint32_t)) == 0);
    bt_ga_run_outcome_free(replay);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

BT_TEST(ga, two_gene_operators_have_pinned_outcomes) {
    bt_rng rng;
    bt_rng_seed(&rng, 3);
    for (int iter = 0; iter < 25; iter++) {
        uint32_t chrom[2] = {5, 9};
        bt_ga_mutate_swap(chrom, 2, &rng);
        uint32_t want[2] = {9, 5};
        BT_CHECK(memcmp(chrom, want, sizeof want) == 0);
        uint32_t zero[1] = {0};
        bt_ga_mutate_replace(zero, 1, &rng, 2);
        BT_CHECK_EQ((long long)zero[0], 1LL);
        uint32_t one[1] = {1};
        bt_ga_mutate_replace(one, 1, &rng, 2);
        BT_CHECK_EQ((long long)one[0], 0LL);
    }
}

static bt_json_value *config_from_literal(const char *text) {
    bt_json_value *config = NULL;
    bt_json_error error;
    if (bt_json_parse(text, strlen(text), &config, &error) != 0) {
        BT_ABORT("config literal does not parse: %s", error.message);
    }
    return config;
}

BT_TEST(ga, config_ga_params_reads_the_documented_shape) {
    bt_json_value *config = config_from_literal(
        "{\"ga\":{\"tournament\":2,\"seed\":7,\"population\":10,\"generations\":3,\"elite\":2,"
        "\"mutation\":{\"swap\":0.1,\"replace\":0.2},"
        "\"weights\":{\"confusion\":1.0,\"flow\":0.5,\"complexity\":0.25}}}");
    bt_ga_params params;
    char err[BT_GA_ERR_MAX];
    if (!bt_ga_config_params(config, &params, err)) {
        BT_ABORT("valid config rejected: %s", err);
    }
    BT_CHECK_EQ((unsigned long long)params.seed, 7ULL);
    BT_CHECK_EQ((long long)params.population, 10LL);
    BT_CHECK_EQ((long long)params.generations, 3LL);
    BT_CHECK_EQ((long long)params.elite, 2LL);
    BT_CHECK_F64_NEAR(params.swap, 0.1, 1e-9);
    BT_CHECK_F64_NEAR(params.replace, 0.2, 1e-9);
    BT_CHECK_F64_NEAR(params.w_conf, 1.0, 1e-6);
    BT_CHECK_F64_NEAR(params.w_flow, 0.5, 1e-6);
    BT_CHECK_F64_NEAR(params.w_cplx, 0.25, 1e-6);
    bt_json_free(config);
}

BT_TEST(ga, config_ga_params_rejects_foreign_tournament_and_null_sections) {
    const char *base =
        "{\"ga\":{\"tournament\":%s,\"seed\":%s,\"population\":%s,\"generations\":%s,"
        "\"elite\":%s,\"mutation\":{\"swap\":%s,\"replace\":%s},"
        "\"weights\":{\"confusion\":%s,\"flow\":%s,\"complexity\":%s}}}";
    char foreign[512];
    snprintf(foreign, sizeof foreign, base, "3", "1", "4", "1", "1", "0.0", "0.0", "1.0", "0.0",
             "0.0");
    bt_json_value *config = config_from_literal(foreign);
    bt_ga_params params;
    char err[BT_GA_ERR_MAX];
    BT_CHECK(!bt_ga_config_params(config, &params, err));
    BT_CHECK(strstr(err, "tournament") != NULL);
    bt_json_free(config);
    static const char *const pointers[9] = {
        "/ga/seed",
        "/ga/population",
        "/ga/generations",
        "/ga/elite",
        "/ga/mutation/swap",
        "/ga/mutation/replace",
        "/ga/weights/confusion",
        "/ga/weights/flow",
        "/ga/weights/complexity",
    };
    for (int p = 0; p < 9; p++) {
        const char *slots[10] = {"2", "1", "4", "1", "1", "0.0", "0.0", "1.0", "0.0", "0.0"};
        const char *nulled[10];
        for (int s = 0; s < 10; s++) {
            nulled[s] = s == p + 1 ? "null" : slots[s];
        }
        char broken[512];
        snprintf(broken, sizeof broken, base, nulled[0], nulled[1], nulled[2], nulled[3], nulled[4],
                 nulled[5], nulled[6], nulled[7], nulled[8], nulled[9]);
        bt_json_value *cfg = config_from_literal(broken);
        BT_CHECK(!bt_ga_config_params(cfg, &params, err));
        BT_CHECK(strstr(err, pointers[p]) != NULL);
        bt_json_free(cfg);
    }
}
