#include "common/bt_harness.h"
#include "common/strbuf.h"
#include "data/commutator.h"

#include <string.h>

static bt_alg alg_of(const char *text) {
    bt_alg alg;
    bt_alg_init(&alg);
    char err[64];
    if (!bt_alg_parse(text, &alg, err, sizeof err)) {
        BT_ABORT("alg_of failed on %s", text);
    }
    return alg;
}

static void alg_text(const bt_alg *alg, char *out, size_t out_size) {
    bt_strbuf sb;
    bt_strbuf_init(&sb, 64);
    bt_alg_format(alg, &sb);
    snprintf(out, out_size, "%s", sb.data);
    bt_strbuf_free(&sb);
}

static void setup_text(const bt_expanded *unit, char *out, size_t out_size) {
    if (!unit->has_setup) {
        out[0] = '\0';
        return;
    }
    alg_text(&unit->setup, out, out_size);
}

static bt_alg concatenation(const bt_expanded_list *units) {
    bt_alg joined;
    bt_alg_init(&joined);
    for (size_t i = 0; i < units->count; i++) {
        bt_alg expansion = bt_expanded_expansion(&units->items[i]);
        for (size_t m = 0; m < bt_alg_len(&expansion); m++) {
            bt_alg_push(&joined, expansion.items[m]);
        }
        bt_alg_free(&expansion);
    }
    return joined;
}

static void render(const bt_expanded_list *units, char *out, size_t out_size) {
    size_t used = 0;
    out[0] = '\0';
    for (size_t i = 0; i < units->count; i++) {
        const bt_expanded *unit = &units->items[i];
        char first[128];
        char second[128];
        alg_text(unit->b_first ? &unit->b : &unit->a, first, sizeof first);
        alg_text(unit->b_first ? &unit->a : &unit->b, second, sizeof second);
        if (unit->has_setup) {
            char setup[128];
            alg_text(&unit->setup, setup, sizeof setup);
            used += (size_t)snprintf(out + used, out_size - used, "%s%s:[%s,%s]", i == 0 ? "" : "+",
                                     setup, first, second);
        } else {
            used += (size_t)snprintf(out + used, out_size - used, "%s[%s,%s]", i == 0 ? "" : "+",
                                     first, second);
        }
    }
}

static bt_expanded_list decompose_ok(const char *text) {
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    if (!bt_decompose(text, &units)) {
        BT_ABORT("decompose failed on %s", text);
    }
    return units;
}

static bt_expanded_list verified_ok(const char *text, const char *alg) {
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    bt_alg reference = alg_of(alg);
    if (!bt_decompose_verified(text, &reference, &units)) {
        BT_ABORT("decompose_verified failed on %s", text);
    }
    bt_alg_free(&reference);
    return units;
}

BT_TEST(commutator, setup_form_parses_and_reports_structure) {
    bt_expanded_list units = decompose_ok("R2 U':[S,R2]");
    BT_CHECK(units.count == 1);
    char setup[128];
    setup_text(&units.items[0], setup, sizeof setup);
    BT_CHECK_EQ(setup, "R2 U'");
    char a[128];
    char b[128];
    alg_text(&units.items[0].a, a, sizeof a);
    alg_text(&units.items[0].b, b, sizeof b);
    BT_CHECK_EQ(a, "S");
    BT_CHECK_EQ(b, "R2");
    BT_CHECK(units.items[0].b_first == false);
    bt_expanded_list_free(&units);
}

BT_TEST(commutator, setup_form_verifies_against_its_row_alg) {
    bt_expanded_list units = verified_ok("R2 U':[S,R2]", "R2 U' S R2 S' R2 U R2");
    BT_CHECK(units.count == 1);
    BT_CHECK(units.items[0].b_first == false);
    bt_expanded_list_free(&units);
}

BT_TEST(commutator, pure_multi_move_slots_parse_and_verify) {
    bt_expanded_list units = decompose_ok("[L E2 L2 E L,U]");
    BT_CHECK(units.count == 1);
    char setup[128];
    char a[128];
    char b[128];
    setup_text(&units.items[0], setup, sizeof setup);
    alg_text(&units.items[0].a, a, sizeof a);
    alg_text(&units.items[0].b, b, sizeof b);
    BT_CHECK_EQ(setup, "");
    BT_CHECK_EQ(a, "L E2 L2 E L");
    BT_CHECK_EQ(b, "U");
    bt_expanded_list verified = verified_ok("[L E2 L2 E L,U]", "L E2 L2 E L U L' E' L2 E2 L' U'");
    BT_CHECK(verified.items[0].b_first == false);
    bt_expanded_list_free(&units);
    bt_expanded_list_free(&verified);
}

BT_TEST(commutator, combo_units_parse_and_verify) {
    bt_expanded_list units = decompose_ok("[M',U2]+U':[R' F' R,S]");
    BT_CHECK(units.count == 2);
    char setup0[128];
    char setup1[128];
    char a0[128];
    char b0[128];
    char a1[128];
    char b1[128];
    setup_text(&units.items[0], setup0, sizeof setup0);
    setup_text(&units.items[1], setup1, sizeof setup1);
    alg_text(&units.items[0].a, a0, sizeof a0);
    alg_text(&units.items[0].b, b0, sizeof b0);
    alg_text(&units.items[1].a, a1, sizeof a1);
    alg_text(&units.items[1].b, b1, sizeof b1);
    BT_CHECK_EQ(setup0, "");
    BT_CHECK_EQ(a0, "M'");
    BT_CHECK_EQ(b0, "U2");
    BT_CHECK_EQ(setup1, "U'");
    BT_CHECK_EQ(a1, "R' F' R");
    BT_CHECK_EQ(b1, "S");
    bt_expanded_list verified =
        verified_ok("[M',U2]+U':[R' F' R,S]", "M' U2 M U R' F' R S R' F R S' U");
    BT_CHECK(verified.count == 2);
    for (size_t i = 0; i < verified.count; i++) {
        BT_CHECK(verified.items[i].b_first == false);
    }
    bt_expanded_list_free(&units);
    bt_expanded_list_free(&verified);
}

BT_TEST(commutator, combo_with_setup_verifies) {
    bt_expanded_list units = verified_ok("[U',M]+M U:[U,M']", "U' M U' M' U' M U' M'");
    BT_CHECK(units.count == 2);
    bt_expanded_list_free(&units);
}

BT_TEST(commutator, nested_sum_distributes_outer_setup) {
    bt_expanded_list units = decompose_ok("U:[[R' E' R,U2]+U2:[S,R2]]");
    BT_CHECK(units.count == 2);
    char setup0[128];
    char setup1[128];
    char a0[128];
    char b0[128];
    char a1[128];
    char b1[128];
    setup_text(&units.items[0], setup0, sizeof setup0);
    setup_text(&units.items[1], setup1, sizeof setup1);
    alg_text(&units.items[0].a, a0, sizeof a0);
    alg_text(&units.items[0].b, b0, sizeof b0);
    alg_text(&units.items[1].a, a1, sizeof a1);
    alg_text(&units.items[1].b, b1, sizeof b1);
    BT_CHECK_EQ(setup0, "U");
    BT_CHECK_EQ(a0, "R' E' R");
    BT_CHECK_EQ(b0, "U2");
    BT_CHECK_EQ(setup1, "U U2");
    BT_CHECK_EQ(a1, "S");
    BT_CHECK_EQ(b1, "R2");
    bt_expanded_list verified =
        verified_ok("U:[[R' E' R,U2]+U2:[S,R2]]", "U R' E' R U2 R' E R S R2 S' R2 U");
    bt_expanded_list_free(&units);
    bt_expanded_list_free(&verified);
}

BT_TEST(commutator, nested_bare_sum_keeps_setup_on_both_units) {
    bt_expanded_list units = decompose_ok("S R':[[E,R2]+[F2,R S' R']]");
    BT_CHECK(units.count == 2);
    char setup0[128];
    char setup1[128];
    char b1[128];
    setup_text(&units.items[0], setup0, sizeof setup0);
    setup_text(&units.items[1], setup1, sizeof setup1);
    alg_text(&units.items[1].b, b1, sizeof b1);
    BT_CHECK_EQ(setup0, "S R'");
    BT_CHECK_EQ(setup1, "S R'");
    BT_CHECK_EQ(b1, "R S' R'");
    bt_expanded_list verified =
        verified_ok("S R':[[E,R2]+[F2,R S' R']]", "S R' E R2 E' R2 F2 R S' R' F2 R");
    bt_expanded_list_free(&units);
    bt_expanded_list_free(&verified);
}

BT_TEST(commutator, b_first_order_is_chosen_by_verification) {
    bt_expanded_list units = verified_ok("[R,U]", "U R U' R'");
    BT_CHECK(units.count == 1);
    BT_CHECK(units.items[0].b_first == true);
    bt_expanded_list plain = decompose_ok("[R,U]");
    BT_CHECK(plain.items[0].b_first == false);
    bt_expanded_list_free(&units);
    bt_expanded_list_free(&plain);
}

BT_TEST(commutator, b_first_applies_to_every_unit_in_a_combo) {
    bt_expanded_list units = verified_ok("[R,U]+[F,D]", "U R U' R' D F D' F'");
    BT_CHECK(units.count == 2);
    for (size_t i = 0; i < units.count; i++) {
        BT_CHECK(units.items[i].b_first == true);
    }
    bt_expanded_list_free(&units);
}

BT_TEST(commutator, b_first_is_chosen_per_unit_in_a_combo) {
    bt_expanded_list units = verified_ok("[R,U]+[F,D]", "U R U' R' F D F' D'");
    BT_CHECK(units.count == 2);
    BT_CHECK(units.items[0].b_first == true);
    BT_CHECK(units.items[1].b_first == false);
    bt_expanded_list_free(&units);
}

BT_TEST(commutator, mismatched_alg_returns_none) {
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    bt_alg one = alg_of("F");
    BT_CHECK(bt_decompose_verified("[R,U]", &one, &units) == false);
    bt_alg_free(&one);
    bt_alg two = alg_of("R U R' U' F");
    BT_CHECK(bt_decompose_verified("[R,U]", &two, &units) == false);
    bt_alg_free(&two);
    bt_alg three = alg_of("R2 U' S R2 S' R2 U R2 F2");
    BT_CHECK(bt_decompose_verified("R2 U':[S,R2]", &three, &units) == false);
    bt_alg_free(&three);
}

BT_TEST(commutator, not_found_empty_and_whitespace_return_none) {
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    const char *bad[] = {"Not found.", "", "   ", " \t \n "};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        BT_CHECK(bt_decompose(bad[i], &units) == false);
    }
    bt_alg alg = alg_of("R U");
    BT_CHECK(bt_decompose_verified("Not found.", &alg, &units) == false);
    BT_CHECK(bt_decompose_verified("", &alg, &units) == false);
    bt_alg_free(&alg);
}

BT_TEST(commutator, garbage_returns_none) {
    const char *bad[] = {
        "N/A",          "-",           "R U R'",
        "R U R' U'",    "[,]",         "[R,]",
        "[,R]",         "[:R,U]",      "[R,U",
        "R,U]",         "[R,U]]",      "[[R,U]",
        "U::[R,U]",     "[Q9,R]",      "[R,U2",
        "[R,U]+",       "+[R,U]",      "[R,U]+junk",
        "junk+[R,U]",   "U:[[R,U]]]",  "U:[[R,U]+[S,D]]]",
        "[[A,B],C],D]", "[R,U]:[S,D]",
    };
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        BT_CHECK(bt_decompose(bad[i], &units) == false);
    }
}

BT_TEST(commutator, slot_nesting_expands_the_inner_commutator) {
    bt_expanded_list units = decompose_ok("[[R,U],S]");
    BT_CHECK(units.count == 1);
    char a[128];
    char b[128];
    alg_text(&units.items[0].a, a, sizeof a);
    alg_text(&units.items[0].b, b, sizeof b);
    BT_CHECK_EQ(a, "R U R' U'");
    BT_CHECK_EQ(b, "S");
    bt_expanded_list verified = verified_ok("[[R,U],S]", "R U R' U' S U R U' R' S'");
    bt_expanded_list_free(&units);
    bt_expanded_list_free(&verified);
}

BT_TEST(commutator, tolerates_padding_around_separators) {
    bt_expanded_list units = decompose_ok("  [ R , U2 ] + U' : [ F , D ]  ");
    BT_CHECK(units.count == 2);
    char setup1[128];
    char a1[128];
    setup_text(&units.items[1], setup1, sizeof setup1);
    alg_text(&units.items[1].a, a1, sizeof a1);
    BT_CHECK_EQ(setup1, "U'");
    BT_CHECK_EQ(a1, "F");
    bt_expanded_list_free(&units);
}

BT_TEST(commutator, round_trip_through_render_is_stable) {
    const char *texts[] = {
        "R2 U':[S,R2]",
        "[L E2 L2 E L,U]",
        "[M',U2]+U':[R' F' R,S]",
        "U:[[R' E' R,U2]+U2:[S,R2]]",
        "S R':[[E,R2]+[F2,R S' R']]",
        "[U',M]+M U:[U,M']",
        "[R,U]+[F,D]",
    };
    for (size_t t = 0; t < sizeof texts / sizeof texts[0]; t++) {
        bt_expanded_list once = decompose_ok(texts[t]);
        char rendered[512];
        render(&once, rendered, sizeof rendered);
        bt_expanded_list twice = decompose_ok(rendered);
        BT_CHECK(once.count == twice.count);
        for (size_t i = 0; i < once.count && i < twice.count; i++) {
            BT_CHECK(bt_expanded_eq(&once.items[i], &twice.items[i]) == true);
        }
        char rendered_again[512];
        render(&twice, rendered_again, sizeof rendered_again);
        BT_CHECK_EQ(rendered_again, rendered);
        bt_expanded_list_free(&once);
        bt_expanded_list_free(&twice);
    }
    bt_expanded_list verified = verified_ok("[R,U]+[F,D]", "U R U' R' D F D' F'");
    char text[512];
    render(&verified, text, sizeof text);
    BT_CHECK_EQ(text, "[U,R]+[D,F]");
    bt_expanded_list again = verified_ok(text, "U R U' R' D F D' F'");
    bt_alg concat_again = concatenation(&again);
    bt_alg concat_verified = concatenation(&verified);
    BT_CHECK(bt_alg_eq(&concat_again, &concat_verified) == true);
    char rendered_again[512];
    render(&again, rendered_again, sizeof rendered_again);
    BT_CHECK_EQ(rendered_again, text);
    bt_alg_free(&concat_again);
    bt_alg_free(&concat_verified);
    bt_expanded_list_free(&verified);
    bt_expanded_list_free(&again);
}

BT_TEST(commutator, deep_nesting_is_rejected) {
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    const char *bad[] = {"U:[[[R,U],S],D]", "[[[R,U],S],D]", "U:[[[R,U]]]", "[[[[R,U]]]]"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        BT_CHECK(bt_decompose(bad[i], &units) == false);
    }
}

BT_TEST(commutator, expansion_matches_the_commutator_definition) {
    bt_expanded_list units = decompose_ok("R2 U':[S,R2]");
    bt_alg expansion = bt_expanded_expansion(&units.items[0]);
    char text[128];
    alg_text(&expansion, text, sizeof text);
    BT_CHECK_EQ(text, "R2 U' S R2 S' R2 U R2");
    bt_alg_free(&expansion);
    bt_expanded manual;
    memset(&manual, 0, sizeof manual);
    manual.b_first = true;
    bt_alg_init(&manual.a);
    bt_alg_init(&manual.b);
    manual.a = alg_of("R");
    manual.b = alg_of("U");
    bt_alg manual_expansion = bt_expanded_expansion(&manual);
    char manual_text[128];
    alg_text(&manual_expansion, manual_text, sizeof manual_text);
    BT_CHECK_EQ(manual_text, "U R U' R'");
    bt_alg_free(&manual_expansion);
    bt_alg_free(&manual.a);
    bt_alg_free(&manual.b);
    bt_expanded_list_free(&units);
}

BT_TEST(commutator, setup_inside_a_slot_expands_inline) {
    bt_expanded_list units = decompose_ok("[U':[R,U],S]");
    BT_CHECK(units.count == 1);
    char setup[128];
    char a[128];
    char b[128];
    setup_text(&units.items[0], setup, sizeof setup);
    alg_text(&units.items[0].a, a, sizeof a);
    alg_text(&units.items[0].b, b, sizeof b);
    BT_CHECK_EQ(setup, "");
    BT_CHECK_EQ(a, "U' R U R' U' U");
    BT_CHECK_EQ(b, "S");
    bt_expanded_list verified = verified_ok("[U':[R,U],S]", "U' R U R' U' U S U' U R U' R' U S'");
    bt_expanded_list_free(&units);
    bt_expanded_list_free(&verified);
}

BT_TEST(commutator, triple_sum_parses_and_verifies) {
    const char *text = "[R,U]+[F,D]+[S,E2]";
    bt_expanded_list units = decompose_ok(text);
    BT_CHECK(units.count == 3);
    bt_alg concat = concatenation(&units);
    bt_alg reference = alg_of("R U R' U' F D F' D' S E2 S' E2");
    BT_CHECK(bt_alg_eq(&concat, &reference) == true);
    bt_expanded_list verified = verified_ok(text, "R U R' U' F D F' D' S E2 S' E2");
    for (size_t i = 0; i < verified.count; i++) {
        BT_CHECK(verified.items[i].b_first == false);
    }
    bt_alg_free(&concat);
    bt_alg_free(&reference);
    bt_expanded_list_free(&units);
    bt_expanded_list_free(&verified);
}

BT_TEST(commutator, empty_unit_inside_a_sum_is_rejected) {
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    BT_CHECK(bt_decompose("[R,U] + + [F,D]", &units) == false);
    BT_CHECK(bt_decompose("+ [R,U]", &units) == false);
}

BT_TEST(commutator, padded_not_found_marker_is_rejected) {
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    const char *bad[] = {" Not found. ", "\tNot found.\n", "Not found. now really",
                         "prefix Not found.",};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        BT_CHECK(bt_decompose(bad[i], &units) == false);
    }
}

BT_TEST(commutator, malformed_cores_return_none) {
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    const char *bad[] = {"[R,U,U]", "[R,U,,F]", "[R U]", "[R,U] U"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        BT_CHECK(bt_decompose(bad[i], &units) == false);
    }
}

BT_TEST(commutator, parse_core_rejects_unbalanced_delimiters) {
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    const char *bad[] = {"[R,U2", "Q[R,U]", "[R,U]Q", "R,U]"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        BT_CHECK(bt_decompose(bad[i], &units) == false);
    }
}
