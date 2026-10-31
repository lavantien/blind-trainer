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
        if ((*p & 0xC0U) != 0x80U) {
            count++;
        }
    }
    return count;
}

static bt_ga_env *upgrade_env(size_t case_count) {
    bt_ga_features *feats = calloc(8, sizeof *feats);
    float *lengths = calloc(8, sizeof *lengths);
    for (size_t i = 0; i < 8; i++) {
        feats[i] = bt_ga_parse_or_abort(POOL8[i]);
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
        BT_ABORT("build_env: %s", err);
    }
    return env;
}

static bt_ga_params upgrade_params(uint64_t seed, size_t pop, size_t gens, size_t elite) {
    bt_ga_params params;
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

static bt_ga_upgrades no_upgrades(void) {
    bt_ga_upgrades upgrades;
    memset(&upgrades, 0, sizeof upgrades);
    return upgrades;
}

static uint32_t f32_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}

static void check_outcome_pins(const bt_ga_run_outcome *outcome, const uint32_t *genes,
                               size_t gene_count, uint32_t best_fitness_bits,
                               const uint32_t *per_gen, size_t per_gen_count,
                               size_t best_generation, uint32_t stagnation_tail,
                               uint32_t max_stagnation, uint64_t evaluations) {
    BT_CHECK_EQ(outcome->best_count, gene_count);
    for (size_t i = 0; i < gene_count; i++) {
        BT_CHECK_EQ(outcome->best[i], genes[i]);
    }
    BT_CHECK_EQ(f32_bits(outcome->best_fitness), best_fitness_bits);
    BT_CHECK_EQ(outcome->per_gen_count, per_gen_count);
    for (size_t i = 0; i < per_gen_count; i++) {
        BT_CHECK_EQ(f32_bits(outcome->per_gen_best[i]), per_gen[i]);
    }
    BT_CHECK_EQ(outcome->best_generation, best_generation);
    BT_CHECK_EQ(outcome->stagnation_tail, stagnation_tail);
    BT_CHECK_EQ(outcome->max_stagnation, max_stagnation);
    BT_CHECK_EQ(outcome->evaluations, evaluations);
}

BT_TEST(ga_upgrade, hang_watchdog_arms_for_this_binary) {
    bt_ga_arm_hang_watchdog();
}

BT_TEST(ga_upgrade, zeroed_upgrades_reproduce_the_legacy_run) {
    bt_ga_env *env = upgrade_env(6);
    bt_ga_params params = upgrade_params(424242, 8, 6, 2);
    bt_ga_upgrades zero = no_upgrades();
    bt_ga_run_outcome *legacy = bt_ga_run(env, &params);
    bt_ga_run_outcome *upgraded = bt_ga_run_ex(env, &params, &zero);
    BT_CHECK_EQ(legacy->best_count, upgraded->best_count);
    BT_CHECK(memcmp(legacy->best, upgraded->best, legacy->best_count * sizeof(uint32_t)) == 0);
    BT_CHECK_EQ(legacy->per_gen_count, upgraded->per_gen_count);
    for (size_t i = 0; i < legacy->per_gen_count; i++) {
        BT_CHECK_EQ(f32_bits(legacy->per_gen_best[i]), f32_bits(upgraded->per_gen_best[i]));
    }
    BT_CHECK_EQ(f32_bits(legacy->best_fitness), f32_bits(upgraded->best_fitness));
    BT_CHECK_EQ(legacy->best_generation, upgraded->best_generation);
    BT_CHECK_EQ(legacy->stagnation_tail, upgraded->stagnation_tail);
    BT_CHECK_EQ(legacy->max_stagnation, upgraded->max_stagnation);
    BT_CHECK_EQ(legacy->evaluations, upgraded->evaluations);
    bt_ga_run_outcome_free(upgraded);
    bt_ga_run_outcome_free(legacy);
    bt_ga_env_free(env);
}

BT_TEST(ga_upgrade, legacy_run_pins_the_reference_constants) {
    bt_ga_env *env = upgrade_env(6);
    bt_ga_params params = upgrade_params(424242, 8, 6, 2);
    bt_ga_upgrades zero = no_upgrades();
    bt_ga_run_outcome *outcome = bt_ga_run_ex(env, &params, &zero);
    const uint32_t genes[6] = {4, 7, 2, 5, 6, 0};
    const uint32_t per_gen[7] = {
        0x3ea06faf, 0x3e91152a, 0x3e91152a, 0x3e91152a, 0x3e91152a, 0x3e91152a, 0x3e91152a,
    };
    check_outcome_pins(outcome, genes, 6, 0x3e91152a, per_gen, 7, 1, 5, 5, 44);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

BT_TEST(ga_upgrade, adaptive_mutation_pins_the_reference_constants) {
    bt_ga_env *env = upgrade_env(6);
    bt_ga_params params = upgrade_params(424242, 8, 6, 2);
    bt_ga_upgrades adaptive = no_upgrades();
    adaptive.stagnation_threshold = 1;
    adaptive.swap_scale = 0.5;
    adaptive.replace_scale = 0.5;
    bt_ga_run_outcome *outcome = bt_ga_run_ex(env, &params, &adaptive);
    const uint32_t genes[6] = {4, 7, 1, 0, 6, 5};
    const uint32_t per_gen[7] = {
        0x3ea06faf, 0x3e91152a, 0x3e91152a, 0x3e91152a, 0x3e91152a, 0x3e5df188, 0x3e5df188,
    };
    check_outcome_pins(outcome, genes, 6, 0x3e5df188, per_gen, 7, 5, 1, 3, 44);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

BT_TEST(ga_upgrade, adaptive_scales_of_one_reproduce_the_legacy_stream) {
    bt_ga_env *env = upgrade_env(6);
    bt_ga_params params = upgrade_params(424242, 8, 6, 2);
    bt_ga_upgrades adaptive = no_upgrades();
    adaptive.stagnation_threshold = 1;
    adaptive.swap_scale = 1.0;
    adaptive.replace_scale = 1.0;
    bt_ga_run_outcome *outcome = bt_ga_run_ex(env, &params, &adaptive);
    const uint32_t genes[6] = {4, 7, 2, 5, 6, 0};
    const uint32_t per_gen[7] = {
        0x3ea06faf, 0x3e91152a, 0x3e91152a, 0x3e91152a, 0x3e91152a, 0x3e91152a, 0x3e91152a,
    };
    check_outcome_pins(outcome, genes, 6, 0x3e91152a, per_gen, 7, 1, 5, 5, 44);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

BT_TEST(ga_upgrade, island_ring_run_pins_the_reference_constants) {
    bt_ga_env *env = upgrade_env(6);
    bt_ga_params params = upgrade_params(424242, 8, 6, 2);
    bt_ga_upgrades islands = no_upgrades();
    islands.island_count = 3;
    islands.migrate_every = 2;
    islands.migrate_count = 1;
    bt_ga_run_outcome *outcome = bt_ga_run_ex(env, &params, &islands);
    const uint32_t genes[6] = {7, 5, 1, 4, 6, 0};
    const uint32_t per_gen[7] = {
        0x3ea06faf, 0x3e8004ea, 0x3e8004ea, 0x3e5e0a1c, 0x3e5e0a1c, 0x3e5e0a1c, 0x3e5e0a1c,
    };
    check_outcome_pins(outcome, genes, 6, 0x3e5e0a1c, per_gen, 7, 3, 3, 3, 132);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

BT_TEST(ga_upgrade, island_runs_are_deterministic_for_a_fixed_seed) {
    bt_ga_env *env = upgrade_env(6);
    bt_ga_params params = upgrade_params(424242, 8, 6, 2);
    bt_ga_upgrades islands = no_upgrades();
    islands.island_count = 3;
    islands.migrate_every = 2;
    islands.migrate_count = 1;
    bt_ga_run_outcome *a = bt_ga_run_ex(env, &params, &islands);
    bt_ga_run_outcome *b = bt_ga_run_ex(env, &params, &islands);
    BT_CHECK(memcmp(a->best, b->best, a->best_count * sizeof(uint32_t)) == 0);
    for (size_t i = 0; i < a->per_gen_count; i++) {
        BT_CHECK_EQ(f32_bits(a->per_gen_best[i]), f32_bits(b->per_gen_best[i]));
    }
    BT_CHECK_EQ(f32_bits(a->best_fitness), f32_bits(b->best_fitness));
    BT_CHECK_EQ(a->evaluations, b->evaluations);
    bt_ga_run_outcome_free(b);
    bt_ga_run_outcome_free(a);
    bt_ga_env_free(env);
}

BT_TEST(ga_upgrade, migration_costs_no_evaluations_and_keeps_elitism_monotone) {
    bt_ga_env *env = upgrade_env(6);
    bt_ga_params params = upgrade_params(424242, 8, 6, 2);
    bt_ga_upgrades islands = no_upgrades();
    islands.island_count = 3;
    islands.migrate_every = 2;
    islands.migrate_count = 1;
    bt_ga_upgrades isolated = no_upgrades();
    isolated.island_count = 3;
    bt_ga_run_outcome *migrated = bt_ga_run_ex(env, &params, &islands);
    bt_ga_run_outcome *isolated_outcome = bt_ga_run_ex(env, &params, &isolated);
    uint64_t want_evals = 3ULL * 8ULL + 6ULL * 3ULL * (8ULL - 2ULL);
    BT_CHECK_EQ(migrated->evaluations, want_evals);
    BT_CHECK_EQ(isolated_outcome->evaluations, want_evals);
    for (size_t i = 1; i < migrated->per_gen_count; i++) {
        BT_CHECK(migrated->per_gen_best[i] <= migrated->per_gen_best[i - 1] + 1.19209290e-07F);
    }
    BT_CHECK(migrated->best_fitness <= migrated->per_gen_best[0] + 1.19209290e-07F);
    bt_ga_run_outcome_free(isolated_outcome);
    bt_ga_run_outcome_free(migrated);
    bt_ga_env_free(env);
}

BT_TEST(ga_upgrade, diversity_restart_pins_the_reference_constants) {
    bt_ga_env *env = upgrade_env(6);
    bt_ga_params params = upgrade_params(2, 4, 6, 3);
    params.swap = 0.0;
    params.replace = 0.0;
    bt_ga_upgrades restart = no_upgrades();
    restart.diversity_threshold = 0.9;
    restart.inject_fraction = 0.5;
    bt_ga_run_outcome *outcome = bt_ga_run_ex(env, &params, &restart);
    const uint32_t genes[6] = {3, 5, 6, 7, 1, 0};
    const uint32_t per_gen[7] = {
        0x3ed779ec, 0x3ed779ec, 0x3ed779ec, 0x3ed779ec, 0x3eb81cf2, 0x3e9799e0, 0x3e9799e0,
    };
    check_outcome_pins(outcome, genes, 6, 0x3e9799e0, per_gen, 7, 5, 1, 3, 14);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

BT_TEST(ga_upgrade, flat_fitness_restart_accounts_every_replacement) {
    bt_ga_env *env = upgrade_env(3);
    bt_ga_params params = upgrade_params(14, 2, 6, 1);
    params.swap = 0.0;
    params.replace = 0.0;
    params.w_conf = 0.0F;
    params.w_flow = 0.0F;
    params.w_cplx = 0.0F;
    bt_ga_upgrades restart = no_upgrades();
    restart.diversity_threshold = 0.75;
    restart.inject_fraction = 0.5;
    bt_ga_run_outcome *outcome = bt_ga_run_ex(env, &params, &restart);
    const uint32_t genes[3] = {1, 2, 5};
    const uint32_t per_gen[7] = {0, 0, 0, 0, 0, 0, 0};
    check_outcome_pins(outcome, genes, 3, 0, per_gen, 7, 0, 6, 6, 11);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

BT_TEST(ga_upgrade, elite_fraction_policy_reproduces_count_policy_at_the_same_elite) {
    bt_ga_env *env = upgrade_env(6);
    bt_ga_params params = upgrade_params(424242, 8, 6, 2);
    bt_ga_upgrades fraction = no_upgrades();
    fraction.elite_fraction_policy = 1;
    fraction.elite_fraction = 0.25;
    bt_ga_run_outcome *outcome = bt_ga_run_ex(env, &params, &fraction);
    const uint32_t genes[6] = {4, 7, 2, 5, 6, 0};
    const uint32_t per_gen[7] = {
        0x3ea06faf, 0x3e91152a, 0x3e91152a, 0x3e91152a, 0x3e91152a, 0x3e91152a, 0x3e91152a,
    };
    check_outcome_pins(outcome, genes, 6, 0x3e91152a, per_gen, 7, 1, 5, 5, 44);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

static bt_ga_island *ring_fixture(size_t pop, size_t case_count) {
    bt_ga_island *islands = calloc(3, sizeof *islands);
    static const uint32_t genes[12][3] = {
        {1, 2, 3}, {4, 5, 6}, {7, 0, 1}, {2, 3, 4}, {7, 6, 5}, {4, 3, 2},
        {1, 0, 7}, {6, 5, 4}, {0, 1, 7}, {2, 3, 6}, {4, 5, 0}, {6, 7, 1},
    };
    static const float scores[12] = {10, 20, 30, 40, 11, 22, 33, 44, 15, 25, 35, 45};
    for (size_t isl = 0; isl < 3; isl++) {
        islands[isl].population = calloc(pop, sizeof *islands[isl].population);
        islands[isl].scores = calloc(pop, sizeof *islands[isl].scores);
        islands[isl].order = calloc(pop, sizeof *islands[isl].order);
        for (size_t i = 0; i < pop; i++) {
            islands[isl].population[i] = malloc(case_count * sizeof(uint32_t));
            memcpy(islands[isl].population[i], genes[isl * pop + i], case_count * sizeof(uint32_t));
            islands[isl].scores[i] = scores[isl * pop + i];
            islands[isl].order[i] = i;
        }
    }
    return islands;
}

static void ring_fixture_free(bt_ga_island *islands, size_t pop) {
    for (size_t isl = 0; isl < 3; isl++) {
        for (size_t i = 0; i < pop; i++) {
            free(islands[isl].population[i]);
        }
        free(islands[isl].population);
        free(islands[isl].scores);
        free(islands[isl].order);
    }
    free(islands);
}

BT_TEST(ga_upgrade, ring_migrate_moves_each_top_into_the_next_island_worst) {
    size_t pop = 4;
    size_t case_count = 3;
    bt_ga_island *islands = ring_fixture(pop, case_count);
    uint32_t **staging = calloc(3, sizeof *staging);
    float *staging_scores = calloc(3, sizeof *staging_scores);
    for (size_t i = 0; i < 3; i++) {
        staging[i] = malloc(case_count * sizeof(uint32_t));
    }
    bt_ga_ring_migrate(islands, 3, pop, 1, case_count, staging, staging_scores);
    BT_CHECK(memcmp(islands[1].population[3], (uint32_t[3]){1, 2, 3}, 3 * sizeof(uint32_t)) == 0);
    BT_CHECK_EQ(islands[1].scores[3], 10.0F);
    BT_CHECK(memcmp(islands[2].population[3], (uint32_t[3]){7, 6, 5}, 3 * sizeof(uint32_t)) == 0);
    BT_CHECK_EQ(islands[2].scores[3], 11.0F);
    BT_CHECK(memcmp(islands[0].population[3], (uint32_t[3]){0, 1, 7}, 3 * sizeof(uint32_t)) == 0);
    BT_CHECK_EQ(islands[0].scores[3], 15.0F);
    const size_t want_order0[4] = {0, 3, 1, 2};
    const size_t want_order1[4] = {3, 0, 1, 2};
    const size_t want_order2[4] = {3, 0, 1, 2};
    for (size_t i = 0; i < 4; i++) {
        BT_CHECK_EQ(islands[0].order[i], want_order0[i]);
        BT_CHECK_EQ(islands[1].order[i], want_order1[i]);
        BT_CHECK_EQ(islands[2].order[i], want_order2[i]);
    }
    for (size_t i = 0; i < 3; i++) {
        free(staging[i]);
    }
    free(staging);
    free(staging_scores);
    ring_fixture_free(islands, pop);
}

static bt_json_value *upgrade_config_from_literal(const char *text) {
    bt_json_value *config = NULL;
    bt_json_error error;
    if (bt_json_parse(text, strlen(text), &config, &error) != 0) {
        BT_ABORT("config literal does not parse: %s", error.message);
    }
    return config;
}

BT_TEST(ga_upgrade, config_upgrades_reads_the_hub_defaults) {
    const bt_json_value *config = bt_config_load();
    bt_ga_upgrades upgrades;
    char err[BT_GA_ERR_MAX];
    if (config == NULL || !bt_ga_config_upgrades(config, &upgrades, err)) {
        BT_ABORT("ga section: %s", err);
    }
    BT_CHECK_EQ(upgrades.island_count, (size_t)1);
    BT_CHECK_EQ(upgrades.migrate_every, (size_t)0);
    BT_CHECK_EQ(upgrades.migrate_count, (size_t)1);
    BT_CHECK_EQ(upgrades.stagnation_threshold, (uint32_t)0);
    BT_CHECK_F64_NEAR(upgrades.swap_scale, 1.0, 1e-9);
    BT_CHECK_F64_NEAR(upgrades.replace_scale, 1.0, 1e-9);
    BT_CHECK_EQ(upgrades.elite_fraction_policy, 0);
    BT_CHECK_F64_NEAR(upgrades.elite_fraction, 0.0, 1e-9);
    BT_CHECK_F64_NEAR(upgrades.diversity_threshold, 0.0, 1e-9);
    BT_CHECK_F64_NEAR(upgrades.inject_fraction, 0.5, 1e-9);
}

BT_TEST(ga_upgrade, config_upgrades_defaults_every_missing_key_to_disabled) {
    bt_json_value *config = upgrade_config_from_literal("{\"ga\":{\"seed\":7}}");
    bt_ga_upgrades upgrades;
    char err[BT_GA_ERR_MAX];
    if (!bt_ga_config_upgrades(config, &upgrades, err)) {
        BT_ABORT("minimal ga section rejected: %s", err);
    }
    BT_CHECK_EQ(upgrades.island_count, (size_t)1);
    BT_CHECK_EQ(upgrades.migrate_every, (size_t)0);
    BT_CHECK_EQ(upgrades.stagnation_threshold, (uint32_t)0);
    BT_CHECK_EQ(upgrades.elite_fraction_policy, 0);
    BT_CHECK_F64_NEAR(upgrades.diversity_threshold, 0.0, 1e-9);
    bt_json_free(config);
}

BT_TEST(ga_upgrade, config_upgrades_parses_every_key_from_a_literal) {
    bt_json_value *config = upgrade_config_from_literal(
        "{\"ga\":{\"islands\":{\"count\":4,\"migrate_every\":3,\"migrate_count\":2},"
        "\"adaptive_mutation\":{\"stagnation_threshold\":5,\"swap_scale\":1.5,"
        "\"replace_scale\":0.25},\"elite_carry\":{\"policy\":\"fraction\",\"fraction\":0.3},"
        "\"restart\":{\"diversity_threshold\":0.2,\"inject_fraction\":0.75}}}");
    bt_ga_upgrades upgrades;
    char err[BT_GA_ERR_MAX];
    if (!bt_ga_config_upgrades(config, &upgrades, err)) {
        BT_ABORT("valid upgrades rejected: %s", err);
    }
    BT_CHECK_EQ(upgrades.island_count, (size_t)4);
    BT_CHECK_EQ(upgrades.migrate_every, (size_t)3);
    BT_CHECK_EQ(upgrades.migrate_count, (size_t)2);
    BT_CHECK_EQ(upgrades.stagnation_threshold, (uint32_t)5);
    BT_CHECK_F64_NEAR(upgrades.swap_scale, 1.5, 1e-9);
    BT_CHECK_F64_NEAR(upgrades.replace_scale, 0.25, 1e-9);
    BT_CHECK_EQ(upgrades.elite_fraction_policy, 1);
    BT_CHECK_F64_NEAR(upgrades.elite_fraction, 0.3, 1e-9);
    BT_CHECK_F64_NEAR(upgrades.diversity_threshold, 0.2, 1e-9);
    BT_CHECK_F64_NEAR(upgrades.inject_fraction, 0.75, 1e-9);
    bt_json_free(config);
}

BT_TEST(ga_upgrade, config_upgrades_rejects_out_of_range_values) {
    static const char *const broken[] = {
        "{\"ga\":{\"islands\":{\"count\":0}}}",
        "{\"ga\":{\"islands\":{\"count\":2,\"migrate_every\":1,\"migrate_count\":0}}}",
        "{\"ga\":{\"islands\":{\"count\":1,\"migrate_every\":1,\"migrate_count\":1}}}",
        "{\"ga\":{\"adaptive_mutation\":{\"stagnation_threshold\":2,\"swap_scale\":-1.0}}}",
        "{\"ga\":{\"adaptive_mutation\":{\"stagnation_threshold\":2,\"replace_scale\":-0.5}}}",
        "{\"ga\":{\"elite_carry\":{\"policy\":\"weird\"}}}",
        "{\"ga\":{\"elite_carry\":{\"policy\":\"fraction\",\"fraction\":1.0}}}",
        "{\"ga\":{\"elite_carry\":{\"policy\":\"fraction\",\"fraction\":-0.1}}}",
        "{\"ga\":{\"restart\":{\"diversity_threshold\":1.5}}}",
        "{\"ga\":{\"restart\":{\"diversity_threshold\":-0.1}}}",
        "{\"ga\":{\"restart\":{\"inject_fraction\":1.5}}}",
        "{\"ga\":{\"restart\":{\"inject_fraction\":-0.1}}}",
    };
    static const char *const pointers[] = {
        "ga.islands.count",
        "ga.islands.migrate_count",
        "ga.islands.count",
        "ga.adaptive_mutation.swap_scale",
        "ga.adaptive_mutation.replace_scale",
        "ga.elite_carry.policy",
        "ga.elite_carry.fraction",
        "ga.elite_carry.fraction",
        "ga.restart.diversity_threshold",
        "ga.restart.diversity_threshold",
        "ga.restart.inject_fraction",
        "ga.restart.inject_fraction",
    };
    for (size_t i = 0; i < sizeof broken / sizeof broken[0]; i++) {
        bt_json_value *config = upgrade_config_from_literal(broken[i]);
        bt_ga_upgrades upgrades;
        char err[BT_GA_ERR_MAX];
        BT_CHECK(!bt_ga_config_upgrades(config, &upgrades, err));
        BT_CHECK(strstr(err, pointers[i]) != NULL);
        bt_json_free(config);
    }
}

static void run_ex_with_migration_on_one_island(void) {
    bt_ga_env *env = upgrade_env(6);
    bt_ga_params params = upgrade_params(424242, 8, 6, 2);
    bt_ga_upgrades islands = no_upgrades();
    islands.island_count = 1;
    islands.migrate_every = 2;
    islands.migrate_count = 1;
    bt_ga_run_outcome *outcome = bt_ga_run_ex(env, &params, &islands);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

static void run_ex_with_zero_migrate_count(void) {
    bt_ga_env *env = upgrade_env(6);
    bt_ga_params params = upgrade_params(424242, 8, 6, 2);
    bt_ga_upgrades islands = no_upgrades();
    islands.island_count = 2;
    islands.migrate_every = 2;
    islands.migrate_count = 0;
    bt_ga_run_outcome *outcome = bt_ga_run_ex(env, &params, &islands);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

static void run_ex_with_migration_starving_the_children(void) {
    bt_ga_env *env = upgrade_env(6);
    bt_ga_params params = upgrade_params(424242, 8, 6, 2);
    bt_ga_upgrades islands = no_upgrades();
    islands.island_count = 2;
    islands.migrate_every = 1;
    islands.migrate_count = 7;
    bt_ga_run_outcome *outcome = bt_ga_run_ex(env, &params, &islands);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

static void run_ex_with_negative_scale(void) {
    bt_ga_env *env = upgrade_env(6);
    bt_ga_params params = upgrade_params(424242, 8, 6, 2);
    bt_ga_upgrades adaptive = no_upgrades();
    adaptive.stagnation_threshold = 1;
    adaptive.swap_scale = -1.0;
    bt_ga_run_outcome *outcome = bt_ga_run_ex(env, &params, &adaptive);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

static void run_ex_with_full_elite_fraction(void) {
    bt_ga_env *env = upgrade_env(6);
    bt_ga_params params = upgrade_params(424242, 8, 6, 2);
    bt_ga_upgrades fraction = no_upgrades();
    fraction.elite_fraction_policy = 1;
    fraction.elite_fraction = 1.0;
    bt_ga_run_outcome *outcome = bt_ga_run_ex(env, &params, &fraction);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

static void run_ex_with_zero_elite_fraction(void) {
    bt_ga_env *env = upgrade_env(6);
    bt_ga_params params = upgrade_params(424242, 8, 6, 2);
    bt_ga_upgrades fraction = no_upgrades();
    fraction.elite_fraction_policy = 1;
    fraction.elite_fraction = 0.0625;
    bt_ga_run_outcome *outcome = bt_ga_run_ex(env, &params, &fraction);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

static void run_ex_with_out_of_range_diversity_threshold(void) {
    bt_ga_env *env = upgrade_env(6);
    bt_ga_params params = upgrade_params(424242, 8, 6, 2);
    bt_ga_upgrades restart = no_upgrades();
    restart.diversity_threshold = 1.5;
    restart.inject_fraction = 0.5;
    bt_ga_run_outcome *outcome = bt_ga_run_ex(env, &params, &restart);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

BT_TEST(ga_upgrade, run_ex_rejects_contradictory_upgrade_structs) {
    bt_ga_death_expect("run_ex_with_migration_on_one_island",
                       "migration requires at least 2 islands",
                       run_ex_with_migration_on_one_island);
    bt_ga_death_expect("run_ex_with_zero_migrate_count", "migration requires a positive migrate",
                       run_ex_with_zero_migrate_count);
    bt_ga_death_expect("run_ex_with_migration_starving_the_children",
                       "migrate count plus elite must stay at or below",
                       run_ex_with_migration_starving_the_children);
    bt_ga_death_expect("run_ex_with_negative_scale",
                       "adaptive scales must be finite and non negative",
                       run_ex_with_negative_scale);
    bt_ga_death_expect("run_ex_with_full_elite_fraction", "elite fraction must stay within 0 to 1",
                       run_ex_with_full_elite_fraction);
    bt_ga_death_expect("run_ex_with_zero_elite_fraction", "elite must be at least 1",
                       run_ex_with_zero_elite_fraction);
    bt_ga_death_expect("run_ex_with_out_of_range_diversity_threshold",
                       "restart thresholds must stay within 0 to 1",
                       run_ex_with_out_of_range_diversity_threshold);
}

BT_TEST(ga_upgrade, forced_keep_restart_pins_the_reference_constants) {
    bt_ga_env *env = upgrade_env(6);
    bt_ga_params params = upgrade_params(0, 8, 6, 7);
    params.swap = 0.0;
    params.replace = 0.0;
    params.w_conf = 0.0F;
    params.w_flow = 0.0F;
    params.w_cplx = 0.0F;
    bt_ga_upgrades restart = no_upgrades();
    restart.diversity_threshold = 0.9;
    restart.inject_fraction = 0.1;
    bt_ga_run_outcome *outcome = bt_ga_run_ex(env, &params, &restart);
    const uint32_t genes[6] = {0, 2, 4, 3, 7, 5};
    const uint32_t per_gen[7] = {
        0, 0, 0, 0, 0, 0, 0,
    };
    check_outcome_pins(outcome, genes, 6, 0, per_gen, 7, 0, 6, 6, 21);
    for (size_t i = 1; i < outcome->per_gen_count; i++) {
        BT_CHECK(outcome->per_gen_best[i] <= outcome->per_gen_best[i - 1] + 1.19209290e-07F);
    }
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

BT_TEST(ga_upgrade, adaptive_gate_clamp_stays_deterministic_at_the_ceiling) {
    bt_ga_env *env = upgrade_env(6);
    bt_ga_params params = upgrade_params(424242, 8, 6, 2);
    bt_ga_upgrades adaptive = no_upgrades();
    adaptive.stagnation_threshold = 1;
    adaptive.swap_scale = 1000000.0;
    adaptive.replace_scale = 1000000.0;
    bt_ga_run_outcome *a = bt_ga_run_ex(env, &params, &adaptive);
    bt_ga_run_outcome *b = bt_ga_run_ex(env, &params, &adaptive);
    BT_CHECK(memcmp(a->best, b->best, a->best_count * sizeof(uint32_t)) == 0);
    for (size_t i = 0; i < a->per_gen_count; i++) {
        BT_CHECK_EQ(f32_bits(a->per_gen_best[i]), f32_bits(b->per_gen_best[i]));
    }
    BT_CHECK_EQ(a->evaluations, b->evaluations);
    for (size_t i = 1; i < a->per_gen_count; i++) {
        BT_CHECK(a->per_gen_best[i] <= a->per_gen_best[i - 1] + 1.19209290e-07F);
    }
    bt_ga_run_outcome_free(b);
    bt_ga_run_outcome_free(a);
    bt_ga_env_free(env);
}

BT_TEST(ga_upgrade, config_upgrades_caps_scale_and_count_ceilings) {
    bt_json_value *config = upgrade_config_from_literal(
        "{\"ga\":{\"islands\":{\"count\":1025},\"adaptive_mutation\":{\"swap_scale\":1000000.0},"
        "\"restart\":{\"diversity_threshold\":0.5}}}");
    bt_ga_upgrades upgrades;
    char err[BT_GA_ERR_MAX];
    BT_CHECK(!bt_ga_config_upgrades(config, &upgrades, err));
    BT_CHECK(strstr(err, "ga.islands.count") != NULL);
    bt_json_free(config);
    config = upgrade_config_from_literal(
        "{\"ga\":{\"adaptive_mutation\":{\"stagnation_threshold\":2,\"swap_scale\":1000000.5}}}");
    BT_CHECK(!bt_ga_config_upgrades(config, &upgrades, err));
    BT_CHECK(strstr(err, "ga.adaptive_mutation.swap_scale") != NULL);
    bt_json_free(config);
    config = upgrade_config_from_literal(
        "{\"ga\":{\"islands\":{\"count\":2,\"migrate_every\":1000000001,\"migrate_count\":1}}}");
    BT_CHECK(!bt_ga_config_upgrades(config, &upgrades, err));
    BT_CHECK(strstr(err, "ga.islands.migrate_every") != NULL);
    bt_json_free(config);
    config = upgrade_config_from_literal(
        "{\"ga\":{\"adaptive_mutation\":{\"stagnation_threshold\":4294967296}}}");
    BT_CHECK(!bt_ga_config_upgrades(config, &upgrades, err));
    BT_CHECK(strstr(err, "ga.adaptive_mutation.stagnation_threshold") != NULL);
    bt_json_free(config);
    config = upgrade_config_from_literal(
        "{\"ga\":{\"islands\":{\"count\":1024,\"migrate_every\":1000000000,\"migrate_count\":1},"
        "\"adaptive_mutation\":{\"stagnation_threshold\":4294967295,\"swap_scale\":1000000.0,"
        "\"replace_scale\":0.25}}}");
    if (!bt_ga_config_upgrades(config, &upgrades, err)) {
        BT_ABORT("ceiling values rejected: %s", err);
    }
    BT_CHECK_EQ(upgrades.island_count, (size_t)1024);
    BT_CHECK_EQ(upgrades.migrate_every, (size_t)1000000000);
    BT_CHECK_EQ(upgrades.stagnation_threshold, 4294967295U);
    BT_CHECK_F64_NEAR(upgrades.swap_scale, 1000000.0, 1e-9);
    bt_json_free(config);
}

BT_TEST(ga_upgrade, upgrades_check_names_each_rejected_shape) {
    bt_ga_params params = upgrade_params(424242, 8, 6, 2);
    bt_ga_upgrades fraction = no_upgrades();
    fraction.elite_fraction_policy = 1;
    fraction.elite_fraction = 0.0625;
    bt_ga_upgrades starving = no_upgrades();
    starving.island_count = 2;
    starving.migrate_every = 1;
    starving.migrate_count = 7;
    bt_ga_upgrades clean = no_upgrades();
    clean.island_count = 3;
    clean.migrate_every = 2;
    clean.migrate_count = 2;
    clean.stagnation_threshold = 2;
    clean.swap_scale = 1.5;
    clean.replace_scale = 0.5;
    clean.diversity_threshold = 0.9;
    clean.inject_fraction = 0.5;
    char err[BT_GA_ERR_MAX];
    BT_CHECK(!bt_ga_upgrades_check(&fraction, &params, err));
    BT_CHECK(strstr(err, "elite must be at least 1") != NULL);
    BT_CHECK(!bt_ga_upgrades_check(&starving, &params, err));
    BT_CHECK(strstr(err, "migrate count plus elite must stay at or below the population") != NULL);
    BT_CHECK(bt_ga_upgrades_check(&clean, &params, err));
}

static void run_ex_with_island_count_overflow(void) {
    bt_ga_env *env = upgrade_env(6);
    bt_ga_params params = upgrade_params(424242, 8, 6, 2);
    bt_ga_upgrades wrap = no_upgrades();
    wrap.island_count = 4611686018427387904ULL;
    bt_ga_run_outcome *outcome = bt_ga_run_ex(env, &params, &wrap);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

static void run_ex_with_generations_at_size_max(void) {
    bt_ga_env *env = upgrade_env(6);
    bt_ga_params params = upgrade_params(424242, 8, 6, 2);
    params.generations = SIZE_MAX;
    bt_ga_run_outcome *outcome = bt_ga_run(env, &params);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

static void ring_migrate_with_migrate_count_above_population(void) {
    bt_ga_island *islands = calloc(2, sizeof *islands);
    for (size_t isl = 0; isl < 2; isl++) {
        islands[isl].population = calloc(2, sizeof *islands[isl].population);
        islands[isl].scores = calloc(2, sizeof *islands[isl].scores);
        islands[isl].order = calloc(2, sizeof *islands[isl].order);
        for (size_t i = 0; i < 2; i++) {
            islands[isl].population[i] = calloc(3, sizeof(uint32_t));
            islands[isl].scores[i] = 1.0F;
            islands[isl].order[i] = i;
        }
    }
    uint32_t **staging = calloc(3, sizeof *staging);
    float *staging_scores = calloc(3, sizeof *staging_scores);
    for (size_t i = 0; i < 3; i++) {
        staging[i] = calloc(3, sizeof(uint32_t));
    }
    bt_ga_ring_migrate(islands, 2, 2, 3, 3, staging, staging_scores);
}

BT_TEST(ga_upgrade, run_ex_rejects_overflow_and_unguarded_ring_calls) {
    bt_ga_death_expect("run_ex_with_island_count_overflow",
                       "allocation size overflow at ga.run.islands",
                       run_ex_with_island_count_overflow);
    bt_ga_death_expect("run_ex_with_generations_at_size_max",
                       "generations must stay below the size_t ceiling",
                       run_ex_with_generations_at_size_max);
    bt_ga_death_expect("ring_migrate_with_migrate_count_above_population",
                       "migrate count must stay at or below the population",
                       ring_migrate_with_migrate_count_above_population);
}
