#include "verify_test_util.h"

#include <string.h>

#include "puzzles/sq1.h"
#include "verify/ev_sq1.h"

static void square_case(verify_sq1_case *out, const char *front_json) {
    memset(out, 0, sizeof *out);
    bt_pz_sq1_solved_state(&out->state);
    for (size_t layer = 0; layer < 2; layer++) {
        out->class_lens[layer] =
            bt_pz_sq1_canonical_shape(&out->state, layer, out->classes[layer], BT_PZ_SQ1_UNITS);
    }
    out->front_json = front_json;
}

BT_TEST(verify_ev_sq1_pred, shape_names_rejects_each_malformed_front) {
    char top[VERIFY_SQ1_NAME_MAX];
    char bottom[VERIFY_SQ1_NAME_MAX];
    char err[VERIFY_ERR_MAX];
    BT_CHECK(verify_sq1_shape_names("{\"case\":", top, bottom, VERIFY_SQ1_NAME_MAX, err,
                                    sizeof err) != 0);
    BT_CHECK(verify_sq1_shape_names("{}", top, bottom, VERIFY_SQ1_NAME_MAX, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "case") != NULL || err[0] != '\0');
    BT_CHECK(verify_sq1_shape_names("{\"case\": 7}", top, bottom, VERIFY_SQ1_NAME_MAX, err,
                                    sizeof err) != 0);
    BT_CHECK(verify_sq1_shape_names("{\"case\": \"shield\"}", top, bottom, VERIFY_SQ1_NAME_MAX, err,
                                    sizeof err) != 0);
    BT_CHECK(strstr(err, "two layers") != NULL);
}

static const uint8_t RING_P[BT_PZ_SQ1_PIECES_PER_LAYER] = {1, 2, 1, 2, 1, 2, 1, 2};
static const uint8_t RING_Q[BT_PZ_SQ1_PIECES_PER_LAYER] = {1, 1, 2, 1, 2, 1, 2, 1};
static const uint8_t RING_NO_CORNERS[BT_PZ_SQ1_PIECES_PER_LAYER] = {1, 1, 1, 1, 1, 1, 3, 3};

static void crafted_case(verify_sq1_case *out, const char *front_json, const uint8_t *top,
                         const uint8_t *bottom) {
    memset(out, 0, sizeof *out);
    bt_pz_sq1_solved_state(&out->state);
    memcpy(out->classes[0], top, BT_PZ_SQ1_PIECES_PER_LAYER);
    memcpy(out->classes[1], bottom, BT_PZ_SQ1_PIECES_PER_LAYER);
    out->class_lens[0] = BT_PZ_SQ1_PIECES_PER_LAYER;
    out->class_lens[1] = BT_PZ_SQ1_PIECES_PER_LAYER;
    out->front_json = front_json;
}

BT_TEST(verify_ev_sq1_pred, class_invariants_reject_short_rings_and_missing_corners) {
    verify_sq1_case case_;
    char err[VERIFY_ERR_MAX];
    crafted_case(&case_, "{\"case\": \"a / b\"}", RING_P, RING_P);
    BT_CHECK_EQ(verify_sq1_class_invariants(&case_, err, sizeof err), 0);
    crafted_case(&case_, "{\"case\": \"a / b\"}", RING_Q, RING_P);
    BT_CHECK(verify_sq1_class_invariants(&case_, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "ring sums") != NULL);
    crafted_case(&case_, "{\"case\": \"a / b\"}", RING_NO_CORNERS, RING_NO_CORNERS);
    BT_CHECK(verify_sq1_class_invariants(&case_, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "corners") != NULL);
}

BT_TEST(verify_ev_sq1_pred, mirrored_breaks_ties_with_the_unchanged_ring) {
    uint8_t widths[4] = {1, 2, 3, 4};
    uint8_t out[4];
    verify_sq1_mirrored(widths, 4, out);
    BT_CHECK_EQ(out[0], 1);
    uint8_t rising[4] = {4, 3, 2, 1};
    verify_sq1_mirrored(rising, 4, out);
    BT_CHECK_EQ(out[0], 1);
    BT_CHECK_EQ(out[1], 2);
    BT_CHECK_EQ(out[2], 3);
    BT_CHECK_EQ(out[3], 4);
    uint8_t fam[4];
    verify_sq1_famkey(rising, 4, fam);
    BT_CHECK_EQ(fam[0], 1);
    uint8_t falling[4] = {1, 2, 3, 4};
    verify_sq1_famkey(falling, 4, fam);
    BT_CHECK_EQ(fam[0], 1);
    BT_CHECK_EQ(fam[3], 4);
}

BT_TEST(verify_ev_sq1_pred, family_keys_pick_winners_flag_ties_and_grow) {
    enum { POOL = 46 };
    verify_sq1_case pool[POOL];
    const verify_sq1_case *cases[POOL];
    for (size_t at = 0; at < POOL; at++) {
        cases[at] = &pool[at];
    }
    crafted_case(&pool[0], "{\"case\": \"alpha / beta\"}", RING_P, RING_P);
    crafted_case(&pool[1], "{\"case\": \"alpha / gamma\"}", RING_P, RING_Q);
    crafted_case(&pool[2], "{\"case\": \"alpha / delta\"}", RING_Q, RING_P);
    crafted_case(&pool[3], "{\"case\": \"tied / x\"}", RING_P, RING_P);
    crafted_case(&pool[4], "{\"case\": \"tied / x\"}", RING_Q, RING_Q);
    char names[2][9][32];
    for (size_t at = 0; at < 9; at++) {
        snprintf(names[0][at], sizeof names[0][at], "{\"case\": \"g%zu / h%zu\"}", at, at);
        crafted_case(&pool[5 + at], names[0][at], RING_P, RING_P);
    }
    for (size_t at = 0; at < 9; at++) {
        snprintf(names[1][at], sizeof names[1][at], "{\"case\": \"k%zu / l%zu\"}", at, at);
    }
    for (size_t at = 0; at < 9; at++) {
        crafted_case(&pool[14 + 2 * at], names[1][at], RING_P, RING_P);
        crafted_case(&pool[15 + 2 * at], names[1][at], RING_Q, RING_Q);
    }
    crafted_case(&pool[32], "{\"case\": \"solo / z\"}", RING_P, RING_P);
    verify_sq1_family_index index;
    verify_sq1_resolve_family_keys(cases, 33, &index);
    BT_CHECK_EQ(index.key_count, 24);
    BT_CHECK_EQ(index.ambiguous_count, 20);
    BT_CHECK_EQ(index.ambiguous[0], "tied");
    bool alpha_found = false;
    for (size_t at = 0; at < index.key_count; at++) {
        if (strcmp(index.keys[at].name, "alpha") == 0) {
            alpha_found = true;
            BT_CHECK_EQ(index.keys[at].key[0], 1);
            BT_CHECK_EQ(index.keys[at].key[1], 2);
        }
    }
    BT_CHECK(alpha_found);
    verify_sq1_family_index_free(&index);
}

BT_TEST(verify_ev_sq1_pred, cubeshape_tier2_names_every_failure) {
    static const uint8_t ring_rotated[BT_PZ_SQ1_PIECES_PER_LAYER] = {2, 1, 2, 1, 2, 1, 2, 1};
    verify_sq1_case case_;
    crafted_case(&case_, "{\"case\": \"a / a\"}", RING_P, ring_rotated);
    verify_tier2 crossed = verify_sq1_cubeshape_tier2(&case_, &(verify_sq1_family_index){0});
    BT_CHECK(strstr(crossed.fail, "different layer classes") != NULL);

    crafted_case(&case_, "not json", RING_P, RING_P);
    verify_tier2 broken = verify_sq1_cubeshape_tier2(&case_, &(verify_sq1_family_index){0});
    BT_CHECK(broken.fail[0] != '\0');

    crafted_case(&case_, "{\"case\": \"a / b\"}", RING_P, RING_P);
    verify_sq1_family_index ambiguous;
    ambiguous.keys = NULL;
    ambiguous.key_count = 0;
    ambiguous.ambiguous = NULL;
    ambiguous.ambiguous_count = 1;
    ambiguous.ambiguous = malloc(sizeof *ambiguous.ambiguous);
    snprintf(ambiguous.ambiguous[0], VERIFY_SQ1_NAME_MAX, "%s", "a");
    verify_tier2 named = verify_sq1_cubeshape_tier2(&case_, &ambiguous);
    BT_CHECK(strstr(named.fail, "ambiguous") != NULL);
    free(ambiguous.ambiguous);

    verify_tier2 missing = verify_sq1_cubeshape_tier2(&case_, &(verify_sq1_family_index){0});
    BT_CHECK(strstr(missing.fail, "no family keys") != NULL);

    verify_sq1_family_index wrong;
    wrong.keys = calloc(2, sizeof *wrong.keys);
    wrong.key_count = 2;
    snprintf(wrong.keys[0].name, sizeof wrong.keys[0].name, "%s", "a");
    memcpy(wrong.keys[0].key, RING_NO_CORNERS, BT_PZ_SQ1_PIECES_PER_LAYER);
    wrong.keys[0].key_len = case_.class_lens[0];
    snprintf(wrong.keys[1].name, sizeof wrong.keys[1].name, "%s", "b");
    memcpy(wrong.keys[1].key, RING_P, BT_PZ_SQ1_PIECES_PER_LAYER);
    wrong.keys[1].key_len = case_.class_lens[1];
    wrong.ambiguous = NULL;
    wrong.ambiguous_count = 0;
    verify_tier2 mismatch = verify_sq1_cubeshape_tier2(&case_, &wrong);
    BT_CHECK(strstr(mismatch.fail, "names crossed") != NULL);
    memcpy(wrong.keys[0].key, RING_P, BT_PZ_SQ1_PIECES_PER_LAYER);
    verify_tier2 passed = verify_sq1_cubeshape_tier2(&case_, &wrong);
    BT_CHECK_EQ(passed.kind, VERIFY_TIER2_PASS);
    free(wrong.keys);
}

static void rotate_ring(uint8_t *units, size_t len, size_t shift) {
    uint8_t copy[BT_PZ_SQ1_UNITS];
    memcpy(copy, units, len);
    for (size_t at = 0; at < len; at++) {
        units[at] = copy[(at + shift) % len];
    }
}

BT_TEST(verify_ev_sq1_pred, stage_outcome_covers_every_predicate_arm) {
    verify_sq1_case case_;
    square_case(&case_, "{\"case\": \"a / b\"}");
    case_.classes[1][0] = 3;
    verify_tier2 shape = verify_sq1_stage_outcome(VERIFY_SQ1_STAGE_CO, &case_);
    BT_CHECK(strstr(shape.fail, "not square") != NULL);

    square_case(&case_, "{\"case\": \"a / b\"}");
    verify_tier2 co_done = verify_sq1_stage_outcome(VERIFY_SQ1_STAGE_CO, &case_);
    BT_CHECK(strstr(co_done.fail, "already oriented") != NULL);

    square_case(&case_, "{\"case\": \"a / b\"}");
    case_.state.units[0] = 1;
    case_.state.units[1] = 1;
    case_.state.units[2] = 0;
    verify_tier2 co_open = verify_sq1_stage_outcome(VERIFY_SQ1_STAGE_CO, &case_);
    BT_CHECK_EQ(co_open.kind, VERIFY_TIER2_PASS);
    verify_tier2 eo_behind = verify_sq1_stage_outcome(VERIFY_SQ1_STAGE_EO, &case_);
    BT_CHECK(strstr(eo_behind.fail, "corners not oriented") != NULL);

    square_case(&case_, "{\"case\": \"a / b\"}");
    case_.state.units[2] = 0;
    verify_tier2 eo_open = verify_sq1_stage_outcome(VERIFY_SQ1_STAGE_EO, &case_);
    BT_CHECK_EQ(eo_open.kind, VERIFY_TIER2_PASS);
    verify_tier2 cp_behind = verify_sq1_stage_outcome(VERIFY_SQ1_STAGE_CP, &case_);
    BT_CHECK(strstr(cp_behind.fail, "edges not oriented") != NULL);

    square_case(&case_, "{\"case\": \"a / b\"}");
    rotate_ring(case_.state.units, BT_PZ_SQ1_UNITS, 1);
    verify_tier2 cp_open = verify_sq1_stage_outcome(VERIFY_SQ1_STAGE_CP, &case_);
    BT_CHECK_EQ(cp_open.kind, VERIFY_TIER2_PASS);
    square_case(&case_, "{\"case\": \"a / b\"}");
    verify_tier2 cp_done = verify_sq1_stage_outcome(VERIFY_SQ1_STAGE_CP, &case_);
    BT_CHECK(strstr(cp_done.fail, "corners already placed") != NULL);

    square_case(&case_, "{\"case\": \"a / b\"}");
    verify_tier2 eo_done = verify_sq1_stage_outcome(VERIFY_SQ1_STAGE_EO, &case_);
    BT_CHECK(strstr(eo_done.fail, "edges already oriented") != NULL);
    verify_tier2 ep_done = verify_sq1_stage_outcome(VERIFY_SQ1_STAGE_EP, &case_);
    BT_CHECK(strstr(ep_done.fail, "already solved") != NULL);
    case_.state.slice_flipped = true;
    verify_tier2 ep_open = verify_sq1_stage_outcome(VERIFY_SQ1_STAGE_EP, &case_);
    BT_CHECK_EQ(ep_open.kind, VERIFY_TIER2_PASS);
    case_.state.slice_flipped = false;
    uint8_t hold_a = case_.state.units[0];
    uint8_t hold_b = case_.state.units[1];
    case_.state.units[0] = case_.state.units[6];
    case_.state.units[1] = case_.state.units[7];
    case_.state.units[6] = hold_a;
    case_.state.units[7] = hold_b;
    verify_tier2 ep_order = verify_sq1_stage_outcome(VERIFY_SQ1_STAGE_EP, &case_);
    BT_CHECK(strstr(ep_order.fail, "solved order") != NULL);
    BT_CHECK(!verify_sq1_corners_ordered(&case_.state));
}

BT_TEST(verify_ev_sq1_pred, ring_spans_collapses_a_uniform_ring) {
    uint8_t ring[BT_PZ_SQ1_UNITS];
    memset(ring, 7, sizeof ring);
    verify_sq1_span spans[BT_PZ_SQ1_UNITS];
    size_t count = 0;
    verify_sq1_ring_spans(ring, BT_PZ_SQ1_UNITS, spans, &count);
    BT_CHECK_EQ(count, 1);
    BT_CHECK_EQ(spans[0].id, 7);
    BT_CHECK_EQ(spans[0].width, BT_PZ_SQ1_UNITS);
}

BT_TEST(verify_ev_sq1_pred, start_case_rejects_engine_parse_failures) {
    verify_sq1_case case_;
    char err[VERIFY_ERR_MAX];
    BT_CHECK(verify_sq1_start_case("(9,0)", &case_, err, sizeof err) != 0);
    BT_CHECK(verify_sq1_start_case("zz", &case_, err, sizeof err) != 0);
    BT_CHECK(err[0] != '\0');
}
