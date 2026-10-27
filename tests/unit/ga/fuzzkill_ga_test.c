#include "ga/features.h"
#include "ga/ga.h"
#include "ga/inventory.h"

#include "common/bt_harness.h"

#include "ga_test_util.h"

BT_TEST(fuzzkill_ga, hang_watchdog_arms_for_this_binary) {
    bt_ga_arm_hang_watchdog();
}

static bt_ga_features tones_only(bt_ga_tone a, bt_ga_tone b, bt_ga_features *second) {
    bt_ga_features first = bt_ga_feat(a, 0, 0, 0, BT_GA_NUC_A, BT_GA_CODA_OPEN);
    *second = bt_ga_feat(b, 0, 0, 0, BT_GA_NUC_A, BT_GA_CODA_OPEN);
    return first;
}

static bt_ga_features nuclei_only(bt_ga_nucleus a, bt_ga_nucleus b, bt_ga_features *second) {
    bt_ga_features first = bt_ga_feat(BT_GA_TONE_NGANG, 0, 0, 0, a, BT_GA_CODA_OPEN);
    *second = bt_ga_feat(BT_GA_TONE_NGANG, 0, 0, 0, b, BT_GA_CODA_OPEN);
    return first;
}

static bt_ga_features codas_only(bt_ga_coda a, bt_ga_coda b, bt_ga_features *second) {
    bt_ga_features first = bt_ga_feat(BT_GA_TONE_NGANG, 0, 0, 0, BT_GA_NUC_A, a);
    *second = bt_ga_feat(BT_GA_TONE_NGANG, 0, 0, 0, BT_GA_NUC_A, b);
    return first;
}

static bt_ga_features onsets_only(bt_ga_place a_place, bt_ga_manner a_manner, bt_ga_place b_place,
                                  bt_ga_manner b_manner, bt_ga_features *second) {
    bt_ga_features first =
        bt_ga_feat(BT_GA_TONE_NGANG, 1, a_place, a_manner, BT_GA_NUC_A, BT_GA_CODA_OPEN);
    *second = bt_ga_feat(BT_GA_TONE_NGANG, 1, b_place, b_manner, BT_GA_NUC_A, BT_GA_CODA_OPEN);
    return first;
}

BT_TEST(fuzzkill_ga, documented_tone_confusions_are_pinned) {
    static const struct {
        bt_ga_tone a;
        bt_ga_tone b;
        float expected;
    } rows[] = {
        {BT_GA_TONE_HOI, BT_GA_TONE_NGA, 0.70f},   {BT_GA_TONE_HUYEN, BT_GA_TONE_NANG, 0.60f},
        {BT_GA_TONE_SAC, BT_GA_TONE_NGA, 0.35f},   {BT_GA_TONE_NGANG, BT_GA_TONE_HUYEN, 0.30f},
        {BT_GA_TONE_HOI, BT_GA_TONE_NANG, 0.25f},  {BT_GA_TONE_NGANG, BT_GA_TONE_SAC, 0.05f},
        {BT_GA_TONE_NGANG, BT_GA_TONE_NGA, 0.05f},
    };
    for (size_t r = 0; r < sizeof rows / sizeof rows[0]; r++) {
        bt_ga_features second;
        bt_ga_features first = tones_only(rows[r].a, rows[r].b, &second);
        BT_CHECK_EQ(bt_ga_confusion(&first, &second), rows[r].expected);
        BT_CHECK_EQ(bt_ga_confusion(&second, &first), rows[r].expected);
    }
    bt_ga_features same_second;
    bt_ga_features same = tones_only(BT_GA_TONE_SAC, BT_GA_TONE_SAC, &same_second);
    BT_CHECK_EQ(bt_ga_confusion(&same, &same_second), 1.0f);
}

BT_TEST(fuzzkill_ga, documented_nucleus_confusions_are_pinned) {
    static const struct {
        bt_ga_nucleus a;
        bt_ga_nucleus b;
        float expected;
    } rows[] = {
        {BT_GA_NUC_E, BT_GA_NUC_EH, 0.40f},  {BT_GA_NUC_O, BT_GA_NUC_OH, 0.40f},
        {BT_GA_NUC_OR, BT_GA_NUC_AH, 0.40f}, {BT_GA_NUC_A, BT_GA_NUC_AB, 0.40f},
        {BT_GA_NUC_A, BT_GA_NUC_E, 0.05f},   {BT_GA_NUC_OR, BT_GA_NUC_O, 0.05f},
    };
    for (size_t r = 0; r < sizeof rows / sizeof rows[0]; r++) {
        bt_ga_features second;
        bt_ga_features first = nuclei_only(rows[r].a, rows[r].b, &second);
        BT_CHECK_EQ(bt_ga_confusion(&first, &second), rows[r].expected);
        BT_CHECK_EQ(bt_ga_confusion(&second, &first), rows[r].expected);
    }
}

BT_TEST(fuzzkill_ga, documented_coda_confusions_are_pinned) {
    static const struct {
        bt_ga_coda a;
        bt_ga_coda b;
        float expected;
    } rows[] = {
        {BT_GA_CODA_P, BT_GA_CODA_T, 0.50f},    {BT_GA_CODA_T, BT_GA_CODA_K, 0.40f},
        {BT_GA_CODA_P, BT_GA_CODA_K, 0.20f},    {BT_GA_CODA_M, BT_GA_CODA_N, 0.30f},
        {BT_GA_CODA_N, BT_GA_CODA_NG, 0.30f},   {BT_GA_CODA_M, BT_GA_CODA_NG, 0.15f},
        {BT_GA_CODA_P, BT_GA_CODA_M, 0.20f},    {BT_GA_CODA_T, BT_GA_CODA_N, 0.20f},
        {BT_GA_CODA_K, BT_GA_CODA_NG, 0.20f},   {BT_GA_CODA_P, BT_GA_CODA_N, 0.10f},
        {BT_GA_CODA_OPEN, BT_GA_CODA_T, 0.10f},
    };
    for (size_t r = 0; r < sizeof rows / sizeof rows[0]; r++) {
        bt_ga_features second;
        bt_ga_features first = codas_only(rows[r].a, rows[r].b, &second);
        BT_CHECK_EQ(bt_ga_confusion(&first, &second), rows[r].expected);
        BT_CHECK_EQ(bt_ga_confusion(&second, &first), rows[r].expected);
    }
}

BT_TEST(fuzzkill_ga, documented_onset_confusions_are_pinned) {
    bt_ga_features second;
    bt_ga_features first = onsets_only(BT_GA_PLACE_LABIAL, BT_GA_MANNER_STOP, BT_GA_PLACE_LABIAL,
                                       BT_GA_MANNER_NASAL, &second);
    BT_CHECK_EQ(bt_ga_confusion(&first, &second), 0.20f);
    first = onsets_only(BT_GA_PLACE_LABIAL, BT_GA_MANNER_STOP, BT_GA_PLACE_DENTAL,
                        BT_GA_MANNER_STOP, &second);
    BT_CHECK_EQ(bt_ga_confusion(&first, &second), 0.20f);
    first = onsets_only(BT_GA_PLACE_LABIAL, BT_GA_MANNER_STOP, BT_GA_PLACE_DENTAL,
                        BT_GA_MANNER_NASAL, &second);
    BT_CHECK_EQ(bt_ga_confusion(&first, &second), 0.10f);
    first = onsets_only(BT_GA_PLACE_LABIAL, BT_GA_MANNER_STOP, BT_GA_PLACE_LABIAL,
                        BT_GA_MANNER_STOP, &second);
    BT_CHECK_EQ(bt_ga_confusion(&first, &second), 1.0f);
    first = bt_ga_feat(BT_GA_TONE_NGANG, 0, 0, 0, BT_GA_NUC_A, BT_GA_CODA_OPEN);
    second = bt_ga_feat(BT_GA_TONE_NGANG, 0, 0, 0, BT_GA_NUC_A, BT_GA_CODA_OPEN);
    BT_CHECK_EQ(bt_ga_confusion(&first, &second), 1.0f);
    first = bt_ga_feat(BT_GA_TONE_NGANG, 0, 0, 0, BT_GA_NUC_A, BT_GA_CODA_OPEN);
    second = bt_ga_feat(BT_GA_TONE_NGANG, 1, BT_GA_PLACE_LABIAL, BT_GA_MANNER_STOP, BT_GA_NUC_A,
                        BT_GA_CODA_OPEN);
    BT_CHECK_EQ(bt_ga_confusion(&first, &second), 0.10f);
}

BT_TEST(fuzzkill_ga, confusion_multiplies_all_feature_factors) {
    bt_ga_features x = bt_ga_feat(BT_GA_TONE_HOI, 0, 0, 0, BT_GA_NUC_E, BT_GA_CODA_P);
    bt_ga_features y = bt_ga_feat(BT_GA_TONE_NGA, 0, 0, 0, BT_GA_NUC_EH, BT_GA_CODA_T);
    BT_CHECK_EQ(bt_ga_confusion(&x, &y), 0.70f * 0.40f * 0.50f);
    x = bt_ga_feat(BT_GA_TONE_NGANG, 0, 0, 0, BT_GA_NUC_A, BT_GA_CODA_M);
    y = bt_ga_feat(BT_GA_TONE_HUYEN, 0, 0, 0, BT_GA_NUC_AB, BT_GA_CODA_NG);
    BT_CHECK_EQ(bt_ga_confusion(&x, &y), 0.30f * 0.40f * 0.15f);
}

BT_TEST(fuzzkill_ga, flow_penalty_terms_are_pinned) {
    bt_ga_features x = bt_ga_feat(BT_GA_TONE_SAC, 1, BT_GA_PLACE_LABIAL, BT_GA_MANNER_STOP,
                                  BT_GA_NUC_A, BT_GA_CODA_P);
    bt_ga_features y = bt_ga_feat(BT_GA_TONE_SAC, 1, BT_GA_PLACE_LABIAL, BT_GA_MANNER_NASAL,
                                  BT_GA_NUC_A, BT_GA_CODA_T);
    BT_CHECK_EQ(bt_ga_flow_penalty(&x, 3, &y, 3), 2.0f);
    x = bt_ga_feat(BT_GA_TONE_SAC, 1, BT_GA_PLACE_DENTAL, BT_GA_MANNER_STOP, BT_GA_NUC_A,
                   BT_GA_CODA_T);
    y = bt_ga_feat(BT_GA_TONE_HUYEN, 1, BT_GA_PLACE_LABIAL, BT_GA_MANNER_STOP, BT_GA_NUC_A,
                   BT_GA_CODA_P);
    BT_CHECK_EQ(bt_ga_flow_penalty(&x, 4, &y, 3), 0.25f);
    BT_CHECK_EQ(bt_ga_flow_penalty(&x, 7, &y, 3), 1.0f);
}

BT_TEST(fuzzkill_ga, parse_supplies_onset_features) {
    bt_ga_features ta = bt_ga_parse_or_abort("ta");
    BT_CHECK(ta.has_onset);
    BT_CHECK_EQ((long long)ta.onset.place, (long long)BT_GA_PLACE_DENTAL);
    BT_CHECK_EQ((long long)ta.onset.manner, (long long)BT_GA_MANNER_STOP);
    bt_ga_features ma = bt_ga_parse_or_abort("ma");
    BT_CHECK(ma.has_onset);
    BT_CHECK_EQ((long long)ma.onset.place, (long long)BT_GA_PLACE_LABIAL);
    BT_CHECK_EQ((long long)ma.onset.manner, (long long)BT_GA_MANNER_NASAL);
    bt_ga_features again;
    BT_CHECK(bt_ga_parse("ta", &again));
    BT_CHECK(again.has_onset == ta.has_onset && again.onset.place == ta.onset.place &&
             again.onset.manner == ta.onset.manner && again.nucleus == ta.nucleus &&
             again.coda == ta.coda && again.tone == ta.tone);
}

BT_TEST(fuzzkill_ga, build_env_rejects_mismatched_tables) {
    bt_ga_features f0 = bt_ga_feat(BT_GA_TONE_HOI, 0, 0, 0, BT_GA_NUC_A, BT_GA_CODA_OPEN);
    bt_ga_features f1 = bt_ga_feat(BT_GA_TONE_NGA, 0, 0, 0, BT_GA_NUC_A, BT_GA_CODA_OPEN);
    bt_ga_env env;
    char err[BT_GA_ERR_MAX];
    float *lengths = malloc(2 * sizeof *lengths);
    lengths[0] = 3.0f;
    lengths[1] = 4.0f;
    bt_ga_features *feats = malloc(2 * sizeof *feats);
    feats[0] = f0;
    feats[1] = f1;
    BT_CHECK(bt_ga_build_env(2, lengths, feats, 2, NULL, 0, NULL, 0, &env, err));
    bt_ga_env_free(&env);
    lengths = malloc(2 * sizeof *lengths);
    lengths[0] = 3.0f;
    lengths[1] = 4.0f;
    feats = malloc(1 * sizeof *feats);
    feats[0] = f0;
    BT_CHECK(!bt_ga_build_env(2, lengths, feats, 2, NULL, 0, NULL, 0, &env, err));
    free(lengths);
    free(feats);
}

BT_TEST(fuzzkill_ga, fitness_parts_weights_are_pinned) {
    bt_ga_features feats[2];
    feats[0] = bt_ga_feat(BT_GA_TONE_HOI, 0, 0, 0, BT_GA_NUC_A, BT_GA_CODA_OPEN);
    feats[1] = bt_ga_feat(BT_GA_TONE_NGA, 0, 0, 0, BT_GA_NUC_A, BT_GA_CODA_OPEN);
    float *lengths = malloc(2 * sizeof *lengths);
    lengths[0] = 3.0f;
    lengths[1] = 4.0f;
    bt_ga_pair_weight *cooc = malloc(sizeof *cooc);
    cooc[0].i = 0;
    cooc[0].j = 1;
    cooc[0].w = 1.0f;
    bt_ga_pair_weight *adj = malloc(sizeof *adj);
    adj[0].i = 1;
    adj[0].j = 0;
    adj[0].w = 2.0f;
    bt_ga_env env;
    char err[BT_GA_ERR_MAX];
    if (!bt_ga_build_env(2, lengths, feats, 2, cooc, 1, adj, 1, &env, err)) {
        BT_ABORT("build_env: %s", err);
    }
    BT_CHECK_EQ(env.conf_table[1], 0.70f);
    BT_CHECK_EQ(env.conf_table[2], 0.70f);
    BT_CHECK_EQ(env.conf_table[0], 1.0f);
    BT_CHECK_EQ(env.flow_table[2], 0.25f);
    uint32_t chrom[2] = {0, 1};
    float parts[3];
    bt_ga_fitness_parts(&env, chrom, parts);
    BT_CHECK_EQ(parts[0], 0.70f);
    BT_CHECK_EQ(parts[1], 0.50f);
    BT_CHECK_EQ(parts[2], 3.5f);
    bt_ga_env_free(&env);
}

BT_TEST(fuzzkill_ga, onset_features_maps_every_consonant) {
    static const struct {
        bt_ga_onset onset;
        bt_ga_place place;
        bt_ga_manner manner;
    } rows[] = {
        {BT_GA_ONSET_B, BT_GA_PLACE_LABIAL, BT_GA_MANNER_IMPLOSIVE},
        {BT_GA_ONSET_DD, BT_GA_PLACE_DENTAL, BT_GA_MANNER_IMPLOSIVE},
        {BT_GA_ONSET_T, BT_GA_PLACE_DENTAL, BT_GA_MANNER_STOP},
        {BT_GA_ONSET_TH, BT_GA_PLACE_DENTAL, BT_GA_MANNER_ASPIRATED},
        {BT_GA_ONSET_CH, BT_GA_PLACE_PALATAL, BT_GA_MANNER_AFFRICATE},
        {BT_GA_ONSET_TR, BT_GA_PLACE_RETROFLEX, BT_GA_MANNER_AFFRICATE},
        {BT_GA_ONSET_K, BT_GA_PLACE_VELAR, BT_GA_MANNER_STOP},
        {BT_GA_ONSET_M, BT_GA_PLACE_LABIAL, BT_GA_MANNER_NASAL},
        {BT_GA_ONSET_N, BT_GA_PLACE_DENTAL, BT_GA_MANNER_NASAL},
        {BT_GA_ONSET_NH, BT_GA_PLACE_PALATAL, BT_GA_MANNER_NASAL},
        {BT_GA_ONSET_NG, BT_GA_PLACE_VELAR, BT_GA_MANNER_NASAL},
        {BT_GA_ONSET_PH, BT_GA_PLACE_LABIAL, BT_GA_MANNER_FRICATIVE},
        {BT_GA_ONSET_X, BT_GA_PLACE_DENTAL, BT_GA_MANNER_FRICATIVE},
        {BT_GA_ONSET_S, BT_GA_PLACE_RETROFLEX, BT_GA_MANNER_FRICATIVE},
        {BT_GA_ONSET_KH, BT_GA_PLACE_VELAR, BT_GA_MANNER_FRICATIVE},
        {BT_GA_ONSET_H, BT_GA_PLACE_GLOTTAL, BT_GA_MANNER_FRICATIVE},
        {BT_GA_ONSET_V, BT_GA_PLACE_LABIAL, BT_GA_MANNER_FRICATIVE},
        {BT_GA_ONSET_Z, BT_GA_PLACE_DENTAL, BT_GA_MANNER_FRICATIVE},
        {BT_GA_ONSET_G, BT_GA_PLACE_VELAR, BT_GA_MANNER_FRICATIVE},
        {BT_GA_ONSET_L, BT_GA_PLACE_DENTAL, BT_GA_MANNER_APPROXIMANT},
    };
    for (size_t r = 0; r < sizeof rows / sizeof rows[0]; r++) {
        bt_ga_onset_features features;
        BT_CHECK(bt_ga_onset_features_of(rows[r].onset, &features));
        BT_CHECK_EQ((long long)features.place, (long long)rows[r].place);
        BT_CHECK_EQ((long long)features.manner, (long long)rows[r].manner);
    }
    bt_ga_onset_features features;
    BT_CHECK(!bt_ga_onset_features_of(BT_GA_ONSET_ZERO, &features));
}

BT_TEST(fuzzkill_ga, draw_cuts_stay_bounded_and_spread) {
    int seen[12][12] = {0};
    int distinct = 0;
    int any_cut1_zero = 0;
    int any_cut2_seven = 0;
    for (uint64_t seed = 0; seed < 400; seed++) {
        bt_rng rng;
        bt_rng_seed(&rng, seed);
        for (size_t n = 2; n <= 12; n++) {
            size_t cut1 = 0;
            size_t cut2 = 0;
            bt_ga_draw_cuts(&rng, n, &cut1, &cut2);
            BT_CHECK(cut1 < cut2);
            BT_CHECK(cut2 < n);
            BT_CHECK(cut1 <= n - 2);
            if (n == 8 && !seen[cut1][cut2]) {
                seen[cut1][cut2] = 1;
                distinct++;
            }
            if (n == 8) {
                any_cut1_zero = any_cut1_zero || cut1 == 0;
                any_cut2_seven = any_cut2_seven || cut2 == 7;
            }
        }
    }
    BT_CHECK(distinct >= 10);
    BT_CHECK(any_cut1_zero);
    BT_CHECK(any_cut2_seven);
}

BT_TEST(fuzzkill_ga, tournament_breaks_ties_toward_the_first_drawn_index) {
    float flat[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
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
        size_t winner = bt_ga_tournament(&draw, flat, 8);
        BT_CHECK_EQ((long long)winner, (long long)a);
        (void)b;
    }
}

BT_TEST(fuzzkill_ga, flat_landscape_reports_full_stagnation) {
    bt_ga_features feats[4];
    feats[0] = bt_ga_feat(BT_GA_TONE_HOI, 0, 0, 0, BT_GA_NUC_A, BT_GA_CODA_OPEN);
    feats[1] = bt_ga_feat(BT_GA_TONE_NGA, 0, 0, 0, BT_GA_NUC_E, BT_GA_CODA_P);
    feats[2] = bt_ga_feat(BT_GA_TONE_SAC, 0, 0, 0, BT_GA_NUC_O, BT_GA_CODA_T);
    feats[3] = bt_ga_feat(BT_GA_TONE_HUYEN, 0, 0, 0, BT_GA_NUC_EH, BT_GA_CODA_M);
    float *lengths = malloc(4 * sizeof *lengths);
    for (int i = 0; i < 4; i++) {
        lengths[i] = 2.5f;
    }
    bt_ga_env env;
    char err[BT_GA_ERR_MAX];
    if (!bt_ga_build_env(4, lengths, feats, 2, NULL, 0, NULL, 0, &env, err)) {
        BT_ABORT("build_env: %s", err);
    }
    bt_ga_params params;
    params.seed = 9;
    params.population = 12;
    params.generations = 25;
    params.elite = 2;
    params.swap = 0.2;
    params.replace = 0.1;
    params.w_conf = 1.0f;
    params.w_flow = 0.5f;
    params.w_cplx = 0.1f;
    bt_ga_run_outcome *outcome = bt_ga_run(&env, &params);
    for (size_t i = 1; i < outcome->per_gen_count; i++) {
        BT_CHECK(outcome->per_gen_best[i - 1] == outcome->per_gen_best[i]);
    }
    BT_CHECK_EQ((long long)outcome->stagnation_tail, 25LL);
    BT_CHECK_EQ((long long)outcome->max_stagnation, 25LL);
    BT_CHECK_EQ((long long)outcome->best_generation, 0LL);
    BT_CHECK_EQ((unsigned long long)outcome->evaluations, (unsigned long long)(12 + 25 * (12 - 2)));
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(&env);
}

BT_TEST(fuzzkill_ga, run_trajectory_is_pinned_for_reproducible_inputs) {
    bt_ga_features feats[6];
    feats[0] = bt_ga_feat(BT_GA_TONE_HOI, 0, 0, 0, BT_GA_NUC_A, BT_GA_CODA_OPEN);
    feats[1] = bt_ga_feat(BT_GA_TONE_NGA, 0, 0, 0, BT_GA_NUC_A, BT_GA_CODA_OPEN);
    feats[2] = bt_ga_feat(BT_GA_TONE_SAC, 0, 0, 0, BT_GA_NUC_E, BT_GA_CODA_P);
    feats[3] = bt_ga_feat(BT_GA_TONE_HUYEN, 0, 0, 0, BT_GA_NUC_O, BT_GA_CODA_T);
    feats[4] = bt_ga_feat(BT_GA_TONE_NGANG, 0, 0, 0, BT_GA_NUC_EH, BT_GA_CODA_M);
    feats[5] = bt_ga_feat(BT_GA_TONE_NANG, 0, 0, 0, BT_GA_NUC_OH, BT_GA_CODA_N);
    float *lengths = malloc(6 * sizeof *lengths);
    lengths[0] = 3.0f;
    lengths[1] = 4.0f;
    lengths[2] = 5.0f;
    lengths[3] = 3.0f;
    lengths[4] = 4.0f;
    lengths[5] = 5.0f;
    bt_ga_pair_weight *cooc = malloc(2 * sizeof *cooc);
    cooc[0].i = 0;
    cooc[0].j = 1;
    cooc[0].w = 0.5f;
    cooc[1].i = 1;
    cooc[1].j = 2;
    cooc[1].w = 0.25f;
    bt_ga_pair_weight *adj = malloc(2 * sizeof *adj);
    adj[0].i = 0;
    adj[0].j = 2;
    adj[0].w = 0.5f;
    adj[1].i = 1;
    adj[1].j = 2;
    adj[1].w = 1.0f;
    bt_ga_env env;
    char err[BT_GA_ERR_MAX];
    if (!bt_ga_build_env(6, lengths, feats, 3, cooc, 2, adj, 2, &env, err)) {
        BT_ABORT("build_env: %s", err);
    }
    bt_ga_params params;
    params.seed = 777;
    params.population = 12;
    params.generations = 25;
    params.elite = 2;
    params.swap = 0.2;
    params.replace = 0.1;
    params.w_conf = 1.0f;
    params.w_flow = 0.5f;
    params.w_cplx = 0.1f;
    bt_ga_run_outcome *outcome = bt_ga_run(&env, &params);
    uint32_t want_best[3] = {4, 0, 3};
    BT_CHECK(memcmp(outcome->best, want_best, sizeof want_best) == 0);
    BT_CHECK_EQ((long long)outcome->per_gen_count, 26LL);
    for (size_t i = 0; i < 4; i++) {
        BT_CHECK_F64_NEAR(outcome->per_gen_best[i], 0.39664584f, 0.0);
    }
    for (size_t i = 4; i < 26; i++) {
        BT_CHECK_F64_NEAR(outcome->per_gen_best[i], 0.39602083f, 0.0);
    }
    BT_CHECK_EQ((long long)outcome->best_generation, 4LL);
    BT_CHECK_EQ((long long)outcome->stagnation_tail, 21LL);
    BT_CHECK_EQ((long long)outcome->max_stagnation, 21LL);
    BT_CHECK_EQ((unsigned long long)outcome->evaluations, 262ULL);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(&env);
}

static void inventory_texts_of(const char *const *texts, size_t count, char out[8][16],
                               size_t *out_count) {
    bt_ga_strset wordlist;
    bt_ga_wordlist_from(texts, count, &wordlist);
    bt_ga_inventory_entry *entries = NULL;
    size_t entry_count = 0;
    bt_ga_inventory_report report;
    bt_ga_build_inventory(&wordlist, &entries, &entry_count, &report);
    size_t written = 0;
    for (size_t i = 0; i < entry_count; i++) {
        strcpy(out[written], entries[i].text);
        written++;
    }
    BT_CHECK(written <= 8);
    for (size_t i = 0; i < written; i++) {
        for (size_t j = i + 1; j < written; j++) {
            if (strcmp(out[j], out[i]) < 0) {
                char tmp[16];
                strcpy(tmp, out[i]);
                strcpy(out[i], out[j]);
                strcpy(out[j], tmp);
            }
        }
    }
    *out_count = written;
    bt_ga_inventory_free(entries);
    bt_ga_strset_free(&wordlist);
}

BT_TEST(fuzzkill_ga, stop_coda_syllables_require_sac_or_nang_tone) {
    char texts[8][16];
    size_t count = 0;
    inventory_texts_of((const char *[]){"át", "at", "àn"}, 3, texts, &count);
    BT_CHECK_EQ((long long)count, 2LL);
    BT_CHECK_EQ(texts[0], "àn");
    BT_CHECK_EQ(texts[1], "át");
}

BT_TEST(fuzzkill_ga, front_nuclei_force_k_and_ngh_spellings) {
    char texts[8][16];
    size_t count = 0;
    inventory_texts_of((const char *[]){"ké", "ce", "nghé", "ngé", "ca"}, 5, texts, &count);
    BT_CHECK_EQ((long long)count, 3LL);
    BT_CHECK_EQ(texts[0], "ca");
    BT_CHECK_EQ(texts[1], "ké");
    BT_CHECK_EQ(texts[2], "nghé");
}

BT_TEST(fuzzkill_ga, nucleus_e_keeps_its_rime_outside_velar_codas) {
    char texts[8][16];
    size_t count = 0;
    inventory_texts_of((const char *[]){"en", "an", "ang"}, 3, texts, &count);
    BT_CHECK_EQ((long long)count, 3LL);
    BT_CHECK_EQ(texts[0], "an");
    BT_CHECK_EQ(texts[1], "ang");
    BT_CHECK_EQ(texts[2], "en");
}

BT_TEST(fuzzkill_ga, uoa_nucleus_rejects_only_the_p_coda) {
    char texts[8][16];
    size_t count = 0;
    inventory_texts_of((const char *[]){"uốt", "uôp", "áp"}, 3, texts, &count);
    BT_CHECK_EQ((long long)count, 2LL);
    BT_CHECK_EQ(texts[0], "uốt");
    BT_CHECK_EQ(texts[1], "áp");
}

BT_TEST(fuzzkill_ga, load_wordlist_skips_comments_and_rejects_non_nfc_lines) {
    char dir[BT_PATH_MAX];
    bt_ga_scratch("fuzzkill-wordlist", dir);
    char path[BT_PATH_MAX];
    snprintf(path, BT_PATH_MAX, "%s/words.txt", dir);
    FILE *file = fopen(path, "wb");
    const char *body = "b\xc3\xa0\n# note\n\nla\n#x\n";
    fwrite(body, 1, strlen(body), file);
    fclose(file);
    bt_ga_strset set;
    char err[BT_GA_ERR_MAX];
    if (!bt_ga_load_wordlist(path, &set, err)) {
        BT_ABORT("load failed: %s", err);
    }
    BT_CHECK_EQ((long long)set.count, 2LL);
    BT_CHECK(bt_ga_strset_contains(&set, "b\xc3\xa0"));
    BT_CHECK(bt_ga_strset_contains(&set, "la"));
    bt_ga_strset_free(&set);
    snprintf(path, BT_PATH_MAX, "%s/marked.txt", dir);
    file = fopen(path, "wb");
    const char *marked = "la\nla\xcc\x81\n";
    fwrite(marked, 1, strlen(marked), file);
    fclose(file);
    BT_CHECK(!bt_ga_load_wordlist(path, &set, err));
    BT_CHECK(strstr(err, "combining mark") != NULL);
    bt_ga_scratch_remove("fuzzkill-wordlist");
}

static bt_ga_env *boundary_env(void) {
    bt_ga_features feats[6];
    feats[0] = bt_ga_feat(BT_GA_TONE_HOI, 0, 0, 0, BT_GA_NUC_A, BT_GA_CODA_OPEN);
    feats[1] = bt_ga_feat(BT_GA_TONE_NGA, 0, 0, 0, BT_GA_NUC_A, BT_GA_CODA_OPEN);
    feats[2] = bt_ga_feat(BT_GA_TONE_SAC, 0, 0, 0, BT_GA_NUC_E, BT_GA_CODA_P);
    feats[3] = bt_ga_feat(BT_GA_TONE_HUYEN, 0, 0, 0, BT_GA_NUC_O, BT_GA_CODA_T);
    feats[4] = bt_ga_feat(BT_GA_TONE_NGANG, 0, 0, 0, BT_GA_NUC_EH, BT_GA_CODA_M);
    feats[5] = bt_ga_feat(BT_GA_TONE_NANG, 0, 0, 0, BT_GA_NUC_OH, BT_GA_CODA_N);
    float *lengths = malloc(6 * sizeof *lengths);
    lengths[0] = 3.0f;
    lengths[1] = 4.0f;
    lengths[2] = 5.0f;
    lengths[3] = 3.0f;
    lengths[4] = 4.0f;
    lengths[5] = 5.0f;
    bt_ga_pair_weight *cooc = malloc(3 * sizeof *cooc);
    cooc[0].i = 0;
    cooc[0].j = 1;
    cooc[0].w = 0.5f;
    cooc[1].i = 1;
    cooc[1].j = 2;
    cooc[1].w = 0.25f;
    cooc[2].i = 2;
    cooc[2].j = 3;
    cooc[2].w = 0.1f;
    bt_ga_pair_weight *adj = malloc(3 * sizeof *adj);
    adj[0].i = 0;
    adj[0].j = 2;
    adj[0].w = 0.5f;
    adj[1].i = 1;
    adj[1].j = 3;
    adj[1].w = 1.0f;
    adj[2].i = 3;
    adj[2].j = 2;
    adj[2].w = 0.75f;
    bt_ga_env *env = malloc(sizeof *env);
    char err[BT_GA_ERR_MAX];
    if (!bt_ga_build_env(6, lengths, feats, 4, cooc, 3, adj, 3, env, err)) {
        BT_ABORT("build_env: %s", err);
    }
    return env;
}

static bt_ga_params boundary_params(uint64_t seed, double swap, double replace) {
    bt_ga_params params;
    params.seed = seed;
    params.population = 2;
    params.generations = 1;
    params.elite = 1;
    params.swap = swap;
    params.replace = replace;
    params.w_conf = 1.0f;
    params.w_flow = 0.5f;
    params.w_cplx = 0.1f;
    return params;
}

BT_TEST(fuzzkill_ga, swap_gate_is_strict_at_the_rate_boundary) {
    bt_ga_env *env = boundary_env();
    bt_ga_params at_draw = boundary_params(1, 645334.0 / 1000000.0, 1.0);
    bt_ga_params below_draw = boundary_params(1, 645333.0 / 1000000.0, 1.0);
    bt_ga_run_outcome *at = bt_ga_run(env, &at_draw);
    bt_ga_run_outcome *below = bt_ga_run(env, &below_draw);
    BT_CHECK(at->per_gen_count == below->per_gen_count);
    BT_CHECK(memcmp(at->per_gen_best, below->per_gen_best, at->per_gen_count * sizeof(float)) == 0);
    BT_CHECK(memcmp(at->best, below->best, at->best_count * sizeof(uint32_t)) == 0);
    bt_ga_params fired_params = boundary_params(1, 645335.0 / 1000000.0, 1.0);
    bt_ga_run_outcome *fired = bt_ga_run(env, &fired_params);
    BT_CHECK(memcmp(fired->per_gen_best, at->per_gen_best, at->per_gen_count * sizeof(float)) != 0);
    bt_ga_run_outcome_free(fired);
    bt_ga_run_outcome_free(below);
    bt_ga_run_outcome_free(at);
    bt_ga_env_free(env);
}

BT_TEST(fuzzkill_ga, replace_gate_is_strict_at_the_rate_boundary) {
    bt_ga_env *env = boundary_env();
    bt_ga_params at_draw = boundary_params(1, 0.0, 815350.0 / 1000000.0);
    bt_ga_params below_draw = boundary_params(1, 0.0, 815349.0 / 1000000.0);
    bt_ga_run_outcome *at = bt_ga_run(env, &at_draw);
    bt_ga_run_outcome *below = bt_ga_run(env, &below_draw);
    BT_CHECK(at->per_gen_count == below->per_gen_count);
    BT_CHECK(memcmp(at->per_gen_best, below->per_gen_best, at->per_gen_count * sizeof(float)) == 0);
    BT_CHECK(memcmp(at->best, below->best, at->best_count * sizeof(uint32_t)) == 0);
    bt_ga_params fired_params = boundary_params(1, 0.0, 815351.0 / 1000000.0);
    bt_ga_run_outcome *fired = bt_ga_run(env, &fired_params);
    BT_CHECK(memcmp(fired->per_gen_best, at->per_gen_best, at->per_gen_count * sizeof(float)) != 0);
    bt_ga_run_outcome_free(fired);
    bt_ga_run_outcome_free(below);
    bt_ga_run_outcome_free(at);
    bt_ga_env_free(env);
}
