#include "gendata/audit.h"
#include "gendata/build.h"
#include "gendata/lib.h"
#include "gendata/model.h"
#include "gendata/scoring.h"
#include "gendata/synth.h"

#include <stdlib.h>
#include <string.h>

#include "common/alloc.h"
#include "common/bt_harness.h"
#include "common/json.h"
#include "common/strbuf.h"
#include "core/alg.h"
#include "data/corpus.h"
#include "data/digest.h"
#include "scheme/pairs.h"
#include "scheme/scheme.h"

enum {
    GD_OOM_WALK_MAX = 512,
};

static const size_t GD_OOM_DOUBLE_CAP = 1u << 22;

static void synth_limits(bt_gd_synth_limits *out) {
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

enum { GD_OOM_FAST_EDGE_T1 = 29, GD_OOM_FAST_EDGE_T2 = 30 };

static bt_json_value *parse_fixture(const char *text) {
    bt_json_value *value = NULL;
    bt_json_error jerr;
    if (bt_json_parse(text, strlen(text), &value, &jerr) != 0) {
        BT_ABORT("fixture parse failed: %s", jerr.message);
    }
    return value;
}

static bool synth_case_armed(bt_orbit orbit, uint8_t t1, uint8_t t2, bt_gd_synth_limits *lim,
                             bt_gd_scoring_cfg *cfg, size_t ok_count) {
    bt_gd_synth_hit hit;
    bt_alloc_fail_arm(ok_count);
    bool ok = bt_gd_synth_case(orbit, t1, t2, lim, cfg, &hit);
    bt_alloc_fail_disarm();
    if (ok) {
        bt_gd_synth_hit_free(&hit);
    }
    return ok;
}

BT_TEST(gendata_oom, synth_case_fails_closed_below_the_full_allocation_budget) {
    bt_gd_synth_limits lim;
    synth_limits(&lim);
    bt_gd_scoring_cfg cfg;
    test_scoring(&cfg);
    size_t hi = 1;
    while (!synth_case_armed(BT_ORBIT_EDGE, GD_OOM_FAST_EDGE_T1, GD_OOM_FAST_EDGE_T2, &lim, &cfg,
                             hi) &&
           hi < GD_OOM_DOUBLE_CAP) {
        hi *= 2;
    }
    BT_CHECK(hi < GD_OOM_DOUBLE_CAP);
    size_t lo = 1;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (synth_case_armed(BT_ORBIT_EDGE, GD_OOM_FAST_EDGE_T1, GD_OOM_FAST_EDGE_T2, &lim, &cfg,
                             mid)) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    BT_CHECK(lo > 8);
    for (size_t k = 0; k <= 24 && k < lo; k++) {
        BT_CHECK(!synth_case_armed(BT_ORBIT_EDGE, GD_OOM_FAST_EDGE_T1, GD_OOM_FAST_EDGE_T2, &lim,
                                   &cfg, k));
    }
    for (size_t k = lo > 24 ? lo - 24 : 0; k < lo; k++) {
        BT_CHECK(!synth_case_armed(BT_ORBIT_EDGE, GD_OOM_FAST_EDGE_T1, GD_OOM_FAST_EDGE_T2, &lim,
                                   &cfg, k));
    }
    BT_CHECK(
        synth_case_armed(BT_ORBIT_EDGE, GD_OOM_FAST_EDGE_T1, GD_OOM_FAST_EDGE_T2, &lim, &cfg, lo));
    BT_CHECK(synth_case_armed(BT_ORBIT_EDGE, GD_OOM_FAST_EDGE_T1, GD_OOM_FAST_EDGE_T2, &lim, &cfg,
                              lo + 8));
}

static bt_alg alg_of(const char *text) {
    bt_alg alg;
    bt_alg_init(&alg);
    char bad[64];
    if (!bt_alg_parse(text, &alg, bad, sizeof bad)) {
        BT_ABORT("fixture alg parse failed: %s", bad);
    }
    return alg;
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
        .alg = {0},
        .alg_text = _strdup(text.data),
        .commutator = _strdup(""),
        .recon_link = _strdup(""),
        .youtube_link = _strdup(""),
    };
    for (size_t i = 0; i < alg->len; i++) {
        bt_alg_push(&row.alg, alg->items[i]);
    }
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

static bool vendor_alg_complete(const bt_gd_vendor_alg *alg) {
    return alg->solver != NULL && alg->position != NULL && alg->alg != NULL &&
           alg->commutator != NULL && alg->recon_link != NULL && alg->youtube_link != NULL;
}

static void check_built_invariants(const bt_gd_built *built) {
    for (size_t i = 0; i < built->cases.count; i++) {
        const bt_gd_case_entry *entry = &built->cases.cases[i];
        BT_CHECK(entry->orbit != NULL);
        BT_CHECK(entry->t1_name != NULL);
        BT_CHECK(entry->t2_name != NULL);
        BT_CHECK(entry->vendor_count <= 1);
        for (size_t v = 0; v < entry->vendor_count; v++) {
            BT_CHECK(vendor_alg_complete(&entry->vendor_algs[v]));
            for (size_t p = 0; p < entry->vendor_algs[v].problem_count; p++) {
                BT_CHECK(entry->vendor_algs[v].problems[p] != NULL);
            }
            for (size_t n = 0; n < entry->vendor_algs[v].note_count; n++) {
                BT_CHECK(entry->vendor_algs[v].notes[n] != NULL);
            }
        }
    }
    for (size_t i = 0; i < built->offender_count; i++) {
        BT_CHECK(built->offenders[i] != NULL);
    }
    for (size_t i = 0; i < built->specials.count; i++) {
        const bt_gd_special_entry *special = &built->specials.specials[i];
        BT_CHECK(special->class_name != NULL);
        BT_CHECK(special->solver != NULL);
        BT_CHECK(special->position != NULL);
        BT_CHECK(special->alg != NULL);
        BT_CHECK(special->commutator != NULL);
        BT_CHECK(special->recon_link != NULL);
        BT_CHECK(special->youtube_link != NULL);
        for (size_t p = 0; p < special->problem_count; p++) {
            BT_CHECK(special->problems[p] != NULL);
        }
        for (size_t n = 0; n < special->note_count; n++) {
            BT_CHECK(special->notes[n] != NULL);
        }
    }
}

static bool build_is_complete(const bt_gd_built *built) {
    if (built->cases.count != (size_t)BT_GD_TOTAL_CASES) {
        return false;
    }
    if (built->offender_count != 0 || built->rows_in_pair_space != 1 ||
        built->rows_other_buffer != 0) {
        return false;
    }
    if (built->specials.count != 1 || built->specials.specials[0].note_count != 2) {
        return false;
    }
    size_t covered = 0;
    for (size_t i = 0; i < built->cases.count; i++) {
        if (built->cases.cases[i].vendor_count == 1) {
            if (!vendor_alg_complete(&built->cases.cases[i].vendor_algs[0])) {
                return false;
            }
            covered++;
        }
    }
    return covered == 1;
}

BT_TEST(gendata_oom, build_stays_consistent_and_frees_clean_at_every_budget) {
    bt_gd_synth_limits lim;
    synth_limits(&lim);
    bt_gd_scoring_cfg cfg;
    test_scoring(&cfg);
    bt_gd_synth_hit hit;
    BT_CHECK(bt_gd_synth_case(BT_ORBIT_EDGE, GD_OOM_FAST_EDGE_T1, GD_OOM_FAST_EDGE_T2, &lim, &cfg,
                              &hit));
    char position[96];
    snprintf(position, sizeof position, "%s-%s-%s",
             bt_scheme_name_of(BT_SCHEME_EDGE_BUFFER_STICKER),
             bt_scheme_name_of(GD_OOM_FAST_EDGE_T1), bt_scheme_name_of(GD_OOM_FAST_EDGE_T2));
    bt_alg twist = alg_of("R' D' R D R' D' R U' R' D R D' R' D R U");
    bt_corpus_row rows[2] = {
        row_of(BT_SOLVER_CHERRY, BT_SHEET_EDGE, 1, position, &hit.alg),
        row_of(BT_SOLVER_CHERRY, BT_SHEET_TWISTS, 3, "UFR & UFL", &twist),
    };
    bt_corpus corpus = corpus_of(rows, 2);
    size_t hi = 1;
    while (hi < GD_OOM_DOUBLE_CAP) {
        bt_gd_built built;
        bt_alloc_fail_arm(hi);
        bt_gd_build(&corpus, &built);
        bt_alloc_fail_disarm();
        bool complete = build_is_complete(&built);
        check_built_invariants(&built);
        bt_gd_built_free(&built);
        if (complete) {
            break;
        }
        hi *= 2;
    }
    BT_CHECK(hi < GD_OOM_DOUBLE_CAP);
    size_t lo = 1;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        bt_gd_built built;
        bt_alloc_fail_arm(mid);
        bt_gd_build(&corpus, &built);
        bt_alloc_fail_disarm();
        bool complete = build_is_complete(&built);
        check_built_invariants(&built);
        bt_gd_built_free(&built);
        if (complete) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    for (size_t delta = 1; delta <= 6; delta++) {
        bt_gd_built built;
        bt_alloc_fail_arm(lo - delta);
        bt_gd_build(&corpus, &built);
        bt_alloc_fail_disarm();
        check_built_invariants(&built);
        bt_gd_built_free(&built);
    }
    for (size_t k = lo > 48 ? lo - 48 : 0; k <= lo; k++) {
        bt_gd_built built;
        bt_alloc_fail_arm(k);
        bt_gd_build(&corpus, &built);
        bt_alloc_fail_disarm();
        check_built_invariants(&built);
        bt_gd_built_free(&built);
    }
    bt_gd_built built;
    bt_alloc_fail_arm(lo);
    bt_gd_build(&corpus, &built);
    bt_alloc_fail_disarm();
    BT_CHECK(build_is_complete(&built));
    check_built_invariants(&built);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
    bt_alg_free(&twist);
    bt_gd_synth_hit_free(&hit);
}

static bt_gd_synth_hit fast_hit(void);
static bt_corpus fast_corpus(const bt_gd_synth_hit *hit);

BT_TEST(gendata_oom, build_names_every_offender_class_and_survives_allocation_failure) {
    bt_gd_synth_hit hit = fast_hit();
    char wrong_walk[96];
    snprintf(wrong_walk, sizeof wrong_walk, "%s-%s-%s",
             bt_scheme_name_of(BT_SCHEME_EDGE_BUFFER_STICKER),
             bt_scheme_name_of(GD_OOM_FAST_EDGE_T2), bt_scheme_name_of(GD_OOM_FAST_EDGE_T1));
    bt_alg solved = alg_of("");
    bt_alg flip = alg_of("M' U M' U M' U2 M U M U M U2");
    bt_alg twist = alg_of("R' D' R D R' D' R U' R' D R D' R' D R U");
    bt_alg parity = alg_of("R U R' U' R' F R2 U' R' U' R U R' F'");
    bt_alg ltct = alg_of("U R U2 R D R' U' R D' R2 U R U2 R'");
    bt_corpus_row rows[6] = {
        row_of(BT_SOLVER_CHERRY, BT_SHEET_EDGE, 1, "UF-UB-UR", &solved),
        row_of(BT_SOLVER_CHERRY, BT_SHEET_EDGE, 2, "UF-UB-UR", &flip),
        row_of(BT_SOLVER_CHERRY, BT_SHEET_EDGE, 3, "UF-UB-UR", &twist),
        row_of(BT_SOLVER_EGGINS, BT_SHEET_EDGE, 4, "UF-UB-UR", &parity),
        row_of(BT_SOLVER_EGGINS, BT_SHEET_EDGE, 5, "UF-UB-UR", &ltct),
        row_of(BT_SOLVER_CHERRY, BT_SHEET_EDGE, 6, wrong_walk, &hit.alg),
    };
    bt_corpus corpus = corpus_of(rows, 6);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    BT_CHECK_EQ(built.offender_count, 11U);
    bool saw_solved = false;
    bool saw_flip = false;
    bool saw_twist = false;
    bool saw_parity = false;
    bool saw_ltct = false;
    bool saw_problems = false;
    for (size_t i = 0; i < built.offender_count; i++) {
        if (strstr(built.offenders[i], ": Solved") != NULL) {
            saw_solved = true;
        }
        if (strstr(built.offenders[i], ": FlipPair") != NULL) {
            saw_flip = true;
        }
        if (strstr(built.offenders[i], ": TwistPair") != NULL) {
            saw_twist = true;
        }
        if (strstr(built.offenders[i], ": Parity") != NULL) {
            saw_parity = true;
        }
        if (strstr(built.offenders[i], ": Ltct") != NULL) {
            saw_ltct = true;
        }
        if (strncmp(built.offenders[i], "cherry/edge/6 ", 14) == 0) {
            saw_problems = true;
        }
    }
    BT_CHECK(saw_solved);
    BT_CHECK(saw_flip);
    BT_CHECK(saw_twist);
    BT_CHECK(saw_parity);
    BT_CHECK(saw_ltct);
    BT_CHECK(saw_problems);
    BT_CHECK_EQ(built.rows_in_pair_space, 1U);
    bool saw_not_ok = false;
    for (size_t i = 0; i < built.cases.count; i++) {
        if (built.cases.cases[i].vendor_count == 1 && !built.cases.cases[i].vendor_algs[0].ok) {
            saw_not_ok = true;
        }
    }
    BT_CHECK(saw_not_ok);
    bt_gd_derive_mirrored_inverses(&built.cases);
    check_built_invariants(&built);
    bt_gd_built_free(&built);
    for (size_t k = 0; k <= 80; k++) {
        bt_gd_built armed;
        bt_alloc_fail_arm(k);
        bt_gd_build(&corpus, &armed);
        bt_gd_derive_mirrored_inverses(&armed.cases);
        bt_alloc_fail_disarm();
        check_built_invariants(&armed);
        bt_gd_built_free(&armed);
    }
    for (size_t k = 0; k <= 48; k++) {
        bt_gd_built armed;
        bt_gd_build(&corpus, &armed);
        bt_alloc_fail_arm(k);
        bt_gd_derive_mirrored_inverses(&armed.cases);
        bt_alloc_fail_disarm();
        check_built_invariants(&armed);
        bt_gd_built_free(&armed);
    }
    bt_corpus_free(&corpus);
    bt_alg_free(&ltct);
    bt_alg_free(&parity);
    bt_alg_free(&twist);
    bt_alg_free(&flip);
    bt_alg_free(&solved);
    bt_gd_synth_hit_free(&hit);
}

static bt_gd_synth_hit fast_hit(void) {
    bt_gd_synth_limits lim;
    synth_limits(&lim);
    bt_gd_scoring_cfg cfg;
    test_scoring(&cfg);
    bt_gd_synth_hit hit;
    if (!bt_gd_synth_case(BT_ORBIT_EDGE, GD_OOM_FAST_EDGE_T1, GD_OOM_FAST_EDGE_T2, &lim, &cfg,
                          &hit)) {
        BT_ABORT("%s", "fast edge pair must synthesize");
    }
    return hit;
}

static bt_corpus fast_corpus(const bt_gd_synth_hit *hit) {
    char position[96];
    snprintf(position, sizeof position, "%s-%s-%s",
             bt_scheme_name_of(BT_SCHEME_EDGE_BUFFER_STICKER),
             bt_scheme_name_of(GD_OOM_FAST_EDGE_T1), bt_scheme_name_of(GD_OOM_FAST_EDGE_T2));
    bt_corpus_row rows[1] = {
        row_of(BT_SOLVER_CHERRY, BT_SHEET_EDGE, 7, position, &hit->alg),
    };
    return corpus_of(rows, 1);
}

static void digest_fixture(bt_corpus_digest *digest) {
    memset(digest, 0, sizeof *digest);
    strcpy(digest->sha256, "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd");
    digest->rows = 1;
    strcpy(digest->files[0].solver, "cherry");
    strcpy(digest->files[0].sheet, "edge");
    digest->files[0].rows = 1;
    strcpy(digest->files[0].sha256,
           "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
    digest->file_count = 1;
}

static bool manifest_is_complete(const bt_gd_manifest *manifest) {
    if (manifest->generated_at == NULL || manifest->generator == NULL ||
        manifest->synth_params_sha256 == NULL || manifest->corpus_sha256 == NULL) {
        return false;
    }
    if (manifest->corpus_file_count != 1 || manifest->corpus_files == NULL ||
        manifest->corpus_files[0].solver == NULL) {
        return false;
    }
    if (manifest->cases.path == NULL || manifest->cases.sha256 == NULL ||
        manifest->specials.path == NULL || manifest->specials.sha256 == NULL) {
        return false;
    }
    if (manifest->specials.count_count != BT_GD_SPECIAL_COUNTS_LEN ||
        manifest->specials.counts == NULL) {
        return false;
    }
    for (size_t i = 0; i < manifest->specials.count_count; i++) {
        if (manifest->specials.counts[i].solver == NULL ||
            manifest->specials.counts[i].class_name == NULL) {
            return false;
        }
    }
    return manifest->cases.entries == (size_t)BT_GD_TOTAL_CASES;
}

static bool manifest_is_empty(const bt_gd_manifest *manifest) {
    return manifest->generated_at == NULL && manifest->corpus_files == NULL &&
           manifest->corpus_file_count == 0 && manifest->specials.count_count == 0 &&
           manifest->specials.counts == NULL && manifest->cases.entries == 0;
}

BT_TEST(gendata_oom, manifest_make_is_complete_or_empty_at_every_budget) {
    bt_gd_synth_hit hit = fast_hit();
    bt_corpus corpus = fast_corpus(&hit);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    bt_corpus_digest digest;
    digest_fixture(&digest);
    size_t hi = 1;
    while (hi < GD_OOM_DOUBLE_CAP) {
        bt_gd_manifest manifest;
        bt_alloc_fail_arm(hi);
        bt_gd_manifest_make(&built, &digest, "cc", "pp", "ss", "stamp", &manifest);
        bt_alloc_fail_disarm();
        bool complete = manifest_is_complete(&manifest);
        BT_CHECK(complete || manifest_is_empty(&manifest));
        bt_gd_manifest_free(&manifest);
        if (complete) {
            break;
        }
        hi *= 2;
    }
    BT_CHECK(hi < GD_OOM_DOUBLE_CAP);
    size_t lo = 1;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        bt_gd_manifest manifest;
        bt_alloc_fail_arm(mid);
        bt_gd_manifest_make(&built, &digest, "cc", "pp", "ss", "stamp", &manifest);
        bt_alloc_fail_disarm();
        bool complete = manifest_is_complete(&manifest);
        BT_CHECK(complete || manifest_is_empty(&manifest));
        bt_gd_manifest_free(&manifest);
        if (complete) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    for (size_t delta = 1; delta <= 6; delta++) {
        bt_gd_manifest manifest;
        bt_alloc_fail_arm(lo - delta);
        bt_gd_manifest_make(&built, &digest, "cc", "pp", "ss", "stamp", &manifest);
        bt_alloc_fail_disarm();
        BT_CHECK(manifest_is_empty(&manifest));
        bt_gd_manifest_free(&manifest);
    }
    for (size_t k = lo > 32 ? lo - 32 : 0; k < lo; k++) {
        bt_gd_manifest manifest;
        bt_alloc_fail_arm(k);
        bt_gd_manifest_make(&built, &digest, "cc", "pp", "ss", "stamp", &manifest);
        bt_alloc_fail_disarm();
        BT_CHECK(manifest_is_empty(&manifest));
        bt_gd_manifest_free(&manifest);
    }
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
    bt_gd_synth_hit_free(&hit);
}

static size_t mirror_index(const bt_gd_cases_file *cases) {
    size_t source = cases->count;
    for (size_t i = 0; i < cases->count; i++) {
        if (cases->cases[i].vendor_count == 1) {
            source = i;
            break;
        }
    }
    BT_CHECK(source < cases->count);
    for (size_t i = 0; i < cases->count; i++) {
        if (i != source && cases->cases[i].t1 == cases->cases[source].t2 &&
            cases->cases[i].t2 == cases->cases[source].t1 &&
            strcmp(cases->cases[i].orbit, cases->cases[source].orbit) == 0) {
            return i;
        }
    }
    BT_ABORT("%s", "mirror case exists in the pair space");
}

BT_TEST(gendata_oom, derive_fills_the_mirror_or_leaves_it_clean_at_every_budget) {
    bt_gd_synth_hit hit = fast_hit();
    bt_corpus corpus = fast_corpus(&hit);
    bt_gd_built probe;
    bt_gd_build(&corpus, &probe);
    size_t target = mirror_index(&probe.cases);
    BT_CHECK_EQ(probe.cases.cases[target].vendor_count, 0U);
    bt_gd_built_free(&probe);
    size_t hi = 1;
    while (hi < GD_OOM_DOUBLE_CAP) {
        bt_gd_built built;
        bt_gd_build(&corpus, &built);
        bt_alloc_fail_arm(hi);
        bt_gd_derive_mirrored_inverses(&built.cases);
        bt_alloc_fail_disarm();
        const bt_gd_case_entry *mirror = &built.cases.cases[target];
        bool filled = mirror->vendor_count == 1 && vendor_alg_complete(&mirror->vendor_algs[0]);
        check_built_invariants(&built);
        bt_gd_built_free(&built);
        if (filled) {
            break;
        }
        hi *= 2;
    }
    BT_CHECK(hi < GD_OOM_DOUBLE_CAP);
    size_t lo = 1;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        bt_gd_built built;
        bt_gd_build(&corpus, &built);
        bt_alloc_fail_arm(mid);
        bt_gd_derive_mirrored_inverses(&built.cases);
        bt_alloc_fail_disarm();
        const bt_gd_case_entry *mirror = &built.cases.cases[target];
        bool filled = mirror->vendor_count == 1 && vendor_alg_complete(&mirror->vendor_algs[0]);
        check_built_invariants(&built);
        bt_gd_built_free(&built);
        if (filled) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    for (size_t delta = 1; delta <= 6; delta++) {
        bt_gd_built built;
        bt_gd_build(&corpus, &built);
        bt_alloc_fail_arm(lo - delta);
        bt_gd_derive_mirrored_inverses(&built.cases);
        bt_alloc_fail_disarm();
        check_built_invariants(&built);
        bt_gd_built_free(&built);
    }
    for (size_t k = lo > 24 ? lo - 24 : 0; k < lo; k++) {
        bt_gd_built built;
        bt_gd_build(&corpus, &built);
        bt_alloc_fail_arm(k);
        bt_gd_derive_mirrored_inverses(&built.cases);
        bt_alloc_fail_disarm();
        check_built_invariants(&built);
        bt_gd_built_free(&built);
    }
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    bt_alloc_fail_arm(lo);
    bt_gd_derive_mirrored_inverses(&built.cases);
    bt_alloc_fail_disarm();
    const bt_gd_case_entry *mirror = &built.cases.cases[target];
    BT_CHECK_EQ(mirror->vendor_count, 1U);
    BT_CHECK(vendor_alg_complete(&mirror->vendor_algs[0]));
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
    bt_gd_synth_hit_free(&hit);
}

static void synth_walk_cases(bt_gd_cases_file *cases) {
    cases->cases = malloc(2 * sizeof(bt_gd_case_entry));
    cases->count = 2;
    cases->cases[0] = (bt_gd_case_entry){
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
        .t1 = GD_OOM_FAST_EDGE_T1,
        .t2 = GD_OOM_FAST_EDGE_T2,
        .t1_letter = 'F',
        .t2_letter = 'G',
        .t1_name = _strdup("LF"),
        .t2_name = _strdup("LD"),
        .covered = false,
        .vendor_algs = NULL,
        .vendor_count = 0,
    };
}

BT_TEST(gendata_oom, synth_generated_rows_land_atomic_or_not_at_all) {
    bt_gd_synth_limits lim;
    synth_limits(&lim);
    bt_gd_scoring_cfg cfg;
    test_scoring(&cfg);
    bt_gd_cases_file healthy;
    synth_walk_cases(&healthy);
    size_t edge_added = 0;
    size_t corner_added = 0;
    bt_gd_synthesize(&healthy, &lim, &cfg, &edge_added, &corner_added);
    BT_CHECK_EQ(edge_added, 1U);
    BT_CHECK_EQ(healthy.cases[1].vendor_count, 1U);
    BT_CHECK(vendor_alg_complete(&healthy.cases[1].vendor_algs[0]));
    bt_gd_cases_file_free(&healthy);
    size_t hi = 1;
    while (hi < GD_OOM_DOUBLE_CAP) {
        bt_gd_cases_file walk;
        synth_walk_cases(&walk);
        size_t edge = 0;
        size_t corner = 0;
        bt_alloc_fail_arm(hi);
        bt_gd_synthesize(&walk, &lim, &cfg, &edge, &corner);
        bt_alloc_fail_disarm();
        bool complete = edge == 1 && walk.cases[1].vendor_count == 1 &&
                        vendor_alg_complete(&walk.cases[1].vendor_algs[0]);
        if (!complete) {
            BT_CHECK_EQ(walk.cases[1].vendor_count, 0U);
            BT_CHECK_EQ(edge, 0U);
        }
        bt_gd_cases_file_free(&walk);
        if (complete) {
            break;
        }
        hi *= 2;
    }
    BT_CHECK(hi < GD_OOM_DOUBLE_CAP);
    for (size_t k = hi > 24 ? hi - 24 : 0; k < hi; k++) {
        bt_gd_cases_file walk;
        synth_walk_cases(&walk);
        size_t edge = 0;
        size_t corner = 0;
        bt_alloc_fail_arm(k);
        bt_gd_synthesize(&walk, &lim, &cfg, &edge, &corner);
        bt_alloc_fail_disarm();
        if (walk.cases[1].vendor_count != 0) {
            BT_CHECK_EQ(walk.cases[1].vendor_count, 1U);
            BT_CHECK(vendor_alg_complete(&walk.cases[1].vendor_algs[0]));
            BT_CHECK_EQ(edge, 1U);
        } else {
            BT_CHECK_EQ(edge, 0U);
        }
        bt_gd_cases_file_free(&walk);
    }
}

BT_TEST(gendata_oom, vendor_alg_from_json_fails_closed_until_the_budget_covers_every_field) {
    bt_json_value *value = parse_fixture(
        "{\"solver\": \"cherry\", \"rowid\": 2, \"position\": \"UF-UB-UR\", "
        "\"alg\": \"R2 U' S R2 S' R2 U R2\", \"qtm\": 12, \"htm\": 9, \"ok\": true, "
        "\"problems\": [\"problem one\", \"problem two\"], \"notes\": [\"twist:UFR:+1\"], "
        "\"commutator\": \"[R2 U',S]\", \"recon_link\": \"r\", \"youtube_link\": \"y\"}");
    char err[128];
    size_t k = 0;
    while (k < GD_OOM_WALK_MAX) {
        err[0] = '\0';
        bt_gd_vendor_alg alg;
        bt_alloc_fail_arm(k);
        int rc = bt_gd_vendor_alg_from_json(value, &alg, err, sizeof err);
        bt_alloc_fail_disarm();
        if (rc == 0) {
            bt_gd_vendor_alg_free(&alg);
            break;
        }
        BT_CHECK(err[0] != '\0');
        k++;
    }
    BT_CHECK(k < GD_OOM_WALK_MAX);
    BT_CHECK(k > 4);
    for (size_t probe = k; probe <= k + 2; probe++) {
        bt_gd_vendor_alg alg;
        bt_alloc_fail_arm(probe);
        int rc = bt_gd_vendor_alg_from_json(value, &alg, err, sizeof err);
        bt_alloc_fail_disarm();
        BT_CHECK_EQ(rc, 0);
        bt_gd_vendor_alg_free(&alg);
    }
    bt_json_free(value);
}

BT_TEST(gendata_oom, cases_file_from_json_fails_closed_until_the_budget_covers_every_entry) {
    bt_json_value *value = parse_fixture(
        "{\"cases\": [{\"orbit\": \"edge\", \"t1\": 24, \"t2\": 25, \"t1_letter\": \"A\", "
        "\"t2_letter\": \"B\", \"t1_name\": \"UB\", \"t2_name\": \"UR\", \"covered\": false, "
        "\"vendor_algs\": [{\"solver\": \"cherry\", \"rowid\": 2, \"position\": \"UF-UB-UR\", "
        "\"alg\": \"R2 U' S R2 S' R2 U R2\", \"qtm\": 12, \"htm\": 9, \"ok\": true, "
        "\"problems\": [], \"notes\": [], \"commutator\": \"[R2 U',S]\", \"recon_link\": \"\", "
        "\"youtube_link\": \"\"}]}]}");
    char err[128];
    size_t k = 0;
    while (k < GD_OOM_WALK_MAX) {
        err[0] = '\0';
        bt_gd_cases_file file;
        bt_alloc_fail_arm(k);
        int rc = bt_gd_cases_file_from_json(value, &file, err, sizeof err);
        bt_alloc_fail_disarm();
        if (rc == 0) {
            bt_gd_cases_file_free(&file);
            break;
        }
        BT_CHECK(err[0] != '\0');
        k++;
    }
    BT_CHECK(k < GD_OOM_WALK_MAX);
    BT_CHECK(k > 8);
    bt_json_free(value);
}

BT_TEST(gendata_oom, specials_file_from_json_fails_closed_until_the_budget_covers_every_entry) {
    bt_json_value *value = parse_fixture(
        "{\"specials\": [{\"class\": \"twists\", \"solver\": \"cherry\", \"rowid\": 3, "
        "\"position\": \"UFR & UFL\", \"alg\": \"R2\", \"qtm\": 2, \"htm\": 2, \"ok\": true, "
        "\"problems\": [\"p\"], \"notes\": [\"twist:UFR:+1\", \"twist:UFL:-1\"], "
        "\"commutator\": \"c\", \"recon_link\": \"r\", \"youtube_link\": \"y\"}]}");
    char err[128];
    size_t k = 0;
    while (k < GD_OOM_WALK_MAX) {
        err[0] = '\0';
        bt_gd_specials_file file;
        bt_alloc_fail_arm(k);
        int rc = bt_gd_specials_file_from_json(value, &file, err, sizeof err);
        bt_alloc_fail_disarm();
        if (rc == 0) {
            bt_gd_specials_file_free(&file);
            break;
        }
        BT_CHECK(err[0] != '\0');
        k++;
    }
    BT_CHECK(k < GD_OOM_WALK_MAX);
    BT_CHECK(k > 8);
    bt_json_free(value);
}

BT_TEST(gendata_oom, manifest_from_json_fails_closed_until_the_budget_covers_every_section) {
    bt_json_value *value = parse_fixture(
        "{\"generated_at\": \"stamp\", \"generator\": \"bt-gendata\", "
        "\"synth_params_sha256\": \"aa\", \"corpus_sha256\": \"bb\", \"corpus_rows\": 1, "
        "\"corpus_files\": [{\"solver\": \"cherry\", \"sheet\": \"edge\", \"rows\": 1, "
        "\"sha256\": \"cc\"}], \"rows_in_pair_space\": 1, \"rows_other_buffer\": 0, "
        "\"cases\": {\"path\": \"cases.json\", \"sha256\": \"dd\", \"entries\": 2, "
        "\"edge\": {\"total\": 1, \"covered\": 1, \"uncovered\": 0}, "
        "\"corner\": {\"total\": 1, \"covered\": 0, \"uncovered\": 1}}, "
        "\"specials\": {\"path\": \"specials.json\", \"sha256\": \"ee\", \"entries\": 1, "
        "\"counts\": [{\"solver\": \"cherry\", \"class\": \"flips\", \"rows\": 1}]}}");
    char err[128];
    size_t k = 0;
    while (k < GD_OOM_WALK_MAX) {
        err[0] = '\0';
        bt_gd_manifest manifest;
        bt_alloc_fail_arm(k);
        int rc = bt_gd_manifest_from_json(value, &manifest, err, sizeof err);
        bt_alloc_fail_disarm();
        if (rc == 0) {
            bt_gd_manifest_free(&manifest);
            break;
        }
        BT_CHECK(err[0] != '\0');
        k++;
    }
    BT_CHECK(k < GD_OOM_WALK_MAX);
    BT_CHECK(k > 8);
    bt_json_free(value);
}

BT_TEST(gendata_oom, audit_offenders_stay_complete_at_every_budget) {
    bt_gd_built built;
    memset(&built, 0, sizeof built);
    bt_corpus corpus;
    bt_corpus_init(&corpus);
    char **want = NULL;
    size_t want_count = 0;
    bt_gd_audit(&built, &corpus, 0, &want, &want_count);
    BT_CHECK(want_count > 4);
    bt_gd_string_list_free(want, want_count);
    for (size_t k = 0; k <= want_count * 2 + 4; k++) {
        char **got = NULL;
        size_t got_count = 0;
        bt_alloc_fail_arm(k);
        bt_gd_audit(&built, &corpus, 0, &got, &got_count);
        bt_alloc_fail_disarm();
        for (size_t i = 0; i < got_count; i++) {
            BT_CHECK(got[i] != NULL);
        }
        if (k >= want_count * 2) {
            BT_CHECK_EQ(got_count, want_count);
        }
        bt_gd_string_list_free(got, got_count);
    }
    bt_corpus_free(&corpus);
}
