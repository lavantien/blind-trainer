#include "ga/eval.h"
#include "ga/ga.h"

#include "common/bt_harness.h"
#include "common/config.h"
#include "common/json.h"
#include "common/rng.h"
#include "data/paths.h"

#include "death_test.h"
#include "ga_test_util.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t hits;
    char sites[8][32];
} fault_log;

static int log_then_fail(const char *site, void *user) {
    fault_log *log = user;
    snprintf(log->sites[log->hits % 8], 32, "%s", site);
    log->hits++;
    return 1;
}

static int log_then_pass(const char *site, void *user) {
    fault_log *log = user;
    snprintf(log->sites[log->hits % 8], 32, "%s", site);
    log->hits++;
    return 0;
}

static bt_json_value *parse_json(const char *text) {
    bt_json_value *value = NULL;
    bt_json_error error;
    if (bt_json_parse(text, strlen(text), &value, &error) != 0) {
        BT_ABORT("json parse failed at %zu:%zu: %s", error.line, error.column, error.message);
    }
    return value;
}

static void use_scratch_config(const char *name, const char *body, char saved[BT_PATH_MAX]) {
    const char *prior = getenv("BT_CONFIG");
    snprintf(saved, BT_PATH_MAX, "%s", prior != NULL ? prior : "");
    char dir[BT_PATH_MAX];
    bt_ga_scratch("ga_eval_cfg", dir);
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/%s.json", dir, name);
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        BT_ABORT("scratch config unreachable at %s", path);
    }
    fwrite(body, 1, strlen(body), file);
    fclose(file);
    _putenv_s("BT_CONFIG", path);
}

static void restore_config(const char saved[BT_PATH_MAX]) {
    if (saved[0] == '\0') {
        _putenv("BT_CONFIG=");
    } else {
        _putenv_s("BT_CONFIG", saved);
    }
}

static double parity_tolerance(void) {
    bt_json_value *config = bt_data_load_config();
    double tol = 0.0;
    if (config == NULL) {
        BT_ABORT("scratch config did not load");
    }
    if (bt_config_f64(config, "cuda.parity_tolerance", &tol) != 0 || !(tol > 0.0)) {
        bt_json_free(config);
        BT_ABORT("cuda.parity_tolerance missing from the hub");
    }
    bt_json_free(config);
    return tol;
}

static const char *const POOL8[] = {"ả", "ã", "không", "lông", "tía", "mút", "án", "đặt"};

static bt_ga_env *make_env(bt_ga_features **feats_out, float **lengths_out) {
    bt_ga_features *feats = calloc(8, sizeof *feats);
    float *lengths = calloc(8, sizeof *lengths);
    for (size_t i = 0; i < 8; i++) {
        feats[i] = bt_ga_parse_or_abort(POOL8[i]);
        lengths[i] = 2.0F + (float)i;
    }
    static bt_ga_pair_weight cooc[6];
    cooc[0].i = 0;
    cooc[0].j = 1;
    cooc[0].w = 0.5F;
    cooc[1].i = 1;
    cooc[1].j = 2;
    cooc[1].w = 0.25F;
    cooc[2].i = 2;
    cooc[2].j = 3;
    cooc[2].w = 0.75F;
    cooc[3].i = 3;
    cooc[3].j = 4;
    cooc[3].w = 0.125F;
    cooc[4].i = 0;
    cooc[4].j = 4;
    cooc[4].w = 0.0625F;
    cooc[5].i = 1;
    cooc[5].j = 3;
    cooc[5].w = 0.375F;
    static bt_ga_pair_weight adj[5];
    adj[0].i = 0;
    adj[0].j = 2;
    adj[0].w = 0.5F;
    adj[1].i = 2;
    adj[1].j = 4;
    adj[1].w = 0.5F;
    adj[2].i = 1;
    adj[2].j = 3;
    adj[2].w = 0.25F;
    adj[3].i = 3;
    adj[3].j = 4;
    adj[3].w = 0.75F;
    adj[4].i = 0;
    adj[4].j = 1;
    adj[4].w = 0.125F;
    bt_ga_env *env = malloc(sizeof *env);
    char err[BT_GA_ERR_MAX];
    if (!bt_ga_build_env(8, lengths, 8, feats, 8, 5, cooc, 6, adj, 5, env, err)) {
        BT_ABORT("build_env: %s", err);
    }
    *feats_out = feats;
    *lengths_out = lengths;
    return env;
}

static void free_env(bt_ga_env *env, bt_ga_features *feats, float *lengths) {
    bt_ga_env_free(env);
    free(env);
    free(feats);
    free(lengths);
}

static bt_ga_params make_params(void) {
    bt_ga_params params;
    params.seed = 424242;
    params.population = 32;
    params.generations = 3;
    params.elite = 2;
    params.swap = 0.15;
    params.replace = 0.05;
    params.w_conf = 1.0F;
    params.w_flow = 0.5F;
    params.w_cplx = 0.1F;
    return params;
}

enum { POP_N = 32, CASE_N = 5 };

static void make_population(bt_ga_params *params, uint32_t (*chroms)[CASE_N],
                            uint32_t *owners[POP_N]) {
    bt_rng rng;
    bt_rng_seed(&rng, params->seed);
    uint32_t scratch[8];
    for (size_t i = 0; i < POP_N; i++) {
        for (size_t g = 0; g < 8; g++) {
            scratch[g] = (uint32_t)g;
        }
        bt_ga_shuffle_u32(&rng, scratch, 8);
        for (size_t g = 0; g < CASE_N; g++) {
            chroms[i][g] = scratch[g];
        }
        owners[i] = chroms[i];
    }
}

static int chrom_less(const uint32_t *a, const uint32_t *b, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (a[i] != b[i]) {
            return a[i] < b[i];
        }
    }
    return 0;
}

static void rank_order(const float *scores, uint32_t *const *population, size_t count,
                       size_t *order) {
    for (size_t i = 0; i < count; i++) {
        order[i] = i;
    }
    int swapped = 1;
    while (swapped) {
        swapped = 0;
        for (size_t i = 0; i + 1 < count; i++) {
            size_t a = order[i];
            size_t b = order[i + 1];
            int flip = scores[b] < scores[a];
            if (!flip && !(scores[a] < scores[b]) &&
                chrom_less(population[b], population[a], CASE_N)) {
                flip = 1;
            }
            if (flip) {
                order[i] = b;
                order[i + 1] = a;
                swapped = 1;
            }
        }
    }
}

BT_TEST(ga_eval, hang_watchdog_arms_for_this_binary) {
    bt_ga_arm_hang_watchdog();
}

BT_TEST(ga_eval, config_allow_reads_the_flag_and_defaults_off) {
    char err[BT_GA_ERR_MAX];
    int allow = 1;
    bt_json_value *config = parse_json("{\"ga\":{}}");
    BT_CHECK_EQ(bt_ga_eval_config_allow(config, &allow, err), 1);
    BT_CHECK_EQ(allow, 0);
    bt_json_free(config);
    config = parse_json("{\"cuda\":{\"enable\":false}}");
    BT_CHECK_EQ(bt_ga_eval_config_allow(config, &allow, err), 1);
    BT_CHECK_EQ(allow, 0);
    bt_json_free(config);
    config = parse_json("{\"cuda\":{\"enable\":true}}");
    BT_CHECK_EQ(bt_ga_eval_config_allow(config, &allow, err), 1);
    BT_CHECK_EQ(allow, 1);
    bt_json_free(config);
}

BT_TEST(ga_eval, config_allow_rejects_wrong_types) {
    static const char *const bodies[] = {
        "{\"cuda\":{\"enable\":\"on\"}}",
        "{\"cuda\":{\"enable\":1}}",
        "{\"cuda\":{\"enable\":[true]}}",
        "{\"cuda\":{\"enable\":{\"on\":true}}}",
    };
    for (size_t i = 0; i < sizeof bodies / sizeof bodies[0]; i++) {
        char err[BT_GA_ERR_MAX];
        int allow = 1;
        bt_json_value *config = parse_json(bodies[i]);
        BT_CHECK_EQ(bt_ga_eval_config_allow(config, &allow, err), 0);
        BT_CHECK_EQ(allow, 0);
        BT_CHECK(strstr(err, "cuda.enable") != NULL);
        bt_json_free(config);
    }
}

BT_TEST(ga_eval, cpu_session_scores_match_fitness_exactly) {
    char saved[BT_PATH_MAX];
    use_scratch_config("cpu", "{\"cuda\":{\"enable\":false,\"parity_tolerance\":1e-06}}", saved);
    bt_ga_features *feats = NULL;
    float *lengths = NULL;
    bt_ga_env *env = make_env(&feats, &lengths);
    bt_ga_params params = make_params();
    uint32_t chroms[POP_N][CASE_N];
    uint32_t *owners[POP_N];
    make_population(&params, chroms, owners);
    float scores[POP_N];
    bt_ga_eval *eval = bt_ga_eval_create(env, &params, params.population);
    BT_CHECK_EQ(bt_ga_eval_uses_cuda(eval), 0);
    bt_ga_eval_population(eval, owners, POP_N, scores);
    for (size_t i = 0; i < POP_N; i++) {
        BT_CHECK_EQ(scores[i], bt_ga_fitness(&params, env, owners[i]));
    }
    bt_ga_eval_destroy(eval);
    free_env(env, feats, lengths);
    restore_config(saved);
}

BT_TEST(ga_eval, enabled_session_reports_the_compiled_lane) {
    char saved[BT_PATH_MAX];
    use_scratch_config("cuda_on", "{\"cuda\":{\"enable\":true,\"parity_tolerance\":1e-06}}", saved);
    bt_ga_features *feats = NULL;
    float *lengths = NULL;
    bt_ga_env *env = make_env(&feats, &lengths);
    bt_ga_params params = make_params();
    bt_ga_eval *eval = bt_ga_eval_create(env, &params, params.population);
    BT_CHECK_EQ(bt_ga_eval_uses_cuda(eval), bt_ga_eval_cuda_compiled());
    bt_ga_eval_destroy(eval);
    free_env(env, feats, lengths);
    restore_config(saved);
}

BT_TEST(ga_eval, fault_seam_forces_the_cpu_fallback) {
    char saved[BT_PATH_MAX];
    use_scratch_config("cuda_seam", "{\"cuda\":{\"enable\":true,\"parity_tolerance\":1e-06}}",
                       saved);
    fault_log log;
    memset(&log, 0, sizeof log);
    bt_ga_eval_fault_install(log_then_fail, &log);
    bt_ga_features *feats = NULL;
    float *lengths = NULL;
    bt_ga_env *env = make_env(&feats, &lengths);
    bt_ga_params params = make_params();
    uint32_t chroms[POP_N][CASE_N];
    uint32_t *owners[POP_N];
    make_population(&params, chroms, owners);
    float scores[POP_N];
    bt_ga_eval *eval = bt_ga_eval_create(env, &params, params.population);
    BT_CHECK_EQ(bt_ga_eval_uses_cuda(eval), 0);
    BT_CHECK(log.hits > 0);
    BT_CHECK(strcmp(log.sites[0], "ga.eval.cuda") == 0);
    bt_ga_eval_population(eval, owners, POP_N, scores);
    for (size_t i = 0; i < POP_N; i++) {
        BT_CHECK_EQ(scores[i], bt_ga_fitness(&params, env, owners[i]));
    }
    bt_ga_eval_destroy(eval);
    bt_ga_eval_fault_reset();
    eval = bt_ga_eval_create(env, &params, params.population);
    BT_CHECK_EQ(bt_ga_eval_uses_cuda(eval), bt_ga_eval_cuda_compiled());
    bt_ga_eval_destroy(eval);
    free_env(env, feats, lengths);
    restore_config(saved);
}

BT_TEST(ga_eval, passing_seam_leaves_the_lane_engaged) {
    char saved[BT_PATH_MAX];
    use_scratch_config("cuda_pass", "{\"cuda\":{\"enable\":true,\"parity_tolerance\":1e-06}}",
                       saved);
    fault_log log;
    memset(&log, 0, sizeof log);
    bt_ga_eval_fault_install(log_then_pass, &log);
    bt_ga_features *feats = NULL;
    float *lengths = NULL;
    bt_ga_env *env = make_env(&feats, &lengths);
    bt_ga_params params = make_params();
    bt_ga_eval *eval = bt_ga_eval_create(env, &params, params.population);
    BT_CHECK_EQ(bt_ga_eval_uses_cuda(eval), bt_ga_eval_cuda_compiled());
    BT_CHECK_EQ(log.hits, (size_t)1);
    BT_CHECK(strcmp(log.sites[0], "ga.eval.cuda") == 0);
    bt_ga_eval_destroy(eval);
    bt_ga_eval_fault_reset();
    free_env(env, feats, lengths);
    restore_config(saved);
}

BT_TEST(ga_eval, both_sessions_agree_within_tolerance_and_rank_exactly) {
    char saved[BT_PATH_MAX];
    use_scratch_config("cpu_ref", "{\"cuda\":{\"enable\":false,\"parity_tolerance\":1e-06}}",
                       saved);
    bt_ga_features *feats = NULL;
    float *lengths = NULL;
    bt_ga_env *env = make_env(&feats, &lengths);
    bt_ga_params params = make_params();
    uint32_t chroms[POP_N][CASE_N];
    uint32_t *owners[POP_N];
    make_population(&params, chroms, owners);
    bt_ga_eval *cpu = bt_ga_eval_create(env, &params, params.population);
    BT_CHECK_EQ(bt_ga_eval_uses_cuda(cpu), 0);
    float cpu_scores[POP_N];
    bt_ga_eval_population(cpu, owners, POP_N, cpu_scores);
    bt_ga_eval_destroy(cpu);
    restore_config(saved);
    use_scratch_config("gpu_lane", "{\"cuda\":{\"enable\":true,\"parity_tolerance\":1e-06}}",
                       saved);
    double tol = parity_tolerance();
    bt_ga_eval *lane = bt_ga_eval_create(env, &params, params.population);
    BT_CHECK_EQ(bt_ga_eval_uses_cuda(lane), bt_ga_eval_cuda_compiled());
    float lane_scores[POP_N];
    bt_ga_eval_population(lane, owners, POP_N, lane_scores);
    bt_ga_eval_destroy(lane);
    for (size_t i = 0; i < POP_N; i++) {
        BT_CHECK_F64_NEAR(lane_scores[i], cpu_scores[i], tol);
    }
    size_t cpu_order[POP_N];
    size_t lane_order[POP_N];
    rank_order(cpu_scores, owners, POP_N, cpu_order);
    rank_order(lane_scores, owners, POP_N, lane_order);
    for (size_t i = 0; i < POP_N; i++) {
        BT_CHECK_EQ(cpu_order[i], lane_order[i]);
    }
    free_env(env, feats, lengths);
    restore_config(saved);
}

BT_TEST(ga_eval, destroy_tolerates_null) {
    bt_ga_eval_destroy(NULL);
}

static void die_on_capacity_overflow(void) {
    bt_ga_features *feats = NULL;
    float *lengths = NULL;
    bt_ga_env *env = make_env(&feats, &lengths);
    bt_ga_params params = make_params();
    uint32_t chroms[8][CASE_N];
    uint32_t *owners[8];
    make_population(&params, chroms, owners);
    float scores[8];
    bt_ga_eval *eval = bt_ga_eval_create(env, &params, 4);
    bt_ga_eval_population(eval, owners, 8, scores);
    bt_ga_eval_destroy(eval);
    free_env(env, feats, lengths);
}

BT_TEST(ga_eval, population_count_over_capacity_aborts) {
    bt_ga_death_expect("die_on_eval_capacity", "count 8 exceeds the eval capacity 4",
                       die_on_capacity_overflow);
}
