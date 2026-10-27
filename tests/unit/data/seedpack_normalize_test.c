#include "common/bt_harness.h"
#include "data/seedpack/normalizer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bt_normalized normalize_ok(const char *event, const char *raw) {
    bt_normalized out;
    bt_normalize_error err;
    if (!bt_seedpack_normalize_alg(event, raw, &out, &err)) {
        char text[768];
        bt_normalize_error_string(&err, text, sizeof text);
        BT_ABORT("normalize failed on %s: %s", raw, text);
    }
    return out;
}

static void expect_error(const char *event, const char *raw, const char *needle) {
    bt_normalized out;
    bt_normalize_error err;
    BT_CHECK(bt_seedpack_normalize_alg(event, raw, &out, &err) == false);
    char text[768];
    bt_normalize_error_string(&err, text, sizeof text);
    if (needle != NULL) {
        BT_CHECK(strstr(text, needle) != NULL);
    }
}

BT_TEST(seedpack_normalize, normalize_strips_grouping_parens) {
    bt_normalized out = normalize_ok("3bld", "(L' U) (R U') (L U2') (R' U) (R U2 R')");
    BT_CHECK_EQ(out.alg, "L' U R U' L U2' R' U R U2 R'");
    BT_CHECK(out.note_count == 0);
    bt_normalized_free(&out);
}

BT_TEST(seedpack_normalize, normalize_expands_group_repeat_2) {
    bt_normalized out = normalize_ok("skewb", "(y2) (F' L F L')2");
    BT_CHECK_EQ(out.alg, "y2 F' L F L' F' L F L'");
    bt_normalized_free(&out);
}

BT_TEST(seedpack_normalize, normalize_expands_group_repeat_3) {
    bt_normalized out = normalize_ok("3x3", "(U R U' R')3");
    BT_CHECK_EQ(out.alg, "U R U' R' U R U' R' U R U' R'");
    bt_normalized_free(&out);
}

BT_TEST(seedpack_normalize, normalize_expands_bracket_star_repeats) {
    bt_normalized out = normalize_ok("megaminx", "[(R U R' U) (R' U' R2 U') (R' U R' U R) U] *2");
    BT_CHECK_EQ(out.alg, "R U R' U R' U' R2 U' R' U R' U R U R U R' U R' U' R2 U' R' U R' U R U");
    bt_normalized_free(&out);
}

BT_TEST(seedpack_normalize, normalize_splits_arrow_chains_into_a_note) {
    bt_normalized out = normalize_ok("skewb", "R' F R F' -> cat. 1");
    BT_CHECK_EQ(out.alg, "R' F R F'");
    BT_CHECK(out.note_count == 1);
    BT_CHECK_EQ(out.notes[0], "chain note: -> cat. 1");
    bt_normalized_free(&out);
}

BT_TEST(seedpack_normalize, normalize_keeps_parens_through_non_sq1_auf_shaped_groups) {
    bt_normalized out = normalize_ok("3x3", "R2 U R' U R' U' R U' R2 (U' D) R' U R D'");
    BT_CHECK_EQ(out.alg, "R2 U R' U R' U' R U' R2 U' D R' U R D'");
    BT_CHECK(out.note_count == 0);
    bt_normalized_free(&out);
}

BT_TEST(seedpack_normalize, normalize_converts_separating_commas_to_whitespace) {
    bt_normalized out = normalize_ok("bigcube", "r2, U2, x");
    BT_CHECK_EQ(out.alg, "r2 U2 x");
    bt_normalized_free(&out);
}

BT_TEST(seedpack_normalize, normalize_maps_the_literal_x_rotation) {
    bt_normalized out = normalize_ok(
        "bigcube", "Rw U2, X, Rw U2, Rw U2 , Rw' U2, Lw U2, Rw' U2, Rw U2, Rw' U2, Rw'");
    BT_CHECK_EQ(out.alg, "Rw U2 x Rw U2 Rw U2 Rw' U2 Lw U2 Rw' U2 Rw U2 Rw' U2 Rw'");
    BT_CHECK(out.note_count == 1);
    BT_CHECK_EQ(out.notes[0], "mapped X to x");
    bt_normalized_free(&out);
}

BT_TEST(seedpack_normalize, normalize_moves_pure_prose_to_a_note) {
    bt_normalized out = normalize_ok("3bld", "apply swap directly, no setup");
    BT_CHECK_EQ(out.alg, "");
    BT_CHECK(out.note_count == 1);
    BT_CHECK_EQ(out.notes[0], "prose alg: apply swap directly, no setup");
    bt_normalized_free(&out);
}

BT_TEST(seedpack_normalize, normalize_wraps_bare_sq1_tuples) {
    bt_normalized out = normalize_ok("sq1", "/ 3,3 /");
    BT_CHECK_EQ(out.alg, "/ (3,3) /");
    bt_normalized_free(&out);
    out = normalize_ok("sq1", "/ 3,0 / -1,1");
    BT_CHECK_EQ(out.alg, "/ (3,0) / (-1,1)");
    bt_normalized_free(&out);
}

BT_TEST(seedpack_normalize, normalize_strips_sq1_auf_prefixes_with_a_note) {
    bt_normalized out = normalize_ok("sq1", "(U) / (3,3) / (0,3)");
    BT_CHECK_EQ(out.alg, "/ (3,3) / (0,3)");
    BT_CHECK(out.note_count == 1);
    BT_CHECK_EQ(out.notes[0], "auf prefix stripped: (U)");
    bt_normalized_free(&out);
    out = normalize_ok("sq1", "(U2 D') (0,1) / (3,0)");
    BT_CHECK_EQ(out.alg, "(0,1) / (3,0)");
    BT_CHECK(out.note_count == 1);
    BT_CHECK_EQ(out.notes[0], "auf prefix stripped: (U2 D')");
    bt_normalized_free(&out);
    out = normalize_ok("sq1", "(U D)(0,-1) / (3,0)");
    BT_CHECK_EQ(out.alg, "(0,-1) / (3,0)");
    BT_CHECK(out.note_count == 1);
    BT_CHECK_EQ(out.notes[0], "auf prefix stripped: (U D)");
    bt_normalized_free(&out);
}

BT_TEST(seedpack_normalize, normalize_strips_a_trailing_sq1_equator_marker_with_a_note) {
    bt_normalized out = normalize_ok("sq1", "(1,0) / (5,0) *");
    BT_CHECK_EQ(out.alg, "(1,0) / (5,0)");
    BT_CHECK(out.note_count == 1);
    BT_CHECK_EQ(out.notes[0], "equator marker stripped: *");
    bt_normalized_free(&out);
}

BT_TEST(seedpack_normalize, normalize_spaces_fused_sq1_slashes) {
    bt_normalized out = normalize_ok("sq1", "(1,0)/(-1,0)");
    BT_CHECK_EQ(out.alg, "(1,0) / (-1,0)");
    bt_normalized_free(&out);
}

BT_TEST(seedpack_normalize, normalize_rejects_unknown_tokens_and_broken_structure) {
    expect_error("3x3", "R U swap", "swap");
    expect_error("3x3", "R U ?", NULL);
    expect_error("3x3", "R w", NULL);
    expect_error("3x3", "R U ->", NULL);
    expect_error("3x3", "(R U", NULL);
    expect_error("3x3", "R U)", NULL);
    expect_error("3x3", "(R U)x", NULL);
    expect_error("3x3", "(((R)99)99)99", NULL);
    expect_error("sq1", "(1,0) (U D) /", NULL);
    expect_error("sq1", "(x,y)", NULL);
    expect_error("sq1", "(0,1,2)", NULL);
    expect_error("sq1", "(01,0)", NULL);
}

BT_TEST(seedpack_normalize, sq1_rejects_turns_the_engine_can_never_parse) {
    const char *bad[] = {
        "(0,0)", "(7,0)", "(0,7)", "(1,-7)", "(-7,0)", "(6,7)", "(0,-7)", "(8,1)", "0,0", "7,-5",
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        expect_error("sq1", bad[i], NULL);
    }
    bt_normalized six_six = normalize_ok("sq1", "(6,6)");
    bt_normalized_free(&six_six);
    bt_normalized five_five = normalize_ok("sq1", "(-5,-5)");
    bt_normalized_free(&five_five);
}

BT_TEST(seedpack_normalize, normalize_rejects_doubled_suffixes_and_mixed_case_letter_pairs) {
    const char *bad[] = {"R U Ru", "R U aU", "R U R''", "R U R22"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        expect_error("3x3", bad[i], "unknown token");
    }
}

BT_TEST(seedpack_normalize, normalize_expands_a_bracket_repeat_at_exactly_the_limit) {
    bt_normalized out = normalize_ok("3x3", "[R]*99");
    char want[256];
    size_t used = 0;
    for (int i = 0; i < 99; i++) {
        used += (size_t)snprintf(want + used, sizeof want - used, "%s%s", i == 0 ? "" : " ", "R");
    }
    BT_CHECK_EQ(out.alg, want);
    bt_normalized_free(&out);
}

BT_TEST(seedpack_normalize, normalize_pins_the_bracket_expand_length_boundary_as_a_token_error) {
    char content[9100];
    for (int i = 0; i < 9090; i++) {
        content[i] = 'R';
    }
    content[9090] = '\0';
    char alg[9200];
    snprintf(alg, sizeof alg, "[%s]*11", content);
    expect_error("3x3", alg, "unknown token");
}

BT_TEST(seedpack_normalize, normalize_rejects_a_repeat_digit_on_a_nested_group) {
    bt_normalized out;
    bt_normalize_error err;
    BT_CHECK(bt_seedpack_normalize_alg("3x3", "(R U (R2))2", &out, &err) == false);
    char text[768];
    bt_normalize_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "unknown token") != NULL);
    BT_CHECK(strstr(text, "R22") != NULL);
}

BT_TEST(seedpack_normalize, normalize_rejects_a_bare_digit_token) {
    struct {
        const char *alg;
        const char *token;
    } cases[] = {
        {"R 2", "2"},
        {"R 7", "7"},
        {"R 8", "8"},
        {"2R 2", "2"},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        bt_normalized out;
        bt_normalize_error err;
        BT_CHECK(bt_seedpack_normalize_alg("3x3", cases[i].alg, &out, &err) == false);
        char text[768];
        bt_normalize_error_string(&err, text, sizeof text);
        char needle[64];
        snprintf(needle, sizeof needle, "unknown token '%s'", cases[i].token);
        BT_CHECK(strstr(text, needle) != NULL);
    }
    bt_normalized out = normalize_ok("3x3", "2R");
    BT_CHECK_EQ(out.alg, "2R");
    bt_normalized_free(&out);
}

BT_TEST(seedpack_normalize, normalize_rejects_a_nested_bracket_repeat) {
    const char *bad[] = {"[[R U]*2]*2", "[[R]*2", "[[R U]*2]"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        bt_normalized out;
        bt_normalize_error err;
        BT_CHECK(bt_seedpack_normalize_alg("3x3", bad[i], &out, &err) == false);
        char text[768];
        bt_normalize_error_string(&err, text, sizeof text);
        BT_CHECK(strstr(text, "malformed repeat marker") != NULL);
        BT_CHECK(strstr(text, bad[i]) != NULL);
    }
}

BT_TEST(seedpack_normalize, normalize_expands_nested_group_repeats_innermost_first) {
    bt_normalized out = normalize_ok("3x3", "((R U)2)2");
    BT_CHECK_EQ(out.alg, "R U R U R U R U");
    BT_CHECK(out.note_count == 0);
    bt_normalized_free(&out);
}

BT_TEST(seedpack_normalize, normalize_rejects_a_zero_bracket_repeat) {
    expect_error("3x3", "U [R]*0", "malformed repeat");
}

BT_TEST(seedpack_normalize, normalize_rejects_a_zero_group_repeat) {
    expect_error("3x3", "(R U)0", "malformed repeat");
}

BT_TEST(seedpack_normalize, normalize_expands_a_group_repeat_at_exactly_the_limit) {
    bt_normalized out = normalize_ok("3x3", "(R U)99");
    char want[512];
    size_t used = 0;
    for (int i = 0; i < 99; i++) {
        used += (size_t)snprintf(want + used, sizeof want - used, "%s%s", i == 0 ? "" : " ", "R U");
    }
    BT_CHECK_EQ(out.alg, want);
    bt_normalized_free(&out);
}

BT_TEST(seedpack_normalize, normalize_rejects_a_suffix_glued_to_a_group_repeat) {
    expect_error("3x3", "(R U)2x", "malformed repeat");
    expect_error("3x3", "(R U)2'", "malformed repeat");
    expect_error("3x3", "(R U)'", "malformed repeat");
}

BT_TEST(seedpack_normalize, normalize_pins_the_group_expand_length_boundary_as_a_token_error) {
    char content[9100];
    for (int i = 0; i < 9090; i++) {
        content[i] = 'R';
    }
    content[9090] = '\0';
    char alg[9200];
    snprintf(alg, sizeof alg, "(%s)11", content);
    expect_error("3x3", alg, "unknown token");
}

BT_TEST(seedpack_normalize, normalize_strips_nested_grouping_parens) {
    bt_normalized out = normalize_ok("3x3", "(R U (R' F))");
    BT_CHECK_EQ(out.alg, "R U R' F");
    bt_normalized_free(&out);
}

BT_TEST(seedpack_normalize, sq1_rejects_a_three_token_auf_shaped_prefix) {
    expect_error("sq1", "(U D U) / (3,3)", "unknown token");
}

BT_TEST(seedpack_normalize, sq1_keeps_a_non_auf_prefix_group_and_rejects_its_token) {
    const char *bad[] = {"(R2) / (3,3)", "(x') (0,1)", "(F) / (0,1)"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        bt_normalized out;
        bt_normalize_error err;
        BT_CHECK(bt_seedpack_normalize_alg("sq1", bad[i], &out, &err) == false);
        char text[768];
        bt_normalize_error_string(&err, text, sizeof text);
        BT_CHECK(strstr(text, "unknown token") != NULL);
        size_t span = strcspn(bad[i], " ");
        char head[32];
        snprintf(head, sizeof head, "%.*s", span > 30 ? 30 : (int)span, bad[i]);
        BT_CHECK(strstr(text, head) != NULL);
    }
}

BT_TEST(seedpack_normalize, sq1_pins_the_reported_span_of_an_unknown_token) {
    expect_error("sq1", "x/(3,3)", "unknown token 'x'");
    expect_error("sq1", "x / (3,3)", "unknown token 'x'");
}
