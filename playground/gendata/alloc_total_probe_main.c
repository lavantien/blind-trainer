#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common/alloc.h"
#include "common/json.h"
#include "gendata/scoring.h"
#include "gendata/synth.h"
#include "scheme/pairs.h"
#include "scheme/scheme.h"

enum {
    PROBE_BUDGET_CAP = 1U << 22U,
    PROBE_NODE_BUDGET = 100000000,
};

static const char *SCORING_TEXT = "{\"synth\": {\"scoring\": {"
                                  "\"preferred\": [\"U\", \"D\", \"R\"],"
                                  "\"preferred_edge\": [\"U\", \"D\", \"R\", \"M\", \"E\", \"S\"],"
                                  "\"penalty_other\": 2.0,"
                                  "\"weight_len\": 1.0,"
                                  "\"weight_subset\": 2.0,"
                                  "\"weight_regrip\": 1.5,"
                                  "\"weight_finger\": 0.5,"
                                  "\"finger_cost\": {"
                                  "\"U\": [0.2, 0.2], \"D\": [0.2, 0.2],"
                                  "\"L\": [0.4, 0.4], \"R\": [0.4, 0.4],"
                                  "\"F\": [0.8, 0.8], \"B\": [0.8, 0.8]"
                                  "}}}}";

typedef struct {
    bt_orbit orbit;
    char first;
    char second;
    const char *name;
} probe_pair;

static bt_gd_scoring_cfg probe_cfg;

static void load_scoring(void) {
    bt_json_error jerr;
    bt_json_value *config = NULL;
    if (bt_json_parse(SCORING_TEXT, strlen(SCORING_TEXT), &config, &jerr) != 0) {
        fprintf(stderr, "scoring parse failed\n");
        exit(2);
    }
    char err[128];
    if (bt_gd_scoring_from_config(config, &probe_cfg, err, sizeof err) != 0) {
        fprintf(stderr, "scoring load failed: %s\n", err);
        exit(2);
    }
    bt_json_free(config);
}

static void pair_ids(const probe_pair *pair, uint8_t *t1, uint8_t *t2) {
    bt_case cases[BT_PAIRS_MAX];
    size_t count = bt_scheme_enumerate_cases(pair->orbit, cases);
    for (size_t i = 0; i < count; i++) {
        char letters[2];
        bt_scheme_case_letters(&cases[i], letters);
        if (letters[0] == pair->first && letters[1] == pair->second) {
            *t1 = cases[i].t1;
            *t2 = cases[i].t2;
            return;
        }
    }
    fprintf(stderr, "pair %s missing\n", pair->name);
    exit(2);
}

static bool run_armed(bt_orbit orbit, uint8_t t1, uint8_t t2, size_t max_setup, size_t max_insert,
                      size_t node_budget, size_t ok_count, size_t *failures) {
    bt_gd_synth_limits lim = {
        .max_setup = max_setup,
        .max_insert = max_insert,
        .node_budget = node_budget,
    };
    bt_gd_synth_hit hit;
    bt_alloc_fail_arm(ok_count);
    bool ok = bt_gd_synth_case(orbit, t1, t2, &lim, &probe_cfg, &hit);
    *failures = bt_alloc_fail_count();
    bt_alloc_fail_disarm();
    if (ok) {
        bt_gd_synth_hit_free(&hit);
    }
    return ok;
}

static size_t probe_cell_total(bt_orbit orbit, uint8_t t1, uint8_t t2, size_t max_setup,
                               size_t max_insert, size_t node_budget, double *ms) {
    size_t failures = 0;
    double start = (double)clock() / CLOCKS_PER_SEC;
    bool ok =
        run_armed(orbit, t1, t2, max_setup, max_insert, node_budget, PROBE_BUDGET_CAP, &failures);
    *ms = ((double)clock() / CLOCKS_PER_SEC - start) * 1000.0;
    if (!ok) {
        return 0;
    }
    size_t hi = 1;
    while (hi < PROBE_BUDGET_CAP) {
        run_armed(orbit, t1, t2, max_setup, max_insert, node_budget, hi, &failures);
        if (failures == 0) {
            break;
        }
        hi *= 2;
    }
    size_t lo = 1;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        run_armed(orbit, t1, t2, max_setup, max_insert, node_budget, mid, &failures);
        if (failures == 0) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    return lo;
}

int main(void) {
    load_scoring();
    static const probe_pair pairs[] = {
        {BT_ORBIT_EDGE, 'A', 'B', "edge-AB"},
        {BT_ORBIT_EDGE, 'F', 'G', "edge-FG"},
        {BT_ORBIT_CORNER, 'A', 'B', "corner-AB"},
        {BT_ORBIT_CORNER, 'E', 'F', "corner-EF"},
    };
    static const size_t setups[] = {0, 1};
    static const size_t inserts[] = {0, 1, 2, 3, 4};
    printf("%-10s %6s %6s %12s %8s\n", "pair", "setup", "insert", "allocs", "ms");
    for (size_t p = 0; p < sizeof pairs / sizeof pairs[0]; p++) {
        uint8_t t1 = 0;
        uint8_t t2 = 0;
        pair_ids(&pairs[p], &t1, &t2);
        for (size_t s = 0; s < sizeof setups / sizeof setups[0]; s++) {
            for (size_t i = 0; i < sizeof inserts / sizeof inserts[0]; i++) {
                double ms = 0;
                size_t total = probe_cell_total(pairs[p].orbit, t1, t2, setups[s], inserts[i],
                                                PROBE_NODE_BUDGET, &ms);
                if (total > 0) {
                    printf("%-10s %6zu %6zu %12zu %8.0f\n", pairs[p].name, setups[s], inserts[i],
                           total, ms);
                } else {
                    printf("%-10s %6zu %6zu %12s %8.0f\n", pairs[p].name, setups[s], inserts[i],
                           "-", ms);
                }
                fflush(stdout);
            }
        }
    }
    printf("%-10s %6s %6s %12s %8s\n", "pair", "setup", "budget", "allocs", "ms");
    static const size_t fg_budgets[] = {500, 1000, 2000, 4000};
    static const size_t ab_budgets[] = {25000, 40000, 60000, 80000};
    static const struct {
        size_t pair;
        size_t max_setup;
        const size_t *budgets;
        size_t budget_count;
    } sweep[] = {
        {1, 0, fg_budgets, sizeof fg_budgets / sizeof fg_budgets[0]},
        {2, 1, ab_budgets, sizeof ab_budgets / sizeof ab_budgets[0]},
    };
    for (size_t c = 0; c < sizeof sweep / sizeof sweep[0]; c++) {
        uint8_t t1 = 0;
        uint8_t t2 = 0;
        pair_ids(&pairs[sweep[c].pair], &t1, &t2);
        for (size_t b = 0; b < sweep[c].budget_count; b++) {
            size_t budget = sweep[c].budgets[b];
            double ms = 0;
            size_t total = probe_cell_total(pairs[sweep[c].pair].orbit, t1, t2, sweep[c].max_setup,
                                            3, budget, &ms);
            if (total > 0) {
                printf("%-10s %6zu %6zu %12zu %8.0f\n", pairs[sweep[c].pair].name,
                       sweep[c].max_setup, budget, total, ms);
            } else {
                printf("%-10s %6zu %6zu %12s %8.0f\n", pairs[sweep[c].pair].name,
                       sweep[c].max_setup, budget, "-", ms);
            }
            fflush(stdout);
        }
    }
    return 0;
}
