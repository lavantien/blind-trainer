#include "gendata/build.h"
#include "gendata/lib.h"
#include "gendata/model.h"
#include "gendata/scoring.h"
#include "gendata/synth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/alloc.h"
#include "common/bt_harness.h"
#include "common/json.h"
#include "common/strbuf.h"
#include "core/alg.h"
#include "data/corpus.h"
#include "scheme/scheme.h"

enum {
    GD_ALLOC_PRETTY_SWEEP_MAX = 96,
    GD_ALLOC_PRETTY_PAD_MAX = 40,
    GD_ALLOC_SYNTH_HEAD_WINDOW = 500,
    GD_ALLOC_SYNTH_TAIL_WINDOW = 200,
    GD_ALLOC_SYNTH_WALK_CAP = 4096,
    GD_ALLOC_SYNTH_DOUBLE_CAP = 1U << 20U,
    GD_ALLOC_BUILD_TAIL_WINDOW = 220,
    GD_ALLOC_FAST_EDGE_T1 = 29,
    GD_ALLOC_FAST_EDGE_T2 = 30,
    GD_ALLOC_DEEP_EDGE_T1 = 24,
    GD_ALLOC_DEEP_EDGE_T2 = 25,
    GD_ALLOC_STARVED_NODE_BUDGET = 1000,
    GD_ALLOC_STARVED_TOTAL_FLOOR = 3000,
};

static void test_scoring(bt_gd_scoring_cfg *cfg) {
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
        BT_ABORT("%s", "reference scoring config must parse");
    }
    char err[128];
    if (bt_gd_scoring_from_config(config, cfg, err, sizeof err) != 0) {
        BT_ABORT("%s", err);
    }
    bt_json_free(config);
}

static void special_text(char *out, size_t pad, bool specials_first) {
    size_t at = 0;
    if (!specials_first) {
        for (size_t i = 0; i < pad; i++) {
            out[at++] = 'a' + (char)(i % 26);
        }
    }
    out[at++] = '"';
    out[at++] = '\\';
    out[at++] = '\n';
    out[at++] = '\r';
    out[at++] = '\t';
    out[at++] = '\b';
    out[at++] = '\f';
    out[at++] = 1;
    if (specials_first) {
        for (size_t i = 0; i < pad; i++) {
            out[at++] = 'a' + (char)(i % 26);
        }
    }
    out[at] = '\0';
}

static bt_json_value *array_document(size_t pad) {
    char text[64];
    special_text(text, pad, true);
    bt_json_value *root = bt_json_new_array();
    bt_json_array_push(root, bt_json_new_string(text));
    special_text(text, pad, false);
    bt_json_array_push(root, bt_json_new_string(text));
    bt_json_array_push(root, bt_json_new_number(10.5));
    bt_json_array_push(root, bt_json_new_number(0.00125));
    bt_json_array_push(root, bt_json_new_number(5e-7));
    bt_json_array_push(root, bt_json_new_integer(7));
    bt_json_array_push(root, bt_json_new_bool(1));
    bt_json_array_push(root, bt_json_new_null());
    bt_json_value *inner = bt_json_new_array();
    bt_json_array_push(inner, bt_json_new_string(text));
    bt_json_array_push(inner, bt_json_new_number(1.5));
    bt_json_array_push(root, inner);
    bt_json_value *leaf = bt_json_new_array();
    bt_json_array_push(leaf, bt_json_new_bool(0));
    bt_json_array_push(root, leaf);
    return root;
}

static bt_json_value *object_document(size_t pad) {
    char text[64];
    special_text(text, pad, true);
    bt_json_value *root = bt_json_new_object();
    char key[96];
    for (size_t i = 0; i < 8; i++) {
        snprintf(key, sizeof key, "key%zu-%.*s", i, (int)pad,
                 "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        bt_json_object_set(root, key, bt_json_new_string(text));
    }
    bt_json_object_set(root, "nested", array_document(pad));
    bt_json_object_set(root, "hot", bt_json_new_number(-1.25e20));
    return root;
}

static void sweep_pretty(bt_json_value *root) {
    size_t armed = 0;
    for (;;) {
        bt_strbuf text;
        bt_alloc_fail_arm(armed);
        bt_strbuf_init(&text, 0);
        int rc = bt_gd_json_pretty(root, &text);
        bt_alloc_fail_disarm();
        bt_strbuf_free(&text);
        if (rc == 0) {
            break;
        }
        armed += 1;
        BT_CHECK(armed < GD_ALLOC_PRETTY_SWEEP_MAX);
    }
    BT_CHECK(armed > 0);
}

BT_TEST(gendata_alloc, pretty_print_survives_allocation_failure_at_every_growth_point) {
    for (size_t pad = 0; pad <= 12; pad++) {
        bt_json_value *document = array_document(pad);
        sweep_pretty(document);
        bt_json_free(document);
    }
    for (size_t pad = 0; pad <= 12; pad++) {
        bt_json_value *document = object_document(pad);
        sweep_pretty(document);
        bt_json_free(document);
    }
}

static bt_json_value *pair_document(size_t pad) {
    char key[64];
    memset(key, 'a', pad);
    key[pad] = '\0';
    bt_json_value *root = bt_json_new_object();
    bt_json_object_set(root, key, bt_json_new_string("v"));
    bt_json_object_set(root, "tail", bt_json_new_integer(1));
    return root;
}

BT_TEST(gendata_alloc, pretty_object_separators_fail_at_every_growth_point) {
    for (size_t pad = 0; pad <= GD_ALLOC_PRETTY_PAD_MAX; pad++) {
        bt_json_value *document = pair_document(pad);
        sweep_pretty(document);
        bt_json_free(document);
    }
}

static bool synth_case_armed(bt_orbit orbit, uint8_t t1, uint8_t t2, bt_gd_synth_limits *lim,
                             size_t ok_count, size_t *failures) {
    bt_gd_scoring_cfg cfg;
    test_scoring(&cfg);
    bt_gd_synth_hit hit;
    bt_alloc_fail_arm(ok_count);
    bool ok = bt_gd_synth_case(orbit, t1, t2, lim, &cfg, &hit);
    *failures = bt_alloc_fail_count();
    bt_alloc_fail_disarm();
    if (ok) {
        BT_CHECK(hit.alg.len > 0);
        BT_CHECK(hit.commutator != NULL);
        bt_gd_synth_hit_free(&hit);
    }
    return ok;
}

static size_t synth_case_total(bt_orbit orbit, uint8_t t1, uint8_t t2, bt_gd_synth_limits *lim) {
    size_t failures = 0;
    size_t hi = 1;
    while (hi < GD_ALLOC_SYNTH_DOUBLE_CAP) {
        synth_case_armed(orbit, t1, t2, lim, hi, &failures);
        if (failures == 0) {
            break;
        }
        hi *= 2;
    }
    BT_CHECK(hi < GD_ALLOC_SYNTH_DOUBLE_CAP);
    size_t lo = 1;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        synth_case_armed(orbit, t1, t2, lim, mid, &failures);
        if (failures == 0) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    return lo;
}

BT_TEST(gendata_alloc, synth_case_at_flat_limits_walks_the_hit_path_allocations) {
    bt_gd_synth_limits lim = {.max_setup = 0, .max_insert = 3, .node_budget = 100000000};
    size_t failures = 0;
    size_t total =
        synth_case_total(BT_ORBIT_EDGE, GD_ALLOC_FAST_EDGE_T1, GD_ALLOC_FAST_EDGE_T2, &lim);
    BT_CHECK(total > 500);
    for (size_t k = 0; k <= GD_ALLOC_SYNTH_HEAD_WINDOW && k < total; k++) {
        synth_case_armed(BT_ORBIT_EDGE, GD_ALLOC_FAST_EDGE_T1, GD_ALLOC_FAST_EDGE_T2, &lim, k,
                         &failures);
    }
    for (size_t k = total > GD_ALLOC_SYNTH_TAIL_WINDOW ? total - GD_ALLOC_SYNTH_TAIL_WINDOW : 0;
         k < total; k++) {
        synth_case_armed(BT_ORBIT_EDGE, GD_ALLOC_FAST_EDGE_T1, GD_ALLOC_FAST_EDGE_T2, &lim, k,
                         &failures);
    }
    synth_case_armed(BT_ORBIT_EDGE, GD_ALLOC_FAST_EDGE_T1, GD_ALLOC_FAST_EDGE_T2, &lim, total,
                     &failures);
    BT_CHECK_EQ(failures, 0U);
}

BT_TEST(gendata_alloc, synth_case_zero_insert_limits_walk_every_setup_allocation) {
    bt_gd_synth_limits lim = {.max_setup = 1, .max_insert = 0, .node_budget = 100000000};
    size_t failures = 0;
    size_t total =
        synth_case_total(BT_ORBIT_EDGE, GD_ALLOC_FAST_EDGE_T1, GD_ALLOC_FAST_EDGE_T2, &lim);
    BT_CHECK(total > 48);
    BT_CHECK(total < GD_ALLOC_SYNTH_WALK_CAP);
    for (size_t k = 0; k <= total; k++) {
        synth_case_armed(BT_ORBIT_EDGE, GD_ALLOC_FAST_EDGE_T1, GD_ALLOC_FAST_EDGE_T2, &lim, k,
                         &failures);
    }
    BT_CHECK_EQ(failures, 0U);
}

BT_TEST(gendata_alloc, synth_case_starved_budget_walks_the_hit_path_dense) {
    bt_gd_synth_limits lim = {
        .max_setup = 0,
        .max_insert = 3,
        .node_budget = GD_ALLOC_STARVED_NODE_BUDGET,
    };
    size_t failures = 0;
    size_t total =
        synth_case_total(BT_ORBIT_EDGE, GD_ALLOC_FAST_EDGE_T1, GD_ALLOC_FAST_EDGE_T2, &lim);
    BT_CHECK(total > GD_ALLOC_STARVED_TOTAL_FLOOR);
    BT_CHECK(total < GD_ALLOC_SYNTH_WALK_CAP);
    for (size_t k = 0; k <= total; k++) {
        synth_case_armed(BT_ORBIT_EDGE, GD_ALLOC_FAST_EDGE_T1, GD_ALLOC_FAST_EDGE_T2, &lim, k,
                         &failures);
    }
    BT_CHECK_EQ(failures, 0U);
}

static void synthesize_pair_cases(bt_gd_cases_file *cases) {
    cases->cases = malloc(2 * sizeof(bt_gd_case_entry));
    cases->count = 2;
    cases->cases[0] = (bt_gd_case_entry){
        .orbit = _strdup("edge"),
        .t1 = GD_ALLOC_DEEP_EDGE_T1,
        .t2 = GD_ALLOC_DEEP_EDGE_T2,
        .t1_letter = 'A',
        .t2_letter = 'B',
        .t1_name = _strdup("UB"),
        .t2_name = _strdup("UR"),
        .covered = true,
        .vendor_algs = malloc(sizeof(bt_gd_vendor_alg)),
        .vendor_count = 1,
    };
    *cases->cases[0].vendor_algs = (bt_gd_vendor_alg){
        .solver = _strdup("cherry"),
        .rowid = 2,
        .position = _strdup("UF-UB-UR"),
        .alg = _strdup("R2 U' S R2 S' R2 U R2"),
        .qtm = 12,
        .htm = 9,
        .ok = true,
        .problems = NULL,
        .problem_count = 0,
        .notes = NULL,
        .note_count = 0,
        .commutator = _strdup("R2 U':[S,R2]"),
        .recon_link = _strdup(""),
        .youtube_link = _strdup(""),
    };
    cases->cases[1] = (bt_gd_case_entry){
        .orbit = _strdup("edge"),
        .t1 = GD_ALLOC_FAST_EDGE_T1,
        .t2 = GD_ALLOC_FAST_EDGE_T2,
        .t1_letter = 'F',
        .t2_letter = 'G',
        .t1_name = _strdup("LF"),
        .t2_name = _strdup("LD"),
        .covered = false,
        .vendor_algs = NULL,
        .vendor_count = 0,
    };
}

BT_TEST(gendata_alloc, synthesize_at_flat_limits_lands_atomic_or_not_at_all) {
    bt_gd_synth_limits lim = {.max_setup = 0, .max_insert = 3, .node_budget = 100000000};
    bt_gd_scoring_cfg cfg;
    test_scoring(&cfg);
    size_t hi = 1;
    while (hi < GD_ALLOC_SYNTH_DOUBLE_CAP) {
        bt_gd_cases_file cases;
        synthesize_pair_cases(&cases);
        size_t edge = 0;
        size_t corner = 0;
        bt_alloc_fail_arm(hi);
        bt_gd_synthesize(&cases, &lim, &cfg, &edge, &corner);
        bt_alloc_fail_disarm();
        bool complete = edge == 1 && cases.cases[1].vendor_count == 1;
        bt_gd_cases_file_free(&cases);
        if (complete) {
            break;
        }
        hi *= 2;
    }
    BT_CHECK(hi < GD_ALLOC_SYNTH_DOUBLE_CAP);
    size_t lo = 1;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        bt_gd_cases_file cases;
        synthesize_pair_cases(&cases);
        size_t edge = 0;
        size_t corner = 0;
        bt_alloc_fail_arm(mid);
        bt_gd_synthesize(&cases, &lim, &cfg, &edge, &corner);
        bt_alloc_fail_disarm();
        bool complete = edge == 1 && cases.cases[1].vendor_count == 1;
        bt_gd_cases_file_free(&cases);
        if (complete) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    BT_CHECK(lo > 500);
    for (size_t k = lo > GD_ALLOC_SYNTH_TAIL_WINDOW ? lo - GD_ALLOC_SYNTH_TAIL_WINDOW : 0; k <= lo;
         k++) {
        bt_gd_cases_file cases;
        synthesize_pair_cases(&cases);
        size_t edge = 0;
        size_t corner = 0;
        bt_alloc_fail_arm(k);
        bt_gd_synthesize(&cases, &lim, &cfg, &edge, &corner);
        bt_alloc_fail_disarm();
        if (cases.cases[1].vendor_count != 0) {
            BT_CHECK_EQ(cases.cases[1].vendor_count, 1U);
            BT_CHECK_EQ(edge, 1U);
            BT_CHECK(cases.cases[1].vendor_algs[0].alg != NULL);
            BT_CHECK(cases.cases[1].vendor_algs[0].commutator != NULL);
        } else {
            BT_CHECK_EQ(edge, 0U);
        }
        bt_gd_cases_file_free(&cases);
    }
}

static bt_alg alg_clone(const bt_alg *src) {
    bt_alg out;
    bt_alg_init(&out);
    for (size_t i = 0; i < src->len; i++) {
        bt_alg_push(&out, src->items[i]);
    }
    return out;
}

static bt_corpus_row row_of(bt_solver solver, bt_sheet_class sheet, size_t rowid,
                            const char *position, const bt_alg *alg) {
    bt_strbuf text;
    bt_strbuf_init(&text, 0);
    bt_alg_format(alg, &text);
    bt_corpus_row row = {
        .solver = solver,
        .sheet = sheet,
        .rowid = rowid,
        .position = _strdup(position),
        .alg = alg_clone(alg),
        .alg_text = _strdup(text.data),
        .commutator = _strdup(""),
        .recon_link = _strdup(""),
        .youtube_link = _strdup(""),
    };
    bt_strbuf_free(&text);
    return row;
}

static bt_corpus corpus_of(bt_corpus_row *stack_rows, size_t count) {
    bt_corpus_row *heap = malloc(count * sizeof *heap);
    memcpy(heap, stack_rows, count * sizeof *heap);
    bt_corpus corpus;
    bt_corpus_from_rows(heap, count, &corpus);
    return corpus;
}

static bool fast_hit(bt_alg *out) {
    bt_gd_synth_limits lim = {.max_setup = 0, .max_insert = 3, .node_budget = 100000000};
    bt_gd_scoring_cfg cfg;
    test_scoring(&cfg);
    bt_gd_synth_hit hit;
    if (!bt_gd_synth_case(BT_ORBIT_EDGE, GD_ALLOC_FAST_EDGE_T1, GD_ALLOC_FAST_EDGE_T2, &lim, &cfg,
                          &hit)) {
        return false;
    }
    *out = hit.alg;
    free(hit.commutator);
    return true;
}

static bool build_complete(const bt_gd_built *built) {
    if (built->cases.count != (size_t)BT_GD_TOTAL_CASES || built->offender_count != 1 ||
        built->rows_in_pair_space != 2) {
        return false;
    }
    size_t covered = 0;
    for (size_t i = 0; i < built->cases.count; i++) {
        if (built->cases.cases[i].vendor_count > 2) {
            return false;
        }
        covered += built->cases.cases[i].vendor_count;
    }
    return covered == 2;
}

static void check_build_invariants(const bt_gd_built *built) {
    BT_CHECK(built->cases.count <= (size_t)BT_GD_TOTAL_CASES);
    BT_CHECK(built->offender_count <= 1);
    for (size_t i = 0; i < built->offender_count; i++) {
        BT_CHECK(built->offenders[i] != NULL);
    }
    for (size_t i = 0; i < built->cases.count; i++) {
        const bt_gd_case_entry *entry = &built->cases.cases[i];
        BT_CHECK(entry->orbit != NULL);
        BT_CHECK(entry->t1_name != NULL);
        BT_CHECK(entry->t2_name != NULL);
        for (size_t v = 0; v < entry->vendor_count; v++) {
            const bt_gd_vendor_alg *alg = &entry->vendor_algs[v];
            BT_CHECK(alg->solver != NULL && alg->position != NULL && alg->alg != NULL);
            BT_CHECK(alg->commutator != NULL && alg->recon_link != NULL);
            BT_CHECK(alg->youtube_link != NULL);
            for (size_t p = 0; p < alg->problem_count; p++) {
                BT_CHECK(alg->problems[p] != NULL);
            }
        }
    }
}

BT_TEST(gendata_alloc, build_row_loop_allocations_fail_closed_through_the_tail) {
    bt_alg hit_alg;
    BT_CHECK(fast_hit(&hit_alg));
    char position[96];
    snprintf(position, sizeof position, "%s-%s-%s",
             bt_scheme_name_of(BT_SCHEME_EDGE_BUFFER_STICKER),
             bt_scheme_name_of(GD_ALLOC_FAST_EDGE_T1), bt_scheme_name_of(GD_ALLOC_FAST_EDGE_T2));
    char wrong_walk[96];
    snprintf(wrong_walk, sizeof wrong_walk, "%s-%s-%s",
             bt_scheme_name_of(BT_SCHEME_EDGE_BUFFER_STICKER),
             bt_scheme_name_of(GD_ALLOC_FAST_EDGE_T2), bt_scheme_name_of(GD_ALLOC_FAST_EDGE_T1));
    bt_corpus_row rows[2] = {
        row_of(BT_SOLVER_CHERRY, BT_SHEET_EDGE, 5, position, &hit_alg),
        row_of(BT_SOLVER_CHERRY, BT_SHEET_EDGE, 6, wrong_walk, &hit_alg),
    };
    bt_corpus corpus = corpus_of(rows, 2);
    size_t hi = 1;
    while (hi < GD_ALLOC_SYNTH_DOUBLE_CAP) {
        bt_gd_built built;
        bt_alloc_fail_arm(hi);
        bt_gd_build(&corpus, &built);
        bt_alloc_fail_disarm();
        bool complete = build_complete(&built);
        check_build_invariants(&built);
        bt_gd_built_free(&built);
        if (complete) {
            break;
        }
        hi *= 2;
    }
    BT_CHECK(hi < GD_ALLOC_SYNTH_DOUBLE_CAP);
    size_t lo = 1;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        bt_gd_built built;
        bt_alloc_fail_arm(mid);
        bt_gd_build(&corpus, &built);
        bt_alloc_fail_disarm();
        bool complete = build_complete(&built);
        check_build_invariants(&built);
        bt_gd_built_free(&built);
        if (complete) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    for (size_t k = lo > GD_ALLOC_BUILD_TAIL_WINDOW ? lo - GD_ALLOC_BUILD_TAIL_WINDOW : 0; k <= lo;
         k++) {
        bt_gd_built built;
        bt_alloc_fail_arm(k);
        bt_gd_build(&corpus, &built);
        bt_alloc_fail_disarm();
        check_build_invariants(&built);
        bt_gd_built_free(&built);
    }
    bt_gd_built built;
    bt_alloc_fail_arm(lo);
    bt_gd_build(&corpus, &built);
    bt_alloc_fail_disarm();
    BT_CHECK(build_complete(&built));
    check_build_invariants(&built);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
    bt_alg_free(&hit_alg);
}
