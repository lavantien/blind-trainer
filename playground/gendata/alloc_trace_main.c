#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/alloc.h"
#include "common/json.h"
#include "gendata/scoring.h"
#include "gendata/synth.h"
#include "scheme/pairs.h"
#include "scheme/scheme.h"

size_t bt_alloc_trace_total(void);
void bt_alloc_trace_dump(const char *path);

enum {
    TRACE_NODE_BUDGET = 100000000,
    TRACE_MAX_SETUP = 1,
    TRACE_MAX_INSERT = 3,
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

static void trace_pair_ids(bt_orbit orbit, char first, char second, uint8_t *t1, uint8_t *t2) {
    bt_case cases[BT_PAIRS_MAX];
    size_t count = bt_scheme_enumerate_cases(orbit, cases);
    for (size_t i = 0; i < count; i++) {
        char letters[2];
        bt_scheme_case_letters(&cases[i], letters);
        if (letters[0] == first && letters[1] == second) {
            *t1 = cases[i].t1;
            *t2 = cases[i].t2;
            return;
        }
    }
    fprintf(stderr, "pair %c%c missing\n", first, second);
    exit(2);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: alloc_trace <dump-path>\n");
        return 2;
    }
    bt_json_error jerr;
    bt_json_value *config = NULL;
    if (bt_json_parse(SCORING_TEXT, strlen(SCORING_TEXT), &config, &jerr) != 0) {
        fprintf(stderr, "scoring parse failed\n");
        return 2;
    }
    bt_gd_scoring_cfg cfg;
    char err[128];
    if (bt_gd_scoring_from_config(config, &cfg, err, sizeof err) != 0) {
        fprintf(stderr, "scoring load failed: %s\n", err);
        return 2;
    }
    bt_json_free(config);
    uint8_t t1 = 0;
    uint8_t t2 = 0;
    trace_pair_ids(BT_ORBIT_CORNER, 'A', 'B', &t1, &t2);
    bt_gd_synth_limits lim = {
        .max_setup = TRACE_MAX_SETUP,
        .max_insert = TRACE_MAX_INSERT,
        .node_budget = TRACE_NODE_BUDGET,
    };
    bt_gd_synth_hit hit;
    size_t start = bt_alloc_trace_total();
    bool ok = bt_gd_synth_case(BT_ORBIT_CORNER, t1, t2, &lim, &cfg, &hit);
    printf("ok=%d start=%zu total=%zu\n", ok ? 1 : 0, start, bt_alloc_trace_total());
    if (ok) {
        printf("commutator=%s\n", hit.commutator != NULL ? hit.commutator : "null");
        bt_gd_synth_hit_free(&hit);
    }
    bt_alloc_trace_dump(argv[1]);
    return 0;
}
