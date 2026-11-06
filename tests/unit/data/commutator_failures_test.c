#include "common/alloc.h"
#include "common/bt_harness.h"
#include "core/alg.h"
#include "data/commutator.h"

#include <stdio.h>
#include <string.h>

static const char *NINE_UNITS = "[R,U]+[F,D]+[L,R']+[B,D2]+[R2,F]+[U,D]+[M,E]+[S,R2]+[R,U2]";
static const char *EIGHT_UNITS = "[R,U]+[F,D]+[L,R']+[B,D2]+[R2,F]+[U,D]+[M,E]+[S,R2]";

static bt_alg alg_of(const char *text) {
    bt_alg alg;
    bt_alg_init(&alg);
    BT_CHECK(bt_alg_try_parse(text, &alg));
    return alg;
}

BT_TEST(commutator_failures, a_nine_unit_combo_grows_both_split_buffers) {
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    BT_CHECK(bt_decompose(NINE_UNITS, &units));
    BT_CHECK_EQ((int)units.count, 9);
    bt_expanded_list_free(&units);
}

BT_TEST(commutator_failures, an_eight_unit_combo_grows_the_final_split_slot) {
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    BT_CHECK(bt_decompose(EIGHT_UNITS, &units));
    BT_CHECK_EQ((int)units.count, 8);
    bt_expanded_list_free(&units);
}

BT_TEST(commutator_failures, expanded_eq_rejects_each_field_mismatch) {
    bt_alg r = alg_of("R");
    bt_alg r2 = alg_of("R2");
    bt_alg f = alg_of("F");
    bt_expanded base;
    memset(&base, 0, sizeof base);
    base.a = r;
    base.b = f;
    bt_expanded flipped = base;
    flipped.b_first = true;
    BT_CHECK(!bt_expanded_eq(&base, &flipped));
    bt_expanded with_setup = base;
    with_setup.has_setup = true;
    with_setup.setup = r2;
    BT_CHECK(!bt_expanded_eq(&base, &with_setup));
    bt_expanded other_setup = with_setup;
    other_setup.setup = r;
    BT_CHECK(!bt_expanded_eq(&with_setup, &other_setup));
    bt_expanded same_setup = with_setup;
    same_setup.setup = r2;
    BT_CHECK(bt_expanded_eq(&with_setup, &same_setup));
    bt_alg_free(&r);
    bt_alg_free(&r2);
    bt_alg_free(&f);
}

BT_TEST(commutator_failures, a_whitespace_setup_is_rejected) {
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    BT_CHECK(!bt_decompose("  :[R,U]", &units));
    bt_expanded_list_free(&units);
}

BT_TEST(commutator_failures, deeper_nesting_than_the_limit_is_rejected) {
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    BT_CHECK(!bt_decompose("[[[R,U],F],D]", &units));
    BT_CHECK_EQ((int)units.count, 0);
    bt_expanded_list_free(&units);
}

BT_TEST(commutator_failures, a_bracketed_slot_with_an_empty_expansion_is_rejected) {
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    BT_CHECK(!bt_decompose("[[[R,U]],F]", &units));
    BT_CHECK_EQ((int)units.count, 0);
    bt_expanded_list_free(&units);
    bt_expanded_list_init(&units);
    BT_CHECK(bt_decompose("[[R,U]+[F,D],L]", &units));
    BT_CHECK_EQ((int)units.count, 1);
    bt_expanded_list_free(&units);
}

BT_TEST(commutator_failures, decompose_fails_closed_under_each_allocation_failure) {
    for (size_t ok = 0; ok < 48; ok++) {
        bt_expanded_list units;
        bt_expanded_list_init(&units);
        bt_alloc_fail_arm(ok);
        bool good = bt_decompose("U:[[R' E' R,U2]+U2:[S,R2]]+[F,D]", &units);
        bt_alloc_fail_disarm();
        if (good) {
            BT_CHECK_EQ((int)units.count, 3);
            bt_expanded_list_free(&units);
        }
    }
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    BT_CHECK(bt_decompose("U:[[R' E' R,U2]+U2:[S,R2]]+[F,D]", &units));
    BT_CHECK_EQ((int)units.count, 3);
    bt_expanded_list_free(&units);
}

BT_TEST(commutator_failures, verified_fails_closed_under_each_allocation_failure) {
    bt_alg reference = alg_of("U' M U' M' U' M U' M'");
    for (size_t ok = 0; ok < 33; ok++) {
        bt_expanded_list units;
        bt_expanded_list_init(&units);
        bt_alloc_fail_arm(ok);
        bool good = bt_decompose_verified("[U',M]+M U:[U,M']", &reference, &units);
        bt_alloc_fail_disarm();
        if (good) {
            BT_CHECK_EQ((int)units.count, 2);
            bt_expanded_list_free(&units);
        }
    }
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    BT_CHECK(bt_decompose_verified("[U',M]+M U:[U,M']", &reference, &units));
    BT_CHECK_EQ((int)units.count, 2);
    bt_expanded_list_free(&units);
    bt_alg_free(&reference);
}
