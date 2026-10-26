#include "scheme/scheme.h"

#include "common/bt_harness.h"
#include "death_test.h"

#include <string.h>

typedef struct {
    char letter;
    const char *name;
} bt_plan_row;

static const bt_plan_row PLAN_EDGE[24] = {
    {'A', "UB"}, {'B', "UR"}, {'C', "UF"}, {'D', "UL"}, {'E', "LU"}, {'F', "LF"},
    {'G', "LD"}, {'H', "LB"}, {'I', "FU"}, {'J', "FR"}, {'K', "FD"}, {'L', "FL"},
    {'M', "RU"}, {'N', "RB"}, {'O', "RD"}, {'P', "RF"}, {'Q', "BU"}, {'R', "BL"},
    {'S', "BD"}, {'T', "BR"}, {'U', "DF"}, {'V', "DR"}, {'W', "DB"}, {'X', "DL"},
};

static const bt_plan_row PLAN_CORNER[24] = {
    {'A', "UBL"}, {'B', "UBR"}, {'C', "UFR"}, {'D', "UFL"}, {'E', "LUB"}, {'F', "LUF"},
    {'G', "LDF"}, {'H', "LDB"}, {'I', "FUL"}, {'J', "FUR"}, {'K', "FDR"}, {'L', "FDL"},
    {'M', "RUF"}, {'N', "RUB"}, {'O', "RDB"}, {'P', "RDF"}, {'Q', "BUR"}, {'R', "BUL"},
    {'S', "BDL"}, {'T', "BDR"}, {'U', "DFL"}, {'V', "DFR"}, {'W', "DBR"}, {'X', "DBL"},
};

BT_TEST(scheme, edge_scheme_matches_plan) {
    for (int i = 0; i < 24; i++) {
        uint8_t sticker = (uint8_t)(24 + i);
        BT_CHECK_EQ(bt_scheme_letter_of(sticker), PLAN_EDGE[i].letter);
        BT_CHECK_EQ(bt_scheme_name_of(sticker), PLAN_EDGE[i].name);
        BT_CHECK_EQ(bt_scheme_sticker_of(BT_ORBIT_EDGE, PLAN_EDGE[i].letter), sticker);
    }
}

BT_TEST(scheme, corner_scheme_matches_plan) {
    for (int i = 0; i < 24; i++) {
        uint8_t sticker = (uint8_t)i;
        BT_CHECK_EQ(bt_scheme_letter_of(sticker), PLAN_CORNER[i].letter);
        BT_CHECK_EQ(bt_scheme_name_of(sticker), PLAN_CORNER[i].name);
        BT_CHECK_EQ(bt_scheme_sticker_of(BT_ORBIT_CORNER, PLAN_CORNER[i].letter), sticker);
    }
}

BT_TEST(scheme, buffers_are_uf_and_ufr_letter_c) {
    BT_CHECK_EQ(BT_SCHEME_EDGE_BUFFER_STICKER, 26);
    BT_CHECK_EQ(bt_scheme_letter_of(BT_SCHEME_EDGE_BUFFER_STICKER), 'C');
    BT_CHECK_EQ(bt_scheme_name_of(BT_SCHEME_EDGE_BUFFER_STICKER), "UF");
    BT_CHECK_EQ(BT_SCHEME_CORNER_BUFFER_STICKER, 2);
    BT_CHECK_EQ(bt_scheme_letter_of(BT_SCHEME_CORNER_BUFFER_STICKER), 'C');
    BT_CHECK_EQ(bt_scheme_name_of(BT_SCHEME_CORNER_BUFFER_STICKER), "UFR");
}

BT_TEST(scheme, floating_orders_pinned) {
    static const char *edge_want[12] = {"UF", "UB", "UR", "UL", "FR", "FL",
                                        "DF", "DB", "DR", "DL", "BR", "BL"};
    static const char *corner_want[8] = {"UFR", "UFL", "UBR", "UBL", "DFR", "DFL", "DBR", "DBL"};
    for (int i = 0; i < 12; i++) {
        BT_CHECK_EQ(BT_SCHEME_EDGE_FLOATING[i], edge_want[i]);
    }
    for (int i = 0; i < 8; i++) {
        BT_CHECK_EQ(BT_SCHEME_CORNER_FLOATING[i], corner_want[i]);
    }
    BT_CHECK_EQ(BT_SCHEME_EDGE_FLOATING[0], "UF");
    BT_CHECK_EQ(BT_SCHEME_CORNER_FLOATING[0], "UFR");
}

BT_TEST(scheme, floating_heads_pinned) {
    static const uint8_t edge_want[12] = {26, 24, 25, 27, 33, 35, 44, 46, 45, 47, 43, 41};
    static const uint8_t corner_want[8] = {2, 3, 1, 0, 21, 20, 22, 23};
    uint8_t edge_heads[12];
    uint8_t corner_heads[8];
    bt_scheme_edge_floating_heads(edge_heads);
    bt_scheme_corner_floating_heads(corner_heads);
    for (int i = 0; i < 12; i++) {
        BT_CHECK_EQ(edge_heads[i], edge_want[i]);
    }
    for (int i = 0; i < 8; i++) {
        BT_CHECK_EQ(corner_heads[i], corner_want[i]);
    }
    BT_CHECK_EQ(edge_heads[0], BT_SCHEME_EDGE_BUFFER_STICKER);
    BT_CHECK_EQ(corner_heads[0], BT_SCHEME_CORNER_BUFFER_STICKER);
}

BT_TEST(scheme, head_sticker_follows_ud_then_fb_rule_on_all_pieces) {
    for (uint8_t sticker = 0; sticker < 48; sticker++) {
        uint8_t head = bt_scheme_head_sticker(bt_scheme_name_of(sticker));
        BT_CHECK(bt_scheme_same_piece(sticker, head));
        char first = bt_scheme_name_of(head)[0];
        bool has_ud = false;
        for (uint8_t other = 0; other < 48; other++) {
            if (bt_scheme_same_piece(other, sticker)) {
                char face = bt_scheme_name_of(other)[0];
                if (face == 'U' || face == 'D') {
                    has_ud = true;
                }
            }
        }
        if (has_ud) {
            BT_CHECK(first == 'U' || first == 'D');
        } else {
            BT_CHECK(first == 'F' || first == 'B');
        }
    }
    BT_CHECK_EQ(bt_scheme_head_sticker("UF"), 26);
    BT_CHECK_EQ(bt_scheme_head_sticker("FU"), 26);
    BT_CHECK_EQ(bt_scheme_head_sticker("FR"), 33);
    BT_CHECK_EQ(bt_scheme_head_sticker("BR"), 43);
    BT_CHECK_EQ(bt_scheme_head_sticker("DFR"), 21);
    BT_CHECK_EQ(bt_scheme_head_sticker("RUF"), 2);
}

BT_TEST(scheme, letter_and_sticker_round_trip) {
    for (uint8_t sticker = 0; sticker < 48; sticker++) {
        char letter = bt_scheme_letter_of(sticker);
        BT_CHECK(letter >= 'A' && letter <= 'X');
        bt_orbit orbit = sticker < 24 ? BT_ORBIT_CORNER : BT_ORBIT_EDGE;
        BT_CHECK_EQ(bt_scheme_sticker_of(orbit, letter), sticker);
    }
    bt_orbit orbits[2] = {BT_ORBIT_EDGE, BT_ORBIT_CORNER};
    for (int i = 0; i < 2; i++) {
        for (char letter = 'A'; letter <= 'X'; letter++) {
            BT_CHECK_EQ(bt_scheme_letter_of(bt_scheme_sticker_of(orbits[i], letter)), letter);
        }
    }
}

static int compare_names(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

BT_TEST(scheme, names_are_unique_and_face_complete) {
    bt_orbit orbits[2] = {BT_ORBIT_EDGE, BT_ORBIT_CORNER};
    for (int i = 0; i < 2; i++) {
        const char *names[24];
        int expected = orbits[i] == BT_ORBIT_EDGE ? 2 : 3;
        for (int j = 0; j < 24; j++) {
            names[j] = bt_scheme_name_of((uint8_t)(bt_scheme_orbit_lo(orbits[i]) + j));
        }
        qsort(names, 24, sizeof names[0], compare_names);
        for (int j = 1; j < 24; j++) {
            BT_CHECK(strcmp(names[j - 1], names[j]) != 0);
        }
        for (int j = 0; j < 24; j++) {
            const char *name = names[j];
            BT_CHECK_EQ(strlen(name), (size_t)expected);
            for (size_t a = 0; a < strlen(name); a++) {
                for (size_t b = a + 1; b < strlen(name); b++) {
                    BT_CHECK(name[a] != name[b]);
                }
                char face = name[a];
                BT_CHECK(face == 'U' || face == 'D' || face == 'F' || face == 'B' || face == 'L' ||
                         face == 'R');
            }
        }
    }
}

BT_TEST(scheme, same_piece_groups_by_face_set) {
    BT_CHECK(bt_scheme_same_piece(26, 32));
    BT_CHECK(bt_scheme_same_piece(32, 26));
    BT_CHECK(!bt_scheme_same_piece(26, 25));
    BT_CHECK(bt_scheme_same_piece(2, 9));
    BT_CHECK(bt_scheme_same_piece(2, 12));
    BT_CHECK(!bt_scheme_same_piece(2, 3));
    BT_CHECK(!bt_scheme_same_piece(2, 26));
    BT_CHECK(bt_scheme_same_piece(47, 30));
    BT_CHECK(bt_scheme_same_piece(23, 18));
    for (uint8_t sticker = 0; sticker < 48; sticker++) {
        BT_CHECK(bt_scheme_same_piece(sticker, sticker));
    }
}

static void run_letter_of_out_of_range(void) {
    bt_scheme_letter_of(48);
}

static void run_name_of_out_of_range(void) {
    bt_scheme_name_of(200);
}

static void run_sticker_of_bad_letter(void) {
    bt_scheme_sticker_of(BT_ORBIT_EDGE, 'Z');
}

static void run_head_sticker_unknown_piece(void) {
    bt_scheme_head_sticker("UU");
}

BT_TEST(scheme, letter_of_rejects_out_of_range) {
    bt_death_expect("scheme.letter_of_rejects_out_of_range", "sticker id out of range: 48",
                    run_letter_of_out_of_range);
}

BT_TEST(scheme, name_of_rejects_out_of_range) {
    bt_death_expect("scheme.name_of_rejects_out_of_range", "sticker id out of range: 200",
                    run_name_of_out_of_range);
}

BT_TEST(scheme, sticker_of_rejects_out_of_range_letter) {
    bt_death_expect("scheme.sticker_of_rejects_out_of_range_letter", "letter out of range: Z",
                    run_sticker_of_bad_letter);
}

BT_TEST(scheme, head_sticker_rejects_unknown_piece) {
    bt_death_expect("scheme.head_sticker_rejects_unknown_piece", "unknown piece",
                    run_head_sticker_unknown_piece);
}
