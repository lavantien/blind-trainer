#include "scheme/pairs.h"
#include "scheme/scheme.h"

#include "common/bt_harness.h"

#include <stdlib.h>
#include <string.h>

static int compare_pairs(const void *a, const void *b) {
    const bt_case *first = a;
    const bt_case *second = b;
    if (first->t1 != second->t1) {
        return (int)first->t1 - (int)second->t1;
    }
    return (int)first->t2 - (int)second->t2;
}

static int compare_letters(const void *a, const void *b) {
    const char *first = a;
    const char *second = b;
    if (first[0] != second[0]) {
        return (int)first[0] - (int)second[0];
    }
    return (int)first[1] - (int)second[1];
}

BT_TEST(pairs, edge_case_count_is_440) {
    bt_case cases[BT_PAIRS_MAX];
    BT_CHECK_EQ(bt_scheme_enumerate_cases(BT_ORBIT_EDGE, cases), (size_t)440);
}

BT_TEST(pairs, corner_case_count_is_378) {
    bt_case cases[BT_PAIRS_MAX];
    BT_CHECK_EQ(bt_scheme_enumerate_cases(BT_ORBIT_CORNER, cases), (size_t)378);
}

BT_TEST(pairs, cases_are_sorted_and_unique) {
    bt_orbit orbits[2] = {BT_ORBIT_EDGE, BT_ORBIT_CORNER};
    for (int i = 0; i < 2; i++) {
        bt_case cases[BT_PAIRS_MAX];
        size_t count = bt_scheme_enumerate_cases(orbits[i], cases);
        bt_case *sorted = malloc(count * sizeof *sorted);
        memcpy(sorted, cases, count * sizeof *sorted);
        qsort(sorted, count, sizeof *sorted, compare_pairs);
        for (size_t j = 0; j < count; j++) {
            BT_CHECK_EQ(cases[j].t1, sorted[j].t1);
            BT_CHECK_EQ(cases[j].t2, sorted[j].t2);
        }
        for (size_t j = 1; j < count; j++) {
            BT_CHECK(compare_pairs(&cases[j - 1], &cases[j]) != 0);
        }
        free(sorted);
    }
}

BT_TEST(pairs, buffer_piece_and_own_piece_stickers_never_appear) {
    bt_orbit orbits[2] = {BT_ORBIT_EDGE, BT_ORBIT_CORNER};
    for (int i = 0; i < 2; i++) {
        uint8_t buffer = orbits[i] == BT_ORBIT_EDGE ? BT_SCHEME_EDGE_BUFFER_STICKER
                                                    : BT_SCHEME_CORNER_BUFFER_STICKER;
        bt_case cases[BT_PAIRS_MAX];
        size_t count = bt_scheme_enumerate_cases(orbits[i], cases);
        for (size_t j = 0; j < count; j++) {
            const bt_case *case_id = &cases[j];
            BT_CHECK(case_id->t1 >= bt_scheme_orbit_lo(orbits[i]));
            BT_CHECK(case_id->t1 < bt_scheme_orbit_hi(orbits[i]));
            BT_CHECK(case_id->t2 >= bt_scheme_orbit_lo(orbits[i]));
            BT_CHECK(case_id->t2 < bt_scheme_orbit_hi(orbits[i]));
            BT_CHECK(!bt_scheme_same_piece(case_id->t1, buffer));
            BT_CHECK(!bt_scheme_same_piece(case_id->t2, buffer));
            BT_CHECK(!bt_scheme_same_piece(case_id->t1, case_id->t2));
        }
    }
}

BT_TEST(pairs, letters_match_scheme_lookup) {
    bt_orbit orbits[2] = {BT_ORBIT_EDGE, BT_ORBIT_CORNER};
    for (int i = 0; i < 2; i++) {
        bt_case cases[BT_PAIRS_MAX];
        size_t count = bt_scheme_enumerate_cases(orbits[i], cases);
        for (size_t j = 0; j < count; j++) {
            char letters[2];
            bt_scheme_case_letters(&cases[j], letters);
            BT_CHECK_EQ(letters[0], bt_scheme_letter_of(cases[j].t1));
            BT_CHECK_EQ(letters[1], bt_scheme_letter_of(cases[j].t2));
        }
    }
}

BT_TEST(pairs, letter_pairs_are_injective) {
    bt_orbit orbits[2] = {BT_ORBIT_EDGE, BT_ORBIT_CORNER};
    for (int i = 0; i < 2; i++) {
        bt_case cases[BT_PAIRS_MAX];
        size_t count = bt_scheme_enumerate_cases(orbits[i], cases);
        char (*pairs)[2] = malloc(count * sizeof *pairs);
        for (size_t j = 0; j < count; j++) {
            bt_scheme_case_letters(&cases[j], pairs[j]);
        }
        qsort(pairs, count, sizeof *pairs, compare_letters);
        for (size_t j = 1; j < count; j++) {
            BT_CHECK(compare_letters(&pairs[j - 1], &pairs[j]) != 0);
        }
        free(pairs);
    }
}

BT_TEST(pairs, edge_enumeration_bounds_pinned) {
    bt_case cases[BT_PAIRS_MAX];
    size_t count = bt_scheme_enumerate_cases(BT_ORBIT_EDGE, cases);
    char first[2];
    char last[2];
    bt_scheme_case_letters(&cases[0], first);
    bt_scheme_case_letters(&cases[count - 1], last);
    BT_CHECK_EQ(cases[0].t1, 24);
    BT_CHECK_EQ(cases[0].t2, 25);
    BT_CHECK_EQ(first[0], 'A');
    BT_CHECK_EQ(first[1], 'B');
    BT_CHECK_EQ(cases[count - 1].t1, 47);
    BT_CHECK_EQ(cases[count - 1].t2, 46);
    BT_CHECK_EQ(last[0], 'X');
    BT_CHECK_EQ(last[1], 'W');
}

BT_TEST(pairs, corner_enumeration_bounds_pinned) {
    bt_case cases[BT_PAIRS_MAX];
    size_t count = bt_scheme_enumerate_cases(BT_ORBIT_CORNER, cases);
    char first[2];
    char last[2];
    bt_scheme_case_letters(&cases[0], first);
    bt_scheme_case_letters(&cases[count - 1], last);
    BT_CHECK_EQ(cases[0].t1, 0);
    BT_CHECK_EQ(cases[0].t2, 1);
    BT_CHECK_EQ(first[0], 'A');
    BT_CHECK_EQ(first[1], 'B');
    BT_CHECK_EQ(cases[count - 1].t1, 23);
    BT_CHECK_EQ(cases[count - 1].t2, 22);
    BT_CHECK_EQ(last[0], 'X');
    BT_CHECK_EQ(last[1], 'W');
}

BT_TEST(pairs, buffer_letters_never_appear) {
    bt_case edges[BT_PAIRS_MAX];
    size_t edge_count = bt_scheme_enumerate_cases(BT_ORBIT_EDGE, edges);
    for (size_t j = 0; j < edge_count; j++) {
        char letters[2];
        bt_scheme_case_letters(&edges[j], letters);
        BT_CHECK(letters[0] != 'C' && letters[0] != 'I');
        BT_CHECK(letters[1] != 'C' && letters[1] != 'I');
    }
    bt_case corners[BT_PAIRS_MAX];
    size_t corner_count = bt_scheme_enumerate_cases(BT_ORBIT_CORNER, corners);
    for (size_t j = 0; j < corner_count; j++) {
        char letters[2];
        bt_scheme_case_letters(&corners[j], letters);
        BT_CHECK(letters[0] != 'C' && letters[0] != 'J' && letters[0] != 'M');
        BT_CHECK(letters[1] != 'C' && letters[1] != 'J' && letters[1] != 'M');
    }
}
