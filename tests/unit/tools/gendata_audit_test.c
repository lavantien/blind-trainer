#include "gendata/audit.h"
#include "gendata/build.h"
#include "gendata/lib.h"
#include "gendata/model.h"

#include <stdlib.h>
#include <string.h>

#include "common/alloc.h"
#include "common/bt_harness.h"
#include "core/alg.h"
#include "data/corpus.h"
#include "data/digest.h"

BT_TEST(gendata_audit, audit_fails_closed_when_an_offender_push_starves) {
    bt_gd_built built = {0};
    built.offenders = malloc(sizeof(char *));
    built.offenders[0] = _strdup("cherry/edge.tsv/9 broken");
    built.offender_count = 1;
    bt_corpus corpus;
    bt_corpus_init(&corpus);
    char **offenders = NULL;
    size_t count = 0;
    bt_alloc_fail_arm(0);
    int rc = bt_gd_audit(&built, &corpus, 0, &offenders, &count);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(rc, -1);
    BT_CHECK_EQ(count, 0U);
    BT_CHECK(offenders == NULL);
    bt_gd_string_list_free(offenders, count);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_audit, audit_lists_every_shortfall_on_an_empty_build) {
    bt_corpus corpus;
    bt_corpus_init(&corpus);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    char **offenders = NULL;
    size_t count = 0;
    BT_CHECK_EQ(bt_gd_audit(&built, &corpus, 3, &offenders, &count), 0);
    BT_CHECK_EQ(count, 12U);
    static const char *const want[12] = {
        "corpus holds 0 rows, expected 1077",         "corpus digest counted 3 rows, expected 1077",
        "edge covered cases are 0, expected 372",     "corner covered cases are 0, expected 288",
        "specials hold 0 entries, expected 89",       "cherry/flips holds 0 entries, expected 25",
        "cherry/twists holds 0 entries, expected 20", "cherry/parity holds 0 entries, expected 26",
        "cherry/ltct holds 0 entries, expected 8",    "eggins/twists holds 0 entries, expected 5",
        "eggins/parity holds 0 entries, expected 2",  "eggins/ltct holds 0 entries, expected 3",
    };
    for (size_t i = 0; i < count && i < 12; i++) {
        BT_CHECK_EQ(strcmp(offenders[i], want[i]), 0);
    }
    bt_gd_string_list_free(offenders, count);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_audit, audit_accepts_a_build_that_matches_every_constant) {
    bt_gd_built built = {0};
    built.cases.count = BT_GD_TOTAL_CASES;
    built.cases.cases = malloc(built.cases.count * sizeof(bt_gd_case_entry));
    size_t at = 0;
    struct {
        const char *orbit;
        size_t total;
        size_t covered;
    } orbits[2] = {{"edge", 440, 372}, {"corner", 378, 288}};
    for (int o = 0; o < 2; o++) {
        for (size_t index = 0; index < orbits[o].total; index++) {
            bt_gd_case_entry *entry = &built.cases.cases[at++];
            entry->orbit = _strdup(orbits[o].orbit);
            entry->t1 = (uint8_t)index;
            entry->t2 = (uint8_t)(index + 1);
            entry->t1_letter = 'A';
            entry->t2_letter = 'B';
            entry->t1_name = _strdup("");
            entry->t2_name = _strdup("");
            entry->covered = index < orbits[o].covered;
            entry->vendor_algs = NULL;
            entry->vendor_count = 0;
        }
    }
    built.specials.count = 0;
    built.specials.specials = malloc(BT_GD_TOTAL_SPECIALS * sizeof(bt_gd_special_entry));
    size_t rowid = 0;
    for (size_t i = 0; i < BT_GD_SPECIAL_COUNTS_LEN; i++) {
        for (size_t n = 0; n < BT_GD_SPECIAL_COUNTS[i].rows; n++) {
            rowid++;
            bt_gd_special_entry *special = &built.specials.specials[built.specials.count++];
            special->class_name = _strdup(BT_GD_SPECIAL_COUNTS[i].class_name);
            special->solver = _strdup(BT_GD_SPECIAL_COUNTS[i].solver);
            special->rowid = rowid;
            special->position = _strdup("");
            special->alg = _strdup("");
            special->qtm = 0;
            special->htm = 0;
            special->ok = true;
            special->problems = NULL;
            special->problem_count = 0;
            special->notes = NULL;
            special->note_count = 0;
            special->commutator = _strdup("");
            special->recon_link = _strdup("");
            special->youtube_link = _strdup("");
        }
    }
    bt_corpus_row *rows = malloc(BT_DATA_TOTAL_ROWS * sizeof(bt_corpus_row));
    for (size_t i = 0; i < BT_DATA_TOTAL_ROWS; i++) {
        rows[i] = (bt_corpus_row){
            .solver = BT_SOLVER_CHERRY,
            .sheet = BT_SHEET_EDGE,
            .rowid = i,
            .position = _strdup(""),
            .alg = {0},
            .alg_text = _strdup(""),
            .commutator = _strdup(""),
            .recon_link = _strdup(""),
            .youtube_link = _strdup(""),
        };
    }
    bt_corpus corpus;
    BT_CHECK_EQ(bt_corpus_from_rows(rows, BT_DATA_TOTAL_ROWS, &corpus), 0);
    char **offenders = NULL;
    size_t count = 0;
    BT_CHECK_EQ(bt_gd_audit(&built, &corpus, BT_DATA_TOTAL_ROWS, &offenders, &count), 0);
    BT_CHECK_EQ(count, 0U);
    bt_gd_string_list_free(offenders, count);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_audit, audit_copies_build_offenders_before_the_shortfalls) {
    bt_gd_built built = {0};
    built.cases.count = 0;
    built.cases.cases = NULL;
    built.offenders = malloc(2 * sizeof(char *));
    built.offenders[0] = _strdup("cherry/edge.tsv/9 broken");
    built.offenders[1] = _strdup("eggins/corner.tsv/4 also broken");
    built.offender_count = 2;
    bt_corpus corpus;
    bt_corpus_init(&corpus);
    char **offenders = NULL;
    size_t count = 0;
    BT_CHECK_EQ(bt_gd_audit(&built, &corpus, 0, &offenders, &count), 0);
    BT_CHECK(count >= 2);
    BT_CHECK_EQ(strcmp(offenders[0], "cherry/edge.tsv/9 broken"), 0);
    BT_CHECK_EQ(strcmp(offenders[1], "eggins/corner.tsv/4 also broken"), 0);
    bt_gd_string_list_free(offenders, count);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_audit, audit_names_catalog_total_and_per_orbit_entry_drift) {
    bt_gd_built built = {0};
    built.cases.count = 3;
    built.cases.cases = malloc(3 * sizeof(bt_gd_case_entry));
    for (size_t i = 0; i < 3; i++) {
        bt_gd_case_entry *entry = &built.cases.cases[i];
        entry->orbit = _strdup(i == 0 ? "edge" : "corner");
        entry->t1 = 0;
        entry->t2 = 1;
        entry->t1_letter = 'A';
        entry->t2_letter = 'B';
        entry->t1_name = _strdup("");
        entry->t2_name = _strdup("");
        entry->covered = false;
        entry->vendor_algs = NULL;
        entry->vendor_count = 0;
    }
    bt_corpus corpus;
    bt_corpus_init(&corpus);
    char **offenders = NULL;
    size_t count = 0;
    BT_CHECK_EQ(bt_gd_audit(&built, &corpus, 0, &offenders, &count), 0);
    bool total_drift = false;
    bool edge_drift = false;
    bool corner_drift = false;
    bool covered_drift = false;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(offenders[i], "case catalog holds 3 entries, expected 818") == 0) {
            total_drift = true;
        }
        if (strcmp(offenders[i], "edge case catalog holds 1 entries, expected 440") == 0) {
            edge_drift = true;
        }
        if (strcmp(offenders[i], "corner case catalog holds 2 entries, expected 378") == 0) {
            corner_drift = true;
        }
        if (strstr(offenders[i], "covered cases are") != NULL) {
            covered_drift = true;
        }
    }
    BT_CHECK(total_drift);
    BT_CHECK(edge_drift);
    BT_CHECK(corner_drift);
    BT_CHECK(!covered_drift);
    bt_gd_string_list_free(offenders, count);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_audit, audit_names_covered_drift_when_entry_counts_hold) {
    bt_gd_built built = {0};
    built.cases.count = BT_GD_TOTAL_CASES;
    built.cases.cases = malloc(built.cases.count * sizeof(bt_gd_case_entry));
    size_t at = 0;
    static const struct {
        const char *orbit;
        size_t total;
    } orbits[2] = {{"edge", 440}, {"corner", 378}};
    for (int o = 0; o < 2; o++) {
        for (size_t index = 0; index < orbits[o].total; index++) {
            bt_gd_case_entry *entry = &built.cases.cases[at++];
            entry->orbit = _strdup(orbits[o].orbit);
            entry->t1 = 0;
            entry->t2 = 1;
            entry->t1_letter = 'A';
            entry->t2_letter = 'B';
            entry->t1_name = _strdup("");
            entry->t2_name = _strdup("");
            entry->covered = false;
            entry->vendor_algs = NULL;
            entry->vendor_count = 0;
        }
    }
    bt_corpus corpus;
    bt_corpus_init(&corpus);
    char **offenders = NULL;
    size_t count = 0;
    BT_CHECK_EQ(bt_gd_audit(&built, &corpus, 0, &offenders, &count), 0);
    bool edge_covered_drift = false;
    bool corner_covered_drift = false;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(offenders[i], "edge covered cases are 0, expected 372") == 0) {
            edge_covered_drift = true;
        }
        if (strcmp(offenders[i], "corner covered cases are 0, expected 288") == 0) {
            corner_covered_drift = true;
        }
    }
    BT_CHECK(edge_covered_drift);
    BT_CHECK(corner_covered_drift);
    bt_gd_string_list_free(offenders, count);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}
