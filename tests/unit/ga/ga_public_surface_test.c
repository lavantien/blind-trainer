#include "ga/ga.h"
#include "ga/hash.h"

#include "common/bt_harness.h"
#include "common/strbuf.h"

#include "ga_test_util.h"

static const char *const POOL[14] = {"không", "lông", "tía", "mút",  "đặt",  "trăng", "của",
                                     "anh",   "ón",   "gì",  "xinh", "vững", "được",  "ngăn"};

static size_t pool_codepoints(const char *text) {
    size_t count = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++) {
        if ((*p & 0xC0) != 0x80) {
            count++;
        }
    }
    return count;
}

static bt_ga_env *surface_env(size_t case_count) {
    bt_ga_features *feats = calloc(14, sizeof *feats);
    float *lengths = calloc(14, sizeof *lengths);
    for (int i = 0; i < 14; i++) {
        feats[i] = bt_ga_parse_or_abort(POOL[i]);
        lengths[i] = (float)pool_codepoints(POOL[i]);
    }
    bt_ga_pair_weight *cooc = calloc(2, sizeof *cooc);
    cooc[0].i = 0;
    cooc[0].j = 3;
    cooc[0].w = 0.4f;
    cooc[1].i = 2;
    cooc[1].j = 5;
    cooc[1].w = 0.6f;
    bt_ga_pair_weight *adj = calloc(2, sizeof *adj);
    adj[0].i = 1;
    adj[0].j = 4;
    adj[0].w = 0.3f;
    adj[1].i = 6;
    adj[1].j = 7;
    adj[1].w = 0.7f;
    bt_ga_env *env = malloc(sizeof *env);
    char err[BT_GA_ERR_MAX];
    if (!bt_ga_build_env(14, lengths, feats, 14, case_count, cooc, 2, adj, 2, env, err)) {
        BT_ABORT("build_env: %s", err);
    }
    return env;
}

static void random_injective(bt_rng *rng, size_t pool_size, size_t n, uint32_t *out) {
    uint32_t *genes = malloc(pool_size * sizeof *genes);
    for (size_t i = 0; i < pool_size; i++) {
        genes[i] = (uint32_t)i;
    }
    bt_ga_shuffle_u32(rng, genes, pool_size);
    memcpy(out, genes, n * sizeof *out);
    free(genes);
}

static bt_ga_params surface_params(uint64_t seed) {
    bt_ga_params params;
    params.seed = seed;
    params.population = 24;
    params.generations = 30;
    params.elite = 2;
    params.swap = 0.15;
    params.replace = 0.05;
    params.w_conf = 1.0f;
    params.w_flow = 0.5f;
    params.w_cplx = 0.1f;
    return params;
}

static void outcome_hash(const bt_ga_run_outcome *outcome, char out[BT_GA_SHA256_HEX_LEN]) {
    bt_strbuf buf;
    bt_strbuf_init(&buf, 256);
    bt_strbuf_append_str(&buf, "[[");
    for (size_t i = 0; i < outcome->best_count; i++) {
        if (i > 0) {
            bt_strbuf_append_str(&buf, ",");
        }
        char item[24];
        snprintf(item, sizeof item, "%u", outcome->best[i]);
        bt_strbuf_append_str(&buf, item);
    }
    bt_strbuf_append_str(&buf, "],[");
    for (size_t i = 0; i < outcome->per_gen_count; i++) {
        if (i > 0) {
            bt_strbuf_append_str(&buf, ",");
        }
        char item[48];
        snprintf(item, sizeof item, "%.9g", (double)outcome->per_gen_best[i]);
        bt_strbuf_append_str(&buf, item);
    }
    bt_strbuf_append_str(&buf, "]]");
    bt_ga_sha256_hex(buf.data, buf.len, out);
    bt_strbuf_free(&buf);
}

BT_TEST(ga_public_surface, ox_pins_hold_through_the_public_api) {
    uint32_t child[6];
    bt_ga_ox((uint32_t[]){1, 2, 3, 4, 5}, (uint32_t[]){3, 1, 2, 5, 4}, 5, 1, 3, child);
    uint32_t want1[5] = {4, 2, 3, 1, 5};
    BT_CHECK(memcmp(child, want1, sizeof want1) == 0);
    bt_ga_ox((uint32_t[]){5, 4, 3, 2, 1}, (uint32_t[]){2, 1, 4, 3, 5}, 5, 1, 4, child);
    uint32_t want2[5] = {5, 4, 3, 2, 1};
    BT_CHECK(memcmp(child, want2, sizeof want2) == 0);
    bt_ga_ox((uint32_t[]){10, 11, 12, 13, 14, 15}, (uint32_t[]){12, 14, 15, 10, 11, 13}, 6, 1, 3,
             child);
    uint32_t want3[6] = {13, 11, 12, 14, 15, 10};
    BT_CHECK(memcmp(child, want3, sizeof want3) == 0);
    bt_ga_ox((uint32_t[]){0, 1, 1, 0, 2, 2}, (uint32_t[]){3, 4, 5, 6, 7, 1}, 6, 1, 3, child);
    uint32_t want4[6] = {6, 1, 1, 3, 4, 5};
    BT_CHECK(memcmp(child, want4, sizeof want4) == 0);
}

BT_TEST(ga_public_surface, hang_watchdog_arms_for_this_binary) {
    bt_ga_arm_hang_watchdog();
}

BT_TEST(ga_public_surface, precomputed_tables_agree_with_the_feature_functions) {
    bt_ga_env *env = surface_env(8);
    bt_rng rng;
    bt_rng_seed(&rng, 3);
    for (int iter = 0; iter < 300; iter++) {
        size_t a = (size_t)bt_rng_below(&rng, 14);
        size_t b = (size_t)bt_rng_below(&rng, 14);
        float direct_conf = bt_ga_confusion(&env->feats[a], &env->feats[b]);
        float table_conf = env->conf_table[a * env->pool_size + b];
        BT_CHECK_F64_NEAR(direct_conf, table_conf, 1e-6);
        float direct_flow = bt_ga_flow_penalty(&env->feats[a], (size_t)env->lengths[a],
                                               &env->feats[b], (size_t)env->lengths[b]);
        float table_flow = env->flow_table[a * env->pool_size + b];
        BT_CHECK_F64_NEAR(direct_flow, table_flow, 1e-6);
    }
    bt_ga_env_free(env);
}

BT_TEST(ga_public_surface, operators_keep_assignments_injective_and_in_bounds) {
    bt_ga_env *env = surface_env(8);
    bt_rng rng;
    bt_rng_seed(&rng, 11);
    uint32_t *p1 = malloc(env->case_count * sizeof *p1);
    uint32_t *p2 = malloc(env->case_count * sizeof *p2);
    uint32_t *child = malloc(env->case_count * sizeof *child);
    for (int iter = 0; iter < 400; iter++) {
        random_injective(&rng, env->pool_size, env->case_count, p1);
        random_injective(&rng, env->pool_size, env->case_count, p2);
        size_t cut1 = 0;
        size_t cut2 = 0;
        bt_ga_draw_cuts(&rng, env->case_count, &cut1, &cut2);
        bt_ga_ox(p1, p2, env->case_count, cut1, cut2, child);
        BT_CHECK(bt_ga_injective(child, env->case_count));
        for (size_t i = 0; i < env->case_count; i++) {
            BT_CHECK(child[i] < env->pool_size);
        }
        BT_CHECK(memcmp(&child[cut1], &p1[cut1], (cut2 - cut1) * sizeof(uint32_t)) == 0);
        memcpy(p1, child, env->case_count * sizeof *p1);
        bt_ga_mutate_swap(p1, env->case_count, &rng);
        BT_CHECK(bt_ga_injective(p1, env->case_count));
        memcpy(p2, child, env->case_count * sizeof *p2);
        bt_ga_mutate_replace(p2, env->case_count, &rng, env->pool_size);
        BT_CHECK(bt_ga_injective(p2, env->case_count));
        for (size_t i = 0; i < env->case_count; i++) {
            BT_CHECK(p2[i] < env->pool_size);
        }
    }
    free(child);
    free(p2);
    free(p1);
    bt_ga_env_free(env);
}

BT_TEST(ga_public_surface, fitness_matches_its_parts) {
    bt_ga_env *env = surface_env(8);
    bt_rng rng;
    bt_rng_seed(&rng, 19);
    uint32_t *chrom = malloc(env->case_count * sizeof *chrom);
    random_injective(&rng, env->pool_size, env->case_count, chrom);
    bt_ga_params p = surface_params(1);
    float parts[3];
    bt_ga_fitness_parts(env, chrom, parts);
    float whole = bt_ga_fitness(&p, env, chrom);
    float recomposed = p.w_conf * parts[0] + p.w_flow * parts[1] + p.w_cplx * parts[2];
    BT_CHECK_F64_NEAR(whole, recomposed, 1e-5);
    free(chrom);
    bt_ga_env_free(env);
}

BT_TEST(ga_public_surface, run_hashes_stable_for_a_seed_and_divergent_across_seeds) {
    bt_ga_env *env = surface_env(8);
    bt_ga_params seeded_a = surface_params(101);
    bt_ga_params seeded_b = surface_params(102);
    bt_ga_run_outcome *a = bt_ga_run(env, &seeded_a);
    bt_ga_run_outcome *a_replay = bt_ga_run(env, &seeded_a);
    char hash_a[BT_GA_SHA256_HEX_LEN];
    char hash_replay[BT_GA_SHA256_HEX_LEN];
    outcome_hash(a, hash_a);
    outcome_hash(a_replay, hash_replay);
    BT_CHECK_EQ(hash_a, hash_replay);
    bt_ga_run_outcome *b = bt_ga_run(env, &seeded_b);
    char hash_b[BT_GA_SHA256_HEX_LEN];
    outcome_hash(b, hash_b);
    BT_CHECK(strcmp(hash_a, hash_b) != 0);
    BT_CHECK(bt_ga_injective(a->best, a->best_count));
    for (size_t i = 0; i < a->best_count; i++) {
        BT_CHECK(a->best[i] < env->pool_size);
    }
    bt_ga_run_outcome_free(b);
    bt_ga_run_outcome_free(a_replay);
    bt_ga_run_outcome_free(a);
    bt_ga_env_free(env);
}

BT_TEST(ga_public_surface, tournament_returns_a_drawn_index_for_every_fitness_shape) {
    bt_rng rng;
    bt_rng_seed(&rng, 23);
    float flat[16] = {0};
    BT_CHECK(bt_ga_tournament(&rng, flat, 16) < 16);
    float ramp[16];
    for (int i = 0; i < 16; i++) {
        ramp[i] = (float)i;
    }
    BT_CHECK(bt_ga_tournament(&rng, ramp, 16) < 16);
    float nans[16];
    for (int i = 0; i < 16; i++) {
        nans[i] = (float)(0.0 / 0.0);
    }
    BT_CHECK(bt_ga_tournament(&rng, nans, 16) < 16);
}
