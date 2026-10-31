#include "ga/alloc.h"
#include "ga/dist.h"
#include "ga/features.h"
#include "ga/ga.h"
#include "ga/inventory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROBE_SITES_MAX 64
#define PROBE_SITE_LEN 64

typedef struct {
    size_t calls;
    size_t frees;
    size_t live;
    size_t peak;
    char fail_site[PROBE_SITE_LEN];
    char seen[PROBE_SITES_MAX][PROBE_SITE_LEN];
    size_t seen_count;
} probe_ctx;

static void probe_note_site(probe_ctx *ctx, const char *site) {
    for (size_t i = 0; i < ctx->seen_count; i++) {
        if (strcmp(ctx->seen[i], site) == 0) {
            return;
        }
    }
    if (ctx->seen_count < PROBE_SITES_MAX) {
        snprintf(ctx->seen[ctx->seen_count], PROBE_SITE_LEN, "%s", site);
        ctx->seen_count++;
    }
}

static void *probe_alloc(size_t bytes, const char *site, void *user) {
    probe_ctx *ctx = user;
    ctx->calls++;
    probe_note_site(ctx, site);
    if (ctx->fail_site[0] != '\0' && strcmp(ctx->fail_site, site) == 0) {
        return NULL;
    }
    void *block = malloc(bytes);
    if (block != NULL) {
        ctx->live++;
        if (ctx->live > ctx->peak) {
            ctx->peak = ctx->live;
        }
    }
    return block;
}

static void probe_free(void *block, void *user) {
    probe_ctx *ctx = user;
    if (block != NULL) {
        ctx->live--;
        ctx->frees++;
    }
    free(block);
}

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

static bt_ga_env *probe_env(size_t case_count) {
    bt_ga_features *feats = calloc(8, sizeof *feats);
    float *lengths = calloc(8, sizeof *lengths);
    for (size_t i = 0; i < 8; i++) {
        if (!bt_ga_parse(POOL8[i], &feats[i])) {
            fprintf(stderr, "parse failed for %s\n", POOL8[i]);
            exit(1);
        }
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
        fprintf(stderr, "build_env failed: %s\n", err);
        exit(1);
    }
    return env;
}

static bt_ga_params probe_params(void) {
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

static void write_fixtures(void) {
    FILE *words = fopen("probe-words.txt", "wb");
    if (words == NULL) {
        exit(1);
    }
    fputs("ma\nmá\nmà\nla\n", words);
    fclose(words);
    FILE *freqs = fopen("probe-freqs.tsv", "wb");
    if (freqs == NULL) {
        exit(1);
    }
    fputs("1\t5\tma má\n2\t3\tmà la\n", freqs);
    fclose(freqs);
    FILE *freqjson = fopen("probe-freq.json", "wb");
    if (freqjson == NULL) {
        exit(1);
    }
    fputs("{\"cases\":[{\"orbit\":\"cv\",\"letters\":\"ma\",\"count\":4},"
          "{\"orbit\":\"cv\",\"letters\":\"la\",\"count\":3}],"
          "\"adjacent\":[{\"from\":0,\"to\":1,\"count\":2}],"
          "\"cooccurring\":[{\"from\":1,\"to\":2,\"count\":5}],"
          "\"total_draws\":9,\"seed\":11}",
          freqjson);
    fclose(freqjson);
}

static void run_mode(const char *mode, probe_ctx *ctx) {
    char err[BT_GA_ERR_MAX];
    if (strcmp(mode, "envonly") == 0) {
        static const char *const words[4] = {"ma", "má", "mà", "mã"};
        bt_ga_features feats[4];
        float lengths[4] = {2.0F, 2.0F, 2.0F, 2.0F};
        for (size_t i = 0; i < 4; i++) {
            if (!bt_ga_parse(words[i], &feats[i])) {
                fprintf(stderr, "parse failed for %s\n", words[i]);
                exit(1);
            }
        }
        bt_ga_env env;
        int ok = bt_ga_build_env(4, lengths, 4, feats, 4, 3, NULL, 0, NULL, 0, &env, err);
        printf("envonly ok=%d err=%s\n", ok, ok ? "-" : err);
        if (ok) {
            bt_ga_env_free(&env);
        }
        return;
    }
    if (strcmp(mode, "loaders") == 0) {
        write_fixtures();
        bt_ga_strset wordlist;
        int wl = bt_ga_load_wordlist("probe-words.txt", &wordlist, err);
        printf("loaders wordlist=%d err=%s live=%zu\n", wl, wl ? "-" : err, ctx->live);
        if (wl) {
            bt_ga_strset_free(&wordlist);
        }
        bt_ga_freqs freqs;
        int fq = bt_ga_load_syllable_frequencies("probe-freqs.tsv", &freqs, err);
        printf("loaders freqs=%d err=%s live=%zu\n", fq, fq ? "-" : err, ctx->live);
        if (fq) {
            bt_ga_freqs_free(&freqs);
        }
        bt_ga_freq_data data;
        int fd = bt_ga_load_freq_data("probe-freq.json", &data, err);
        printf("loaders freqjson=%d err=%s live=%zu\n", fd, fd ? "-" : err, ctx->live);
        if (fd) {
            bt_ga_freq_data_free(&data);
        }
        bt_ga_strset set;
        set.items = calloc(4, sizeof *set.items);
        set.count = 4;
        set.items[0] = _strdup("ma");
        set.items[1] = _strdup("má");
        set.items[2] = _strdup("mà");
        set.items[3] = _strdup("la");
        bt_ga_inventory_entry *entries = NULL;
        size_t count = 0;
        bt_ga_inventory_report report;
        int inv = bt_ga_build_inventory(&set, &entries, &count, &report, err);
        printf("loaders inventory=%d err=%s live=%zu\n", inv, inv ? "-" : err, ctx->live);
        if (inv) {
            bt_ga_freqs pool_freqs;
            pool_freqs.keys = calloc(2, sizeof *pool_freqs.keys);
            pool_freqs.values = calloc(2, sizeof *pool_freqs.values);
            pool_freqs.count = 2;
            pool_freqs.keys[0] = _strdup("ma");
            pool_freqs.keys[1] = _strdup("la");
            pool_freqs.values[0] = 5;
            pool_freqs.values[1] = 3;
            bt_ga_pool_entry *pool = NULL;
            size_t pool_count = 0;
            bt_ga_build_pool(entries, count, &pool_freqs, 4, &pool, &pool_count);
            printf("loaders pool_count=%zu live=%zu\n", pool_count, ctx->live);
            bt_ga_pool_free(pool);
            free(pool_freqs.keys[0]);
            free(pool_freqs.keys[1]);
            free(pool_freqs.keys);
            free(pool_freqs.values);
            bt_ga_inventory_free(entries);
        }
        for (size_t i = 0; i < set.count; i++) {
            free(set.items[i]);
        }
        free(set.items);
        return;
    }
    bt_ga_env *env = probe_env(5);
    bt_ga_params params = probe_params();
    bt_ga_upgrades upgrades;
    memset(&upgrades, 0, sizeof upgrades);
    if (strcmp(mode, "upgraded") == 0) {
        upgrades.island_count = 3;
        upgrades.migrate_every = 1;
        upgrades.migrate_count = 2;
        upgrades.stagnation_threshold = 1;
        upgrades.swap_scale = 1.5;
        upgrades.replace_scale = 0.5;
        upgrades.diversity_threshold = 0.9;
        upgrades.inject_fraction = 0.5;
    } else if (strcmp(mode, "fraction") == 0) {
        upgrades.elite_fraction_policy = 1;
        upgrades.elite_fraction = 0.34;
    }
    bt_ga_run_outcome *outcome = bt_ga_run_ex(env, &params, &upgrades);
    printf("run mode=%s evals=%llu best_gen=%zu\n", mode,
           (unsigned long long)outcome->evaluations, outcome->best_generation);
    bt_ga_run_outcome_free(outcome);
    bt_ga_env_free(env);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: faultprobe <mode> [fail_site]\n");
        return 2;
    }
    probe_ctx ctx;
    memset(&ctx, 0, sizeof ctx);
    if (argc >= 3) {
        snprintf(ctx.fail_site, sizeof ctx.fail_site, "%s", argv[2]);
    }
    bt_ga_alloc_install(probe_alloc, probe_free, &ctx);
    run_mode(argv[1], &ctx);
    bt_ga_alloc_reset();
    printf("probe allocs=%zu frees=%zu live=%zu peak=%zu sites=%zu\n", ctx.calls, ctx.frees,
           ctx.live, ctx.peak, ctx.seen_count);
    for (size_t i = 0; i < ctx.seen_count; i++) {
        printf("site %s\n", ctx.seen[i]);
    }
    return 0;
}
