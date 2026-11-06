#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common/json.h"
#include "common/strbuf.h"
#include "core/alg.h"
#include "gendata/scoring.h"
#include "gendata/synth.h"
#include "scheme/pairs.h"
#include "scheme/scheme.h"

static void scoring_cfg(bt_gd_scoring_cfg *cfg) {
    static const char *text = "{\"synth\": {\"scoring\": {"
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
    bt_json_error jerr;
    bt_json_value *config = NULL;
    if (bt_json_parse(text, strlen(text), &config, &jerr) != 0) {
        fprintf(stderr, "probe: scoring config parse failed\n");
        exit(2);
    }
    char err[128];
    if (bt_gd_scoring_from_config(config, cfg, err, sizeof err) != 0) {
        fprintf(stderr, "probe: scoring config rejected: %s\n", err);
        exit(2);
    }
    bt_json_free(config);
}

static void case_of(char first, char second, uint8_t *t1, uint8_t *t2) {
    bt_case cases[BT_PAIRS_MAX];
    size_t count = bt_scheme_enumerate_cases(BT_ORBIT_CORNER, cases);
    for (size_t i = 0; i < count; i++) {
        char pair[2];
        bt_scheme_case_letters(&cases[i], pair);
        if (pair[0] == first && pair[1] == second) {
            *t1 = cases[i].t1;
            *t2 = cases[i].t2;
            return;
        }
    }
    fprintf(stderr, "probe: pair %c%c not enumerated\n", first, second);
    exit(2);
}

static void run_one(const char *label, size_t max_setup, size_t max_insert, size_t node_budget,
                    const bt_gd_scoring_cfg *cfg, uint8_t t1, uint8_t t2) {
    bt_gd_synth_limits lim;
    lim.max_setup = max_setup;
    lim.max_insert = max_insert;
    lim.node_budget = node_budget;
    bt_gd_synth_hit hit;
    clock_t started = clock();
    bool found = bt_gd_synth_case(BT_ORBIT_CORNER, t1, t2, &lim, cfg, &hit);
    double secs = (double)(clock() - started) / CLOCKS_PER_SEC;
    if (found) {
        bt_strbuf text;
        bt_strbuf_init(&text, 0);
        bt_alg_format(&hit.alg, &text);
        printf("%-18s found in %7.1fs qtm=%u %s [%s]\n", label, secs, bt_alg_qtm(&hit.alg),
               text.data, hit.commutator);
        bt_strbuf_free(&text);
        bt_gd_synth_hit_free(&hit);
    } else {
        printf("%-18s none   in %7.1fs\n", label, secs);
    }
    fflush(stdout);
}

int main(void) {
    uint8_t t1;
    uint8_t t2;
    case_of('N', 'I', &t1, &t2);
    printf("probe: corner N-I is t1=%u t2=%u %s-%s\n", t1, t2, bt_scheme_name_of(t1),
           bt_scheme_name_of(t2));
    bt_gd_scoring_cfg cfg;
    scoring_cfg(&cfg);
    run_one("hub 3,4,50M", 3, 4, 50000000, &cfg, t1, t2);
    run_one("budget 3,4,100M", 3, 4, 100000000, &cfg, t1, t2);
    run_one("insert 3,5,50M", 3, 5, 50000000, &cfg, t1, t2);
    run_one("setup 4,4,50M", 4, 4, 50000000, &cfg, t1, t2);
    return 0;
}
