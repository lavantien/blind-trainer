#include "ga/alloc.h"
#include "ga/ga.h"
#include "ga/inventory.h"

#include "common/bt_harness.h"

#include "death_test.h"
#include "ga_test_util.h"

typedef struct {
    size_t calls;
    size_t fail_at;
    char fail_site[48];
    size_t live;
    char last_site[48];
} fault_ctx;

static void *fault_alloc(size_t bytes, const char *site, void *user) {
    fault_ctx *ctx = user;
    ctx->calls++;
    snprintf(ctx->last_site, sizeof ctx->last_site, "%s", site);
    int hit = (ctx->fail_at != 0 && ctx->calls == ctx->fail_at) ||
              (ctx->fail_site[0] != '\0' && strcmp(ctx->fail_site, site) == 0);
    if (hit) {
        return NULL;
    }
    void *block = malloc(bytes);
    if (block != NULL) {
        ctx->live++;
    }
    return block;
}

static void fault_free(void *block, void *user) {
    fault_ctx *ctx = user;
    if (block != NULL) {
        ctx->live--;
    }
    free(block);
}

static void install_fault_ctx(fault_ctx *ctx) {
    memset(ctx, 0, sizeof *ctx);
    bt_ga_alloc_install(fault_alloc, fault_free, ctx);
}

static const char *const POOL8[] = {"ả", "ã", "không", "lông", "tía", "mút", "án", "đặt"};

static bt_ga_env *fault_env(size_t case_count) {
    bt_ga_features *feats = calloc(8, sizeof *feats);
    float *lengths = calloc(8, sizeof *lengths);
    for (size_t i = 0; i < 8; i++) {
        feats[i] = bt_ga_parse_or_abort(POOL8[i]);
        lengths[i] = 2.0F;
    }
    bt_ga_env *env = malloc(sizeof *env);
    char err[BT_GA_ERR_MAX];
    if (!bt_ga_build_env(8, lengths, 8, feats, 8, case_count, NULL, 0, NULL, 0, env, err)) {
        BT_ABORT("build_env: %s", err);
    }
    return env;
}

static bt_ga_params fault_params(void) {
    bt_ga_params params;
    params.seed = 424242;
    params.population = 6;
    params.generations = 3;
    params.elite = 2;
    params.swap = 0.15;
    params.replace = 0.05;
    params.w_conf = 1.0F;
    params.w_flow = 0.5F;
    params.w_cplx = 0.1F;
    return params;
}

BT_TEST(ga_alloc_fault, hang_watchdog_arms_for_this_binary) {
    bt_ga_arm_hang_watchdog();
}

BT_TEST(ga_alloc_fault, seam_leaks_nothing_over_a_full_run) {
    fault_ctx ctx;
    install_fault_ctx(&ctx);
    bt_ga_env *env = fault_env(5);
    bt_ga_params params = fault_params();
    bt_ga_run_outcome *outcome = bt_ga_run(env, &params);
    BT_CHECK(outcome != NULL);
    size_t run_calls = ctx.calls;
    size_t peak_live = ctx.live;
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
    BT_CHECK(ctx.calls == run_calls);
    BT_CHECK(ctx.live == 0);
    BT_CHECK(peak_live > 0);
    bt_ga_alloc_reset();
}

BT_TEST(ga_alloc_fault, reset_restores_the_libc_allocator) {
    fault_ctx ctx;
    install_fault_ctx(&ctx);
    bt_ga_alloc_reset();
    bt_ga_env *env = fault_env(5);
    bt_ga_params params = fault_params();
    bt_ga_run_outcome *outcome = bt_ga_run(env, &params);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
    BT_CHECK_EQ(ctx.calls, (size_t)0);
}

BT_TEST(ga_alloc_fault, build_env_names_each_failed_table) {
    fault_ctx ctx;
    install_fault_ctx(&ctx);
    snprintf(ctx.fail_site, sizeof ctx.fail_site, "ga.build_env.conf_table");
    bt_ga_features feats[4];
    feats[0] = bt_ga_parse_or_abort("ma");
    feats[1] = bt_ga_parse_or_abort("má");
    feats[2] = bt_ga_parse_or_abort("mà");
    feats[3] = bt_ga_parse_or_abort("mã");
    float lengths[4] = {2.0F, 2.0F, 2.0F, 2.0F};
    bt_ga_env env;
    char err[BT_GA_ERR_MAX];
    BT_CHECK(!bt_ga_build_env(4, lengths, 4, feats, 4, 3, NULL, 0, NULL, 0, &env, err));
    BT_CHECK(strstr(err, "conf table allocation failed") != NULL);
    ctx.fail_at = 0;
    snprintf(ctx.fail_site, sizeof ctx.fail_site, "ga.build_env.flow_table");
    BT_CHECK(!bt_ga_build_env(4, lengths, 4, feats, 4, 3, NULL, 0, NULL, 0, &env, err));
    BT_CHECK(strstr(err, "flow table allocation failed") != NULL);
    BT_CHECK(ctx.live == 0);
    bt_ga_alloc_reset();
}

BT_TEST(ga_alloc_fault, build_inventory_names_each_failed_site) {
    fault_ctx ctx;
    install_fault_ctx(&ctx);
    bt_ga_strset wordlist;
    bt_ga_wordlist_from((const char *[]){"ma", "má", "mà", "la"}, 4, &wordlist);
    bt_ga_inventory_entry *entries = NULL;
    size_t count = 0;
    bt_ga_inventory_report report;
    char err[BT_GA_ERR_MAX];
    static const char *const sites[4] = {
        "ga.build_inventory.sorted",
        "ga.build_inventory.list",
        "ga.build_inventory.covered",
        "ga.build_inventory.word_copy",
    };
    static const char *const messages[4] = {
        "inventory sorted allocation failed",
        "inventory list allocation failed",
        "inventory covered allocation failed",
        "inventory word copy allocation failed",
    };
    for (size_t i = 0; i < 4; i++) {
        ctx.fail_at = 0;
        snprintf(ctx.fail_site, sizeof ctx.fail_site, "%s", sites[i]);
        BT_CHECK(!bt_ga_build_inventory(&wordlist, &entries, &count, &report, err));
        BT_CHECK(strstr(err, messages[i]) != NULL);
    }
    for (size_t i = 0; i < wordlist.count; i++) {
        free(wordlist.items[i]);
    }
    free(wordlist.items);
    BT_CHECK(ctx.live == 0);
    bt_ga_alloc_reset();
}

BT_TEST(ga_alloc_fault, build_inventory_names_each_failed_growth_site) {
    bt_ga_strset wordlist;
    if (!bt_ga_full_wordlist(&wordlist)) {
        return;
    }
    fault_ctx ctx;
    install_fault_ctx(&ctx);
    bt_ga_inventory_entry *entries = NULL;
    size_t count = 0;
    bt_ga_inventory_report report;
    char err[BT_GA_ERR_MAX];
    static const char *const sites[2] = {
        "ga.build_inventory.list_grow",
        "ga.build_inventory.covered_grow",
    };
    static const char *const messages[2] = {
        "inventory list growth allocation failed",
        "inventory covered growth allocation failed",
    };
    for (size_t i = 0; i < 2; i++) {
        ctx.fail_at = 0;
        snprintf(ctx.fail_site, sizeof ctx.fail_site, "%s", sites[i]);
        BT_CHECK(!bt_ga_build_inventory(&wordlist, &entries, &count, &report, err));
        BT_CHECK(strstr(err, messages[i]) != NULL);
    }
    BT_CHECK(ctx.live == 0);
    bt_ga_alloc_reset();
    bt_ga_strset_free(&wordlist);
}

BT_TEST(ga_alloc_fault, wordlist_loader_reports_entry_allocation_failures) {
    fault_ctx ctx;
    install_fault_ctx(&ctx);
    char dir[BT_PATH_MAX];
    bt_ga_vietnamese_dir(dir);
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/syllables-7184-abc.txt", dir);
    bt_ga_strset wordlist;
    char err[BT_GA_ERR_MAX];
    ctx.fail_at = 3;
    int got = bt_ga_load_wordlist(path, &wordlist, err);
    BT_CHECK(got == 0);
    BT_CHECK(strstr(err, "wordlist entry allocation failed at entry") != NULL);
    snprintf(ctx.fail_site, sizeof ctx.fail_site, "ga.strset.items");
    got = bt_ga_load_wordlist(path, &wordlist, err);
    BT_CHECK(got == 0);
    BT_CHECK(strstr(err, "wordlist items allocation failed at entry") != NULL);
    ctx.fail_at = 0;
    snprintf(ctx.fail_site, sizeof ctx.fail_site, "ga.read_file.buffer");
    got = bt_ga_load_wordlist(path, &wordlist, err);
    BT_CHECK(got == 0);
    BT_CHECK(strstr(err, "allocation failed reading") != NULL);
    BT_CHECK(ctx.live == 0);
    bt_ga_alloc_reset();
}

BT_TEST(ga_alloc_fault, freq_loader_reports_entry_allocation_failures) {
    fault_ctx ctx;
    install_fault_ctx(&ctx);
    char dir[BT_PATH_MAX];
    bt_ga_vietnamese_dir(dir);
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/vn-word-frequencies.tsv", dir);
    FILE *probe = fopen(path, "rb");
    if (probe == NULL) {
        bt_ga_alloc_reset();
        return;
    }
    fclose(probe);
    bt_ga_freqs freqs;
    char err[BT_GA_ERR_MAX];
    static const char *const sites[3] = {
        "ga.freqs.entry",
        "ga.freqs.keys",
        "ga.freqs.values",
    };
    static const char *const messages[3] = {
        "frequency entry allocation failed at row",
        "frequency keys allocation failed at row",
        "frequency values allocation failed at row",
    };
    for (size_t i = 0; i < 3; i++) {
        snprintf(ctx.fail_site, sizeof ctx.fail_site, "%s", sites[i]);
        int got = bt_ga_load_syllable_frequencies(path, &freqs, err);
        BT_CHECK(got == 0);
        BT_CHECK(strstr(err, messages[i]) != NULL);
    }
    BT_CHECK(ctx.live == 0);
    bt_ga_alloc_reset();
}

static void die_on_run_scores(void) {
    fault_ctx ctx;
    install_fault_ctx(&ctx);
    snprintf(ctx.fail_site, sizeof ctx.fail_site, "ga.run.scores");
    bt_ga_env *env = fault_env(5);
    bt_ga_params params = fault_params();
    bt_ga_run_outcome *outcome = bt_ga_run(env, &params);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

static void die_on_run_individual(void) {
    fault_ctx ctx;
    install_fault_ctx(&ctx);
    snprintf(ctx.fail_site, sizeof ctx.fail_site, "ga.run.individual");
    bt_ga_env *env = fault_env(5);
    bt_ga_params params = fault_params();
    bt_ga_run_outcome *outcome = bt_ga_run(env, &params);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

static void die_on_ox_fill(void) {
    fault_ctx ctx;
    install_fault_ctx(&ctx);
    snprintf(ctx.fail_site, sizeof ctx.fail_site, "ga.ox.fill");
    bt_ga_env *env = fault_env(5);
    bt_ga_params params = fault_params();
    bt_ga_run_outcome *outcome = bt_ga_run(env, &params);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

static void die_on_mutate_replace_used(void) {
    fault_ctx ctx;
    install_fault_ctx(&ctx);
    snprintf(ctx.fail_site, sizeof ctx.fail_site, "ga.mutate_replace.used");
    bt_ga_env *env = fault_env(5);
    bt_ga_params always = fault_params();
    always.replace = 1.0;
    bt_ga_run_outcome *outcome = bt_ga_run(env, &always);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

static void die_on_build_pool_ranked(void) {
    fault_ctx ctx;
    install_fault_ctx(&ctx);
    snprintf(ctx.fail_site, sizeof ctx.fail_site, "ga.build_pool.ranked");
    bt_ga_inventory_entry entries[2];
    snprintf(entries[0].text, BT_GA_TEXT_MAX, "ma");
    snprintf(entries[1].text, BT_GA_TEXT_MAX, "má");
    bt_ga_freqs freqs;
    freqs.keys = calloc(2, sizeof *freqs.keys);
    freqs.values = calloc(2, sizeof *freqs.values);
    freqs.count = 2;
    freqs.keys[0] = _strdup("ma");
    freqs.keys[1] = _strdup("má");
    freqs.values[0] = 5;
    freqs.values[1] = 3;
    bt_ga_pool_entry *pool = NULL;
    size_t pool_count = 0;
    bt_ga_build_pool(entries, 2, &freqs, 2, &pool, &pool_count);
}

BT_TEST(ga_alloc_fault, run_and_operators_abort_at_the_named_sites) {
    bt_ga_death_expect("die_on_run_scores", "allocation failed at ga.run.scores",
                       die_on_run_scores);
    bt_ga_death_expect("die_on_run_individual", "allocation failed at ga.run.individual",
                       die_on_run_individual);
    bt_ga_death_expect("die_on_ox_fill", "allocation failed at ga.ox.fill", die_on_ox_fill);
    bt_ga_death_expect("die_on_mutate_replace_used", "allocation failed at ga.mutate_replace.used",
                       die_on_mutate_replace_used);
    bt_ga_death_expect("die_on_build_pool_ranked", "allocation failed at ga.build_pool.ranked",
                       die_on_build_pool_ranked);
}

static void die_on_configured_run_site(void) {
    fault_ctx ctx;
    install_fault_ctx(&ctx);
    const char *site = getenv("BT_FAULT_SITE");
    snprintf(ctx.fail_site, sizeof ctx.fail_site, "%s", site != NULL ? site : "");
    bt_ga_env *env = fault_env(5);
    bt_ga_params params = fault_params();
    bt_ga_upgrades upgrades;
    memset(&upgrades, 0, sizeof upgrades);
    upgrades.island_count = 2;
    upgrades.migrate_every = 1;
    upgrades.migrate_count = 1;
    upgrades.stagnation_threshold = 1;
    upgrades.swap_scale = 1.5;
    upgrades.replace_scale = 0.5;
    upgrades.diversity_threshold = 0.9;
    upgrades.inject_fraction = 0.5;
    bt_ga_run_outcome *outcome = bt_ga_run_ex(env, &params, &upgrades);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

BT_TEST(ga_alloc_fault, every_run_site_aborts_with_its_own_label) {
    static const char *const sites[] = {
        "ga.run.scratch",    "ga.run.islands",         "ga.run.stagnation", "ga.run.island_best",
        "ga.run.population", "ga.run.order",           "ga.run.best",       "ga.run.per_gen",
        "ga.run.next",       "ga.run.is_elite",        "ga.run.migrants",   "ga.run.migrant_scores",
        "ga.run.migrant",    "ga.run.diversity_order", "ga.run.child",      "ga.run.outcome",
    };
    char want[96];
    for (size_t i = 0; i < sizeof sites / sizeof sites[0]; i++) {
        char setting[64];
        snprintf(setting, sizeof setting, "%s", sites[i]);
        _putenv_s("BT_FAULT_SITE", setting);
        char case_name[80];
        const char *tag = sites[i] + strlen("ga.run.");
        snprintf(case_name, sizeof case_name, "die_on_run_site_%s", tag);
        snprintf(want, sizeof want, "allocation failed at %s", sites[i]);
        bt_ga_death_expect(case_name, want, die_on_configured_run_site);
    }
    _putenv("BT_FAULT_SITE=");
}
