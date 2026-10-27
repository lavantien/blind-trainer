#include "puzzles/tier1.h"

#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"

bt_tier1_status bt_verify_tier1(const bt_pz_engine *eng, const char *text, bt_tier1_error *err) {
    BT_TRACE_ENTER("bt_verify_tier1");
    bt_pz_error sink;
    bt_pz_error *cause = err != NULL ? &err->cause : &sink;
    bt_tier1_status status = BT_TIER1_OK;
    void *alg = NULL;
    void *inverse = NULL;
    void *forward = NULL;
    void *reverse = NULL;
    alg = bt_pz_alg_create();
    if (!eng->parse_alg(eng, text, strlen(text), alg, cause)) {
        status = BT_TIER1_PARSE;
        goto done;
    }
    inverse = bt_pz_alg_create();
    eng->invert_alg(eng, alg, inverse);
    forward = malloc(eng->state_size);
    reverse = malloc(eng->state_size);
    if (forward == NULL || reverse == NULL) {
        BT_TRACE_FAIL("bt_verify_tier1", "state malloc failed");
        bt_trace_dump();
        abort();
    }
    eng->solved_state(eng, forward);
    if (!eng->apply_alg(eng, forward, alg, cause)) {
        status = BT_TIER1_APPLY;
        goto done;
    }
    if (!eng->apply_alg(eng, forward, inverse, cause)) {
        status = BT_TIER1_APPLY;
        goto done;
    }
    if (!eng->is_solved(eng, forward)) {
        status = BT_TIER1_FORWARD;
        goto done;
    }
    eng->solved_state(eng, reverse);
    if (!eng->apply_alg(eng, reverse, inverse, cause)) {
        status = BT_TIER1_APPLY;
        goto done;
    }
    if (!eng->apply_alg(eng, reverse, alg, cause)) {
        status = BT_TIER1_APPLY;
        goto done;
    }
    if (!eng->is_solved(eng, reverse)) {
        status = BT_TIER1_REVERSE;
        goto done;
    }
done:
    bt_pz_alg_destroy(alg);
    bt_pz_alg_destroy(inverse);
    free(forward);
    free(reverse);
    if (status == BT_TIER1_OK) {
        BT_TRACE_EXIT("bt_verify_tier1");
        return status;
    }
    if (err != NULL) {
        err->status = status;
    }
    BT_TRACE_FAIL("bt_verify_tier1", "status %d", (int)status);
    return status;
}

void bt_tier1_error_message(const bt_tier1_error *err, bt_strbuf *out) {
    BT_TRACE_ENTER("bt_tier1_error_message");
    if (err->status == BT_TIER1_PARSE) {
        bt_strbuf_append_str(out, "tier 1 parse failed: ");
        bt_pz_error_message(&err->cause, out);
    } else if (err->status == BT_TIER1_APPLY) {
        bt_strbuf_append_str(out, "tier 1 apply failed: ");
        bt_pz_error_message(&err->cause, out);
    } else if (err->status == BT_TIER1_FORWARD) {
        bt_strbuf_append_str(out, "tier 1 round trip failed: alg then inverse");
    } else if (err->status == BT_TIER1_REVERSE) {
        bt_strbuf_append_str(out, "tier 1 round trip failed: inverse then alg");
    }
    BT_TRACE_EXIT("bt_tier1_error_message");
}
