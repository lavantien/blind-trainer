#include <string.h>

#include "common/bt_harness.h"
#include "verify/ev_bld.h"
#include "verify/packs.h"

#define T_PERM "R U R' U' R' F R2 U' R' U' R U R' F'"
#define FLIP_UF_UB "M' U2 M U R' F' R S R' F R S' U"
#define TWIST_UFR_UBR "R' D' R D R' D' R U R' D R D' R' D R U'"
#define SIX_EDGE "F2 U2 R U' R U R U R U' R' U' R2"
#define H_PERM "M2 U M2 U2 M2 U M2"

static bt_seed_rows rows_holder;

static void one_row(const char *alg, const char *front_json) {
    static bt_seed_row row;
    memset(&row, 0, sizeof row);
    snprintf(row.case_key, sizeof row.case_key, "%s", "kinds.1");
    row.alg = (char *)alg;
    row.front_json = (char *)front_json;
    row.source_url = (char *)"https://example.test";
    row.source_at = (char *)"2026-10-10";
    rows_holder.items = &row;
    rows_holder.count = 1;
}

static const verify_row_outcome *single(verify_section_result *section) {
    if (section->count != 1) {
        return NULL;
    }
    return &section->rows[0];
}

#define RUN_AND_CHECK(entry, tier2_kind, reason_needle)                                            \
    do {                                                                                           \
        verify_section_result section;                                                             \
        entry(&rows_holder, &section);                                                             \
        const verify_row_outcome *outcome = single(&section);                                      \
        BT_CHECK(outcome != NULL);                                                                 \
        if (outcome != NULL) {                                                                     \
            BT_CHECK_EQ(outcome->tier1_ok, true);                                                  \
            BT_CHECK(outcome->tier2.kind == tier2_kind);                                           \
            if (reason_needle != NULL) {                                                           \
                const char *text =                                                                 \
                    tier2_kind == VERIFY_TIER2_FAIL ? outcome->tier2.fail : outcome->tier2.skip;   \
                BT_CHECK(text != NULL && strstr(text, reason_needle) != NULL);                     \
            }                                                                                      \
        }                                                                                          \
        verify_section_free(&section);                                                             \
    } while (0)

BT_TEST(verify_ev_bld_kinds, oldpoch_parity_with_swaps_passes_on_a_t_perm) {
    one_row(T_PERM, "{\"kind\":\"parity\",\"swaps\":\"UR-UL+UFR-UBR\"}");
    RUN_AND_CHECK(verify_bld_verify_oldpoch, VERIFY_TIER2_PASS, NULL);
}

BT_TEST(verify_ev_bld_kinds, oldpoch_parity_swaps_parse_failure_names_the_spec) {
    one_row(T_PERM, "{\"kind\":\"parity\",\"swaps\":\"UR-UL\"}");
    RUN_AND_CHECK(verify_bld_verify_oldpoch, VERIFY_TIER2_FAIL, "does not hold two pairs");
}

BT_TEST(verify_ev_bld_kinds, oldpoch_parity_swaps_reject_an_unrelated_alg) {
    one_row("R U R'", "{\"kind\":\"parity\",\"swaps\":\"UR-UL+UFR-UBR\"}");
    RUN_AND_CHECK(verify_bld_verify_oldpoch, VERIFY_TIER2_FAIL, "are not transposed");
}

BT_TEST(verify_ev_bld_kinds, oldpoch_parity_without_swaps_passes_on_two_and_two) {
    one_row(T_PERM, "{\"kind\":\"parity\"}");
    RUN_AND_CHECK(verify_bld_verify_oldpoch, VERIFY_TIER2_PASS, NULL);
}

BT_TEST(verify_ev_bld_kinds, oldpoch_parity_without_swaps_rejects_four_edges) {
    one_row("M2", "{\"kind\":\"parity\"}");
    RUN_AND_CHECK(verify_bld_verify_oldpoch, VERIFY_TIER2_FAIL, "expected 2 and 2");
}

BT_TEST(verify_ev_bld_kinds, oldpoch_corner_twist_passes_confined_to_two_corners) {
    one_row(TWIST_UFR_UBR, "{\"kind\":\"corner twist\",\"twists\":\"UFR+UBR\"}");
    RUN_AND_CHECK(verify_bld_verify_oldpoch, VERIFY_TIER2_PASS, NULL);
}

BT_TEST(verify_ev_bld_kinds, oldpoch_corner_twist_without_names_fails) {
    one_row(TWIST_UFR_UBR, "{\"kind\":\"corner twist\"}");
    RUN_AND_CHECK(verify_bld_verify_oldpoch, VERIFY_TIER2_FAIL, "names no twists");
}

BT_TEST(verify_ev_bld_kinds, oldpoch_corner_twist_rejects_a_non_face_name) {
    one_row(TWIST_UFR_UBR, "{\"kind\":\"corner twist\",\"twists\":\"UFR+XX\"}");
    RUN_AND_CHECK(verify_bld_verify_oldpoch, VERIFY_TIER2_FAIL, "non-face letter");
}

BT_TEST(verify_ev_bld_kinds, oldpoch_corner_twist_rejects_a_permuting_alg) {
    one_row(H_PERM, "{\"kind\":\"corner twist\",\"twists\":\"UB+UF+UL+UR\"}");
    RUN_AND_CHECK(verify_bld_verify_oldpoch, VERIFY_TIER2_FAIL, "permutes pieces");
}

BT_TEST(verify_ev_bld_kinds, oldpoch_corner_twist_rejects_a_solved_state) {
    one_row("R R'", "{\"kind\":\"corner twist\",\"twists\":\"UFR+UBR\"}");
    RUN_AND_CHECK(verify_bld_verify_oldpoch, VERIFY_TIER2_FAIL, "leaves the cube solved");
}

BT_TEST(verify_ev_bld_kinds, oldpoch_edge_swap_passes_when_only_the_pair_moves) {
    one_row(T_PERM, "{\"kind\":\"edge swap\",\"buffer\":\"UR\",\"target\":\"UL\"}");
    RUN_AND_CHECK(verify_bld_verify_oldpoch, VERIFY_TIER2_PASS, NULL);
}

BT_TEST(verify_ev_bld_kinds, oldpoch_corner_swap_passes_on_a_t_perm) {
    one_row(T_PERM, "{\"kind\":\"corner swap\",\"buffer\":\"UFR\",\"target\":\"UBR\"}");
    RUN_AND_CHECK(verify_bld_verify_oldpoch, VERIFY_TIER2_PASS, NULL);
}

BT_TEST(verify_ev_bld_kinds, oldpoch_setup_example_uses_the_swap_predicate) {
    one_row(T_PERM, "{\"kind\":\"setup example\",\"buffer\":\"UFR\",\"target\":\"UBR\"}");
    RUN_AND_CHECK(verify_bld_verify_oldpoch, VERIFY_TIER2_PASS, NULL);
}

BT_TEST(verify_ev_bld_kinds, oldpoch_swap_without_buffer_fails) {
    one_row(T_PERM, "{\"kind\":\"edge swap\"}");
    RUN_AND_CHECK(verify_bld_verify_oldpoch, VERIFY_TIER2_FAIL, "names no buffer");
}

BT_TEST(verify_ev_bld_kinds, oldpoch_swap_without_target_fails) {
    one_row(T_PERM, "{\"kind\":\"edge swap\",\"buffer\":\"UR\"}");
    RUN_AND_CHECK(verify_bld_verify_oldpoch, VERIFY_TIER2_FAIL, "names no target");
}

BT_TEST(verify_ev_bld_kinds, oldpoch_swap_rejects_extra_displaced_edges) {
    one_row(H_PERM, "{\"kind\":\"edge swap\",\"buffer\":\"UR\",\"target\":\"UL\"}");
    RUN_AND_CHECK(verify_bld_verify_oldpoch, VERIFY_TIER2_FAIL, "left its slot");
}

BT_TEST(verify_ev_bld_kinds, oldpoch_unknown_kind_has_no_predicate) {
    one_row(T_PERM, "{\"kind\":\"weird\"}");
    RUN_AND_CHECK(verify_bld_verify_oldpoch, VERIFY_TIER2_FAIL, "has no predicate");
}

BT_TEST(verify_ev_bld_kinds, oldpoch_row_without_kind_or_letter_fails) {
    one_row(T_PERM, "{}");
    RUN_AND_CHECK(verify_bld_verify_oldpoch, VERIFY_TIER2_FAIL, "neither kind nor letter");
}

BT_TEST(verify_ev_bld_kinds, oldpoch_prose_alg_earns_a_honesty_note_and_skips_tier2) {
    one_row("", "{}");
    verify_section_result section;
    verify_bld_verify_oldpoch(&rows_holder, &section);
    const verify_row_outcome *outcome = single(&section);
    BT_CHECK(outcome != NULL);
    if (outcome != NULL) {
        BT_CHECK(!outcome->tier1_ok);
        BT_CHECK(strstr(outcome->tier1_err, VERIFY_PROSE_ALG) != NULL);
        BT_CHECK(outcome->tier2.kind == VERIFY_TIER2_SKIPPED);
        BT_CHECK(outcome->tier2.skip != NULL);
        BT_CHECK(strstr(outcome->tier2.skip, "tier 1 failed") != NULL);
    }
    BT_CHECK_EQ(section.honesty_count, 1);
    if (section.honesty_count == 1) {
        BT_CHECK(strstr(section.honesty[0], "prose alg") != NULL);
    }
    verify_section_free(&section);
}

BT_TEST(verify_ev_bld_kinds, oldpoch_unknown_token_fails_tier1_and_skips_tier2) {
    one_row("X1 U", "{\"kind\":\"parity\"}");
    verify_section_result section;
    verify_bld_verify_oldpoch(&rows_holder, &section);
    const verify_row_outcome *outcome = single(&section);
    BT_CHECK(outcome != NULL);
    if (outcome != NULL) {
        BT_CHECK(!outcome->tier1_ok);
        BT_CHECK(strstr(outcome->tier1_err, "unknown token") != NULL);
        BT_CHECK(outcome->tier2.kind == VERIFY_TIER2_SKIPPED);
    }
    verify_section_free(&section);
}

BT_TEST(verify_ev_bld_kinds, oldpoch_broken_json_fails_tier2) {
    one_row(T_PERM, "{bad");
    RUN_AND_CHECK(verify_bld_verify_oldpoch, VERIFY_TIER2_FAIL, NULL);
}

BT_TEST(verify_ev_bld_kinds, m2_plain_target_lands_in_the_buffer) {
    one_row("M2", "{\"buffer\":\"DF\",\"target\":\"UB\"}");
    RUN_AND_CHECK(verify_bld_verify_m2, VERIFY_TIER2_PASS, NULL);
}

BT_TEST(verify_ev_bld_kinds, m2_row_without_buffer_fails) {
    one_row("M2", "{}");
    RUN_AND_CHECK(verify_bld_verify_m2, VERIFY_TIER2_FAIL, "names no buffer");
}

BT_TEST(verify_ev_bld_kinds, m2_row_without_target_fails) {
    one_row("M2", "{\"buffer\":\"DF\"}");
    RUN_AND_CHECK(verify_bld_verify_m2, VERIFY_TIER2_FAIL, "names no target");
}

BT_TEST(verify_ev_bld_kinds, m2_bad_target_name_fails_the_slot_lookup) {
    one_row("M2", "{\"buffer\":\"DF\",\"target\":\"XX\"}");
    RUN_AND_CHECK(verify_bld_verify_m2, VERIFY_TIER2_FAIL, "non-face letter");
}

BT_TEST(verify_ev_bld_kinds, m2_wrong_landing_names_the_pair) {
    one_row("M2", "{\"buffer\":\"DF\",\"target\":\"UF\"}");
    RUN_AND_CHECK(verify_bld_verify_m2, VERIFY_TIER2_FAIL, "does not land in the DF buffer");
}

BT_TEST(verify_ev_bld_kinds, m2_execution_displacing_over_four_edges_fails) {
    one_row(SIX_EDGE, "{\"buffer\":\"DF\",\"target\":\"UB\"}");
    RUN_AND_CHECK(verify_bld_verify_m2, VERIFY_TIER2_FAIL, "expected at most 4");
}

BT_TEST(verify_ev_bld_kinds, m2_parity_with_four_edges_passes) {
    one_row("M2", "{\"kind\":\"parity\"}");
    RUN_AND_CHECK(verify_bld_verify_m2, VERIFY_TIER2_PASS, NULL);
}

BT_TEST(verify_ev_bld_kinds, m2_parity_with_displaced_corners_fails) {
    one_row(T_PERM, "{\"kind\":\"parity\"}");
    RUN_AND_CHECK(verify_bld_verify_m2, VERIFY_TIER2_FAIL, "no corners");
}

BT_TEST(verify_ev_bld_kinds, m2_buffer_edge_flip_passes_confined_to_two_edges) {
    one_row(FLIP_UF_UB, "{\"kind\":\"buffer edge flip\",\"flips\":\"UF+UB\"}");
    RUN_AND_CHECK(verify_bld_verify_m2, VERIFY_TIER2_PASS, NULL);
}

BT_TEST(verify_ev_bld_kinds, m2_flip_without_names_fails) {
    one_row(FLIP_UF_UB, "{\"kind\":\"buffer edge flip\"}");
    RUN_AND_CHECK(verify_bld_verify_m2, VERIFY_TIER2_FAIL, "names no flips");
}

BT_TEST(verify_ev_bld_kinds, m2_flip_rejects_a_non_face_name) {
    one_row(FLIP_UF_UB, "{\"kind\":\"buffer edge flip\",\"flips\":\"UF+XX\"}");
    RUN_AND_CHECK(verify_bld_verify_m2, VERIFY_TIER2_FAIL, "non-face letter");
}

BT_TEST(verify_ev_bld_kinds, m2_flip_rejects_too_many_names) {
    one_row(FLIP_UF_UB, "{\"kind\":\"buffer edge flip\",\"flips\":\"UF+UB+UR+UL+FR+FL+BR+BL+DF\"}");
    RUN_AND_CHECK(verify_bld_verify_m2, VERIFY_TIER2_FAIL, "too many names");
}

BT_TEST(verify_ev_bld_kinds, m2_flip_rejects_a_permuting_alg) {
    one_row(H_PERM, "{\"kind\":\"buffer edge flip\",\"flips\":\"UB+UF+UL+UR\"}");
    RUN_AND_CHECK(verify_bld_verify_m2, VERIFY_TIER2_FAIL, "permutes pieces");
}

BT_TEST(verify_ev_bld_kinds, m2_flip_rejects_a_solved_state) {
    one_row("R R'", "{\"kind\":\"buffer edge flip\",\"flips\":\"UF+UB\"}");
    RUN_AND_CHECK(verify_bld_verify_m2, VERIFY_TIER2_FAIL, "leaves the cube solved");
}

BT_TEST(verify_ev_bld_kinds, m2_unknown_token_fails_tier1_and_skips_tier2) {
    one_row("X1 U", "{\"buffer\":\"DF\",\"target\":\"UB\"}");
    verify_section_result section;
    verify_bld_verify_m2(&rows_holder, &section);
    const verify_row_outcome *outcome = single(&section);
    BT_CHECK(outcome != NULL);
    if (outcome != NULL) {
        BT_CHECK(!outcome->tier1_ok);
        BT_CHECK(outcome->tier2.kind == VERIFY_TIER2_SKIPPED);
    }
    verify_section_free(&section);
}

BT_TEST(verify_ev_bld_kinds, style_pair_flip_passes_confined_to_two_edges) {
    one_row(FLIP_UF_UB, "{\"position\":\"UF & UB\"}");
    RUN_AND_CHECK(verify_bld_verify_3style, VERIFY_TIER2_PASS, NULL);
}

BT_TEST(verify_ev_bld_kinds, style_pair_twist_passes_confined_to_two_corners) {
    one_row(TWIST_UFR_UBR, "{\"position\":\"UFR & UBR\"}");
    RUN_AND_CHECK(verify_bld_verify_3style, VERIFY_TIER2_PASS, NULL);
}

BT_TEST(verify_ev_bld_kinds, style_pair_rejects_a_non_face_name) {
    one_row(FLIP_UF_UB, "{\"position\":\"UF & XX\"}");
    RUN_AND_CHECK(verify_bld_verify_3style, VERIFY_TIER2_FAIL, "non-face letter");
}

BT_TEST(verify_ev_bld_kinds, style_pair_parse_failure_names_the_spec) {
    one_row(FLIP_UF_UB, "{\"position\":\"UF & UB & UR\"}");
    RUN_AND_CHECK(verify_bld_verify_3style, VERIFY_TIER2_FAIL, "does not hold two names");
}

BT_TEST(verify_ev_bld_kinds, style_pair_rejects_a_solved_state) {
    one_row("R R'", "{\"position\":\"UF & UB\"}");
    RUN_AND_CHECK(verify_bld_verify_3style, VERIFY_TIER2_FAIL, "leaves the cube solved");
}

BT_TEST(verify_ev_bld_kinds, style_double_transposition_passes_on_a_t_perm) {
    one_row(T_PERM, "{\"position\":\"UR-UL,UFR-UBR\"}");
    RUN_AND_CHECK(verify_bld_verify_3style, VERIFY_TIER2_PASS, NULL);
}

BT_TEST(verify_ev_bld_kinds, style_single_swap_position_fails) {
    one_row(T_PERM, "{\"position\":\"UR-UL\"}");
    RUN_AND_CHECK(verify_bld_verify_3style, VERIFY_TIER2_FAIL, "neither a pair nor two swaps");
}

BT_TEST(verify_ev_bld_kinds, style_malformed_second_swap_fails) {
    one_row(T_PERM, "{\"position\":\"UR-UL,UFR\"}");
    RUN_AND_CHECK(verify_bld_verify_3style, VERIFY_TIER2_FAIL, "does not hold two names");
}

BT_TEST(verify_ev_bld_kinds, style_double_transposition_rejects_an_unrelated_alg) {
    one_row("R U R'", "{\"position\":\"UR-UL,UFR-UBR\"}");
    RUN_AND_CHECK(verify_bld_verify_3style, VERIFY_TIER2_FAIL, "are not transposed");
}

BT_TEST(verify_ev_bld_kinds, style_row_without_position_fails) {
    one_row(T_PERM, "{}");
    RUN_AND_CHECK(verify_bld_verify_3style, VERIFY_TIER2_FAIL, "names no position");
}

BT_TEST(verify_ev_bld_kinds, style_unknown_token_fails_tier1_and_skips_tier2) {
    one_row("X1 U", "{\"position\":\"UF & UB\"}");
    verify_section_result section;
    verify_bld_verify_3style(&rows_holder, &section);
    const verify_row_outcome *outcome = single(&section);
    BT_CHECK(outcome != NULL);
    if (outcome != NULL) {
        BT_CHECK(!outcome->tier1_ok);
        BT_CHECK(outcome->tier2.kind == VERIFY_TIER2_SKIPPED);
    }
    verify_section_free(&section);
}

BT_TEST(verify_ev_bld_kinds, wide_suffix_mapping_handles_spacing_and_truncation) {
    char out[64];
    verify_bld_map_wide_suffix("  Uw2\t F'  Rw ", out, sizeof out);
    BT_CHECK_EQ(out, "u2 F' r ");
    verify_bld_map_wide_suffix("Uw2 F' Rw", out, 8);
    BT_CHECK_EQ(out, "u2 F' r");
    verify_bld_map_wide_suffix("Uw'", out, 3);
    BT_CHECK_EQ(out, "u'");
    verify_bld_map_wide_suffix("Uw Uw Uw", out, 4);
    BT_CHECK_EQ(out, "u u");
    verify_bld_map_wide_suffix("F'", out, 2);
    BT_CHECK_EQ(out, "");
}

BT_TEST(verify_ev_bld_kinds, slot_helpers_report_lookup_failures) {
    verify_bld_layout layout;
    verify_bld_layout_probe(&layout);
    bt_cube solved = bt_cube_solved();
    verify_bld_state_audit audit;
    verify_bld_audit(&solved, &layout, &audit);
    char err[VERIFY_ERR_MAX];
    const uint8_t *slot = NULL;
    size_t slot_len = 0;
    BT_CHECK(verify_bld_slot(&layout, "XX", &slot, &slot_len, err, sizeof err) != 0);
    BT_CHECK_EQ(verify_bld_transposed(&audit, &layout, "XX", "UF", err, sizeof err), 1);
    BT_CHECK(strstr(err, "non-face letter") != NULL);
}

BT_TEST(verify_ev_bld_kinds, parse_pair_trims_spaces_around_names) {
    char pair[2][VERIFY_BLD_NAME_MAX];
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_bld_parse_pair(" UF - UB ", '-', pair, err, sizeof err), 0);
    BT_CHECK_EQ(pair[0], "UF");
    BT_CHECK_EQ(pair[1], "UB");
    BT_CHECK(verify_bld_parse_pair("UF-UB-UR", '-', pair, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "does not hold two names") != NULL);
}
