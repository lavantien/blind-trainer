#include "gendata/audit.h"
#include "gendata/build.h"
#include "gendata/lib.h"
#include "gendata/model.h"

#include <stdlib.h>
#include <string.h>

#include "common/bt_harness.h"
#include "core/alg.h"
#include "data/corpus.h"
#include "data/digest.h"

BT_TEST(gendata_audit, audit_lists_every_shortfall_on_an_empty_build) {
    bt_corpus corpus;
    bt_corpus_init(&corpus);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    char **offenders = NULL;
    size_t count = 0;
    bt_gd_audit(&built, &corpus, 3, &offenders, &count);
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
    bt_gd_audit(&built, &corpus, BT_DATA_TOTAL_ROWS, &offenders, &count);
    BT_CHECK_EQ(count, 0U);
    bt_gd_string_list_free(offenders, count);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}
