#include "puzzles/skewb.h"
#include "puzzles/tier1.h"

#include "common/bt_harness.h"
#include "common/rng.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void applied(const bt_skewb *skewb, const char *text, bt_skewb_state *out) {
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    if (!bt_skewb_parse_alg(skewb, text, strlen(text), alg, &err)) {
        BT_ABORT("parse %s failed: %s", text, err.token);
    }
    bt_skewb_solved_state(skewb, out);
    if (!bt_skewb_apply_alg(skewb, out, alg, &err)) {
        BT_ABORT("apply %s failed: %s", text, err.token);
    }
    bt_pz_alg_destroy(alg);
}

static void after_text(const bt_skewb *skewb, const char *text,
                       uint8_t out[BT_SKEWB_FACELET_COUNT]) {
    bt_skewb_state state;
    applied(skewb, text, &state);
    memcpy(out, state.facelets, BT_SKEWB_FACELET_COUNT);
}

static size_t changed_slots(const bt_skewb *skewb, const char *text,
                            size_t out[BT_SKEWB_FACELET_COUNT]) {
    uint8_t facelets[BT_SKEWB_FACELET_COUNT];
    after_text(skewb, text, facelets);
    size_t count = 0;
    for (size_t i = 0; i < BT_SKEWB_FACELET_COUNT; i++) {
        if (facelets[i] != (uint8_t)(i / 9)) {
            out[count] = i;
            count++;
        }
    }
    return count;
}

static bool tier1_ok(const bt_skewb *skewb, const char *text) {
    bt_pz_engine eng = bt_skewb_engine(skewb);
    bt_tier1_error err;
    return bt_verify_tier1(&eng, text, &err) == BT_TIER1_OK;
}

static int cmp_size(const void *a, const void *b) {
    size_t left = *(const size_t *)a;
    size_t right = *(const size_t *)b;
    if (left < right) {
        return -1;
    }
    return left > right ? 1 : 0;
}

static int cmp_string_p(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static size_t expected_layer(size_t vertex, size_t out[BT_SKEWB_FACELET_COUNT]) {
    static const size_t cells[4][6][8] = {
        {{1, 2, 3, 4, 5, 6, 7, 8},
         {0, 1, 2, 3, 4, 5, 7, 8},
         {0, 1, 2, 3, 4, 5, 6, 7},
         {2, 0, 0, 0, 0, 0, 0, 0},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {2, 0, 0, 0, 0, 0, 0, 0}},
        {{0, 1, 3, 4, 5, 6, 7, 8},
         {0, 1, 2, 3, 4, 5, 6, 7},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {0, 1, 2, 3, 4, 5, 7, 8},
         {2, 0, 0, 0, 0, 0, 0, 0},
         {0, 0, 0, 0, 0, 0, 0, 0}},
        {{0, 1, 2, 3, 4, 5, 7, 8},
         {2, 0, 0, 0, 0, 0, 0, 0},
         {0, 1, 2, 3, 4, 5, 7, 8},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {0, 1, 2, 3, 4, 5, 6, 7},
         {8, 0, 0, 0, 0, 0, 0, 0}},
        {{8, 0, 0, 0, 0, 0, 0, 0},
         {1, 2, 3, 4, 5, 6, 7, 8},
         {0, 1, 3, 4, 5, 6, 7, 8},
         {8, 0, 0, 0, 0, 0, 0, 0},
         {6, 0, 0, 0, 0, 0, 0, 0},
         {0, 1, 2, 3, 4, 5, 7, 8}},
    };
    static const size_t widths[4][6] = {
        {8, 8, 8, 1, 1, 1},
        {8, 8, 1, 8, 1, 1},
        {8, 1, 8, 1, 8, 1},
        {1, 8, 8, 1, 1, 8},
    };
    size_t count = 0;
    for (size_t face = 0; face < 6; face++) {
        for (size_t i = 0; i < widths[vertex][face]; i++) {
            out[count] = face * 9 + cells[vertex][face][i];
            count++;
        }
    }
    qsort(out, count, sizeof out[0], cmp_size);
    return count;
}

BT_TEST(skewb_oracle, solved_grid_renders_six_square_faces) {
    bt_skewb skewb = bt_skewb_new();
    bt_skewb_state state;
    bt_skewb_solved_state(&skewb, &state);
    bt_pz_grid grid;
    bt_skewb_facelets(&skewb, &state, &grid);
    BT_CHECK_EQ((long long)grid.face_count, 6);
    for (size_t face = 0; face < grid.face_count; face++) {
        BT_CHECK_EQ((long long)grid.faces[face].row_count, 3);
        for (size_t row = 0; row < grid.faces[face].row_count; row++) {
            BT_CHECK_EQ((long long)grid.faces[face].rows[row].cols, 3);
            for (size_t col = 0; col < grid.faces[face].rows[row].cols; col++) {
                BT_CHECK_EQ(grid.faces[face].rows[row].cells[col], (uint8_t)face);
            }
        }
    }
    BT_CHECK(bt_skewb_is_solved(&skewb, &state));
}

BT_TEST(skewb_oracle, every_turn_is_a_bijective_order_three_permutation) {
    bt_skewb skewb = bt_skewb_new();
    for (uint8_t vertex = 0; vertex < 4; vertex++) {
        for (uint8_t quarter = 1; quarter <= 2; quarter++) {
            uint8_t table[BT_SKEWB_FACELET_COUNT];
            bt_skewb_turn_table(&skewb, vertex, quarter, table);
            uint8_t sorted[BT_SKEWB_FACELET_COUNT];
            memcpy(sorted, table, sizeof sorted);
            for (size_t i = 1; i < BT_SKEWB_FACELET_COUNT; i++) {
                uint8_t key = sorted[i];
                size_t j = i;
                while (j > 0 && sorted[j - 1] > key) {
                    sorted[j] = sorted[j - 1];
                    j--;
                }
                sorted[j] = key;
            }
            for (size_t i = 0; i < BT_SKEWB_FACELET_COUNT; i++) {
                BT_CHECK_EQ(sorted[i], (uint8_t)i);
            }
            for (size_t i = 0; i < BT_SKEWB_FACELET_COUNT; i++) {
                uint8_t doubled = table[table[i]];
                BT_CHECK_EQ(table[doubled], (uint8_t)i);
            }
            bool seen[BT_SKEWB_FACELET_COUNT] = {false};
            size_t fixed = 0;
            size_t threes = 0;
            for (size_t i = 0; i < BT_SKEWB_FACELET_COUNT; i++) {
                if (seen[i]) {
                    continue;
                }
                size_t len = 0;
                size_t j = i;
                while (!seen[j]) {
                    seen[j] = true;
                    j = table[j];
                    len++;
                }
                if (len == 1) {
                    fixed++;
                } else if (len == 3) {
                    threes++;
                } else {
                    BT_ABORT("vertex %u quarter %u cycle len %zu", vertex, quarter, len);
                }
            }
            BT_CHECK_EQ((long long)fixed, 27);
            BT_CHECK_EQ((long long)threes, 9);
        }
    }
}

BT_TEST(skewb_oracle, turns_recolor_exactly_the_derived_layer) {
    bt_skewb skewb = bt_skewb_new();
    for (size_t vertex = 0; vertex < 4; vertex++) {
        size_t expected[BT_SKEWB_FACELET_COUNT];
        size_t expected_count = expected_layer(vertex, expected);
        BT_CHECK_EQ((long long)expected_count, 27);
        char prime[8];
        snprintf(prime, sizeof prime, "%s'", BT_SKEWB_VERTEX_NAMES[vertex]);
        for (size_t variant = 0; variant < 2; variant++) {
            const char *text = variant == 0 ? BT_SKEWB_VERTEX_NAMES[vertex] : prime;
            size_t moved[BT_SKEWB_FACELET_COUNT];
            size_t moved_count = changed_slots(&skewb, text, moved);
            BT_CHECK(moved_count == expected_count);
            for (size_t k = 0; k < moved_count && k < expected_count; k++) {
                BT_CHECK(moved[k] == expected[k]);
            }
        }
    }
}

BT_TEST(skewb_oracle, center_cycles_match_derived_geometry) {
    bt_skewb skewb = bt_skewb_new();
    const char *names[4] = {"F", "L", "R", "B"};
    const size_t moved_positions[4][3] = {
        {0 * 9 + 4, 2 * 9 + 4, 1 * 9 + 4},
        {0 * 9 + 4, 1 * 9 + 4, 3 * 9 + 4},
        {0 * 9 + 4, 4 * 9 + 4, 2 * 9 + 4},
        {1 * 9 + 4, 2 * 9 + 4, 5 * 9 + 4},
    };
    const uint8_t moved_values[4][3] = {
        {1, 0, 2},
        {3, 0, 1},
        {2, 0, 4},
        {5, 1, 2},
    };
    const size_t frozen_positions[4][3] = {
        {3 * 9 + 4, 4 * 9 + 4, 5 * 9 + 4},
        {2 * 9 + 4, 4 * 9 + 4, 5 * 9 + 4},
        {1 * 9 + 4, 3 * 9 + 4, 5 * 9 + 4},
        {0 * 9 + 4, 3 * 9 + 4, 4 * 9 + 4},
    };
    for (size_t i = 0; i < 4; i++) {
        uint8_t facelets[BT_SKEWB_FACELET_COUNT];
        after_text(&skewb, names[i], facelets);
        for (size_t k = 0; k < 3; k++) {
            BT_CHECK_EQ(facelets[moved_positions[i][k]], moved_values[i][k]);
        }
        for (size_t k = 0; k < 3; k++) {
            BT_CHECK_EQ(facelets[frozen_positions[i][k]], (uint8_t)(frozen_positions[i][k] / 9));
        }
    }
}

BT_TEST(skewb_oracle, crosses_travel_with_their_centers) {
    bt_skewb skewb = bt_skewb_new();
    uint8_t facelets[BT_SKEWB_FACELET_COUNT];
    after_text(&skewb, "F", facelets);
    const size_t cells[5] = {1, 3, 4, 5, 7};
    for (size_t i = 0; i < 5; i++) {
        BT_CHECK_EQ(facelets[2 * 9 + cells[i]], 0);
        BT_CHECK_EQ(facelets[1 * 9 + cells[i]], 2);
        BT_CHECK_EQ(facelets[0 * 9 + cells[i]], 1);
    }
}

BT_TEST(skewb_oracle, corner_stickers_match_derived_geometry) {
    bt_skewb skewb = bt_skewb_new();
    const char *names[4] = {"F", "L", "R", "B"};
    const size_t pin_counts[4] = {12, 11, 12, 12};
    const struct {
        size_t position;
        uint8_t value;
    } pins[4][12] = {
        {{2 * 9 + 0, 0},
         {2 * 9 + 2, 0},
         {2 * 9 + 6, 0},
         {1 * 9 + 0, 2},
         {1 * 9 + 2, 2},
         {1 * 9 + 8, 2},
         {0 * 9 + 2, 1},
         {0 * 9 + 6, 1},
         {0 * 9 + 8, 1},
         {3 * 9 + 2, 5},
         {4 * 9 + 0, 3},
         {5 * 9 + 2, 4}},
        {{1 * 9 + 0, 0},
         {1 * 9 + 6, 0},
         {3 * 9 + 0, 1},
         {3 * 9 + 2, 1},
         {3 * 9 + 8, 1},
         {0 * 9 + 0, 3},
         {0 * 9 + 6, 3},
         {0 * 9 + 8, 3},
         {5 * 9 + 0, 2},
         {4 * 9 + 2, 5},
         {2 * 9 + 0, 4},
         {0, 0}},
        {{4 * 9 + 0, 0},
         {4 * 9 + 2, 0},
         {4 * 9 + 6, 0},
         {2 * 9 + 0, 4},
         {2 * 9 + 2, 4},
         {2 * 9 + 8, 4},
         {0 * 9 + 0, 2},
         {0 * 9 + 2, 2},
         {0 * 9 + 8, 2},
         {3 * 9 + 0, 1},
         {5 * 9 + 8, 3},
         {1 * 9 + 2, 5}},
        {{2 * 9 + 0, 1},
         {2 * 9 + 6, 1},
         {2 * 9 + 8, 1},
         {5 * 9 + 0, 2},
         {5 * 9 + 2, 2},
         {5 * 9 + 8, 2},
         {1 * 9 + 2, 5},
         {1 * 9 + 6, 5},
         {1 * 9 + 8, 5},
         {4 * 9 + 6, 0},
         {3 * 9 + 8, 4},
         {0 * 9 + 8, 3}},
    };
    for (size_t i = 0; i < 4; i++) {
        uint8_t facelets[BT_SKEWB_FACELET_COUNT];
        after_text(&skewb, names[i], facelets);
        for (size_t k = 0; k < pin_counts[i]; k++) {
            BT_CHECK_EQ(facelets[pins[i][k].position], pins[i][k].value);
        }
    }
}

BT_TEST(skewb_oracle, every_move_has_order_three_and_counter_doubles_clock) {
    bt_skewb skewb = bt_skewb_new();
    uint8_t solved[BT_SKEWB_FACELET_COUNT];
    bt_skewb_state solved_state;
    bt_skewb_solved_state(&skewb, &solved_state);
    memcpy(solved, solved_state.facelets, BT_SKEWB_FACELET_COUNT);
    for (size_t vertex = 0; vertex < 4; vertex++) {
        char thrice[16];
        char twice[16];
        char prime[8];
        snprintf(prime, sizeof prime, "%s'", BT_SKEWB_VERTEX_NAMES[vertex]);
        snprintf(twice, sizeof twice, "%s %s", BT_SKEWB_VERTEX_NAMES[vertex],
                 BT_SKEWB_VERTEX_NAMES[vertex]);
        snprintf(thrice, sizeof thrice, "%s %s", twice, BT_SKEWB_VERTEX_NAMES[vertex]);
        uint8_t after3[BT_SKEWB_FACELET_COUNT];
        uint8_t after2[BT_SKEWB_FACELET_COUNT];
        uint8_t counter[BT_SKEWB_FACELET_COUNT];
        after_text(&skewb, thrice, after3);
        after_text(&skewb, twice, after2);
        after_text(&skewb, prime, counter);
        BT_CHECK(memcmp(after3, solved, BT_SKEWB_FACELET_COUNT) == 0);
        BT_CHECK(memcmp(counter, after2, BT_SKEWB_FACELET_COUNT) == 0);
    }
}

BT_TEST(skewb_oracle, grammar_accepts_exactly_the_sarah_alphabet) {
    bt_skewb skewb = bt_skewb_new();
    for (uint8_t vertex = 0; vertex < 4; vertex++) {
        bt_skewb_move move;
        BT_CHECK(bt_skewb_parse_move(BT_SKEWB_VERTEX_NAMES[vertex],
                                     strlen(BT_SKEWB_VERTEX_NAMES[vertex]), &move, NULL));
        BT_CHECK(bt_skewb_move_eq(move, (bt_skewb_move){vertex, 1}));
        char prime[8];
        snprintf(prime, sizeof prime, "%s'", BT_SKEWB_VERTEX_NAMES[vertex]);
        BT_CHECK(bt_skewb_parse_move(prime, strlen(prime), &move, NULL));
        BT_CHECK(bt_skewb_move_eq(move, (bt_skewb_move){vertex, 2}));
    }
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    if (!bt_skewb_parse_alg(&skewb, "F' L B' R", strlen("F' L B' R"), alg, &err)) {
        BT_ABORT("parse failed: %s", err.token);
    }
    bt_strbuf text;
    bt_strbuf_init(&text, 32);
    bt_skewb_format_alg(&skewb, alg, &text);
    BT_CHECK_EQ(text.data, "F' L B' R");
    void *inverse = bt_pz_alg_create();
    bt_skewb_invert_alg(&skewb, alg, inverse);
    bt_strbuf inverted;
    bt_strbuf_init(&inverted, 32);
    bt_skewb_format_alg(&skewb, inverse, &inverted);
    BT_CHECK_EQ(inverted.data, "R' B L' F");
    bt_pz_engine eng = bt_skewb_engine(&skewb);
    void *reparsed = bt_pz_alg_create();
    if (!bt_skewb_parse_alg(&skewb, text.data, text.len, reparsed, &err)) {
        BT_ABORT("reparse failed: %s", err.token);
    }
    BT_CHECK_EQ(bt_pz_alg_len(reparsed), bt_pz_alg_len(alg));
    for (size_t i = 0; i < bt_pz_alg_len(alg); i++) {
        bt_skewb_move a = *(const bt_skewb_move *)bt_pz_alg_at(&eng, alg, i);
        bt_skewb_move b = *(const bt_skewb_move *)bt_pz_alg_at(&eng, reparsed, i);
        BT_CHECK(bt_skewb_move_eq(a, b));
    }
    bt_pz_alg_destroy(reparsed);
    bt_strbuf_free(&inverted);
    bt_pz_alg_destroy(inverse);
    bt_strbuf_free(&text);
    bt_pz_alg_destroy(alg);
}

BT_TEST(skewb_oracle, rejects_garbage_and_cube_face_tokens) {
    const char *tokens[] = {
        "U", "U'", "D", "D'",  "F2", "F2'", "f", "l'", "u",   "d", "x", "y",   "z",  "M",
        "E", "S",  "w", "wut", "",   "'",   "2", "FF", "F''", "0", "-", "F F", "F ",
    };
    for (size_t i = 0; i < sizeof tokens / sizeof tokens[0]; i++) {
        bt_skewb_move move;
        BT_CHECK(!bt_skewb_parse_move(tokens[i], strlen(tokens[i]), &move, NULL));
    }
    bt_skewb skewb = bt_skewb_new();
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    BT_CHECK(!bt_skewb_parse_alg(&skewb, "F U", strlen("F U"), alg, &err));
    BT_CHECK_EQ(err.kind, (int)BT_PZ_ERR_PARSE);
    BT_CHECK_EQ(err.token, "U");
    bt_pz_alg_destroy(alg);
}

static bool is_rotation(const char *token) {
    if (token[0] != 'x' && token[0] != 'y' && token[0] != 'z') {
        return false;
    }
    for (const char *scan = token + 1; *scan != '\0'; scan++) {
        if (*scan != '\'' && *scan != '2') {
            return false;
        }
    }
    return true;
}

static void append_token(char *out, size_t out_size, size_t *at, const char *token) {
    size_t len = strlen(token);
    if (*at > 0 && *at < out_size) {
        out[*at] = ' ';
        (*at)++;
    }
    if (*at + len < out_size) {
        memcpy(out + *at, token, len);
        *at += len;
        out[*at] = '\0';
    }
}

static char *split_space(char *str, char **save) {
    char *start = str != NULL ? str : *save;
    if (start == NULL) {
        return NULL;
    }
    while (*start == ' ') {
        start++;
    }
    if (*start == '\0') {
        *save = start;
        return NULL;
    }
    char *end = start;
    while (*end != '\0' && *end != ' ') {
        end++;
    }
    if (*end != '\0') {
        *end = '\0';
        end++;
    }
    *save = end;
    return start;
}

static void normalize(const char *alg, char *out, size_t out_size) {
    char core[512];
    const char *arrow = strstr(alg, "->");
    size_t core_len = arrow == NULL ? strlen(alg) : (size_t)(arrow - alg);
    if (core_len >= sizeof core) {
        core_len = sizeof core - 1;
    }
    memcpy(core, alg, core_len);
    core[core_len] = '\0';
    out[0] = '\0';
    size_t at = 0;
    char plain[64] = "";
    char group[256] = "";
    char suffix[16] = "";
    size_t plain_at = 0;
    size_t group_at = 0;
    size_t suffix_at = 0;
    size_t depth = 0;
    bool pending = false;
    for (size_t index = 0; index <= core_len; index++) {
        char ch = index == core_len ? ' ' : core[index];
        if (pending) {
            if (ch >= '0' && ch <= '9') {
                if (suffix_at + 1 < sizeof suffix) {
                    suffix[suffix_at] = ch;
                    suffix_at++;
                    suffix[suffix_at] = '\0';
                }
                continue;
            }
            size_t repeats = 1;
            if (suffix_at > 0) {
                repeats = (size_t)strtoull(suffix, NULL, 10);
                if (repeats < 1) {
                    repeats = 1;
                }
            }
            char *save = NULL;
            char tokens[32][32];
            size_t token_count = 0;
            for (char *token = split_space(group, &save); token != NULL;
                 token = split_space(NULL, &save)) {
                if (token_count < 32) {
                    snprintf(tokens[token_count], 32, "%s", token);
                    token_count++;
                }
            }
            bool all_rotations = true;
            for (size_t i = 0; i < token_count; i++) {
                if (!is_rotation(tokens[i])) {
                    all_rotations = false;
                }
            }
            if (!all_rotations) {
                for (size_t repeat = 0; repeat < repeats; repeat++) {
                    for (size_t i = 0; i < token_count; i++) {
                        append_token(out, out_size, &at, tokens[i]);
                    }
                }
            }
            suffix_at = 0;
            suffix[0] = '\0';
            pending = false;
            if (ch == '(') {
                depth = 1;
                group_at = 0;
                group[0] = '\0';
            } else if (ch != ' ') {
                if (plain_at + 1 < sizeof plain) {
                    plain[plain_at] = ch;
                    plain_at++;
                    plain[plain_at] = '\0';
                }
            }
        } else if (depth == 0) {
            if (ch == '(') {
                depth = 1;
                group_at = 0;
                group[0] = '\0';
            } else if (ch == ' ') {
                if (plain_at > 0) {
                    append_token(out, out_size, &at, plain);
                    plain_at = 0;
                    plain[0] = '\0';
                }
            } else {
                if (plain_at + 1 < sizeof plain) {
                    plain[plain_at] = ch;
                    plain_at++;
                    plain[plain_at] = '\0';
                }
            }
        } else {
            if (ch == '(') {
                depth++;
                if (group_at + 1 < sizeof group) {
                    group[group_at] = ch;
                    group_at++;
                    group[group_at] = '\0';
                }
            } else if (ch == ')') {
                depth--;
                if (depth == 0) {
                    pending = true;
                } else if (group_at + 1 < sizeof group) {
                    group[group_at] = ch;
                    group_at++;
                    group[group_at] = '\0';
                }
            } else if (group_at + 1 < sizeof group) {
                group[group_at] = ch;
                group_at++;
                group[group_at] = '\0';
            }
        }
    }
}

BT_TEST(skewb_oracle, rotation_groups_arrow_chains_and_group_repeats_strip_to_move_streams) {
    char out[512];
    normalize("L F' L'", out, sizeof out);
    BT_CHECK_EQ(out, "L F' L'");
    normalize("(y2) L F' L' F", out, sizeof out);
    BT_CHECK_EQ(out, "L F' L' F");
    normalize("(y) R' F R F'", out, sizeof out);
    BT_CHECK_EQ(out, "R' F R F'");
    normalize("(y2) (F' L F L')2", out, sizeof out);
    BT_CHECK_EQ(out, "F' L F L' F' L F L'");
    normalize("R' F R F' -> cat. 1", out, sizeof out);
    BT_CHECK_EQ(out, "R' F R F'");
    normalize("L F' L' F -> cat. 1", out, sizeof out);
    BT_CHECK_EQ(out, "L F' L' F");
    normalize("R' F R F' (y2 x') R' F R F'", out, sizeof out);
    BT_CHECK_EQ(out, "R' F R F' R' F R F'");
    normalize("R' F R F' (y2) R' F R F' (y2 x') R' F R F' (y2) R' F R F'", out, sizeof out);
    BT_CHECK_EQ(out, "R' F R F' R' F R F' R' F R F' R' F R F'");
    normalize("R' F R F' (y2) R' F R F' (z2 x') R' F R F' (y2) R' F R F'", out, sizeof out);
    BT_CHECK_EQ(out, "R' F R F' R' F R F' R' F R F' R' F R F'");
}

static char *read_pack(void) {
    const char *root = getenv("BT_REF_GROUND");
    if (root == NULL || root[0] == '\0') {
        root = "ref_ground";
    }
    char path[512];
    snprintf(path, sizeof path, "%s/packs/skewb_sarah.tsv", root);
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        BT_ABORT("pack unreadable: %s", path);
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = malloc((size_t)size + 1);
    if (text == NULL || fread(text, 1, (size_t)size, file) != (size_t)size) {
        BT_ABORT("pack unreadable: %s", path);
    }
    text[size] = '\0';
    fclose(file);
    return text;
}

BT_TEST(skewb_oracle, pack_rows_pass_tier1_after_stripping) {
    bt_skewb skewb = bt_skewb_new();
    char *text = read_pack();
    size_t rows = 0;
    char unique[64][16];
    size_t unique_count = 0;
    char *cursor = text;
    bool first = true;
    while (*cursor != '\0') {
        char *line_end = strchr(cursor, '\n');
        if (line_end == NULL) {
            line_end = cursor + strlen(cursor);
        }
        char *line = cursor;
        bool last = *line_end == '\0';
        if (line_end > line && line_end[-1] == '\r') {
            line_end[-1] = '\0';
        }
        *line_end = '\0';
        cursor = last ? line_end : line_end + 1;
        if (line[0] == '\0') {
            continue;
        }
        if (first) {
            first = false;
            continue;
        }
        char *fields[8];
        size_t field_count = 0;
        char *field = line;
        for (char *scan = line;; scan++) {
            if (*scan == '\t' || *scan == '\0') {
                if (field_count < 8) {
                    fields[field_count] = field;
                }
                field_count++;
                bool done = *scan == '\0';
                *scan = '\0';
                if (done) {
                    break;
                }
                field = scan + 1;
            }
        }
        char alg[512];
        normalize(fields[1], alg, sizeof alg);
        if (!tier1_ok(&skewb, alg)) {
            BT_ABORT("tier1 failed for row %s", fields[0]);
        }
        char walker[512];
        snprintf(walker, sizeof walker, "%s", alg);
        char *save = NULL;
        for (char *token = split_space(walker, &save); token != NULL;
             token = split_space(NULL, &save)) {
            bool known = false;
            for (size_t i = 0; i < unique_count; i++) {
                if (strcmp(unique[i], token) == 0) {
                    known = true;
                }
            }
            if (!known && unique_count < 64) {
                snprintf(unique[unique_count], 16, "%s", token);
                unique_count++;
            }
        }
        rows++;
    }
    BT_CHECK_EQ((long long)rows, 15);
    BT_CHECK_EQ((long long)unique_count, 6);
    const char *pointers[64];
    for (size_t i = 0; i < unique_count; i++) {
        pointers[i] = unique[i];
    }
    qsort(pointers, unique_count, sizeof pointers[0], cmp_string_p);
    const char *want[6] = {"F", "F'", "L", "L'", "R", "R'"};
    for (size_t i = 0; i < 6; i++) {
        BT_CHECK_EQ(pointers[i], want[i]);
    }
    free(text);
}

static uint64_t xorshift(uint64_t *seed) {
    uint64_t x = *seed;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *seed = x;
    return x;
}

BT_TEST(skewb_oracle, inverse_round_trips_through_solved) {
    bt_skewb skewb = bt_skewb_new();
    bt_pz_engine eng = bt_skewb_engine(&skewb);
    bt_rng rng;
    bt_rng_seed(&rng, 20261012);
    bt_pz_error err;
    for (size_t iteration = 0; iteration < 256; iteration++) {
        size_t length = (size_t)bt_rng_below(&rng, 24);
        void *alg = bt_pz_alg_create();
        for (size_t i = 0; i < length; i++) {
            bt_skewb_move move = {(uint8_t)bt_rng_below(&rng, 4),
                                  (uint8_t)(bt_rng_below(&rng, 2) + 1)};
            bt_pz_alg_push(&eng, alg, &move);
        }
        void *inverse = bt_pz_alg_create();
        bt_skewb_invert_alg(&skewb, alg, inverse);
        bt_skewb_state forward;
        bt_skewb_state reverse;
        bt_skewb_solved_state(&skewb, &forward);
        bt_skewb_solved_state(&skewb, &reverse);
        if (!bt_skewb_apply_alg(&skewb, &forward, alg, &err) ||
            !bt_skewb_apply_alg(&skewb, &forward, inverse, &err) ||
            !bt_skewb_is_solved(&skewb, &forward)) {
            BT_ABORT("forward round trip failed");
        }
        if (!bt_skewb_apply_alg(&skewb, &reverse, inverse, &err) ||
            !bt_skewb_apply_alg(&skewb, &reverse, alg, &err) ||
            !bt_skewb_is_solved(&skewb, &reverse)) {
            BT_ABORT("reverse round trip failed");
        }
        bt_strbuf text;
        bt_strbuf_init(&text, 256);
        bt_skewb_format_alg(&skewb, alg, &text);
        void *reparsed = bt_pz_alg_create();
        if (!bt_skewb_parse_alg(&skewb, text.data, text.len, reparsed, &err) ||
            bt_pz_alg_len(reparsed) != bt_pz_alg_len(alg)) {
            BT_ABORT("format round trip failed for %s", text.data);
        }
        for (size_t i = 0; i < bt_pz_alg_len(alg); i++) {
            bt_skewb_move a = *(const bt_skewb_move *)bt_pz_alg_at(&eng, alg, i);
            bt_skewb_move b = *(const bt_skewb_move *)bt_pz_alg_at(&eng, reparsed, i);
            if (!bt_skewb_move_eq(a, b)) {
                BT_ABORT("format round trip differs for %s", text.data);
            }
        }
        bt_strbuf inverse_text;
        bt_strbuf_init(&inverse_text, 256);
        bt_skewb_format_alg(&skewb, inverse, &inverse_text);
        void *reparsed_inverse = bt_pz_alg_create();
        if (!bt_skewb_parse_alg(&skewb, inverse_text.data, inverse_text.len, reparsed_inverse,
                                &err) ||
            bt_pz_alg_len(reparsed_inverse) != bt_pz_alg_len(inverse)) {
            BT_ABORT("inverse format round trip failed for %s", inverse_text.data);
        }
        void *twice = bt_pz_alg_create();
        bt_skewb_invert_alg(&skewb, inverse, twice);
        if (bt_pz_alg_len(twice) != bt_pz_alg_len(alg)) {
            BT_ABORT("double inverse length differs");
        }
        for (size_t i = 0; i < bt_pz_alg_len(alg); i++) {
            bt_skewb_move a = *(const bt_skewb_move *)bt_pz_alg_at(&eng, alg, i);
            bt_skewb_move b = *(const bt_skewb_move *)bt_pz_alg_at(&eng, twice, i);
            if (!bt_skewb_move_eq(a, b)) {
                BT_ABORT("double inverse differs");
            }
        }
        bt_pz_alg_destroy(twice);
        bt_pz_alg_destroy(reparsed_inverse);
        bt_strbuf_free(&inverse_text);
        bt_pz_alg_destroy(reparsed);
        bt_strbuf_free(&text);
        bt_pz_alg_destroy(inverse);
        bt_pz_alg_destroy(alg);
    }
}

BT_TEST(skewb_oracle, quarter_one_direction_is_pinned_per_vertex) {
    bt_skewb skewb = bt_skewb_new();
    const char *names[4] = {"F", "L", "R", "B"};
    const size_t positions[4] = {2, 0, 0, 8};
    const uint8_t values[4] = {1, 3, 2, 3};
    for (size_t index = 0; index < 4; index++) {
        bt_skewb_state state;
        applied(&skewb, names[index], &state);
        BT_CHECK_EQ(state.facelets[positions[index]], values[index]);
        uint8_t table[BT_SKEWB_FACELET_COUNT];
        bt_skewb_turn_table(&skewb, (uint8_t)index, 1, table);
        size_t source = BT_SKEWB_FACELET_COUNT;
        for (size_t i = 0; i < BT_SKEWB_FACELET_COUNT; i++) {
            if (table[i] == (uint8_t)positions[index]) {
                source = i;
            }
        }
        BT_CHECK(source < BT_SKEWB_FACELET_COUNT);
        BT_CHECK_EQ(source / 9, (size_t)values[index]);
    }
}

BT_TEST(skewb_oracle, apply_rejects_hand_built_illegal_quarters) {
    bt_skewb skewb = bt_skewb_new();
    bt_pz_engine eng = bt_skewb_engine(&skewb);
    const uint8_t quarters[2] = {0, 3};
    for (size_t i = 0; i < 2; i++) {
        void *alg = bt_pz_alg_create();
        bt_skewb_move move = {0, quarters[i]};
        bt_pz_alg_push(&eng, alg, &move);
        bt_skewb_state state;
        bt_skewb_solved_state(&skewb, &state);
        bt_pz_error err;
        BT_CHECK(!bt_skewb_apply_alg(&skewb, &state, alg, &err));
        BT_CHECK_EQ(err.kind, (int)BT_PZ_ERR_APPLY);
        bt_strbuf message;
        bt_strbuf_init(&message, 32);
        bt_pz_error_message(&err, &message);
        BT_CHECK(strstr(message.data, "illegal") != NULL);
        bt_strbuf_free(&message);
        bt_pz_alg_destroy(alg);
    }
}

BT_TEST(skewb_inline, solved_state_is_solved) {
    bt_skewb skewb = bt_skewb_new();
    bt_skewb_state state;
    bt_skewb_solved_state(&skewb, &state);
    BT_CHECK(bt_skewb_is_solved(&skewb, &state));
}

BT_TEST(skewb_inline, every_single_turn_breaks_and_reverts) {
    bt_skewb skewb = bt_skewb_new();
    bt_pz_engine eng = bt_skewb_engine(&skewb);
    bt_pz_error err;
    for (uint8_t vertex = 0; vertex < 4; vertex++) {
        for (uint8_t quarter = 1; quarter <= 2; quarter++) {
            void *alg = bt_pz_alg_create();
            bt_skewb_move move = {vertex, quarter};
            bt_pz_alg_push(&eng, alg, &move);
            bt_skewb_state state;
            bt_skewb_solved_state(&skewb, &state);
            if (!bt_skewb_apply_alg(&skewb, &state, alg, &err)) {
                BT_ABORT("apply failed: %s", err.token);
            }
            BT_CHECK(!bt_skewb_is_solved(&skewb, &state));
            void *inverse = bt_pz_alg_create();
            bt_skewb_invert_alg(&skewb, alg, inverse);
            if (!bt_skewb_apply_alg(&skewb, &state, inverse, &err)) {
                BT_ABORT("apply inverse failed: %s", err.token);
            }
            BT_CHECK(bt_skewb_is_solved(&skewb, &state));
            bt_pz_alg_destroy(inverse);
            bt_pz_alg_destroy(alg);
        }
    }
}

BT_TEST(skewb_inline, move_tokens_round_trip_through_format) {
    bt_skewb skewb = bt_skewb_new();
    bt_pz_engine eng = bt_skewb_engine(&skewb);
    for (uint8_t vertex = 0; vertex < 4; vertex++) {
        for (uint8_t quarter = 1; quarter <= 2; quarter++) {
            void *alg = bt_pz_alg_create();
            bt_skewb_move move = {vertex, quarter};
            bt_pz_alg_push(&eng, alg, &move);
            bt_strbuf text;
            bt_strbuf_init(&text, 8);
            bt_skewb_format_alg(&skewb, alg, &text);
            bt_skewb_move parsed;
            BT_CHECK(bt_skewb_parse_move(text.data, text.len, &parsed, NULL));
            BT_CHECK(bt_skewb_move_eq(parsed, move));
            bt_strbuf_free(&text);
            bt_pz_alg_destroy(alg);
        }
    }
}

BT_TEST(skewb_inline, inverse_round_trips_a_deterministic_alg_sweep) {
    bt_skewb skewb = bt_skewb_new();
    bt_pz_engine eng = bt_skewb_engine(&skewb);
    uint64_t seed = 0x5eed1234abcdf00dULL;
    bt_pz_error err;
    for (size_t iteration = 0; iteration < 4096; iteration++) {
        size_t length = (size_t)(xorshift(&seed) % 24);
        void *alg = bt_pz_alg_create();
        for (size_t i = 0; i < length; i++) {
            bt_skewb_move move = {(uint8_t)(xorshift(&seed) % 4),
                                  (uint8_t)(xorshift(&seed) % 2 + 1)};
            bt_pz_alg_push(&eng, alg, &move);
        }
        void *inverse = bt_pz_alg_create();
        bt_skewb_invert_alg(&skewb, alg, inverse);
        bt_skewb_state forward;
        bt_skewb_state reverse;
        bt_skewb_solved_state(&skewb, &forward);
        bt_skewb_solved_state(&skewb, &reverse);
        if (!bt_skewb_apply_alg(&skewb, &forward, alg, &err) ||
            !bt_skewb_apply_alg(&skewb, &forward, inverse, &err) ||
            !bt_skewb_is_solved(&skewb, &forward)) {
            BT_ABORT("forward round trip failed");
        }
        if (!bt_skewb_apply_alg(&skewb, &reverse, inverse, &err) ||
            !bt_skewb_apply_alg(&skewb, &reverse, alg, &err) ||
            !bt_skewb_is_solved(&skewb, &reverse)) {
            BT_ABORT("reverse round trip failed");
        }
        bt_strbuf text;
        bt_strbuf_init(&text, 256);
        bt_skewb_format_alg(&skewb, alg, &text);
        void *reparsed = bt_pz_alg_create();
        if (!bt_skewb_parse_alg(&skewb, text.data, text.len, reparsed, &err) ||
            bt_pz_alg_len(reparsed) != bt_pz_alg_len(alg)) {
            BT_ABORT("format round trip failed for %s", text.data);
        }
        for (size_t i = 0; i < bt_pz_alg_len(alg); i++) {
            bt_skewb_move a = *(const bt_skewb_move *)bt_pz_alg_at(&eng, alg, i);
            bt_skewb_move b = *(const bt_skewb_move *)bt_pz_alg_at(&eng, reparsed, i);
            if (!bt_skewb_move_eq(a, b)) {
                BT_ABORT("format round trip differs for %s", text.data);
            }
        }
        void *twice = bt_pz_alg_create();
        bt_skewb_invert_alg(&skewb, inverse, twice);
        if (bt_pz_alg_len(twice) != bt_pz_alg_len(alg)) {
            BT_ABORT("double inverse length differs");
        }
        if (!tier1_ok(&skewb, text.data)) {
            BT_ABORT("tier1 failed for %s", text.data);
        }
        bt_pz_alg_destroy(twice);
        bt_pz_alg_destroy(reparsed);
        bt_strbuf_free(&text);
        bt_pz_alg_destroy(inverse);
        bt_pz_alg_destroy(alg);
    }
}
