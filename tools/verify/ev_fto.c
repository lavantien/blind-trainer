#include "verify/ev_fto.h"

#include <stdio.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/strbuf.h"
#include "puzzles/fto.h"
#include "puzzles/tier1.h"

static const char *const FTO_HONESTY =
    "fto l4t tags name source-sheet case families (Last Layer, Last Three Edges, bar shapes), "
    "the reduction layer geometry has no engine predicate, rows are tier-1 round trips only";

void verify_fto_verify_l4t(const bt_seed_rows *rows, verify_section_result *out) {
    BT_TRACE_ENTER("ev_fto.verify_l4t");
    bt_fto engine = bt_fto_new();
    bt_pz_engine eng = bt_fto_engine(&engine);
    verify_section_init(out);
    for (size_t at = 0; at < rows->count; at++) {
        const bt_seed_row *row = &rows->items[at];
        verify_row_outcome outcome = {0};
        snprintf(outcome.case_key, sizeof outcome.case_key, "%s", row->case_key);
        if (row->alg[0] == '\0') {
            snprintf(outcome.tier1_err, sizeof outcome.tier1_err, "%s", "alg is empty");
        } else {
            bt_tier1_error tier1_err;
            if (bt_verify_tier1(&eng, row->alg, &tier1_err) == BT_TIER1_OK) {
                outcome.tier1_ok = true;
            } else {
                bt_strbuf sb;
                bt_strbuf_init(&sb, 64);
                bt_tier1_error_message(&tier1_err, &sb);
                snprintf(outcome.tier1_err, sizeof outcome.tier1_err, "%s", sb.data);
                bt_strbuf_free(&sb);
            }
        }
        outcome.tier2 = verify_tier2_skipped("l4t case families have no state predicate");
        verify_section_push_row(out, &outcome);
    }
    verify_section_push_honesty(out, FTO_HONESTY);
    BT_TRACE_EXIT("ev_fto.verify_l4t");
}
