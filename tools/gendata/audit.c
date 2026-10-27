#include "gendata/audit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "data/digest.h"
#include "gendata/lib.h"

static int push(char ***out, size_t *count, const char *text) {
    char **grown = realloc(*out, (*count + 1) * sizeof(char *));
    if (grown == NULL) {
        return -1;
    }
    *out = grown;
    (*out)[*count] = _strdup(text);
    *count += 1;
    return 0;
}

void bt_gd_audit(const bt_gd_built *built, const bt_corpus *corpus, size_t digest_rows, char ***out,
                 size_t *count) {
    BT_TRACE_ENTER("gd.audit.audit");
    *out = NULL;
    *count = 0;
    for (size_t i = 0; i < built->offender_count; i++) {
        push(out, count, built->offenders[i]);
    }
    char line[192];
    size_t total_rows = bt_corpus_total_rows(corpus);
    if (total_rows != (size_t)BT_DATA_TOTAL_ROWS) {
        snprintf(line, sizeof line, "corpus holds %zu rows, expected %d", total_rows,
                 BT_DATA_TOTAL_ROWS);
        push(out, count, line);
    }
    if (digest_rows != (size_t)BT_DATA_TOTAL_ROWS) {
        snprintf(line, sizeof line, "corpus digest counted %zu rows, expected %d", digest_rows,
                 BT_DATA_TOTAL_ROWS);
        push(out, count, line);
    }
    if (built->cases.count != (size_t)BT_GD_TOTAL_CASES) {
        snprintf(line, sizeof line, "case catalog holds %zu entries, expected %d",
                 built->cases.count, BT_GD_TOTAL_CASES);
        push(out, count, line);
    }
    static const struct {
        const char *orbit;
        size_t total;
        size_t covered;
    } orbits[2] = {
        {"edge", BT_GD_EDGE_CASES, BT_GD_EDGE_COVERED},
        {"corner", BT_GD_CORNER_CASES, BT_GD_CORNER_COVERED},
    };
    for (int o = 0; o < 2; o++) {
        size_t entries = 0;
        size_t hit = 0;
        for (size_t i = 0; i < built->cases.count; i++) {
            if (strcmp(built->cases.cases[i].orbit, orbits[o].orbit) == 0) {
                entries++;
                if (built->cases.cases[i].covered) {
                    hit++;
                }
            }
        }
        if (entries != orbits[o].total) {
            snprintf(line, sizeof line, "%s case catalog holds %zu entries, expected %zu",
                     orbits[o].orbit, entries, orbits[o].total);
            push(out, count, line);
            continue;
        }
        if (hit != orbits[o].covered) {
            snprintf(line, sizeof line, "%s covered cases are %zu, expected %zu", orbits[o].orbit,
                     hit, orbits[o].covered);
            push(out, count, line);
        }
    }
    if (built->specials.count != (size_t)BT_GD_TOTAL_SPECIALS) {
        snprintf(line, sizeof line, "specials hold %zu entries, expected %d", built->specials.count,
                 BT_GD_TOTAL_SPECIALS);
        push(out, count, line);
    }
    for (size_t i = 0; i < BT_GD_SPECIAL_COUNTS_LEN; i++) {
        size_t found = 0;
        for (size_t s = 0; s < built->specials.count; s++) {
            if (strcmp(built->specials.specials[s].solver, BT_GD_SPECIAL_COUNTS[i].solver) == 0 &&
                strcmp(built->specials.specials[s].class_name,
                       BT_GD_SPECIAL_COUNTS[i].class_name) == 0) {
                found++;
            }
        }
        if (found != BT_GD_SPECIAL_COUNTS[i].rows) {
            snprintf(line, sizeof line, "%s/%s holds %zu entries, expected %zu",
                     BT_GD_SPECIAL_COUNTS[i].solver, BT_GD_SPECIAL_COUNTS[i].class_name, found,
                     BT_GD_SPECIAL_COUNTS[i].rows);
            push(out, count, line);
        }
    }
    BT_TRACE_EXIT("gd.audit.audit");
}
