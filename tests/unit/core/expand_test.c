#include <string.h>

#include "common/bt_harness.h"
#include "common/strbuf.h"
#include "core/alg.h"
#include "core/cube.h"
#include "core/expand.h"

static void fmt_of(const char *text, bt_strbuf *out) {
    bt_alg alg;
    bt_alg_init(&alg);
    if (!bt_expand(text, &alg)) {
        BT_ABORT("expansible text failed: %s", text);
    }
    bt_alg_format(&alg, out);
    bt_alg_free(&alg);
}

static bt_cube state_of(const bt_alg *alg) {
    bt_cube cube = bt_cube_solved();
    bt_cube_apply_alg(&cube, alg);
    return cube;
}

static bt_cube state_of_text(const char *text) {
    bt_alg alg;
    bt_alg_init(&alg);
    if (!bt_expand(text, &alg)) {
        BT_ABORT("expansible text failed: %s", text);
    }
    bt_cube cube = state_of(&alg);
    bt_alg_free(&alg);
    return cube;
}

static bt_cube repeated_ref(const char *text, size_t count) {
    bt_alg alg;
    bt_alg_init(&alg);
    char err[64];
    if (!bt_alg_parse(text, &alg, err, sizeof err)) {
        BT_ABORT("flat reference failed: %s: %s", text, err);
    }
    bt_cube cube = bt_cube_solved();
    for (size_t i = 0; i < count; i++) {
        bt_cube_apply_alg(&cube, &alg);
    }
    bt_alg_free(&alg);
    return cube;
}

static void cancel_of(const char *text, bt_strbuf *out) {
    bt_alg alg;
    bt_alg_init(&alg);
    char err[64];
    if (!bt_alg_parse(text, &alg, err, sizeof err)) {
        BT_ABORT("parse failed: %s: %s", text, err);
    }
    bt_alg cancelled;
    bt_alg_init(&cancelled);
    bt_expand_cancel(&alg, &cancelled);
    bt_alg_format(&cancelled, out);
    bt_alg_free(&alg);
    bt_alg_free(&cancelled);
}

BT_TEST(expand, cancellation_drops_inverse_pairs) {
    const char *texts[] = {"R R'", "R' R", "U U'", "x x'", "R U U' R'"};
    for (size_t i = 0; i < 5; i++) {
        bt_strbuf sb;
        bt_strbuf_init(&sb, 16);
        cancel_of(texts[i], &sb);
        BT_CHECK_EQ(sb.len, (size_t)0);
        bt_strbuf_free(&sb);
    }
}

BT_TEST(expand, cancellation_merges_same_base_quarters) {
    struct {
        const char *text;
        const char *want;
    } cases[] = {
        {"R R", "R2"},  {"R' R'", "R2"}, {"R2 R", "R'"}, {"R2 R'", "R"},  {"R R2 R'", "R2"},
        {"U2 U", "U'"}, {"U2 U'", "U"},  {"x2 x", "x'"}, {"M M M", "M'"},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        bt_strbuf sb;
        bt_strbuf_init(&sb, 16);
        cancel_of(cases[i].text, &sb);
        BT_CHECK_EQ(sb.data, cases[i].want);
        bt_strbuf_free(&sb);
    }
}

BT_TEST(expand, cancellation_never_crosses_other_moves_or_move_kinds) {
    const char *fixed[] = {
        "R U R U", "R U R' U'", "R r", "r R", "R 2R", "M 2L", "U u", "R U2 R' U' R U R' U'",
    };
    for (size_t i = 0; i < sizeof fixed / sizeof fixed[0]; i++) {
        bt_strbuf sb;
        bt_strbuf_init(&sb, 32);
        cancel_of(fixed[i], &sb);
        BT_CHECK_EQ(sb.data, fixed[i]);
        bt_strbuf_free(&sb);
    }
}

BT_TEST(expand, cancellation_preserves_cube_state) {
    const char *cases[] = {
        "R U U' R'", "R2 R' U U2", "x x' R R R", "M2 M U U' U", "R U R' U' R R' R2",
    };
    for (size_t i = 0; i < 5; i++) {
        bt_alg alg;
        bt_alg_init(&alg);
        char err[64];
        BT_CHECK(bt_alg_parse(cases[i], &alg, err, sizeof err));
        bt_alg cancelled;
        bt_alg_init(&cancelled);
        bt_expand_cancel(&alg, &cancelled);
        bt_cube a = state_of(&alg);
        bt_cube b = state_of(&cancelled);
        BT_CHECK(bt_cube_eq(&a, &b));
        bt_alg_free(&alg);
        bt_alg_free(&cancelled);
    }
}

BT_TEST(expand, cancellation_is_idempotent) {
    const char *texts[] = {"R U U' R'", "R2 R' U U2", "R R2 R'", "M M M", "x2 x x'"};
    for (size_t i = 0; i < 5; i++) {
        bt_alg alg;
        bt_alg_init(&alg);
        char err[64];
        BT_CHECK(bt_alg_parse(texts[i], &alg, err, sizeof err));
        bt_alg once;
        bt_alg_init(&once);
        bt_expand_cancel(&alg, &once);
        bt_alg twice;
        bt_alg_init(&twice);
        bt_expand_cancel(&once, &twice);
        bt_strbuf a;
        bt_strbuf b;
        bt_strbuf_init(&a, 16);
        bt_strbuf_init(&b, 16);
        bt_alg_format(&once, &a);
        bt_alg_format(&twice, &b);
        BT_CHECK_EQ(a.data, b.data);
        bt_strbuf_free(&a);
        bt_strbuf_free(&b);
        bt_alg_free(&alg);
        bt_alg_free(&once);
        bt_alg_free(&twice);
    }
}

BT_TEST(expand, flat_algs_pass_through_unchanged) {
    const char *texts[] = {
        "R U R' U'",
        "U M2 U M' U2 M U M2 U'",
        "2L 2R' S2 y'",
        "M' U2 M U R' F' R S R' F R S' U",
    };
    for (size_t i = 0; i < 4; i++) {
        bt_strbuf sb;
        bt_strbuf_init(&sb, 64);
        fmt_of(texts[i], &sb);
        BT_CHECK_EQ(sb.data, texts[i]);
        bt_strbuf_free(&sb);
        bt_cube a = state_of_text(texts[i]);
        bt_alg flat;
        bt_alg_init(&flat);
        char err[64];
        BT_CHECK(bt_alg_parse(texts[i], &flat, err, sizeof err));
        bt_cube b = state_of(&flat);
        BT_CHECK(bt_cube_eq(&a, &b));
        bt_alg_free(&flat);
    }
}

BT_TEST(expand, the_pinned_repeat_example_cancels_to_a_half_turn) {
    bt_strbuf sb;
    bt_strbuf_init(&sb, 32);
    fmt_of("(R U R')2", &sb);
    BT_CHECK_EQ(sb.data, "R U2 R'");
    bt_strbuf_free(&sb);
    bt_cube a = state_of_text("(R U R')2");
    bt_cube b = repeated_ref("R U R'", 2);
    BT_CHECK(bt_cube_eq(&a, &b));
    bt_strbuf_init(&sb, 32);
    fmt_of("(R U R' )2", &sb);
    BT_CHECK_EQ(sb.data, "R U2 R'");
    bt_strbuf_free(&sb);
    bt_strbuf_init(&sb, 32);
    fmt_of("( R U R')2", &sb);
    BT_CHECK_EQ(sb.data, "R U2 R'");
    bt_strbuf_free(&sb);
}

BT_TEST(expand, repeat_counts_fold_into_quarter_arithmetic) {
    struct {
        const char *text;
        const char *want;
    } pins[] = {
        {"(R)2", "R2"},
        {"(R)3", "R'"},
        {"(R)5", "R"},
        {"(R)99", "R'"},
    };
    for (size_t i = 0; i < 4; i++) {
        bt_strbuf sb;
        bt_strbuf_init(&sb, 16);
        fmt_of(pins[i].text, &sb);
        BT_CHECK_EQ(sb.data, pins[i].want);
        bt_strbuf_free(&sb);
    }
    bt_cube a = state_of_text("(R)99");
    bt_cube b = repeated_ref("R", 99);
    BT_CHECK(bt_cube_eq(&a, &b));
    bt_alg out;
    bt_alg_init(&out);
    BT_CHECK(!bt_expand("(R)4", &out));
    bt_alg_free(&out);
    bt_alg_init(&out);
    BT_CHECK(!bt_expand("(R2)2", &out));
    bt_alg_free(&out);
}

BT_TEST(expand, nested_and_bare_groups_unfold) {
    struct {
        const char *text;
        const char *want;
    } pins[] = {
        {"((R U)2)2", "R U R U R U R U"},
        {"(R U R')", "R U R'"},
        {"(L' U) (R U') (L U2')", "L' U R U' L U2"},
        {"[R]", "R"},
        {"[[R,U]+[F,D]]", "R U R' U' F D F' D'"},
    };
    for (size_t i = 0; i < 5; i++) {
        bt_strbuf sb;
        bt_strbuf_init(&sb, 64);
        fmt_of(pins[i].text, &sb);
        BT_CHECK_EQ(sb.data, pins[i].want);
        bt_strbuf_free(&sb);
    }
    bt_cube a = state_of_text("((R U)2)2");
    bt_cube b = repeated_ref("R U R U", 2);
    BT_CHECK(bt_cube_eq(&a, &b));
}

BT_TEST(expand, commutator_spans_decompose_in_written_order) {
    struct {
        const char *text;
        const char *want;
    } pins[] = {
        {"[R,U]", "R U R' U'"},
        {"[L E2 L2 E L,U]", "L E2 L2 E L U L' E' L2 E2 L' U'"},
        {"[[R,U],S]", "R U R' U' S U R U' R' S'"},
        {"[U':[R,U],S]", "U' R U R' S R U' R' U S'"},
    };
    for (size_t i = 0; i < 4; i++) {
        bt_strbuf sb;
        bt_strbuf_init(&sb, 64);
        fmt_of(pins[i].text, &sb);
        BT_CHECK_EQ(sb.data, pins[i].want);
        bt_strbuf_free(&sb);
    }
}

BT_TEST(expand, setups_wrap_and_sums_concatenate) {
    struct {
        const char *text;
        const char *want;
    } pins[] = {
        {"R2 U':[S,R2]", "R2 U' S R2 S' R2 U R2"},
        {"U M2 U:[M',U2]", "U M2 U M' U2 M U M2 U'"},
        {"[R,U]+[F,D]", "R U R' U' F D F' D'"},
        {"[M',U2]+U':[R' F' R,S]", "M' U2 M U R' F' R S R' F R S' U"},
        {"U:[[R' E' R,U2]+U2:[S,R2]]", "U R' E' R U2 R' E R S R2 S' R2 U"},
    };
    for (size_t i = 0; i < 5; i++) {
        bt_strbuf sb;
        bt_strbuf_init(&sb, 96);
        fmt_of(pins[i].text, &sb);
        BT_CHECK_EQ(sb.data, pins[i].want);
        bt_strbuf_free(&sb);
    }
}

BT_TEST(expand, repeats_containing_commutators_cancel_across_copies) {
    bt_strbuf sb;
    bt_strbuf_init(&sb, 64);
    fmt_of("([R,U])2", &sb);
    BT_CHECK_EQ(sb.data, "R U R' U' R U R' U'");
    bt_strbuf_free(&sb);
    bt_cube a = state_of_text("([R,U])2");
    bt_cube b = repeated_ref("R U R' U'", 2);
    BT_CHECK(bt_cube_eq(&a, &b));
    bt_strbuf_init(&sb, 64);
    fmt_of("(U':[R,U])2", &sb);
    BT_CHECK_EQ(sb.data, "U' R U R' U' R U R'");
    bt_strbuf_free(&sb);
    bt_cube c = state_of_text("(U':[R,U])2");
    bt_cube d = repeated_ref("U' R U R' U' U", 2);
    BT_CHECK(bt_cube_eq(&c, &d));
}

BT_TEST(expand, vendor_forms_stay_state_true_against_the_reference_cube) {
    struct {
        const char *text;
        const char *reference;
    } cases[] = {
        {"[R,U]", "R U R' U'"},
        {"R2 U':[S,R2]", "R2 U' S R2 S' R2 U R2"},
        {"[M',U2]+U':[R' F' R,S]", "M' U2 M U R' F' R S R' F R S' U"},
        {"[L E2 L2 E L,U]", "L E2 L2 E L U L' E' L2 E2 L' U'"},
        {"U:[[R' E' R,U2]+U2:[S,R2]]", "U R' E' R U2 R' E R S R2 S' R2 U"},
        {"[[R,U],S]", "R U R' U' S U R U' R' S'"},
        {"[U':[R,U],S]", "U' R U R' U' U S U' U R U' R' U S'"},
        {"U M2 U:[M',U2]", "U M2 U M' U2 M U M2 U'"},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        bt_cube a = state_of_text(cases[i].text);
        bt_cube b;
        bt_alg flat;
        bt_alg_init(&flat);
        char err[64];
        BT_CHECK(bt_alg_parse(cases[i].reference, &flat, err, sizeof err));
        b = state_of(&flat);
        BT_CHECK(bt_cube_eq(&a, &b));
        bt_alg_free(&flat);
    }
}

BT_TEST(expand, expansion_reaches_a_fixed_point) {
    const char *texts[] = {
        "(R U R')2",
        "(R)99",
        "((R U)2)2",
        "[R,U]",
        "R2 U':[S,R2]",
        "[M',U2]+U':[R' F' R,S]",
        "U:[[R' E' R,U2]+U2:[S,R2]]",
        "[[R,U],S]",
        "[U':[R,U],S]",
        "(U':[R,U])2",
        "U M2 U:[M',U2]",
    };
    for (size_t i = 0; i < 11; i++) {
        bt_alg once;
        bt_alg_init(&once);
        BT_CHECK(bt_expand(texts[i], &once));
        bt_strbuf formatted;
        bt_strbuf_init(&formatted, 96);
        bt_alg_format(&once, &formatted);
        bt_alg twice;
        bt_alg_init(&twice);
        BT_CHECK(bt_expand(formatted.data, &twice));
        BT_CHECK(bt_alg_eq(&twice, &once));
        bt_strbuf again;
        bt_strbuf_init(&again, 96);
        bt_alg_format(&twice, &again);
        bt_alg thrice;
        bt_alg_init(&thrice);
        BT_CHECK(bt_expand(again.data, &thrice));
        BT_CHECK(bt_alg_eq(&thrice, &twice));
        bt_strbuf_free(&formatted);
        bt_strbuf_free(&again);
        bt_alg_free(&once);
        bt_alg_free(&twice);
        bt_alg_free(&thrice);
    }
}

BT_TEST(expand, expansion_is_deterministic) {
    const char *texts[] = {"(R U R')2", "[M',U2]+U':[R' F' R,S]", "(U':[R,U])2"};
    for (size_t i = 0; i < 3; i++) {
        bt_alg a;
        bt_alg b;
        bt_alg_init(&a);
        bt_alg_init(&b);
        BT_CHECK(bt_expand(texts[i], &a));
        BT_CHECK(bt_expand(texts[i], &b));
        BT_CHECK(bt_alg_eq(&a, &b));
        bt_alg_free(&a);
        bt_alg_free(&b);
    }
}

BT_TEST(expand, unparsable_and_non_alg_input_returns_none) {
    const char *bad[] = {
        "",  " ",   "   ",   "Not found.", " Not found. ", "junk text", "N/A",
        "-", "R Q", "U wut", "R T",        "Rw U",         "U-",
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        bt_alg out;
        bt_alg_init(&out);
        BT_CHECK(!bt_expand(bad[i], &out));
        bt_alg_free(&out);
    }
}

BT_TEST(expand, malformed_structure_returns_none) {
    const char *bad[] = {
        "(R",
        "R)",
        "((R)",
        "[R,U",
        "[[R,U]",
        "[R,U]]",
        "[,]",
        "[R,]",
        "[,R]",
        "[:R,U]",
        ":[R,U]",
        "U::[R,U]",
        "U:[R,U]]",
        "[R,U,U]",
        "[R,U,,F]",
        "()",
        "( )",
        "(R, U)",
        "(R)0",
        "(R)100",
        "(R)99999999999999999999",
        "2[R]",
        "R 2",
        "U:[[R,U]]]",
        "((((((R U R')2)2)2)2)2)2",
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        bt_alg out;
        bt_alg_init(&out);
        BT_CHECK(!bt_expand(bad[i], &out));
        bt_alg_free(&out);
    }
}

BT_TEST(expand, unbalanced_and_runaway_inputs_return_none) {
    bt_alg out;
    bt_alg_init(&out);
    BT_CHECK(bt_expand("((((R U)2)2)2)2", &out));
    bt_alg_free(&out);
    bt_alg_init(&out);
    BT_CHECK(!bt_expand("(((((R U)2)2)2)2)2", &out));
    bt_alg_free(&out);
    bt_alg_init(&out);
    BT_CHECK(!bt_expand("((((R U R')2)2)2)2", &out));
    bt_alg_free(&out);
    bt_alg_init(&out);
    BT_CHECK(bt_expand("(R U R' F)99", &out));
    bt_alg_free(&out);
    bt_alg_init(&out);
    BT_CHECK(!bt_expand("((R U R' F B L D S E M2 U')99)99", &out));
    bt_alg_free(&out);
    bt_alg_init(&out);
    BT_CHECK(!bt_expand("(R)101", &out));
    bt_alg_free(&out);
    bt_alg_init(&out);
    BT_CHECK(!bt_expand("(R U)100", &out));
    bt_alg_free(&out);
}

BT_TEST(expand, the_move_cap_is_exclusive_at_both_levels) {
    bt_strbuf exact;
    bt_strbuf_init(&exact, 300000);
    for (size_t i = 0; i < 50000; i++) {
        bt_strbuf_append_str(&exact, i == 0 ? "R U" : " R U");
    }
    bt_alg alg;
    bt_alg_init(&alg);
    char err[64];
    BT_CHECK(bt_alg_parse(exact.data, &alg, err, sizeof err));
    BT_CHECK_EQ(bt_alg_len(&alg), (size_t)100000);
    bt_alg out;
    bt_alg_init(&out);
    BT_CHECK(bt_expand(exact.data, &out));
    bt_alg_free(&out);
    bt_strbuf over;
    bt_strbuf_init(&over, 300000);
    bt_strbuf_append_str(&over, exact.data);
    bt_strbuf_append_str(&over, " U U");
    bt_alg_init(&out);
    BT_CHECK(!bt_expand(over.data, &out));
    bt_alg_free(&out);
    bt_strbuf unit;
    bt_strbuf_init(&unit, 64);
    bt_strbuf_append_str(&unit, "R U F D L B R U F D L B R U F D L B R U F D L B R");
    bt_alg_init(&alg);
    BT_CHECK(bt_alg_parse(unit.data, &alg, err, sizeof err));
    BT_CHECK_EQ(bt_alg_len(&alg), (size_t)25);
    bt_strbuf mid;
    bt_strbuf_init(&mid, 4096);
    bt_strbuf_appendf(&mid, "(%s)50", unit.data);
    bt_alg_init(&out);
    BT_CHECK(bt_expand(mid.data, &out));
    bt_alg_free(&out);
    bt_strbuf projected;
    bt_strbuf_init(&projected, 204800);
    bt_strbuf_appendf(&projected, "(%s)80", mid.data);
    bt_alg_init(&out);
    BT_CHECK(bt_expand(projected.data, &out));
    bt_alg_free(&out);
    bt_strbuf over2;
    bt_strbuf_init(&over2, 204800);
    bt_strbuf_appendf(&over2, "(%s)81", mid.data);
    bt_alg_init(&out);
    BT_CHECK(!bt_expand(over2.data, &out));
    bt_alg_free(&out);
    bt_alg_free(&alg);
    bt_strbuf_free(&exact);
    bt_strbuf_free(&over);
    bt_strbuf_free(&unit);
    bt_strbuf_free(&mid);
    bt_strbuf_free(&projected);
    bt_strbuf_free(&over2);
}

BT_TEST(expand, bracket_nesting_is_bounded_like_parens) {
    bt_alg out;
    bt_alg_init(&out);
    BT_CHECK(bt_expand("[[[[R,U],S],D],F]", &out));
    bt_alg_free(&out);
    bt_alg_init(&out);
    BT_CHECK(!bt_expand("[[[[[R,U],S],D],F],E]", &out));
    bt_alg_free(&out);
    bt_alg_init(&out);
    BT_CHECK(bt_expand("[R,[R,[R,[R,U]]]]", &out));
    bt_alg_free(&out);
    bt_alg_init(&out);
    BT_CHECK(!bt_expand("[R,[R,[R,[R,[R,U]]]]]", &out));
    bt_alg_free(&out);
}

BT_TEST(expand, stray_separators_at_top_level_return_none) {
    const char *bad[] = {"R,U", "R)", "R]", "(R)2,"};
    for (size_t i = 0; i < 4; i++) {
        bt_alg out;
        bt_alg_init(&out);
        BT_CHECK(!bt_expand(bad[i], &out));
        bt_alg_free(&out);
    }
}
