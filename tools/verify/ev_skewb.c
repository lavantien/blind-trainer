#include "verify/ev_skewb.h"

#include <stdio.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/strbuf.h"
#include "data/notation.h"
#include "puzzles/skewb.h"
#include "puzzles/tier1.h"

static const char *const SKEWB_HONESTY =
    "skewb sarah tags name method stages and repeat across rows holding one shared alg (R' F R "
    "F' covers L2 and L4), so no per-row case state exists to compare";

void verify_skewb_verify_sarah(const bt_seed_rows *rows, verify_section_result *out) {
    BT_TRACE_ENTER("ev_skewb.verify_sarah");
    bt_skewb engine = bt_skewb_new();
    bt_pz_engine eng = bt_skewb_engine(&engine);
    verify_section_init(out);
    for (size_t at = 0; at < rows->count; at++) {
        const bt_seed_row *row = &rows->items[at];
        verify_row_outcome outcome = {0};
        snprintf(outcome.case_key, sizeof outcome.case_key, "%s", row->case_key);
        char stripped[VERIFY_ERR_MAX];
        if (bt_notation_strip_rotations(row->alg, stripped, sizeof stripped) != 0 ||
            stripped[0] == '\0') {
            snprintf(outcome.tier1_err, sizeof outcome.tier1_err, "%s",
                     "alg is empty after rotation stripping");
        } else {
            bt_tier1_error tier1_err;
            if (bt_verify_tier1(&eng, stripped, &tier1_err) == BT_TIER1_OK) {
                outcome.tier1_ok = true;
            } else {
                bt_strbuf sb;
                bt_strbuf_init(&sb, 64);
                bt_tier1_error_message(&tier1_err, &sb);
                snprintf(outcome.tier1_err, sizeof outcome.tier1_err, "%s", sb.data);
                bt_strbuf_free(&sb);
            }
        }
        outcome.tier2 = verify_tier2_skipped("sarah stage tags are not state-derivable");
        verify_section_push_row(out, &outcome);
    }
    verify_section_push_honesty(out, SKEWB_HONESTY);
    BT_TRACE_EXIT("ev_skewb.verify_sarah");
}
