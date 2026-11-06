#include "common/alloc.h"
#include "common/bt_harness.h"
#include "core/alg.h"
#include "data/commutator.h"

#include <stdio.h>
#include <string.h>

static const char *NINE_UNITS = "[R,U]+[F,D]+[L,R']+[B,D2]+[R2,F]+[U,D]+[M,E]+[S,R2]+[R,U2]";

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

BT_TEST(commutator_failures, deeper_nesting_than_the_limit_is_rejected) {
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    BT_CHECK(!bt_decompose("[[[R,U],F],D]", &units));
    BT_CHECK_EQ((int)units.count, 0);
    bt_expanded_list_free(&units);
}

BT_TEST(commutator_failures, decompose_fails_closed_under_each_allocation_failure) {
    for (size_t ok = 0; ok < 28; ok++) {
        bt_expanded_list units;
        bt_expanded_list_init(&units);
        bt_alloc_fail_arm(ok);
        bool good = bt_decompose("U:[[R' E' R,U2]+U2:[S,R2]]+[F,D]", &units);
        bt_alloc_fail_disarm();
        if (good) {
            BT_CHECK_EQ((int)units.count, 3);
        }
        bt_expanded_list_free(&units);
    }
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    BT_CHECK(bt_decompose("U:[[R' E' R,U2]+U2:[S,R2]]+[F,D]", &units));
    BT_CHECK_EQ((int)units.count, 3);
    bt_expanded_list_free(&units);
}

BT_TEST(commutator_failures, verified_fails_closed_under_each_allocation_failure) {
    bt_alg reference = alg_of("U' M U' M' U' M U' M'");
    for (size_t ok = 0; ok < 30; ok++) {
        bt_expanded_list units;
        bt_expanded_list_init(&units);
        bt_alloc_fail_arm(ok);
        bool good = bt_decompose_verified("[U',M]+M U:[U,M']", &reference, &units);
        bt_alloc_fail_disarm();
        if (good) {
            BT_CHECK_EQ((int)units.count, 2);
        }
        bt_expanded_list_free(&units);
    }
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    BT_CHECK(bt_decompose_verified("[U',M]+M U:[U,M']", &reference, &units));
    BT_CHECK_EQ((int)units.count, 2);
    bt_expanded_list_free(&units);
    bt_alg_free(&reference);
}
