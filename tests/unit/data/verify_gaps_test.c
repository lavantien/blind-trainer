#include "common/alloc.h"
#include "common/bt_harness.h"
#include "core/alg.h"
#include "data/verify.h"

#include <stdio.h>
#include <string.h>

static const char TPERM[] = "R U R' U' R' F R2 U' R' U' R U R' F'";
static const char EDGE_CYCLE[] = "M2 U M U2 M' U M2";

static bt_alg alg_of(const char *text) {
    bt_alg alg;
    bt_alg_init(&alg);
    char err[64];
    if (!bt_alg_parse(text, &alg, err, sizeof err)) {
        BT_ABORT("alg_of failed on %s", text);
    }
    return alg;
}

BT_TEST(verify_gaps, verdict_string_stops_at_the_buffer_edge) {
    bt_alg alg;
    bt_alg_init(&alg);
    bt_verdict verdict;
    bt_verify(&alg, "UF & UB", BT_SHEET_FLIPS, &verdict);
    BT_CHECK(verdict.problem_count == 2);
    char text[45];
    bt_verdict_string(&verdict, text, sizeof text);
    BT_CHECK_EQ(text, "piece UF should be flipped but is intact\n");
    char tight[4];
    bt_verdict_string(&verdict, tight, sizeof tight);
    BT_CHECK_EQ(tight, "");
    bt_verdict_free(&verdict);
    bt_alg_free(&alg);
}

BT_TEST(verify_gaps, contains_problem_rejects_absent_shapes) {
    bt_alg alg;
    bt_alg_init(&alg);
    bt_verdict verdict;
    bt_verify(&alg, "UF & UB", BT_SHEET_FLIPS, &verdict);
    BT_CHECK(bt_verdict_contains_problem(&verdict, BT_PROBLEM_MISSING_DEFECT, "UF", NULL, NULL));
    BT_CHECK(!bt_verdict_contains_problem(&verdict, BT_PROBLEM_MISSING_DEFECT, "UR", NULL, NULL));
    BT_CHECK(!bt_verdict_contains_problem(&verdict, BT_PROBLEM_DISPLACED, NULL, NULL, NULL));
    bt_verdict_free(&verdict);
    bt_alg_free(&alg);
}

BT_TEST(verify_gaps, contains_note_matches_twists_and_rejects_misses) {
    bt_alg alg;
    bt_alg_init(&alg);
    bt_verdict twists;
    bt_verify(&alg, "UFR & UBR", BT_SHEET_TWISTS, &twists);
    BT_CHECK(twists.note_count == 2);
    BT_CHECK(bt_verdict_contains_note(&twists, BT_NOTE_TWIST, "UFR", 0));
    BT_CHECK(bt_verdict_contains_note(&twists, BT_NOTE_TWIST, "UBR", 0));
    BT_CHECK(!bt_verdict_contains_note(&twists, BT_NOTE_TWIST, "UFR", 1));
    BT_CHECK(!bt_verdict_contains_note(&twists, BT_NOTE_TWIST, "UFL", 0));
    bt_verdict_free(&twists);
    bt_verdict clean;
    bt_verify(&alg, "UF & UB", BT_SHEET_FLIPS, &clean);
    BT_CHECK(clean.note_count == 0);
    BT_CHECK(!bt_verdict_contains_note(&clean, BT_NOTE_TWIST, "UFR", 0));
    bt_verdict_free(&clean);
    bt_alg_free(&alg);
}

BT_TEST(verify_gaps, three_style_rejects_more_than_six_tokens) {
    bt_alg alg;
    bt_alg_init(&alg);
    bt_verdict verdict;
    bt_verify(&alg, "UF-UB-UR-UL-UB-UR-UF", BT_SHEET_EDGE, &verdict);
    BT_CHECK(!bt_verdict_ok(&verdict));
    char text[128];
    bt_verdict_string(&verdict, text, sizeof text);
    BT_CHECK(strstr(text, "expected 3 tokens, found 7") != NULL);
    bt_verdict_free(&verdict);
    bt_alg_free(&alg);
}

BT_TEST(verify_gaps, three_style_trims_whitespace_inside_tokens) {
    bt_alg alg = alg_of(EDGE_CYCLE);
    bt_verdict verdict;
    bt_verify(&alg, " UF - UR - UL", BT_SHEET_EDGE, &verdict);
    BT_CHECK(bt_verdict_ok(&verdict));
    bt_verdict_free(&verdict);
    bt_alg_free(&alg);
}

BT_TEST(verify_gaps, parity_trims_whitespace_around_the_comma) {
    bt_alg alg = alg_of(TPERM);
    bt_verdict verdict;
    bt_verify(&alg, " UR-UL , UFR-UBR ", BT_SHEET_PARITY, &verdict);
    BT_CHECK(bt_verdict_ok(&verdict));
    bt_verdict_free(&verdict);
    bt_alg_free(&alg);
}

BT_TEST(verify_gaps, ltct_rejects_each_malformed_position_shape) {
    struct {
        const char *position;
        const char *needle;
    } cases[] = {
        {"UR-UL UFR-RUB[LUB]", "expected an edge pair and a corner pair"},
        {"UR-ZZZ, UFR-RUB[LUB]", "unknown sticker name ZZZ"},
        {"UR-UL, ZZ-RUB[LUB]", "unknown sticker name ZZ"},
    };
    bt_alg alg;
    bt_alg_init(&alg);
    for (int i = 0; i < 3; i++) {
        bt_verdict verdict;
        bt_verify(&alg, cases[i].position, BT_SHEET_LTCT, &verdict);
        BT_CHECK(!bt_verdict_ok(&verdict));
        char text[192];
        bt_verdict_string(&verdict, text, sizeof text);
        BT_CHECK(strstr(text, cases[i].needle) != NULL);
        bt_verdict_free(&verdict);
    }
    bt_alg_free(&alg);
}

BT_TEST(verify_gaps, twists_rejects_an_edge_token_in_the_corner_pair) {
    bt_alg alg;
    bt_alg_init(&alg);
    bt_verdict verdict;
    bt_verify(&alg, "UFR & UF", BT_SHEET_TWISTS, &verdict);
    BT_CHECK(!bt_verdict_ok(&verdict));
    char text[192];
    bt_verdict_string(&verdict, text, sizeof text);
    BT_CHECK(strstr(text, "token UF is in the edge orbit") != NULL);
    bt_verdict_free(&verdict);
    bt_alg_free(&alg);
}

BT_TEST(verify_gaps, parity_rejects_an_unknown_corner_name) {
    bt_alg alg;
    bt_alg_init(&alg);
    bt_verdict verdict;
    bt_verify(&alg, "UR-UL, UFR-ZZZ", BT_SHEET_PARITY, &verdict);
    BT_CHECK(!bt_verdict_ok(&verdict));
    char text[192];
    bt_verdict_string(&verdict, text, sizeof text);
    BT_CHECK(strstr(text, "unknown sticker name ZZZ") != NULL);
    bt_verdict_free(&verdict);
    bt_alg_free(&alg);
}

BT_TEST(verify_gaps, twist_notes_fail_closed_under_each_allocation_failure) {
    bt_alg alg;
    bt_alg_init(&alg);
    for (size_t ok = 0; ok < 4; ok++) {
        bt_verdict verdict;
        bt_verdict_init(&verdict);
        bt_alloc_fail_arm(ok);
        bt_verify(&alg, "UFR & UBR", BT_SHEET_TWISTS, &verdict);
        bt_alloc_fail_disarm();
        BT_CHECK(!bt_verdict_ok(&verdict));
        BT_CHECK_EQ((int)bt_alloc_fail_count() > 0, 1);
        bt_verdict_free(&verdict);
    }
    bt_verdict verdict;
    bt_verify(&alg, "UFR & UBR", BT_SHEET_TWISTS, &verdict);
    BT_CHECK(!bt_verdict_ok(&verdict));
    BT_CHECK(verdict.note_count == 2);
    bt_verdict_free(&verdict);
    bt_alg_free(&alg);
}
