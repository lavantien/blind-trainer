#include "gendata/audit.h"
#include "gendata/build.h"
#include "gendata/lib.h"
#include "gendata/model.h"

#include <stdlib.h>
#include <string.h>

#include "common/bt_harness.h"
#include "common/paths.h"
#include "core/alg.h"
#include "data/corpus.h"
#include "data/digest.h"
#include "scheme/pairs.h"
#include "scheme/scheme.h"
#include "unit/tools/spawn_util.h"

static void ref_ground(char out[BT_PATH_MAX]) {
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    snprintf(out, BT_PATH_MAX, "%s/ref_ground", root);
}

static void loaded(bt_corpus *corpus) {
    char root[BT_PATH_MAX];
    ref_ground(root);
    bt_corpus_error err;
    if (bt_corpus_load(root, corpus, &err) != 0) {
        char text[BT_PATH_MAX + 96];
        bt_corpus_error_string(&err, text, sizeof text);
        BT_ABORT("%s", text);
    }
}

static const bt_gd_case_entry *find_case(const bt_gd_built *built, const char *orbit, uint8_t t1,
                                         uint8_t t2) {
    for (size_t i = 0; i < built->cases.count; i++) {
        const bt_gd_case_entry *entry = &built->cases.cases[i];
        if (strcmp(entry->orbit, orbit) == 0 && entry->t1 == t1 && entry->t2 == t2) {
            return entry;
        }
    }
    BT_ABORT("%s", "case missing");
    return NULL;
}

BT_TEST(gendata_vendor_pins, case_space_counts_match_coverage_md) {
    bt_corpus corpus;
    loaded(&corpus);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    BT_CHECK_EQ(built.cases.count, 818U);
    size_t edge = 0;
    size_t corner = 0;
    size_t edge_covered = 0;
    size_t corner_covered = 0;
    for (size_t i = 0; i < built.cases.count; i++) {
        const bt_gd_case_entry *entry = &built.cases.cases[i];
        bool is_edge = strcmp(entry->orbit, "edge") == 0;
        if (is_edge) {
            edge++;
        } else {
            corner++;
        }
        if (entry->covered) {
            if (is_edge) {
                edge_covered++;
            } else {
                corner_covered++;
            }
        }
    }
    BT_CHECK_EQ(edge, 440U);
    BT_CHECK_EQ(corner, 378U);
    BT_CHECK_EQ(edge_covered, 372U);
    BT_CHECK_EQ(edge - edge_covered, 68U);
    BT_CHECK_EQ(corner_covered, 288U);
    BT_CHECK_EQ(corner - corner_covered, 90U);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_vendor_pins, covered_flag_equals_vendor_algs_presence) {
    bt_corpus corpus;
    loaded(&corpus);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    for (size_t i = 0; i < built.cases.count; i++) {
        const bt_gd_case_entry *entry = &built.cases.cases[i];
        BT_CHECK_EQ(entry->covered, entry->vendor_count != 0);
    }
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_vendor_pins, case_entries_are_unique_sorted_and_fully_labeled) {
    bt_corpus corpus;
    loaded(&corpus);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    for (size_t i = 0; i + 1 < built.cases.count; i++) {
        const bt_gd_case_entry *left = &built.cases.cases[i];
        const bt_gd_case_entry *right = &built.cases.cases[i + 1];
        uint8_t left_orbit = strcmp(left->orbit, "edge") == 0 ? 0 : 1;
        uint8_t right_orbit = strcmp(right->orbit, "edge") == 0 ? 0 : 1;
        BT_CHECK(left_orbit < right_orbit ||
                 (left_orbit == right_orbit &&
                  (left->t1 < right->t1 || (left->t1 == right->t1 && left->t2 < right->t2))));
    }
    for (size_t i = 0; i < built.cases.count; i++) {
        const bt_gd_case_entry *entry = &built.cases.cases[i];
        BT_CHECK(entry->t1_letter >= 'A' && entry->t1_letter <= 'Z');
        BT_CHECK_EQ(entry->t1_letter, bt_scheme_letter_of(entry->t1));
        BT_CHECK_EQ(entry->t2_letter, bt_scheme_letter_of(entry->t2));
        BT_CHECK_EQ(strcmp(entry->t1_name, bt_scheme_name_of(entry->t1)), 0);
        BT_CHECK_EQ(strcmp(entry->t2_name, bt_scheme_name_of(entry->t2)), 0);
        bool is_edge = strcmp(entry->orbit, "edge") == 0;
        BT_CHECK(is_edge ? (entry->t1 >= 24 && entry->t1 < 48 && entry->t2 >= 24 && entry->t2 < 48)
                         : (entry->t1 < 24 && entry->t2 < 24));
    }
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_vendor_pins, row_flow_matches_coverage_md) {
    bt_corpus corpus;
    loaded(&corpus);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    BT_CHECK_EQ(built.rows_in_pair_space, 796U);
    BT_CHECK_EQ(built.rows_other_buffer, 192U);
    BT_CHECK_EQ(built.rows_in_pair_space + built.rows_other_buffer, 988U);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_vendor_pins, known_case_join_carries_every_matching_row) {
    bt_corpus corpus;
    loaded(&corpus);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    const bt_gd_case_entry *entry = find_case(&built, "edge", 24, 27);
    BT_CHECK_EQ(strcmp(entry->t1_name, "UB"), 0);
    BT_CHECK_EQ(strcmp(entry->t2_name, "UL"), 0);
    BT_CHECK_EQ(entry->t1_letter, 'A');
    BT_CHECK_EQ(entry->t2_letter, 'D');
    BT_CHECK(entry->covered);
    BT_CHECK_EQ(entry->vendor_count, 3U);
    BT_CHECK_EQ(strcmp(entry->vendor_algs[0].solver, "cherry"), 0);
    BT_CHECK_EQ(entry->vendor_algs[0].rowid, 2U);
    BT_CHECK_EQ(strcmp(entry->vendor_algs[0].position, "UF-UB-UL"), 0);
    BT_CHECK_EQ(strcmp(entry->vendor_algs[0].alg, "U M2 U M' U2 M U M2 U'"), 0);
    BT_CHECK_EQ(entry->vendor_algs[0].qtm, 12U);
    BT_CHECK_EQ(entry->vendor_algs[0].htm, 9U);
    BT_CHECK(entry->vendor_algs[0].ok);
    BT_CHECK_EQ(strcmp(entry->vendor_algs[1].solver, "cherry"), 0);
    BT_CHECK_EQ(entry->vendor_algs[1].rowid, 3U);
    BT_CHECK_EQ(strcmp(entry->vendor_algs[2].solver, "eggins"), 0);
    for (size_t i = 0; i < entry->vendor_count; i++) {
        BT_CHECK(entry->vendor_algs[i].ok);
        BT_CHECK_EQ(strcmp(entry->vendor_algs[i].position, "UF-UB-UL"), 0);
    }
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_vendor_pins, corner_case_join_spans_solvers) {
    bt_corpus corpus;
    loaded(&corpus);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    const bt_gd_case_entry *entry = NULL;
    for (size_t i = 0; i < built.cases.count; i++) {
        const bt_gd_case_entry *candidate = &built.cases.cases[i];
        if (strcmp(candidate->orbit, "corner") == 0 && strcmp(candidate->t1_name, "BDR") == 0 &&
            strcmp(candidate->t2_name, "BUL") == 0) {
            entry = candidate;
            break;
        }
    }
    BT_CHECK(entry != NULL);
    if (entry == NULL) {
        return;
    }
    BT_CHECK(entry->covered);
    bool saw_cherry = false;
    bool saw_eggins = false;
    for (size_t i = 0; i < entry->vendor_count; i++) {
        if (strcmp(entry->vendor_algs[i].solver, "cherry") == 0) {
            saw_cherry = true;
        }
        if (strcmp(entry->vendor_algs[i].solver, "eggins") == 0) {
            saw_eggins = true;
        }
        BT_CHECK_EQ(strcmp(entry->vendor_algs[i].position, "UFR-BDR-BUL"), 0);
    }
    BT_CHECK(saw_cherry);
    BT_CHECK(saw_eggins);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_vendor_pins, specials_counts_match_coverage_md) {
    bt_corpus corpus;
    loaded(&corpus);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    BT_CHECK_EQ(built.specials.count, 89U);
    size_t counts[2][4] = {0};
    for (size_t i = 0; i < built.specials.count; i++) {
        const bt_gd_special_entry *entry = &built.specials.specials[i];
        int solver = strcmp(entry->solver, "cherry") == 0 ? 0 : 1;
        int class_index = -1;
        if (strcmp(entry->class_name, "flips") == 0) {
            class_index = 0;
        } else if (strcmp(entry->class_name, "twists") == 0) {
            class_index = 1;
        } else if (strcmp(entry->class_name, "parity") == 0) {
            class_index = 2;
        } else if (strcmp(entry->class_name, "ltct") == 0) {
            class_index = 3;
        }
        BT_CHECK(class_index >= 0);
        counts[solver][class_index]++;
    }
    BT_CHECK_EQ(counts[0][0], 25U);
    BT_CHECK_EQ(counts[0][1], 20U);
    BT_CHECK_EQ(counts[0][2], 26U);
    BT_CHECK_EQ(counts[0][3], 8U);
    BT_CHECK_EQ(counts[1][0], 0U);
    BT_CHECK_EQ(counts[1][1], 5U);
    BT_CHECK_EQ(counts[1][2], 2U);
    BT_CHECK_EQ(counts[1][3], 3U);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_vendor_pins, every_vendor_row_and_special_verifies_ok) {
    bt_corpus corpus;
    loaded(&corpus);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    BT_CHECK_EQ(built.offender_count, 0U);
    for (size_t i = 0; i < built.cases.count; i++) {
        const bt_gd_case_entry *entry = &built.cases.cases[i];
        for (size_t a = 0; a < entry->vendor_count; a++) {
            BT_CHECK(entry->vendor_algs[a].ok);
            BT_CHECK_EQ(entry->vendor_algs[a].problem_count, 0U);
        }
    }
    for (size_t i = 0; i < built.specials.count; i++) {
        BT_CHECK(built.specials.specials[i].ok);
    }
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

static bt_corpus_row clone_row(const bt_corpus_row *src) {
    bt_alg alg;
    bt_alg_init(&alg);
    for (size_t i = 0; i < src->alg.len; i++) {
        bt_alg_push(&alg, src->alg.items[i]);
    }
    return (bt_corpus_row){
        .solver = src->solver,
        .sheet = src->sheet,
        .rowid = src->rowid,
        .position = _strdup(src->position),
        .alg = alg,
        .alg_text = _strdup(src->alg_text),
        .commutator = _strdup(src->commutator),
        .recon_link = _strdup(src->recon_link),
        .youtube_link = _strdup(src->youtube_link),
    };
}

BT_TEST(gendata_vendor_pins, audit_passes_on_the_real_corpus) {
    bt_corpus corpus;
    loaded(&corpus);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    char **offenders = NULL;
    size_t count = 0;
    bt_gd_audit(&built, &corpus, 1077, &offenders, &count);
    BT_CHECK_EQ(count, 0U);
    bt_gd_string_list_free(offenders, count);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_vendor_pins, audit_flags_count_and_verify_breaks) {
    bt_corpus corpus;
    loaded(&corpus);
    bt_corpus_row *rows = malloc(corpus.count * sizeof(bt_corpus_row));
    for (size_t i = 0; i < corpus.count; i++) {
        rows[i] = clone_row(&corpus.rows[i]);
    }
    const bt_corpus_row *donor = bt_corpus_row_at(&corpus, BT_SOLVER_CHERRY, BT_SHEET_EDGE, 4);
    BT_CHECK(donor != NULL);
    if (donor == NULL) {
        return;
    }
    bt_alg_free(&rows[0].alg);
    bt_alg swapped;
    bt_alg_init(&swapped);
    for (size_t i = 0; i < donor->alg.len; i++) {
        bt_alg_push(&swapped, donor->alg.items[i]);
    }
    rows[0].alg = swapped;
    bt_corpus broken;
    BT_CHECK_EQ(bt_corpus_from_rows(rows, corpus.count, &broken), 0);
    bt_gd_built built;
    bt_gd_build(&broken, &built);
    BT_CHECK(built.offender_count > 0);
    char **offenders = NULL;
    size_t count = 0;
    bt_gd_audit(&built, &broken, 1077, &offenders, &count);
    bool saw = false;
    for (size_t i = 0; i < count; i++) {
        if (strstr(offenders[i], "cherry/edge/2") != NULL) {
            saw = true;
        }
    }
    BT_CHECK(saw);
    bt_gd_string_list_free(offenders, count);
    bt_gd_built_free(&built);
    bt_corpus_free(&broken);

    bt_corpus_row *truncated = malloc(corpus.count * sizeof(bt_corpus_row));
    for (size_t i = 0; i < corpus.count; i++) {
        truncated[i] = clone_row(&corpus.rows[i]);
    }
    bt_corpus_row *popped = &truncated[corpus.count - 1];
    free(popped->position);
    free(popped->alg_text);
    free(popped->commutator);
    free(popped->recon_link);
    free(popped->youtube_link);
    bt_alg_free(&popped->alg);
    bt_corpus short_corpus;
    BT_CHECK_EQ(bt_corpus_from_rows(truncated, corpus.count - 1, &short_corpus), 0);
    bt_gd_built short_built;
    bt_gd_build(&short_corpus, &short_built);
    char **short_offenders = NULL;
    size_t short_count = 0;
    bt_gd_audit(&short_built, &short_corpus, 1076, &short_offenders, &short_count);
    bool saw_expected = false;
    for (size_t i = 0; i < short_count; i++) {
        if (strstr(short_offenders[i], "expected 1077") != NULL) {
            saw_expected = true;
        }
    }
    BT_CHECK(saw_expected);
    bt_gd_string_list_free(short_offenders, short_count);
    bt_gd_built_free(&short_built);
    bt_corpus_free(&short_corpus);
    bt_corpus_free(&corpus);
}
