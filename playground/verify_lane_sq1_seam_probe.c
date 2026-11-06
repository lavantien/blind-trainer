#include <stdio.h>
#include <string.h>

#include "common/alloc.h"
#include "verify/ev_sq1.h"

void verify_section_init(verify_section_result *section) {
    (void)section;
}
void verify_section_push_row(verify_section_result *section, const verify_row_outcome *row) {
    (void)section;
    (void)row;
}
void verify_section_push_honesty(verify_section_result *section, const char *text) {
    (void)section;
    (void)text;
}
verify_tier2 verify_tier2_skipped(const char *reason) {
    verify_tier2 out = {0};
    out.kind = VERIFY_TIER2_SKIPPED;
    out.skip = reason;
    return out;
}
verify_tier2 verify_tier2_pass(void) {
    verify_tier2 out = {0};
    return out;
}
verify_tier2 verify_tier2_fail(const char *reason) {
    verify_tier2 out = {0};
    out.kind = VERIFY_TIER2_FAIL;
    snprintf(out.fail, sizeof out.fail, "%s", reason);
    return out;
}

static void case_of(const char *alg, const char *front_json, verify_sq1_case *out) {
    char err[512];
    if (verify_sq1_start_case(alg, out, err, sizeof err) != 0) {
        printf("case failed %s: %s\n", alg, err);
    }
    out->front_json = front_json;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    verify_sq1_case winner_a;
    case_of("(1,0) / (-1,0)", "{\"case\":\"square / square\"}", &winner_a);
    verify_sq1_case winner_b;
    case_of("(1,0) / (-1,0)", "{\"case\":\"square / square\"}", &winner_b);
    verify_sq1_case tie_a;
    case_of("/", "{\"case\":\"kite / pawn\"}", &tie_a);
    verify_sq1_case tie_b;
    case_of("(1,0) / (-1,0)", "{\"case\":\"kite / pawn2\"}", &tie_b);
    const verify_sq1_case *cases[4] = {&winner_a, &winner_b, &tie_a, &tie_b};
    verify_sq1_family_index baseline;
    verify_sq1_resolve_family_keys(cases, 4, &baseline);
    printf("baseline keys=%zu ambiguous=%zu\n", baseline.key_count, baseline.ambiguous_count);
    verify_sq1_family_index_free(&baseline);
    for (size_t k = 0; k < 400; k++) {
        verify_sq1_family_index index;
        bt_alloc_fail_arm(k);
        verify_sq1_resolve_family_keys(cases, 4, &index);
        bt_alloc_fail_disarm();
        printf("k=%zu keys=%zu ambiguous=%zu\n", k, index.key_count, index.ambiguous_count);
        verify_sq1_family_index_free(&index);
    }
    static verify_sq1_case nine[9];
    static char fronts[9][40];
    static const verify_sq1_case *nine_ptrs[9];
    for (size_t at = 0; at < 9; at++) {
        case_of("(1,0) / (-1,0)", NULL, &nine[at]);
        snprintf(fronts[at], sizeof fronts[at], "{\"case\":\"n%zu / n%zu\"}", at, at);
        nine[at].front_json = fronts[at];
        nine_ptrs[at] = &nine[at];
    }
    for (size_t k = 60; k < 140; k++) {
        verify_sq1_family_index index;
        bt_alloc_fail_arm(k);
        verify_sq1_resolve_family_keys(nine_ptrs, 9, &index);
        bt_alloc_fail_disarm();
        printf("nine k=%zu keys=%zu ambiguous=%zu\n", k, index.key_count, index.ambiguous_count);
        verify_sq1_family_index_free(&index);
    }
    static const char *const stage_algs[] = {
        "/",
        "(1,0) /",
        "(0,1)",
        "(1,0) / (-1,0)",
        "(-2,0) /",
        "(1,1) /",
        "(2,-1) /",
        "(3,0) /",
        "(0,-1) /",
        "/ (1,0) /",
        "/ /",
        "(1,0) / (-1,0) (1,0) / (-1,0)",
        "(2,0) / (-2,0)",
        "(1,1) / (-1,-1)",
        "(3,0) / (-3,0)",
        "(0,2)",
        "(0,3)",
        "(0,4)",
        "(0,5)",
        "(0,6)",
        "(1,0) / (0,-1)",
        "(0,1) (1,0) / (-1,0)",
    };
    for (size_t at = 0; at < sizeof stage_algs / sizeof stage_algs[0]; at++) {
        verify_sq1_case probe_case;
        case_of(stage_algs[at], "{\"case\":\"square / square\"}", &probe_case);
        bool corners = false;
        bool edges = false;
        bool placed = false;
        verify_sq1_stage_predicates(&probe_case.state, &corners, &edges, &placed);
        verify_tier2 eo = verify_sq1_stage_outcome(VERIFY_SQ1_STAGE_EO, &probe_case);
        verify_tier2 cp = verify_sq1_stage_outcome(VERIFY_SQ1_STAGE_CP, &probe_case);
        printf("stage %s corners=%d edges=%d placed=%d eo=%d cp=%d\n", stage_algs[at], corners,
               edges, placed, eo.kind == VERIFY_TIER2_PASS, cp.kind == VERIFY_TIER2_PASS);
    }
    return 0;
}
