#include "common/bt_harness.h"
#include "data/corpus.h"
#include "data/verify.h"

#include <stdio.h>
#include <string.h>

static const char X2[] = "R' D' R D R' D' R D";
static const char X4[] = "R' D' R D R' D' R D R' D' R D R' D' R D";
static const char TPERM[] = "R U R' U' R' F R2 U' R' U' R U R' F'";
static const char FLIP[] = "M' U M' U M' U2 M U M U M U2";
static const char FLIP_UR_UL[] = "U M' U M' U M' U2 M U M U M U2 U'";
static const char EDGE_CYCLE[] = "M2 U M U2 M' U M2";
static const char CORNER_CYCLE[] = "R U R' D' R U' R' D";

static bt_alg alg_of(const char *text) {
    bt_alg alg;
    bt_alg_init(&alg);
    char err[64];
    if (!bt_alg_parse(text, &alg, err, sizeof err)) {
        BT_ABORT("alg_of failed on %s", text);
    }
    return alg;
}

static void t3(char *out, size_t out_size) {
    snprintf(out, out_size, "%s U %s U %s U2", X2, X2, X2);
}

static void twist_pair_alg(char *out, size_t out_size) {
    snprintf(out, out_size, "%s U %s U'", X2, X4);
}

static size_t twist_notes(const bt_verdict *verdict, bt_note out[4]) {
    size_t count = 0;
    for (size_t i = 0; i < verdict->note_count && count < 4; i++) {
        if (verdict->notes[i].kind == BT_NOTE_TWIST) {
            out[count++] = verdict->notes[i];
        }
    }
    return count;
}

static size_t count_kind(const bt_verdict *verdict, bt_problem_kind kind) {
    size_t count = 0;
    for (size_t i = 0; i < verdict->problem_count; i++) {
        if (verdict->problems[i].kind == kind) {
            count += 1;
        }
    }
    return count;
}

BT_TEST(verify_more, constructed_edge_and_corner_cycles_verify_ok) {
    bt_alg edge_alg = alg_of(EDGE_CYCLE);
    bt_verdict edge;
    bt_verify(&edge_alg, "UF-UR-UL", BT_SHEET_EDGE, &edge);
    BT_CHECK(bt_verdict_ok(&edge));
    BT_CHECK(edge.note_count == 0);
    bt_verdict_free(&edge);
    bt_alg_free(&edge_alg);
    bt_alg corner_alg = alg_of(CORNER_CYCLE);
    bt_verdict corner;
    bt_verify(&corner_alg, "UFR-RDF-BDR", BT_SHEET_CORNER, &corner);
    BT_CHECK(bt_verdict_ok(&corner));
    BT_CHECK(corner.note_count == 0);
    bt_verdict_free(&corner);
    bt_alg_free(&corner_alg);
}

BT_TEST(verify_more, inflated_alg_with_cancelling_tail_still_verifies) {
    char text[256];
    snprintf(text, sizeof text, "%s M2 M2", EDGE_CYCLE);
    bt_alg alg = alg_of(text);
    bt_verdict verdict;
    bt_verify(&alg, "UF-UR-UL", BT_SHEET_EDGE, &verdict);
    BT_CHECK(bt_verdict_ok(&verdict));
    bt_verdict_free(&verdict);
    bt_alg_free(&alg);
}

BT_TEST(verify_more, three_style_disturbing_unnamed_pieces_reports_residue) {
    char text[64];
    snprintf(text, sizeof text, "%s D", EDGE_CYCLE);
    bt_alg alg = alg_of(text);
    bt_verdict verdict;
    bt_verify(&alg, "UF-UR-UL", BT_SHEET_EDGE, &verdict);
    BT_CHECK(bt_verdict_ok(&verdict) == false);
    BT_CHECK(count_kind(&verdict, BT_PROBLEM_NON_FIXED_RESIDUE) > 0);
    for (size_t i = 0; i < verdict.problem_count; i++) {
        BT_CHECK(verdict.problems[i].kind == BT_PROBLEM_NON_FIXED_RESIDUE);
    }
    bt_verdict_free(&verdict);
    bt_alg_free(&alg);
}

BT_TEST(verify_more, opposite_direction_cycle_fails_the_original_claim) {
    bt_alg alg = alg_of("M2 U' M' U2 M U' M2");
    bt_verdict verdict;
    bt_verify(&alg, "UF-UR-UL", BT_SHEET_EDGE, &verdict);
    BT_CHECK(bt_verdict_ok(&verdict) == false);
    BT_CHECK(bt_verdict_contains_problem(&verdict, BT_PROBLEM_WRONG_CYCLE, "UF", "UR", "UF"));
    BT_CHECK(bt_verdict_contains_problem(&verdict, BT_PROBLEM_WRONG_CYCLE, "UL", "UF", "UB"));
    bt_verdict_free(&verdict);
    bt_alg_free(&alg);
}

BT_TEST(verify_more, flip_pair_alg_verifies_with_no_notes) {
    bt_alg flip = alg_of(FLIP);
    bt_verdict verdict;
    bt_verify(&flip, "UF & UB", BT_SHEET_FLIPS, &verdict);
    BT_CHECK(bt_verdict_ok(&verdict));
    BT_CHECK(verdict.note_count == 0);
    bt_verdict_free(&verdict);
    bt_alg_free(&flip);
    bt_alg conjugated = alg_of(FLIP_UR_UL);
    bt_verify(&conjugated, "UR & UL", BT_SHEET_FLIPS, &verdict);
    BT_CHECK(bt_verdict_ok(&verdict));
    BT_CHECK(verdict.note_count == 0);
    bt_verdict_free(&verdict);
    bt_alg_free(&conjugated);
}

BT_TEST(verify_more, twist_pair_alg_verifies_with_opposite_sign_notes) {
    char pair[256];
    twist_pair_alg(pair, sizeof pair);
    bt_alg alg = alg_of(pair);
    bt_verdict verdict;
    bt_verify(&alg, "UFR & UBR", BT_SHEET_TWISTS, &verdict);
    BT_CHECK(bt_verdict_ok(&verdict));
    bt_note twists[4];
    size_t count = twist_notes(&verdict, twists);
    BT_CHECK(count == 2);
    if (count == 2) {
        BT_CHECK(strcmp(twists[0].piece, "UFR") == 0 && twists[0].twist == 1);
        BT_CHECK(strcmp(twists[1].piece, "UBR") == 0 && twists[1].twist == -1);
    }
    bt_verdict_free(&verdict);
    bt_alg_free(&alg);
}

BT_TEST(verify_more, t_perm_verifies_as_parity_without_orientation_notes) {
    bt_alg alg = alg_of(TPERM);
    bt_verdict verdict;
    bt_verify(&alg, "UR-UL, UFR-UBR", BT_SHEET_PARITY, &verdict);
    BT_CHECK(bt_verdict_ok(&verdict));
    BT_CHECK(verdict.note_count == 0);
    bt_verdict_free(&verdict);
    bt_alg_free(&alg);
}

BT_TEST(verify_more, parity_with_flipped_swaps_reports_flip_notes_only) {
    char text[256];
    snprintf(text, sizeof text, "%s %s", TPERM, FLIP_UR_UL);
    bt_alg alg = alg_of(text);
    bt_verdict verdict;
    bt_verify(&alg, "UR-UL, UFR-UBR", BT_SHEET_PARITY, &verdict);
    BT_CHECK(bt_verdict_ok(&verdict));
    BT_CHECK(verdict.note_count == 2);
    BT_CHECK(bt_verdict_contains_note(&verdict, BT_NOTE_FLIP, "UR", 0));
    BT_CHECK(bt_verdict_contains_note(&verdict, BT_NOTE_FLIP, "UL", 0));
    for (size_t i = 0; i < verdict.note_count; i++) {
        BT_CHECK(verdict.notes[i].kind == BT_NOTE_FLIP);
    }
    bt_verdict_free(&verdict);
    bt_alg_free(&alg);
}

BT_TEST(verify_more, ltct_composite_verifies_with_three_twist_notes) {
    char text[256];
    char t3_text[128];
    t3(t3_text, sizeof t3_text);
    snprintf(text, sizeof text, "%s %s", TPERM, t3_text);
    bt_alg alg = alg_of(text);
    bt_verdict verdict;
    bt_verify(&alg, "UR-UL, UFR-RUB[LUB]", BT_SHEET_LTCT, &verdict);
    BT_CHECK(bt_verdict_ok(&verdict));
    bt_note twists[4];
    size_t count = twist_notes(&verdict, twists);
    BT_CHECK(count == 3);
    if (count == 3) {
        BT_CHECK(strcmp(twists[0].piece, "UFR") == 0 && twists[0].twist == 1);
        BT_CHECK(strcmp(twists[1].piece, "UBR") == 0 && twists[1].twist == 1);
        BT_CHECK(strcmp(twists[2].piece, "UBL") == 0 && twists[2].twist == 1);
    }
    bt_verdict_free(&verdict);
    bt_alg_free(&alg);
}

BT_TEST(verify_more, ltct_with_flipped_edge_swaps_reports_unexpected_flip) {
    char text[256];
    char t3_text[128];
    t3(t3_text, sizeof t3_text);
    snprintf(text, sizeof text, "%s %s %s", TPERM, FLIP_UR_UL, t3_text);
    bt_alg alg = alg_of(text);
    bt_verdict verdict;
    bt_verify(&alg, "UR-UL, UFR-RUB[LUB]", BT_SHEET_LTCT, &verdict);
    BT_CHECK(bt_verdict_ok(&verdict) == false);
    BT_CHECK(verdict.problem_count == 2);
    BT_CHECK(bt_verdict_contains_problem(&verdict, BT_PROBLEM_UNEXPECTED_DEFECT, "UR", NULL, NULL));
    BT_CHECK(bt_verdict_contains_problem(&verdict, BT_PROBLEM_UNEXPECTED_DEFECT, "UL", NULL, NULL));
    bt_note twists[4];
    size_t count = twist_notes(&verdict, twists);
    BT_CHECK(count == 3);
    if (count == 3) {
        BT_CHECK(strcmp(twists[0].piece, "UFR") == 0 && twists[0].twist == 1);
        BT_CHECK(strcmp(twists[1].piece, "UBR") == 0 && twists[1].twist == 1);
        BT_CHECK(strcmp(twists[2].piece, "UBL") == 0 && twists[2].twist == 1);
    }
    char text_shown[512];
    bt_verdict_string(&verdict, text_shown, sizeof text_shown);
    BT_CHECK_EQ(
        text_shown,
        "piece UR is flipped but should be intact\npiece UL is flipped but should be intact");
    bt_verdict_free(&verdict);
    bt_alg_free(&alg);
}

BT_TEST(verify_more, solved_cube_fails_every_named_edge_cycle_slot) {
    bt_alg alg;
    bt_alg_init(&alg);
    bt_verdict verdict;
    bt_verify(&alg, "UF-UB-UL", BT_SHEET_EDGE, &verdict);
    BT_CHECK(bt_verdict_ok(&verdict) == false);
    BT_CHECK(verdict.problem_count == 6);
    BT_CHECK(bt_verdict_contains_problem(&verdict, BT_PROBLEM_WRONG_CYCLE, "UF", "UB", "UF"));
    BT_CHECK(bt_verdict_contains_problem(&verdict, BT_PROBLEM_WRONG_CYCLE, "UB", "UL", "UB"));
    BT_CHECK(bt_verdict_contains_problem(&verdict, BT_PROBLEM_WRONG_CYCLE, "UL", "UF", "UL"));
    bt_verdict_free(&verdict);
    bt_alg_free(&alg);
}

BT_TEST(verify_more, edge_sheet_flags_unnamed_piece_disturbance) {
    bt_alg m2 = alg_of("M2");
    bt_verdict verdict;
    bt_verify(&m2, "UF-UB-UL", BT_SHEET_EDGE, &verdict);
    BT_CHECK(bt_verdict_ok(&verdict) == false);
    BT_CHECK(verdict.problem_count == 11);
    BT_CHECK(bt_verdict_contains_problem(&verdict, BT_PROBLEM_CENTERS_MOVED, NULL, NULL, NULL));
    BT_CHECK(count_kind(&verdict, BT_PROBLEM_WRONG_CYCLE) == 6);
    BT_CHECK(count_kind(&verdict, BT_PROBLEM_NON_FIXED_RESIDUE) == 4);
    BT_CHECK(bt_verdict_contains_problem(&verdict, BT_PROBLEM_WRONG_CYCLE, "UL", "UF", "UL"));
    bt_verdict_free(&verdict);
    bt_alg_free(&m2);
    char text[128];
    snprintf(text, sizeof text, "%s %s", EDGE_CYCLE, CORNER_CYCLE);
    bt_alg extra = alg_of(text);
    bt_verify(&extra, "UF-UR-UL", BT_SHEET_EDGE, &verdict);
    BT_CHECK(bt_verdict_ok(&verdict) == false);
    BT_CHECK(count_kind(&verdict, BT_PROBLEM_NON_FIXED_RESIDUE) == 9);
    bt_verdict_free(&verdict);
    bt_alg_free(&extra);
}

BT_TEST(verify_more, solved_cube_reports_missing_flips_and_twists_with_zero_notes) {
    bt_alg alg;
    bt_alg_init(&alg);
    bt_verdict flips;
    bt_verify(&alg, "UF & UB", BT_SHEET_FLIPS, &flips);
    BT_CHECK(flips.problem_count == 2);
    BT_CHECK(bt_verdict_contains_problem(&flips, BT_PROBLEM_MISSING_DEFECT, "UF", NULL, NULL));
    BT_CHECK(bt_verdict_contains_problem(&flips, BT_PROBLEM_MISSING_DEFECT, "UB", NULL, NULL));
    BT_CHECK(flips.note_count == 0);
    bt_verdict_free(&flips);
    bt_verdict twists;
    bt_verify(&alg, "UFR & UBR", BT_SHEET_TWISTS, &twists);
    BT_CHECK(twists.problem_count == 2);
    BT_CHECK(bt_verdict_contains_problem(&twists, BT_PROBLEM_MISSING_DEFECT, "UFR", NULL, NULL));
    BT_CHECK(bt_verdict_contains_problem(&twists, BT_PROBLEM_MISSING_DEFECT, "UBR", NULL, NULL));
    bt_note notes[4];
    size_t count = twist_notes(&twists, notes);
    BT_CHECK(count == 2);
    if (count == 2) {
        BT_CHECK(strcmp(notes[0].piece, "UFR") == 0 && notes[0].twist == 0);
        BT_CHECK(strcmp(notes[1].piece, "UBR") == 0 && notes[1].twist == 0);
    }
    bt_verdict_free(&twists);
    bt_alg_free(&alg);
}

BT_TEST(verify_more, solved_cube_reports_both_parity_swaps_displaced) {
    bt_alg alg;
    bt_alg_init(&alg);
    bt_verdict verdict;
    bt_verify(&alg, "UR-UL, UFR-UBR", BT_SHEET_PARITY, &verdict);
    BT_CHECK(bt_verdict_ok(&verdict) == false);
    BT_CHECK(verdict.problem_count == 4);
    for (size_t i = 0; i < verdict.problem_count; i++) {
        BT_CHECK(verdict.problems[i].kind == BT_PROBLEM_DISPLACED);
    }
    BT_CHECK(bt_verdict_contains_problem(&verdict, BT_PROBLEM_DISPLACED, "UR", "UL", "UR"));
    BT_CHECK(bt_verdict_contains_problem(&verdict, BT_PROBLEM_DISPLACED, "UBR", "UFR", "UBR"));
    BT_CHECK(verdict.note_count == 0);
    bt_verdict_free(&verdict);
    bt_alg_free(&alg);
}

BT_TEST(verify_more, solved_cube_reports_ltct_swaps_displaced_and_bracket_untwisted) {
    bt_alg alg;
    bt_alg_init(&alg);
    bt_verdict verdict;
    bt_verify(&alg, "UR-UL, UFR-UBR[UBL]", BT_SHEET_LTCT, &verdict);
    BT_CHECK(verdict.problem_count == 5);
    BT_CHECK(verdict.problems[verdict.problem_count - 1].kind == BT_PROBLEM_MISSING_DEFECT);
    BT_CHECK(strcmp(verdict.problems[verdict.problem_count - 1].piece, "UBL") == 0);
    bt_note notes[4];
    size_t count = twist_notes(&verdict, notes);
    BT_CHECK(count == 1);
    if (count == 1) {
        BT_CHECK(strcmp(notes[0].piece, "UBL") == 0 && notes[0].twist == 0);
    }
    bt_verdict_free(&verdict);
    bt_alg_free(&alg);
}

BT_TEST(verify_more, three_equal_twists_fail_the_pair_claim_with_residue) {
    char t3_text[128];
    t3(t3_text, sizeof t3_text);
    bt_alg alg = alg_of(t3_text);
    bt_verdict verdict;
    bt_verify(&alg, "UFR & UBR", BT_SHEET_TWISTS, &verdict);
    BT_CHECK(bt_verdict_ok(&verdict) == false);
    BT_CHECK(
        bt_verdict_contains_problem(&verdict, BT_PROBLEM_TWISTS_NOT_OPPOSITE, "UFR", "UBR", NULL));
    BT_CHECK(count_kind(&verdict, BT_PROBLEM_NON_FIXED_RESIDUE) == 3);
    bt_note notes[4];
    size_t count = twist_notes(&verdict, notes);
    BT_CHECK(count == 2);
    if (count == 2) {
        BT_CHECK(strcmp(notes[0].piece, "UFR") == 0 && notes[0].twist == 1);
        BT_CHECK(strcmp(notes[1].piece, "UBR") == 0 && notes[1].twist == 1);
    }
    bt_verdict_free(&verdict);
    bt_alg_free(&alg);
}

BT_TEST(verify_more, t_perm_on_twists_sheet_reports_displaced_and_missing_twists) {
    bt_alg alg = alg_of(TPERM);
    bt_verdict verdict;
    bt_verify(&alg, "UFR & UBR", BT_SHEET_TWISTS, &verdict);
    BT_CHECK(bt_verdict_ok(&verdict) == false);
    BT_CHECK(bt_verdict_contains_problem(&verdict, BT_PROBLEM_DISPLACED, "UFR", "UFR", "UBR"));
    BT_CHECK(bt_verdict_contains_problem(&verdict, BT_PROBLEM_MISSING_DEFECT, "UFR", NULL, NULL));
    BT_CHECK(count_kind(&verdict, BT_PROBLEM_NON_FIXED_RESIDUE) == 4);
    bt_verdict_free(&verdict);
    bt_alg_free(&alg);
}

BT_TEST(verify_more, ltct_bracket_on_untwisted_corner_reports_sum_and_missing_defect) {
    char text[256];
    char t3_text[128];
    t3(t3_text, sizeof t3_text);
    snprintf(text, sizeof text, "%s %s", TPERM, t3_text);
    bt_alg alg = alg_of(text);
    bt_verdict verdict;
    bt_verify(&alg, "UR-UL, UFR-RUB[UFL]", BT_SHEET_LTCT, &verdict);
    BT_CHECK(bt_verdict_ok(&verdict) == false);
    bool found_sum = false;
    for (size_t i = 0; i < verdict.problem_count; i++) {
        if (verdict.problems[i].kind == BT_PROBLEM_TWIST_SUM_NOT_ZERO &&
            verdict.problems[i].sum == 2) {
            found_sum = true;
        }
    }
    BT_CHECK(found_sum);
    BT_CHECK(bt_verdict_contains_problem(&verdict, BT_PROBLEM_MISSING_DEFECT, "UFL", NULL, NULL));
    bt_note notes[4];
    size_t count = twist_notes(&verdict, notes);
    BT_CHECK(count == 3);
    if (count == 3) {
        BT_CHECK(strcmp(notes[0].piece, "UFR") == 0 && notes[0].twist == 1);
        BT_CHECK(strcmp(notes[1].piece, "UBR") == 0 && notes[1].twist == 1);
        BT_CHECK(strcmp(notes[2].piece, "UFL") == 0 && notes[2].twist == 0);
    }
    bt_verdict_free(&verdict);
    bt_alg_free(&alg);
}

BT_TEST(verify_more, bad_three_style_positions_report_their_reason) {
    struct {
        const char *position;
        const char *needle;
    } cases[] = {
        {"UF-UB", "expected 3 tokens"},
        {"UF-UB-UL-UR", "expected 3 tokens"},
        {"UF-UB-UFR", "orbit"},
        {"UF-UB-UF", "twice"},
        {"GARBAGE-UB-UL", "unknown sticker"},
        {"", "unknown sticker"},
    };
    bt_alg alg = alg_of(EDGE_CYCLE);
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        bt_verdict verdict;
        bt_verify(&alg, cases[i].position, BT_SHEET_EDGE, &verdict);
        BT_CHECK(verdict.problem_count == 1);
        if (verdict.problem_count == 1) {
            BT_CHECK(verdict.problems[0].kind == BT_PROBLEM_BAD_POSITION);
            BT_CHECK(strstr(verdict.problems[0].detail, cases[i].needle) != NULL);
        }
        bt_verdict_free(&verdict);
    }
    bt_alg corner_alg = alg_of(CORNER_CYCLE);
    bt_verdict corner;
    bt_verify(&corner_alg, "UF-UB-UL", BT_SHEET_CORNER, &corner);
    BT_CHECK(corner.problem_count == 1);
    if (corner.problem_count == 1) {
        BT_CHECK(corner.problems[0].kind == BT_PROBLEM_BAD_POSITION);
        BT_CHECK(strstr(corner.problems[0].detail, "orbit") != NULL);
    }
    bt_verdict_free(&corner);
    bt_alg_free(&corner_alg);
    bt_alg_free(&alg);
}

BT_TEST(verify_more, bad_pair_positions_report_their_reason) {
    struct {
        const char *position;
        const char *needle;
    } flips[] = {
        {"UF & UB & UL", "expected 2 names"},
        {"UF-UB", "expected 2 names"},
        {"UFR & UBR", "orbit"},
        {"UF & GARBAGE", "unknown sticker"},
    };
    bt_alg alg = alg_of(FLIP);
    for (size_t i = 0; i < sizeof flips / sizeof flips[0]; i++) {
        bt_verdict verdict;
        bt_verify(&alg, flips[i].position, BT_SHEET_FLIPS, &verdict);
        BT_CHECK(verdict.problem_count == 1);
        if (verdict.problem_count == 1) {
            BT_CHECK(verdict.problems[0].kind == BT_PROBLEM_BAD_POSITION);
            BT_CHECK(strstr(verdict.problems[0].detail, flips[i].needle) != NULL);
        }
        bt_verdict_free(&verdict);
    }
    bt_alg_free(&alg);
    char pair[256];
    twist_pair_alg(pair, sizeof pair);
    bt_alg twist_alg = alg_of(pair);
    bt_verdict twists;
    bt_verify(&twist_alg, "UF & UB", BT_SHEET_TWISTS, &twists);
    BT_CHECK(twists.problem_count == 1);
    if (twists.problem_count == 1) {
        BT_CHECK(twists.problems[0].kind == BT_PROBLEM_BAD_POSITION);
        BT_CHECK(strstr(twists.problems[0].detail, "orbit") != NULL);
    }
    bt_verdict_free(&twists);
    bt_alg_free(&twist_alg);
}

BT_TEST(verify_more, bad_parity_positions_report_their_reason) {
    struct {
        const char *position;
        const char *needle;
    } cases[] = {
        {"UF-UL UFR-UBR", "expected an edge pair"},
        {"UF-UL, UFR", "needs 2 edges and 2 corners"},
        {"UF-UL-UR, UFR-UBR", "needs 2 edges and 2 corners"},
        {"UF-UL, UFR-UFL-UBL", "needs 2 edges and 2 corners"},
        {"UF-UL, UF-UB", "orbit"},
    };
    bt_alg alg = alg_of(TPERM);
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        bt_verdict verdict;
        bt_verify(&alg, cases[i].position, BT_SHEET_PARITY, &verdict);
        BT_CHECK(verdict.problem_count == 1);
        if (verdict.problem_count == 1) {
            BT_CHECK(verdict.problems[0].kind == BT_PROBLEM_BAD_POSITION);
            BT_CHECK(strstr(verdict.problems[0].detail, cases[i].needle) != NULL);
        }
        bt_verdict_free(&verdict);
    }
    bt_alg_free(&alg);
}

BT_TEST(verify_more, bad_ltct_positions_report_their_reason) {
    struct {
        const char *position;
        const char *needle;
    } cases[] = {
        {"UF-UR, UBR-LUB[FUR", "unclosed bracket"},
        {"UF-UR, UBR-LUB", "bracketed twist corner"},
        {"UF-UR-UB, UBR-LUB[FUR]", "2 swapped edges"},
        {"UF-UR, UBR-LUB-FUR[UFL]", "2 swapped corners"},
        {"UF-UR, UBR-LUB[UF]", "orbit"},
        {"UF-UR, UBR-LUB[GARBAGE]", "unknown sticker"},
    };
    char text[256];
    char t3_text[128];
    t3(t3_text, sizeof t3_text);
    snprintf(text, sizeof text, "%s %s", TPERM, t3_text);
    bt_alg alg = alg_of(text);
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        bt_verdict verdict;
        bt_verify(&alg, cases[i].position, BT_SHEET_LTCT, &verdict);
        BT_CHECK(verdict.problem_count == 1);
        if (verdict.problem_count == 1) {
            BT_CHECK(verdict.problems[0].kind == BT_PROBLEM_BAD_POSITION);
            BT_CHECK(strstr(verdict.problems[0].detail, cases[i].needle) != NULL);
        }
        bt_verdict_free(&verdict);
    }
    bt_alg_free(&alg);
}
