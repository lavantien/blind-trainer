#include "gendata/build.h"
#include "gendata/lib.h"
#include "gendata/model.h"
#include "gendata/scoring.h"
#include "gendata/synth.h"

#include <stdlib.h>
#include <string.h>

#include "common/bt_harness.h"
#include "common/json.h"
#include "common/strbuf.h"
#include "core/alg.h"
#include "data/corpus.h"
#include "data/digest.h"
#include "data/verify.h"
#include "scheme/classifier.h"
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

static void position_of(bt_orbit orbit, uint8_t t1, uint8_t t2, char out[BT_PATH_MAX]) {
    uint8_t buffer =
        orbit == BT_ORBIT_EDGE ? BT_SCHEME_EDGE_BUFFER_STICKER : BT_SCHEME_CORNER_BUFFER_STICKER;
    snprintf(out, BT_PATH_MAX, "%s-%s-%s", bt_scheme_name_of(buffer), bt_scheme_name_of(t1),
             bt_scheme_name_of(t2));
}

static void empty_corpus(bt_corpus *corpus) {
    bt_corpus_init(corpus);
}

BT_TEST(gendata_build, regen_is_needed_without_a_manifest_or_on_any_hash_mismatch) {
    bt_corpus corpus;
    empty_corpus(&corpus);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    bt_corpus_digest digest = {.sha256 = "bb", .rows = 0, .file_count = 0};
    bt_gd_manifest record;
    bt_gd_manifest_make(&built, &digest, "cases", "specials", "aa", "2026-01-01T00:00:00Z",
                        &record);
    BT_CHECK(bt_gd_regen_needed(NULL, false, "aa", "bb"));
    BT_CHECK(bt_gd_regen_needed(&record, true, "cc", "bb"));
    BT_CHECK(bt_gd_regen_needed(&record, true, "aa", "dd"));
    bt_gd_manifest_free(&record);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_build, regen_is_skipped_only_when_both_hashes_match) {
    bt_corpus corpus;
    empty_corpus(&corpus);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    bt_corpus_digest digest = {.sha256 = "bb", .rows = 0, .file_count = 0};
    bt_gd_manifest record;
    bt_gd_manifest_make(&built, &digest, "cases", "specials", "aa", "2026-01-01T00:00:00Z",
                        &record);
    BT_CHECK(!bt_gd_regen_needed(&record, true, "aa", "bb"));
    bt_gd_manifest_free(&record);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_build, build_catalogs_the_whole_pair_space_from_an_empty_corpus) {
    bt_corpus corpus;
    empty_corpus(&corpus);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    BT_CHECK_EQ(built.cases.count, BT_GD_TOTAL_CASES);
    bool all_uncovered = true;
    for (size_t i = 0; i < built.cases.count; i++) {
        if (built.cases.cases[i].covered || built.cases.cases[i].vendor_count != 0) {
            all_uncovered = false;
        }
    }
    BT_CHECK(all_uncovered);
    BT_CHECK_EQ(built.specials.count, 0U);
    BT_CHECK_EQ(built.rows_in_pair_space, 0U);
    BT_CHECK_EQ(built.rows_other_buffer, 0U);
    BT_CHECK_EQ(built.offender_count, 0U);
    const bt_gd_case_entry *first = &built.cases.cases[0];
    BT_CHECK_EQ(strcmp(first->orbit, "edge"), 0);
    BT_CHECK_EQ(first->t1, 24);
    BT_CHECK_EQ(first->t2, 25);
    BT_CHECK_EQ(first->t1_letter, 'A');
    BT_CHECK_EQ(first->t2_letter, 'B');
    BT_CHECK_EQ(strcmp(first->t1_name, "UB"), 0);
    BT_CHECK_EQ(strcmp(first->t2_name, "UR"), 0);
    const bt_gd_case_entry *boundary = &built.cases.cases[440];
    BT_CHECK_EQ(strcmp(boundary->orbit, "corner"), 0);
    BT_CHECK_EQ(boundary->t1, 0);
    BT_CHECK_EQ(boundary->t2, 1);
    const bt_gd_case_entry *last = &built.cases.cases[built.cases.count - 1];
    BT_CHECK_EQ(strcmp(last->orbit, "corner"), 0);
    BT_CHECK_EQ(last->t1, 23);
    BT_CHECK_EQ(last->t2, 22);
    BT_CHECK_EQ(last->t1_letter, 'X');
    BT_CHECK_EQ(last->t2_letter, 'W');
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_build, build_attaches_a_verified_edge_three_style_to_its_case) {
    bt_case cases[BT_PAIRS_MAX];
    bt_scheme_enumerate_cases(BT_ORBIT_EDGE, cases);
    bt_gd_synth_limits lim;
    limits(&lim);
    bt_gd_scoring_cfg cfg;
    test_scoring(&cfg);
    bt_gd_synth_hit hit;
    BT_CHECK(bt_gd_synth_case(BT_ORBIT_EDGE, cases[0].t1, cases[0].t2, &lim, &cfg, &hit));
    char position[BT_PATH_MAX];
    position_of(BT_ORBIT_EDGE, cases[0].t1, cases[0].t2, position);
    bt_corpus_row rows[1] = {
        row_of(BT_SOLVER_CHERRY, BT_SHEET_EDGE, 1, position, &hit.alg),
    };
    bt_corpus corpus = corpus_of(rows, 1);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    BT_CHECK_EQ(built.offender_count, 0U);
    BT_CHECK_EQ(built.rows_in_pair_space, 1U);
    BT_CHECK_EQ(built.rows_other_buffer, 0U);
    size_t slot = 0;
    bool found = false;
    for (size_t i = 0; i < built.cases.count; i++) {
        if (built.cases.cases[i].t1 == cases[0].t1 && built.cases.cases[i].t2 == cases[0].t2) {
            slot = i;
            found = true;
            break;
        }
    }
    BT_CHECK(found);
    const bt_gd_case_entry *entry = &built.cases.cases[slot];
    BT_CHECK(entry->covered);
    BT_CHECK_EQ(entry->vendor_count, 1U);
    const bt_gd_vendor_alg *alg = &entry->vendor_algs[0];
    BT_CHECK_EQ(strcmp(alg->solver, "cherry"), 0);
    BT_CHECK_EQ(alg->rowid, 1U);
    BT_CHECK_EQ(strcmp(alg->position, position), 0);
    bt_strbuf want;
    bt_strbuf_init(&want, 0);
    bt_alg_format(&hit.alg, &want);
    BT_CHECK_EQ(strcmp(alg->alg, want.data), 0);
    BT_CHECK_EQ(alg->qtm, bt_alg_qtm(&hit.alg));
    BT_CHECK_EQ(alg->htm, bt_alg_htm(&hit.alg));
    BT_CHECK(alg->ok);
    BT_CHECK_EQ(alg->problem_count, 0U);
    size_t covered = 0;
    for (size_t i = 0; i < built.cases.count; i++) {
        if (built.cases.cases[i].covered) {
            covered++;
        }
    }
    BT_CHECK_EQ(covered, 1U);
    bt_strbuf_free(&want);
    bt_gd_synth_hit_free(&hit);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_build, build_attaches_a_verified_corner_three_style_to_its_case) {
    bt_case cases[BT_PAIRS_MAX];
    bt_scheme_enumerate_cases(BT_ORBIT_CORNER, cases);
    bt_gd_synth_limits lim;
    limits(&lim);
    bt_gd_scoring_cfg cfg;
    test_scoring(&cfg);
    bt_gd_synth_hit hit;
    BT_CHECK(bt_gd_synth_case(BT_ORBIT_CORNER, cases[0].t1, cases[0].t2, &lim, &cfg, &hit));
    char position[BT_PATH_MAX];
    position_of(BT_ORBIT_CORNER, cases[0].t1, cases[0].t2, position);
    bt_corpus_row rows[1] = {
        row_of(BT_SOLVER_EGGINS, BT_SHEET_CORNER, 1, position, &hit.alg),
    };
    bt_corpus corpus = corpus_of(rows, 1);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    BT_CHECK_EQ(built.offender_count, 0U);
    BT_CHECK_EQ(built.rows_in_pair_space, 1U);
    size_t slot = 0;
    bool found = false;
    for (size_t i = 0; i < built.cases.count; i++) {
        if (built.cases.cases[i].t1 == cases[0].t1 && built.cases.cases[i].t2 == cases[0].t2) {
            slot = i;
            found = true;
            break;
        }
    }
    BT_CHECK(found);
    const bt_gd_case_entry *entry = &built.cases.cases[slot];
    BT_CHECK_EQ(strcmp(entry->orbit, "corner"), 0);
    BT_CHECK(entry->covered);
    BT_CHECK_EQ(strcmp(entry->vendor_algs[0].solver, "eggins"), 0);
    size_t covered = 0;
    for (size_t i = 0; i < built.cases.count; i++) {
        if (built.cases.cases[i].covered) {
            covered++;
        }
    }
    BT_CHECK_EQ(covered, 1U);
    bt_gd_synth_hit_free(&hit);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_build, build_routes_special_sheets_with_their_verdicts) {
    bt_alg alg = {0};
    char bad[64];
    BT_CHECK(bt_alg_parse("", &alg, bad, sizeof bad));
    bt_corpus_row rows[1] = {
        row_of(BT_SOLVER_CHERRY, BT_SHEET_FLIPS, 1, "UF & zz", &alg),
    };
    bt_corpus corpus = corpus_of(rows, 1);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    BT_CHECK_EQ(built.specials.count, 1U);
    const bt_gd_special_entry *special = &built.specials.specials[0];
    BT_CHECK_EQ(strcmp(special->class_name, "flips"), 0);
    BT_CHECK_EQ(strcmp(special->solver, "cherry"), 0);
    BT_CHECK_EQ(special->rowid, 1U);
    BT_CHECK_EQ(strcmp(special->alg, ""), 0);
    BT_CHECK(!special->ok);
    bool has_bad_position = false;
    for (size_t i = 0; i < special->problem_count; i++) {
        if (strstr(special->problems[i], "bad position") != NULL) {
            has_bad_position = true;
        }
    }
    BT_CHECK(has_bad_position);
    BT_CHECK_EQ(built.rows_in_pair_space, 0U);
    BT_CHECK_EQ(built.offender_count, 1U);
    BT_CHECK_EQ(strncmp(built.offenders[0], "cherry/flips/1 UF & zz: ", 24) == 0, 1);
    bool none_covered = true;
    for (size_t i = 0; i < built.cases.count; i++) {
        if (built.cases.cases[i].covered) {
            none_covered = false;
        }
    }
    BT_CHECK(none_covered);
    bt_alg_free(&alg);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_build, build_flags_non_three_style_and_wrong_orbit_rows) {
    bt_alg plain = {0};
    char bad[64];
    BT_CHECK(bt_alg_parse("R", &plain, bad, sizeof bad));
    bt_case corner_cases[BT_PAIRS_MAX];
    bt_scheme_enumerate_cases(BT_ORBIT_CORNER, corner_cases);
    bt_gd_synth_limits lim;
    limits(&lim);
    bt_gd_scoring_cfg cfg;
    test_scoring(&cfg);
    bt_gd_synth_hit corner_hit;
    BT_CHECK(bt_gd_synth_case(BT_ORBIT_CORNER, corner_cases[0].t1, corner_cases[0].t2, &lim, &cfg,
                              &corner_hit));
    bt_corpus_row rows[2] = {
        row_of(BT_SOLVER_CHERRY, BT_SHEET_EDGE, 1, "UF-UB-UR", &plain),
        row_of(BT_SOLVER_EGGINS, BT_SHEET_EDGE, 2, "UF-UB-UR", &corner_hit.alg),
    };
    bt_corpus corpus = corpus_of(rows, 2);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    BT_CHECK_EQ(built.offender_count, 4U);
    bool saw_not_three_style = false;
    bool saw_wrong_orbit = false;
    bool saw_cherry_verify = false;
    bool saw_eggins_verify = false;
    for (size_t i = 0; i < built.offender_count; i++) {
        if (strncmp(built.offenders[i], "cherry/edge/1 is not a three style case: ", 41) == 0) {
            saw_not_three_style = true;
        }
        if (strncmp(built.offenders[i], "eggins/edge/2 walks the wrong orbit Corner", 41) == 0) {
            saw_wrong_orbit = true;
        }
        if (strncmp(built.offenders[i], "cherry/edge/1 UF-UB-UR: ", 24) == 0) {
            saw_cherry_verify = true;
        }
        if (strncmp(built.offenders[i], "eggins/edge/2 UF-UB-UR: ", 24) == 0) {
            saw_eggins_verify = true;
        }
    }
    BT_CHECK(saw_not_three_style);
    BT_CHECK(saw_wrong_orbit);
    BT_CHECK(saw_cherry_verify);
    BT_CHECK(saw_eggins_verify);
    BT_CHECK_EQ(built.rows_in_pair_space, 0U);
    BT_CHECK_EQ(built.rows_other_buffer, 0U);
    bool none_covered = true;
    for (size_t i = 0; i < built.cases.count; i++) {
        if (built.cases.cases[i].covered) {
            none_covered = false;
        }
    }
    BT_CHECK(none_covered);
    bt_alg_free(&plain);
    bt_gd_synth_hit_free(&corner_hit);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_build, build_counts_other_buffer_walks_without_covering) {
    bt_case cases[BT_PAIRS_MAX];
    bt_scheme_enumerate_cases(BT_ORBIT_EDGE, cases);
    bt_gd_synth_limits lim;
    limits(&lim);
    bt_gd_scoring_cfg cfg;
    test_scoring(&cfg);
    bt_gd_synth_hit hit;
    BT_CHECK(bt_gd_synth_case(BT_ORBIT_EDGE, cases[0].t1, cases[0].t2, &lim, &cfg, &hit));
    bt_strbuf hit_text;
    bt_strbuf_init(&hit_text, 0);
    bt_alg_format(&hit.alg, &hit_text);
    char conjugated_text[BT_PATH_MAX];
    snprintf(conjugated_text, sizeof conjugated_text, "U %s U'", hit_text.data);
    bt_alg conjugated = {0};
    char bad[64];
    BT_CHECK(bt_alg_parse(conjugated_text, &conjugated, bad, sizeof bad));
    bt_report report;
    bt_scheme_classify_alg(&conjugated, &report);
    char position[BT_CLASS_POSITION_MAX];
    BT_CHECK(bt_scheme_report_position(&report, position));
    BT_CHECK_EQ(strcmp(position, "UB-UR-UL"), 0);
    bt_corpus_row rows[1] = {
        row_of(BT_SOLVER_CHERRY, BT_SHEET_EDGE, 1, position, &conjugated),
    };
    bt_corpus corpus = corpus_of(rows, 1);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    BT_CHECK_EQ(built.offender_count, 0U);
    BT_CHECK_EQ(built.rows_other_buffer, 1U);
    BT_CHECK_EQ(built.rows_in_pair_space, 0U);
    bool none_covered = true;
    for (size_t i = 0; i < built.cases.count; i++) {
        if (built.cases.cases[i].covered) {
            none_covered = false;
        }
    }
    BT_CHECK(none_covered);
    bt_strbuf_free(&hit_text);
    bt_alg_free(&conjugated);
    bt_gd_synth_hit_free(&hit);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_build, manifest_counts_orbits_and_special_classes) {
    bt_case cases[BT_PAIRS_MAX];
    bt_scheme_enumerate_cases(BT_ORBIT_EDGE, cases);
    bt_gd_synth_limits lim;
    limits(&lim);
    bt_gd_scoring_cfg cfg;
    test_scoring(&cfg);
    bt_gd_synth_hit hit;
    BT_CHECK(bt_gd_synth_case(BT_ORBIT_EDGE, cases[0].t1, cases[0].t2, &lim, &cfg, &hit));
    char position[BT_PATH_MAX];
    position_of(BT_ORBIT_EDGE, cases[0].t1, cases[0].t2, position);
    bt_alg plain = {0};
    char bad[64];
    BT_CHECK(bt_alg_parse("", &plain, bad, sizeof bad));
    bt_corpus_row rows[3] = {
        row_of(BT_SOLVER_CHERRY, BT_SHEET_EDGE, 1, position, &hit.alg),
        row_of(BT_SOLVER_CHERRY, BT_SHEET_FLIPS, 1, "UF & zz", &plain),
        row_of(BT_SOLVER_EGGINS, BT_SHEET_TWISTS, 1, "zz & yy", &plain),
    };
    bt_corpus corpus = corpus_of(rows, 3);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    bt_corpus_digest digest = {0};
    strcpy(digest.sha256, "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd");
    digest.rows = 3;
    strcpy(digest.files[0].solver, "cherry");
    strcpy(digest.files[0].sheet, "edge");
    digest.files[0].rows = 1;
    strcpy(digest.files[0].sha256,
           "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
    digest.file_count = 1;
    bt_gd_manifest manifest;
    bt_gd_manifest_make(
        &built, &digest, "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
        "pppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp",
        "ssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssss", "stamp", &manifest);
    BT_CHECK_EQ(strcmp(manifest.generated_at, "stamp"), 0);
    BT_CHECK_EQ(strcmp(manifest.generator, "bt-gendata"), 0);
    BT_CHECK_EQ(manifest.corpus_rows, 3U);
    BT_CHECK_EQ(manifest.corpus_file_count, 1U);
    BT_CHECK_EQ(strcmp(manifest.corpus_files[0].solver, "cherry"), 0);
    BT_CHECK_EQ(manifest.rows_in_pair_space, 1U);
    BT_CHECK_EQ(manifest.rows_other_buffer, 0U);
    BT_CHECK_EQ(strcmp(manifest.cases.path, "cases.json"), 0);
    BT_CHECK_EQ(manifest.cases.entries, BT_GD_TOTAL_CASES);
    BT_CHECK_EQ(manifest.cases.edge.total, 440U);
    BT_CHECK_EQ(manifest.cases.edge.covered, 1U);
    BT_CHECK_EQ(manifest.cases.edge.uncovered, 439U);
    BT_CHECK_EQ(manifest.cases.corner.total, 378U);
    BT_CHECK_EQ(manifest.cases.corner.covered, 0U);
    BT_CHECK_EQ(manifest.cases.corner.uncovered, 378U);
    BT_CHECK_EQ(strcmp(manifest.specials.path, "specials.json"), 0);
    BT_CHECK_EQ(manifest.specials.entries, 2U);
    BT_CHECK_EQ(manifest.specials.count_count, BT_GD_SPECIAL_COUNTS_LEN);
    for (size_t i = 0; i < BT_GD_SPECIAL_COUNTS_LEN; i++) {
        BT_CHECK_EQ(strcmp(manifest.specials.counts[i].solver, BT_GD_SPECIAL_COUNTS[i].solver), 0);
        BT_CHECK_EQ(
            strcmp(manifest.specials.counts[i].class_name, BT_GD_SPECIAL_COUNTS[i].class_name), 0);
    }
    BT_CHECK_EQ(strcmp(manifest.specials.counts[0].solver, "cherry"), 0);
    BT_CHECK_EQ(strcmp(manifest.specials.counts[0].class_name, "flips"), 0);
    BT_CHECK_EQ(manifest.specials.counts[0].rows, 1U);
    BT_CHECK_EQ(strcmp(manifest.specials.counts[4].solver, "eggins"), 0);
    BT_CHECK_EQ(strcmp(manifest.specials.counts[4].class_name, "flips"), 0);
    BT_CHECK_EQ(manifest.specials.counts[4].rows, 0U);
    BT_CHECK_EQ(strcmp(manifest.specials.counts[5].solver, "eggins"), 0);
    BT_CHECK_EQ(strcmp(manifest.specials.counts[5].class_name, "twists"), 0);
    BT_CHECK_EQ(manifest.specials.counts[5].rows, 1U);
    bt_gd_manifest_free(&manifest);
    bt_alg_free(&plain);
    bt_gd_synth_hit_free(&hit);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}
