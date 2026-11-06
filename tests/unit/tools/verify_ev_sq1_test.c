#include "verify_test_util.h"

#include <string.h>

#include "common/alloc.h"
#include "unit/core/death_test.h"
#include "verify/ev_sq1.h"

static void case_of(const char *alg, verify_sq1_case *out) {
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_sq1_start_case(alg, out, err, sizeof err), 0);
}

static void check_spans(const uint8_t *ring, const verify_sq1_span *want, size_t want_count) {
    verify_sq1_span spans[BT_PZ_SQ1_UNITS];
    size_t count = 0;
    verify_sq1_ring_spans(ring, BT_PZ_SQ1_UNITS, spans, &count);
    BT_CHECK_EQ(count, want_count);
    for (size_t at = 0; at < count && at < want_count; at++) {
        BT_CHECK_EQ(spans[at].id, want[at].id);
        BT_CHECK_EQ(spans[at].width, want[at].width);
    }
}

BT_TEST(verify_ev_sq1, solved_rings_split_into_eight_spans) {
    static const uint8_t top[BT_PZ_SQ1_UNITS] = {0, 0, 1, 2, 2, 3, 4, 4, 5, 6, 6, 7};
    static const uint8_t bottom[BT_PZ_SQ1_UNITS] = {8, 9, 9, 10, 11, 11, 12, 13, 13, 14, 15, 15};
    static const verify_sq1_span want_top[8] = {
        {0, 2}, {1, 1}, {2, 2}, {3, 1}, {4, 2}, {5, 1}, {6, 2}, {7, 1},
    };
    static const verify_sq1_span want_bottom[8] = {
        {8, 1}, {9, 2}, {10, 1}, {11, 2}, {12, 1}, {13, 2}, {14, 1}, {15, 2},
    };
    check_spans(top, want_top, 8);
    check_spans(bottom, want_bottom, 8);
}

BT_TEST(verify_ev_sq1, spans_follow_a_run_wrapped_over_the_ring_seam) {
    static const uint8_t wrapped[BT_PZ_SQ1_UNITS] = {0, 1, 2, 2, 3, 4, 4, 5, 6, 6, 7, 0};
    static const verify_sq1_span want[8] = {
        {1, 1}, {2, 2}, {3, 1}, {4, 2}, {5, 1}, {6, 2}, {7, 1}, {0, 2},
    };
    check_spans(wrapped, want, 8);
}

BT_TEST(verify_ev_sq1, famkey_picks_the_lexicographically_smaller_mirror) {
    uint8_t out[BT_PZ_SQ1_UNITS];
    static const uint8_t widths[3] = {1, 2, 3};
    static const uint8_t mirrored[3] = {1, 3, 2};
    static const uint8_t ascending[3] = {1, 2, 3};
    verify_sq1_mirrored(widths, 3, out);
    BT_CHECK_EQ(memcmp(out, mirrored, 3), 0);
    verify_sq1_famkey(widths, 3, out);
    BT_CHECK_EQ(memcmp(out, ascending, 3), 0);
    verify_sq1_famkey(mirrored, 3, out);
    BT_CHECK_EQ(memcmp(out, ascending, 3), 0);
    verify_sq1_famkey(BT_PZ_SQ1_SQUARE_SHAPE, BT_PZ_SQ1_PIECES_PER_LAYER, out);
    BT_CHECK_EQ(memcmp(out, BT_PZ_SQ1_SQUARE_SHAPE, BT_PZ_SQ1_PIECES_PER_LAYER), 0);
}

BT_TEST(verify_ev_sq1, empty_algs_fail_start_case) {
    verify_sq1_case case_;
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_sq1_start_case("", &case_, err, sizeof err), 1);
    BT_CHECK_EQ(err, "alg is empty");
}

static void sq1_death_cubeshape_malloc(void) {
    bt_alloc_fail_arm(0);
    bt_seed_rows rows;
    rows.items = NULL;
    rows.count = 0;
    verify_section_result section;
    verify_sq1_verify_cubeshape(&rows, &section);
}

BT_TEST(verify_ev_sq1, cubeshape_exits_when_its_case_arrays_starve) {
    bt_death_expect_code("verify_ev_sq1.cubeshape_malloc", "", 1, sq1_death_cubeshape_malloc);
}

BT_TEST(verify_ev_sq1, the_start_state_round_trip_holds_for_a_pack_alg) {
    verify_sq1_case case_;
    case_of("(1,0) / (-1,0)", &case_);
    BT_CHECK_EQ(case_.class_lens[0], BT_PZ_SQ1_PIECES_PER_LAYER);
    BT_CHECK_EQ(memcmp(case_.classes[0], BT_PZ_SQ1_SQUARE_SHAPE, BT_PZ_SQ1_PIECES_PER_LAYER), 0);
    BT_CHECK_EQ(case_.class_lens[1], BT_PZ_SQ1_PIECES_PER_LAYER);
    BT_CHECK_EQ(memcmp(case_.classes[1], BT_PZ_SQ1_SQUARE_SHAPE, BT_PZ_SQ1_PIECES_PER_LAYER), 0);
}

BT_TEST(verify_ev_sq1, a_slash_start_state_is_kite_kite_not_square) {
    verify_sq1_case case_;
    case_of("/", &case_);
    static const uint8_t kite[8] = {1, 1, 2, 1, 2, 2, 1, 2};
    BT_CHECK_EQ(memcmp(case_.classes[0], kite, 8), 0);
    BT_CHECK(memcmp(case_.classes[0], BT_PZ_SQ1_SQUARE_SHAPE, 8) != 0);
}

BT_TEST(verify_ev_sq1, solved_state_passes_every_stronger_predicate) {
    bt_pz_sq1_state state;
    bt_pz_sq1_solved_state(&state);
    bool corners = false;
    bool edges = false;
    bool placed = false;
    verify_sq1_stage_predicates(&state, &corners, &edges, &placed);
    BT_CHECK(corners);
    BT_CHECK(edges);
    BT_CHECK(placed);
}

BT_TEST(verify_ev_sq1, slash_swaps_corners_across_layers_so_orientation_fails) {
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    bt_pz_engine eng = bt_pz_sq1_engine();
    BT_CHECK(eng.parse_alg(&eng, "/", 1, alg, &err));
    bt_pz_sq1_state state;
    bt_pz_sq1_solved_state(&state);
    BT_CHECK(bt_pz_sq1_apply_alg(&state, alg, &err));
    bool corners = true;
    bool edges = true;
    bool placed = true;
    verify_sq1_stage_predicates(&state, &corners, &edges, &placed);
    BT_CHECK(!corners);
    bt_pz_alg_destroy(alg);
}

BT_TEST(verify_ev_sq1, stage_predicates_partition_post_cubeshape_states) {
    verify_sq1_case co_case;
    case_of("(1,0) / (-1,0)", &co_case);
    bool corners = true;
    bool edges = true;
    bool placed = true;
    verify_sq1_stage_predicates(&co_case.state, &corners, &edges, &placed);
    BT_CHECK(!corners);
    bt_pz_sq1_state solved;
    bt_pz_sq1_solved_state(&solved);
    corners = false;
    edges = false;
    placed = false;
    verify_sq1_stage_predicates(&solved, &corners, &edges, &placed);
    BT_CHECK(corners);
    BT_CHECK(edges);
    BT_CHECK(placed);
}

static void case_with_front(const char *alg, const char *front_json, verify_sq1_case *out) {
    char start_err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_sq1_start_case(alg, out, start_err, sizeof start_err), 0);
    out->front_json = front_json;
}

BT_TEST(verify_ev_sq1, a_u_slice_state_passes_cp_but_not_eo) {
    verify_sq1_case case_;
    case_with_front("(0,1)", "{\"case\":\"square / square\"}", &case_);
    verify_tier2 cp = verify_sq1_stage_outcome(VERIFY_SQ1_STAGE_CP, &case_);
    BT_CHECK_EQ(cp.kind, VERIFY_TIER2_PASS);
    verify_tier2 eo = verify_sq1_stage_outcome(VERIFY_SQ1_STAGE_EO, &case_);
    BT_CHECK_EQ(eo.kind, VERIFY_TIER2_FAIL);
    BT_CHECK_EQ(eo.fail, "edges already oriented");
}

BT_TEST(verify_ev_sq1, family_resolution_skips_cases_with_broken_front_json) {
    verify_sq1_case broken;
    case_with_front("(1,0) / (-1,0)", "{bad", &broken);
    const verify_sq1_case *cases[1] = {&broken};
    verify_sq1_family_index index;
    verify_sq1_resolve_family_keys(cases, 1, &index);
    BT_CHECK_EQ(index.key_count, 0U);
    BT_CHECK_EQ(index.ambiguous_count, 0U);
    verify_sq1_family_index_free(&index);
}

BT_TEST(verify_ev_sq1, family_resolution_prefers_the_heavier_later_shape) {
    verify_sq1_case light;
    case_with_front("/", "{\"case\":\"shared / left\"}", &light);
    verify_sq1_case heavy_a;
    case_with_front("(1,0) / (-1,0)", "{\"case\":\"shared / left\"}", &heavy_a);
    verify_sq1_case heavy_b;
    case_with_front("(1,0) / (-1,0)", "{\"case\":\"shared / left\"}", &heavy_b);
    const verify_sq1_case *cases[3] = {&light, &heavy_a, &heavy_b};
    verify_sq1_family_index index;
    verify_sq1_resolve_family_keys(cases, 3, &index);
    BT_CHECK_EQ(index.key_count, 2U);
    BT_CHECK_EQ(index.ambiguous_count, 0U);
    BT_CHECK_EQ(index.keys[0].key_len, BT_PZ_SQ1_PIECES_PER_LAYER);
    BT_CHECK_EQ(index.keys[1].key_len, BT_PZ_SQ1_PIECES_PER_LAYER);
    verify_sq1_family_index_free(&index);
}

enum {
    SQ1_SEAM_AMBIGUOUS_AT = 31,
    SQ1_SEAM_FULL_AT = 32,
    SQ1_SEAM_KEYS_AT = 66,
    SQ1_SEAM_NINE_FULL_AT = 67,
    SQ1_SEAM_SWEEP = 40,
};

BT_TEST(verify_ev_sq1, family_resolution_degrades_under_alloc_starvation) {
    verify_sq1_case winner_a;
    case_with_front("(1,0) / (-1,0)", "{\"case\":\"square / square\"}", &winner_a);
    verify_sq1_case winner_b;
    case_with_front("(1,0) / (-1,0)", "{\"case\":\"square / square\"}", &winner_b);
    verify_sq1_case tie_a;
    case_with_front("/", "{\"case\":\"kite / pawn\"}", &tie_a);
    verify_sq1_case tie_b;
    case_with_front("(1,0) / (-1,0)", "{\"case\":\"kite / pawn2\"}", &tie_b);
    const verify_sq1_case *cases[4] = {&winner_a, &winner_b, &tie_a, &tie_b};
    verify_sq1_family_index baseline;
    verify_sq1_resolve_family_keys(cases, 4, &baseline);
    BT_CHECK_EQ(baseline.key_count, 3U);
    BT_CHECK_EQ(baseline.ambiguous_count, 1U);
    verify_sq1_family_index_free(&baseline);
    for (size_t k = 0; k <= SQ1_SEAM_SWEEP; k++) {
        verify_sq1_family_index index;
        bt_alloc_fail_arm(k);
        verify_sq1_resolve_family_keys(cases, 4, &index);
        bt_alloc_fail_disarm();
        BT_CHECK(index.key_count <= 4U);
        BT_CHECK(index.ambiguous_count <= 4U);
        verify_sq1_family_index_free(&index);
    }
}

BT_TEST(verify_ev_sq1, family_resolution_names_each_late_oom_arm) {
    verify_sq1_case winner_a;
    case_with_front("(1,0) / (-1,0)", "{\"case\":\"square / square\"}", &winner_a);
    verify_sq1_case winner_b;
    case_with_front("(1,0) / (-1,0)", "{\"case\":\"square / square\"}", &winner_b);
    verify_sq1_case tie_a;
    case_with_front("/", "{\"case\":\"kite / pawn\"}", &tie_a);
    verify_sq1_case tie_b;
    case_with_front("(1,0) / (-1,0)", "{\"case\":\"kite / pawn2\"}", &tie_b);
    const verify_sq1_case *cases[4] = {&winner_a, &winner_b, &tie_a, &tie_b};
    verify_sq1_family_index index;
    bt_alloc_fail_arm(SQ1_SEAM_AMBIGUOUS_AT);
    verify_sq1_resolve_family_keys(cases, 4, &index);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(index.key_count, 1U);
    BT_CHECK_EQ(index.ambiguous_count, 0U);
    verify_sq1_family_index_free(&index);
    bt_alloc_fail_arm(SQ1_SEAM_FULL_AT);
    verify_sq1_resolve_family_keys(cases, 4, &index);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(index.key_count, 3U);
    BT_CHECK_EQ(index.ambiguous_count, 1U);
    verify_sq1_family_index_free(&index);
    static verify_sq1_case nine[9];
    static char fronts[9][40];
    static const verify_sq1_case *nine_ptrs[9];
    for (size_t at = 0; at < 9; at++) {
        case_with_front("(1,0) / (-1,0)", NULL, &nine[at]);
        snprintf(fronts[at], sizeof fronts[at], "{\"case\":\"n%zu / n%zu\"}", at, at);
        nine[at].front_json = fronts[at];
        nine_ptrs[at] = &nine[at];
    }
    bt_alloc_fail_arm(SQ1_SEAM_KEYS_AT);
    verify_sq1_resolve_family_keys(nine_ptrs, 9, &index);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(index.key_count, 8U);
    verify_sq1_family_index_free(&index);
    bt_alloc_fail_arm(SQ1_SEAM_NINE_FULL_AT);
    verify_sq1_resolve_family_keys(nine_ptrs, 9, &index);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(index.key_count, 9U);
    verify_sq1_family_index_free(&index);
}

BT_TEST(verify_ev_sq1, stage_outcome_reports_the_strongest_failure_reason) {
    verify_sq1_case case_;
    case_of("/", &case_);
    static const uint8_t kite[8] = {1, 1, 2, 1, 2, 2, 1, 2};
    memcpy(case_.classes[0], kite, 8);
    memcpy(case_.classes[1], kite, 8);
    verify_tier2 outcome = verify_sq1_stage_outcome(VERIFY_SQ1_STAGE_CO, &case_);
    BT_CHECK_EQ(outcome.kind, VERIFY_TIER2_FAIL);
}

BT_TEST(verify_ev_sq1, shape_names_split_the_declared_pair) {
    char top[VERIFY_SQ1_NAME_MAX];
    char bottom[VERIFY_SQ1_NAME_MAX];
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_sq1_shape_names("{\"case\":\"Kite / Shield\"}", top, bottom,
                                       VERIFY_SQ1_NAME_MAX, err, sizeof err),
                0);
    BT_CHECK_EQ(top, "Kite");
    BT_CHECK_EQ(bottom, "Shield");
    BT_CHECK(verify_sq1_shape_names("{\"case\":\"Kite\"}", top, bottom, VERIFY_SQ1_NAME_MAX, err,
                                    sizeof err) != 0);
    BT_CHECK(verify_sq1_shape_names("{\"note\":\"x\"}", top, bottom, VERIFY_SQ1_NAME_MAX, err,
                                    sizeof err) != 0);
}

BT_TEST(verify_ev_sq1, class_invariants_hold_on_the_solved_shapes) {
    bt_pz_error pz_err;
    void *alg = bt_pz_alg_create();
    bt_pz_engine eng = bt_pz_sq1_engine();
    BT_CHECK(eng.parse_alg(&eng, "/ 3,3 /", 7, alg, &pz_err));
    bt_pz_sq1_state state;
    bt_pz_sq1_solved_state(&state);
    BT_CHECK(bt_pz_sq1_apply_alg(&state, alg, &pz_err));
    bt_pz_alg_destroy(alg);
    verify_sq1_case case_ = {0};
    case_.state = state;
    case_.class_lens[0] = bt_pz_sq1_canonical_shape(&state, 0, case_.classes[0], BT_PZ_SQ1_UNITS);
    case_.class_lens[1] = bt_pz_sq1_canonical_shape(&state, 1, case_.classes[1], BT_PZ_SQ1_UNITS);
    case_.front_json = "";
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_sq1_class_invariants(&case_, err, sizeof err), 0);
    verify_sq1_case broken = case_;
    broken.classes[0][0] += 1;
    BT_CHECK(verify_sq1_class_invariants(&broken, err, sizeof err) != 0);
}
