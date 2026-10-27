#include "data/seedpack/rulings.h"

#include "common/bt_trace.h"

#include <string.h>

const char *bt_failure_class_name(bt_failure_class class) {
    switch (class) {
    case BT_FAILURE_PARSE:
        return "parse";
    case BT_FAILURE_APPLY:
        return "apply";
    case BT_FAILURE_TIER2:
        return "tier2";
    }
    return "parse";
}

bool bt_classify_failure(const char *tier1_err, bool tier2_failed, bt_failure_class *out) {
    BT_TRACE_ENTER("seedpack.classify_failure");
    if (tier1_err != NULL) {
        bool parse = (strncmp(tier1_err, "unknown token", 13) == 0 ||
                      strstr(tier1_err, "parse failed") != NULL) != 0;
        *out = parse ? BT_FAILURE_PARSE : BT_FAILURE_APPLY;
        BT_TRACE_EXIT("seedpack.classify_failure");
        return true;
    }
    if (tier2_failed) {
        *out = BT_FAILURE_TIER2;
        BT_TRACE_EXIT("seedpack.classify_failure");
        return true;
    }
    BT_TRACE_EXIT("seedpack.classify_failure");
    return false;
}

static const char R_TRACE_TWIST[] =
    "unexecutable slash sequence: hand traced, cs.122 differs by one twist sign and executes";
static const char R_TRACE_LONG[] =
    "unexecutable slash sequence: hand traced, no state maps to solved through the stored alg";
static const char R_AUF[] =
    "AUF-dependent executability: the stripped prefix plausibly restores legality, in-engine "
    "conversion rests on an unpinned sign convention";
static const char R_FLIPPED[] =
    "flipped layer convention in the source data, recorded defect, row kept verbatim";
static const char R_MEGA[] =
    "EP row displaces corners beyond every center-pinned y-spin frame, prime-slip suspect family "
    "of the R2 U2-prime cubeskills rows, no single-token suffix flip reconciles it (pinned "
    "probe), recorded source defect";
static const char R_PYRA[] =
    "disturbs five or more edge facelets outside last four edges scope once the U layer frame is "
    "composed out, 27 of 35 rows fit the exact l4e shape, hand diff on l4e.11 vertex bands, "
    "recorded source defect";
static const char R_BIGCUBE[] =
    "engine semantics proven canonical by the wide-turn identity pins (Rw equals R 2R and kin), "
    "the stored alg solves no reduction state under canonical semantics in either direction, no "
    "global conjugation of six reconciles it (pinned probe), recorded source defect";
static const char R_M2_CONVENTION[] =
    "pack written in the speedsolving wiki M E convention (M like L-prime, E like D-prime) while "
    "bt-core M follows L and E follows D, single-direction M and E tokens act inverted, M2 "
    "tokens are convention-invariant which is why only 4 of 24 rows fail, m2.18 under the wiki "
    "reading delivers DB into the DF buffer (convention proof), recorded convention defect";
static const char R_M2_LABEL[] =
    "fails under both the wiki and bt-core M E readings, label defect on top of the M E "
    "convention divergence, recorded source defect";

static const bt_expected_failure_row EXPECTED_FAILURES[] = {
    {"sq1", "lbl", "cubeshape", "cs.119", BT_FAILURE_APPLY, R_TRACE_TWIST},
    {"sq1", "lbl", "cubeshape", "cs.075", BT_FAILURE_TIER2, R_FLIPPED},
    {"sq1", "lbl", "cubeshape", "cs.078", BT_FAILURE_TIER2, R_FLIPPED},
    {"sq1", "lbl", "cubeshape", "cs.084", BT_FAILURE_TIER2, R_FLIPPED},
    {"sq1", "lbl", "cubeshape", "cs.089", BT_FAILURE_TIER2, R_FLIPPED},
    {"sq1", "lbl", "cubeshape", "cs.097", BT_FAILURE_TIER2, R_FLIPPED},
    {"sq1", "lbl", "cubeshape", "cs.109", BT_FAILURE_TIER2, R_FLIPPED},
    {"sq1", "lbl", "cubeshape", "cs.118", BT_FAILURE_TIER2, R_FLIPPED},
    {"sq1", "lbl", "cubeshape", "cs.129", BT_FAILURE_TIER2, R_FLIPPED},
    {"sq1", "lbl", "eo", "eo.01", BT_FAILURE_APPLY, R_TRACE_LONG},
    {"sq1", "lbl", "eo", "eo.04", BT_FAILURE_APPLY, R_TRACE_LONG},
    {"sq1", "lbl", "eo", "eo.06", BT_FAILURE_APPLY, R_TRACE_LONG},
    {"sq1", "lbl", "ep", "ep.40", BT_FAILURE_APPLY, R_TRACE_LONG},
    {"sq1", "lbl", "ep", "ep.66", BT_FAILURE_APPLY, R_TRACE_LONG},
    {"sq1", "lbl", "ep", "ep.80", BT_FAILURE_APPLY, R_TRACE_LONG},
    {"sq1", "lbl", "ep", "ep.05", BT_FAILURE_APPLY, R_AUF},
    {"sq1", "lbl", "ep", "ep.18", BT_FAILURE_APPLY, R_AUF},
    {"sq1", "lbl", "ep", "ep.50", BT_FAILURE_APPLY, R_AUF},
    {"megaminx", "4lll", "4lll", "4lll.20", BT_FAILURE_TIER2, R_MEGA},
    {"megaminx", "4lll", "4lll", "4lll.21", BT_FAILURE_TIER2, R_MEGA},
    {"megaminx", "4lll", "4lll", "4lll.22", BT_FAILURE_TIER2, R_MEGA},
    {"megaminx", "4lll", "4lll", "4lll.24", BT_FAILURE_TIER2, R_MEGA},
    {"pyraminx", "l4e", "l4e", "l4e.06", BT_FAILURE_TIER2, R_PYRA},
    {"pyraminx", "l4e", "l4e", "l4e.11", BT_FAILURE_TIER2, R_PYRA},
    {"pyraminx", "l4e", "l4e", "l4e.21", BT_FAILURE_TIER2, R_PYRA},
    {"pyraminx", "l4e", "l4e", "l4e.22", BT_FAILURE_TIER2, R_PYRA},
    {"pyraminx", "l4e", "l4e", "l4e.24", BT_FAILURE_TIER2, R_PYRA},
    {"pyraminx", "l4e", "l4e", "l4e.25", BT_FAILURE_TIER2, R_PYRA},
    {"pyraminx", "l4e", "l4e", "l4e.28", BT_FAILURE_TIER2, R_PYRA},
    {"pyraminx", "l4e", "l4e", "l4e.35", BT_FAILURE_TIER2, R_PYRA},
    {"bigcube", "yau", "parity", "parity.oll4", BT_FAILURE_TIER2, R_BIGCUBE},
    {"bigcube", "yau", "parity", "parity.oll4flip", BT_FAILURE_TIER2, R_BIGCUBE},
    {"bigcube", "yau", "parity", "parity.pll4", BT_FAILURE_TIER2, R_BIGCUBE},
    {"bigcube", "yau", "parity", "parity.pll4adj", BT_FAILURE_TIER2, R_BIGCUBE},
    {"bigcube", "yau", "parity", "parity.wing5", BT_FAILURE_TIER2, R_BIGCUBE},
    {"bigcube", "reduction", "parity", "l2e.01", BT_FAILURE_TIER2, R_BIGCUBE},
    {"bigcube", "reduction", "parity", "l2e.02", BT_FAILURE_TIER2, R_BIGCUBE},
    {"bigcube", "reduction", "parity", "l2e.03", BT_FAILURE_TIER2, R_BIGCUBE},
    {"bigcube", "reduction", "parity", "l2e.04", BT_FAILURE_TIER2, R_BIGCUBE},
    {"bigcube", "reduction", "parity", "l2e.05", BT_FAILURE_TIER2, R_BIGCUBE},
    {"bigcube", "reduction", "parity", "l2e.06", BT_FAILURE_TIER2, R_BIGCUBE},
    {"bigcube", "reduction", "parity", "l2e.07", BT_FAILURE_TIER2, R_BIGCUBE},
    {"bigcube", "reduction", "parity", "l2e.08", BT_FAILURE_TIER2, R_BIGCUBE},
    {"bigcube", "reduction", "parity", "l2e.09", BT_FAILURE_TIER2, R_BIGCUBE},
    {"bigcube", "reduction", "parity", "l2e.10", BT_FAILURE_TIER2, R_BIGCUBE},
    {"bigcube", "reduction", "parity", "l2e.11", BT_FAILURE_TIER2, R_BIGCUBE},
    {"bigcube", "reduction", "parity", "l2e.12", BT_FAILURE_TIER2, R_BIGCUBE},
    {"3bld", "m2", "concept", "m2.18", BT_FAILURE_TIER2, R_M2_CONVENTION},
    {"3bld", "m2", "concept", "m2.20", BT_FAILURE_TIER2, R_M2_CONVENTION},
    {"3bld", "m2", "concept", "m2.22", BT_FAILURE_TIER2, R_M2_CONVENTION},
    {"3bld", "m2", "concept", "m2.19", BT_FAILURE_TIER2, R_M2_LABEL},
};

const bt_expected_failure_row *bt_seedpack_expected_failures(size_t *count) {
    *count = sizeof EXPECTED_FAILURES / sizeof EXPECTED_FAILURES[0];
    return EXPECTED_FAILURES;
}

static const bt_expected_failure_row *find_ruling(const char *event, const char *method,
                                                  const char *stage, const char *case_key) {
    for (size_t i = 0; i < sizeof EXPECTED_FAILURES / sizeof EXPECTED_FAILURES[0]; i++) {
        const bt_expected_failure_row *row = &EXPECTED_FAILURES[i];
        if (strcmp(row->event, event) == 0 && strcmp(row->method, method) == 0 &&
            strcmp(row->stage, stage) == 0 && strcmp(row->case_key, case_key) == 0) {
            return row;
        }
    }
    return NULL;
}

const char *bt_expected_failure(const char *event, const char *method, const char *stage,
                                const char *case_key) {
    BT_TRACE_ENTER("seedpack.expected_failure");
    const bt_expected_failure_row *row = find_ruling(event, method, stage, case_key);
    BT_TRACE_EXIT("seedpack.expected_failure");
    return row == NULL ? NULL : row->reason;
}

bool bt_expected_failure_class(const char *event, const char *method, const char *stage,
                               const char *case_key, bt_failure_class *out) {
    BT_TRACE_ENTER("seedpack.expected_failure_class");
    const bt_expected_failure_row *row = find_ruling(event, method, stage, case_key);
    if (row == NULL) {
        BT_TRACE_EXIT("seedpack.expected_failure_class");
        return false;
    }
    *out = row->class;
    BT_TRACE_EXIT("seedpack.expected_failure_class");
    return true;
}
