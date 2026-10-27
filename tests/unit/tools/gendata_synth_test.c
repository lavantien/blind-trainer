#include "gendata/scoring.h"
#include "gendata/synth.h"

#include <stdlib.h>
#include <string.h>

#include "common/bt_harness.h"
#include "common/json.h"
#include "common/strbuf.h"
#include "core/alg.h"
#include "core/cube.h"
#include "data/commutator.h"
#include "data/verify.h"
#include "scheme/memo.h"
#include "scheme/pairs.h"
#include "scheme/scheme.h"

static void limits(bt_gd_synth_limits *out) {
    out->max_setup = 2;
    out->max_insert = 4;
    out->node_budget = 100000000;
}

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

static void case_of(bt_orbit orbit, char first, char second, uint8_t *t1, uint8_t *t2) {
    bt_case cases[BT_PAIRS_MAX];
    size_t count = bt_scheme_enumerate_cases(orbit, cases);
    for (size_t i = 0; i < count; i++) {
        char pair[2];
        bt_scheme_case_letters(&cases[i], pair);
        if (pair[0] == first && pair[1] == second) {
            *t1 = cases[i].t1;
            *t2 = cases[i].t2;
            return;
        }
    }
    BT_ABORT("%s", "case exists in the pair space");
}

static uint8_t buffer_of(bt_orbit orbit) {
    return orbit == BT_ORBIT_EDGE ? BT_SCHEME_EDGE_BUFFER_STICKER : BT_SCHEME_CORNER_BUFFER_STICKER;
}

static void assert_hit_matches_the_ideal_cycle(bt_orbit orbit, uint8_t t1, uint8_t t2,
                                               const bt_gd_synth_hit *hit) {
    uint8_t buffer = buffer_of(orbit);
    uint8_t target[54];
    for (size_t i = 0; i < 54; i++) {
        target[i] = (uint8_t)i;
    }
    bt_scheme_cycles cycles;
    bt_scheme_ideal_cycle_at(orbit, buffer, t1, t2, &cycles);
    bt_scheme_apply_sticker_cycles(target, &cycles);
    bt_cube cube = bt_cube_solved();
    bt_cube_apply_alg(&cube, &hit->alg);
    for (size_t pos = 0; pos < 54; pos++) {
        if (bt_cube_sticker_at(&cube, (uint8_t)pos) != target[pos]) {
            bt_strbuf text;
            bt_strbuf_init(&text, 0);
            bt_alg_format(&hit->alg, &text);
            char line[192];
            snprintf(line, sizeof line, "sticker %zu differs for %s %s-%s", pos, text.data,
                     bt_scheme_name_of(t1), bt_scheme_name_of(t2));
            bt_strbuf_free(&text);
            BT_ABORT("%s", line);
        }
    }
    BT_CHECK(bt_cube_centers_fixed(&cube));
    BT_CHECK(bt_alg_qtm(&hit->alg) <= 24);
    bt_expanded_list expanded;
    BT_CHECK(bt_decompose_verified(hit->commutator, &hit->alg, &expanded));
    bt_expanded_list_free(&expanded);
}

BT_TEST(gendata_synth, edge_head_pair_gets_a_verified_commutator) {
    uint8_t t1, t2;
    case_of(BT_ORBIT_EDGE, 'A', 'B', &t1, &t2);
    bt_gd_synth_limits lim;
    limits(&lim);
    bt_gd_scoring_cfg cfg;
    test_scoring(&cfg);
    bt_gd_synth_hit hit;
    BT_CHECK(bt_gd_synth_case(BT_ORBIT_EDGE, t1, t2, &lim, &cfg, &hit));
    assert_hit_matches_the_ideal_cycle(BT_ORBIT_EDGE, t1, t2, &hit);
    bt_gd_synth_hit_free(&hit);
}

BT_TEST(gendata_synth, corner_head_pair_gets_a_verified_commutator) {
    uint8_t t1, t2;
    case_of(BT_ORBIT_CORNER, 'A', 'B', &t1, &t2);
    bt_gd_synth_limits lim;
    limits(&lim);
    bt_gd_scoring_cfg cfg;
    test_scoring(&cfg);
    bt_gd_synth_hit hit;
    BT_CHECK(bt_gd_synth_case(BT_ORBIT_CORNER, t1, t2, &lim, &cfg, &hit));
    assert_hit_matches_the_ideal_cycle(BT_ORBIT_CORNER, t1, t2, &hit);
    bt_gd_synth_hit_free(&hit);
}

BT_TEST(gendata_synth, edge_tail_pair_gets_a_verified_commutator) {
    uint8_t t1, t2;
    case_of(BT_ORBIT_EDGE, 'E', 'B', &t1, &t2);
    bt_gd_synth_limits lim;
    limits(&lim);
    bt_gd_scoring_cfg cfg;
    test_scoring(&cfg);
    bt_gd_synth_hit hit;
    BT_CHECK(bt_gd_synth_case(BT_ORBIT_EDGE, t1, t2, &lim, &cfg, &hit));
    assert_hit_matches_the_ideal_cycle(BT_ORBIT_EDGE, t1, t2, &hit);
    bt_gd_synth_hit_free(&hit);
}

BT_TEST(gendata_synth, corner_tail_pair_gets_a_verified_commutator) {
    uint8_t t1, t2;
    case_of(BT_ORBIT_CORNER, 'E', 'B', &t1, &t2);
    bt_gd_synth_limits lim;
    limits(&lim);
    bt_gd_scoring_cfg cfg;
    test_scoring(&cfg);
    bt_gd_synth_hit hit;
    BT_CHECK(bt_gd_synth_case(BT_ORBIT_CORNER, t1, t2, &lim, &cfg, &hit));
    assert_hit_matches_the_ideal_cycle(BT_ORBIT_CORNER, t1, t2, &hit);
    bt_gd_synth_hit_free(&hit);
}

BT_TEST(gendata_synth, synthesis_is_deterministic) {
    uint8_t t1, t2;
    case_of(BT_ORBIT_EDGE, 'A', 'B', &t1, &t2);
    bt_gd_synth_limits lim;
    limits(&lim);
    bt_gd_scoring_cfg cfg;
    test_scoring(&cfg);
    bt_gd_synth_hit once = {0};
    bt_gd_synth_hit twice = {0};
    BT_CHECK(bt_gd_synth_case(BT_ORBIT_EDGE, t1, t2, &lim, &cfg, &once));
    BT_CHECK(bt_gd_synth_case(BT_ORBIT_EDGE, t1, t2, &lim, &cfg, &twice));
    bt_strbuf a;
    bt_strbuf b;
    bt_strbuf_init(&a, 0);
    bt_strbuf_init(&b, 0);
    bt_alg_format(&once.alg, &a);
    bt_alg_format(&twice.alg, &b);
    BT_CHECK_EQ(strcmp(a.data, b.data), 0);
    BT_CHECK_EQ(strcmp(once.commutator, twice.commutator), 0);
    bt_strbuf_free(&a);
    bt_strbuf_free(&b);
    bt_gd_synth_hit_free(&once);
    bt_gd_synth_hit_free(&twice);
}

BT_TEST(gendata_synth, best_plateau_selection_picks_the_rank_minimum) {
    uint8_t t1, t2;
    case_of(BT_ORBIT_CORNER, 'A', 'B', &t1, &t2);
    bt_gd_synth_limits lim;
    limits(&lim);
    bt_gd_scoring_cfg cfg;
    test_scoring(&cfg);
    bt_gd_synth_hits candidates = {0};
    BT_CHECK_EQ(bt_gd_synth_candidates(BT_ORBIT_CORNER, t1, t2, &lim, &candidates), 0);
    BT_CHECK(candidates.count > 0);
    bt_gd_synth_hit hit = {0};
    BT_CHECK(bt_gd_synth_case(BT_ORBIT_CORNER, t1, t2, &lim, &cfg, &hit));
    size_t expected_index = 0;
    for (size_t i = 1; i < candidates.count; i++) {
        if (bt_gd_rank_cmp(candidates.items[i].alg.items, candidates.items[i].alg.len,
                           candidates.items[expected_index].alg.items,
                           candidates.items[expected_index].alg.len, BT_ORBIT_CORNER, &cfg) < 0) {
            expected_index = i;
        }
    }
    bt_strbuf got;
    bt_strbuf want;
    bt_strbuf_init(&got, 0);
    bt_strbuf_init(&want, 0);
    bt_alg_format(&hit.alg, &got);
    bt_alg_format(&candidates.items[expected_index].alg, &want);
    BT_CHECK_EQ(strcmp(got.data, want.data), 0);
    BT_CHECK_EQ(strcmp(hit.commutator, candidates.items[expected_index].commutator), 0);
    bt_strbuf_free(&got);
    bt_strbuf_free(&want);
    bt_gd_synth_hit_free(&hit);
    bt_gd_synth_hits_free(&candidates);
}

BT_TEST(gendata_synth, plateau_stays_inside_the_first_matching_setup) {
    uint8_t t1, t2;
    case_of(BT_ORBIT_CORNER, 'A', 'K', &t1, &t2);
    bt_gd_synth_limits lim;
    limits(&lim);
    bt_gd_synth_hits candidates = {0};
    BT_CHECK_EQ(bt_gd_synth_candidates(BT_ORBIT_CORNER, t1, t2, &lim, &candidates), 0);
    BT_CHECK(candidates.count >= 2);
    uint32_t q0 = bt_alg_qtm(&candidates.items[0].alg);
    const char *colon = strchr(candidates.items[0].commutator, ':');
    char setup[64] = "";
    if (colon != NULL) {
        memcpy(setup, candidates.items[0].commutator,
               (size_t)(colon - candidates.items[0].commutator));
        setup[colon - candidates.items[0].commutator] = '\0';
    }
    for (size_t i = 0; i < candidates.count; i++) {
        BT_CHECK(bt_alg_qtm(&candidates.items[i].alg) <= q0);
        const char *candidate_colon = strchr(candidates.items[i].commutator, ':');
        char candidate_setup[64] = "";
        if (candidate_colon != NULL) {
            memcpy(candidate_setup, candidates.items[i].commutator,
                   (size_t)(candidate_colon - candidates.items[i].commutator));
            candidate_setup[candidate_colon - candidates.items[i].commutator] = '\0';
        }
        BT_CHECK_EQ(strcmp(candidate_setup, setup), 0);
    }
    bt_gd_synth_hits_free(&candidates);
}

BT_TEST(gendata_synth, scoring_flips_the_selected_alg_for_the_pinned_case) {
    uint8_t t1, t2;
    case_of(BT_ORBIT_CORNER, 'A', 'K', &t1, &t2);
    bt_gd_synth_limits lim;
    limits(&lim);
    bt_gd_scoring_cfg base_cfg;
    bt_gd_scoring_cfg inverted_cfg;
    test_scoring(&base_cfg);
    test_scoring(&inverted_cfg);
    for (size_t lane = 0; lane < 9; lane++) {
        inverted_cfg.preferred[lane] = false;
        inverted_cfg.preferred_edge[lane] = false;
    }
    inverted_cfg.preferred[BT_FACE_L] = true;
    inverted_cfg.preferred[BT_FACE_F] = true;
    inverted_cfg.preferred[BT_FACE_B] = true;
    inverted_cfg.preferred_edge[BT_FACE_L] = true;
    inverted_cfg.preferred_edge[BT_FACE_F] = true;
    inverted_cfg.preferred_edge[BT_FACE_B] = true;
    bt_gd_synth_hits candidates = {0};
    BT_CHECK_EQ(bt_gd_synth_candidates(BT_ORBIT_CORNER, t1, t2, &lim, &candidates), 0);
    BT_CHECK(candidates.count >= 2);
    bt_gd_synth_hit base = candidates.items[0];
    bt_gd_synth_hit alt = candidates.items[0];
    for (size_t i = 1; i < candidates.count; i++) {
        if (bt_gd_rank_cmp(candidates.items[i].alg.items, candidates.items[i].alg.len,
                           base.alg.items, base.alg.len, BT_ORBIT_CORNER, &base_cfg) < 0) {
            base = candidates.items[i];
        }
        if (bt_gd_rank_cmp(candidates.items[i].alg.items, candidates.items[i].alg.len,
                           alt.alg.items, alt.alg.len, BT_ORBIT_CORNER, &inverted_cfg) < 0) {
            alt = candidates.items[i];
        }
    }
    bt_strbuf base_text;
    bt_strbuf alt_text;
    bt_strbuf_init(&base_text, 0);
    bt_strbuf_init(&alt_text, 0);
    bt_alg_format(&base.alg, &base_text);
    bt_alg_format(&alt.alg, &alt_text);
    BT_CHECK(strcmp(base_text.data, alt_text.data) != 0);
    bt_strbuf_free(&base_text);
    bt_strbuf_free(&alt_text);
    assert_hit_matches_the_ideal_cycle(BT_ORBIT_CORNER, t1, t2, &base);
    assert_hit_matches_the_ideal_cycle(BT_ORBIT_CORNER, t1, t2, &alt);
    bt_gd_synth_hits_free(&candidates);
}

BT_TEST(gendata_synth, generated_entries_fill_only_uncovered_cases) {
    bt_gd_synth_limits lim;
    limits(&lim);
    bt_gd_scoring_cfg cfg;
    test_scoring(&cfg);
    bt_gd_cases_file cases = {.cases = malloc(2 * sizeof(bt_gd_case_entry)), .count = 2};
    cases.cases[0] = (bt_gd_case_entry){
        .orbit = _strdup("edge"),
        .t1 = 24,
        .t2 = 25,
        .t1_letter = 'A',
        .t2_letter = 'B',
        .t1_name = _strdup("UB"),
        .t2_name = _strdup("UR"),
        .covered = true,
        .vendor_algs = malloc(sizeof(bt_gd_vendor_alg)),
        .vendor_count = 1,
    };
    cases.cases[0].vendor_algs[0] = (bt_gd_vendor_alg){
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
    cases.cases[1] = (bt_gd_case_entry){
        .orbit = _strdup("edge"),
        .t1 = 29,
        .t2 = 30,
        .t1_letter = 'F',
        .t2_letter = 'G',
        .t1_name = _strdup("LF"),
        .t2_name = _strdup("LD"),
        .covered = false,
        .vendor_algs = NULL,
        .vendor_count = 0,
    };
    size_t edge_added = 0;
    size_t corner_added = 0;
    bt_gd_synthesize(&cases, &lim, &cfg, &edge_added, &corner_added);
    BT_CHECK_EQ(edge_added, 1u);
    BT_CHECK_EQ(corner_added, 0u);
    BT_CHECK_EQ(cases.cases[0].vendor_count, 1u);
    BT_CHECK_EQ(strcmp(cases.cases[0].vendor_algs[0].solver, "cherry"), 0);
    BT_CHECK(cases.cases[0].covered);
    const bt_gd_vendor_alg *generated = &cases.cases[1].vendor_algs[0];
    BT_CHECK_EQ(strcmp(generated->solver, "generated"), 0);
    BT_CHECK_EQ(strcmp(generated->position, "UF-LF-LD"), 0);
    BT_CHECK(generated->ok);
    BT_CHECK_EQ(generated->problem_count, 0u);
    BT_CHECK(cases.cases[1].covered);
    bt_alg alg = {0};
    char bad[64];
    BT_CHECK(bt_alg_parse(generated->alg, &alg, bad, sizeof bad));
    bt_strbuf text;
    bt_strbuf_init(&text, 0);
    bt_alg_format(&alg, &text);
    BT_CHECK_EQ(strcmp(text.data, generated->alg), 0);
    bt_expanded_list expanded;
    BT_CHECK(bt_decompose_verified(generated->commutator, &alg, &expanded));
    bt_expanded_list_free(&expanded);
    bt_strbuf_free(&text);
    bt_alg_free(&alg);
    bt_gd_cases_file_free(&cases);
}
