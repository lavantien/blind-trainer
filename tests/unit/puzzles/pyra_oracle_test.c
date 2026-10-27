#include "puzzles/pyra.h"
#include "puzzles/tier1.h"

#include "common.h"

#include "common/bt_harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const TOKENS[16] = {
    "U", "U'", "L", "L'", "R", "R'", "B", "B'", "u", "u'", "l", "l'", "r", "r'", "b", "b'",
};

static const uint8_t U_AFTER[BT_PYRA_FACELET_COUNT] = {
    2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3,
};

static const uint8_t L_AFTER[BT_PYRA_FACELET_COUNT] = {
    0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 3, 1, 1, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 0, 3, 3, 0, 0, 0,
};

static const uint8_t R_AFTER[BT_PYRA_FACELET_COUNT] = {
    0, 0, 0, 2, 0, 0, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 3, 2, 2, 3, 3, 3, 2, 2, 3, 0, 3, 3, 0, 0, 0, 3, 3,
};

static const uint8_t B_AFTER[BT_PYRA_FACELET_COUNT] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 1, 2, 2, 2, 1, 1,
    2, 2, 2, 3, 2, 2, 3, 3, 3, 1, 1, 1, 1, 3, 3, 3, 3, 3,
};

static bool tier1_ok(const char *text) {
    bt_pz_engine eng = bt_pyra_engine();
    bt_tier1_error err;
    return bt_verify_tier1(&eng, text, &err) == BT_TIER1_OK;
}

static void applied(const char *text, bt_pyra_state *out) {
    void *alg = bt_pz_alg_create();
    bt_pz_error err;
    if (!bt_pyra_parse_alg(text, strlen(text), alg, &err)) {
        BT_ABORT("parse %s failed: %s", text, err.token);
    }
    bt_pyra_solved_state(out);
    if (!bt_pyra_apply_alg(out, alg, &err)) {
        BT_ABORT("apply %s failed: %s", text, err.token);
    }
    bt_pz_alg_destroy(alg);
}

static void colors_of(const char *text, uint8_t out[BT_PYRA_FACELET_COUNT]) {
    bt_pyra_state state;
    applied(text, &state);
    bt_pz_grid grid;
    bt_pyra_facelets(&state, &grid);
    size_t at = 0;
    for (size_t face = 0; face < grid.face_count; face++) {
        for (size_t row = 0; row < grid.faces[face].row_count; row++) {
            for (size_t col = 0; col < grid.faces[face].rows[row].cols; col++) {
                if (at < BT_PYRA_FACELET_COUNT) {
                    out[at] = grid.faces[face].rows[row].cells[col];
                    at++;
                }
            }
        }
    }
    BT_CHECK(at == BT_PYRA_FACELET_COUNT);
}

static void solved_colors(uint8_t out[BT_PYRA_FACELET_COUNT]) {
    colors_of("", out);
}

static size_t changed_positions(const char *text, size_t out[BT_PYRA_FACELET_COUNT]) {
    uint8_t after[BT_PYRA_FACELET_COUNT];
    uint8_t base[BT_PYRA_FACELET_COUNT];
    colors_of(text, after);
    solved_colors(base);
    size_t count = 0;
    for (size_t i = 0; i < BT_PYRA_FACELET_COUNT; i++) {
        if (after[i] != base[i]) {
            out[count] = i;
            count++;
        }
    }
    return count;
}

static size_t band_slots(size_t face, const size_t *locals, size_t local_count, size_t *out) {
    for (size_t i = 0; i < local_count; i++) {
        out[i] = face * 9 + locals[i];
    }
    return local_count;
}

static int cmp_size(const void *a, const void *b) {
    size_t left = *(const size_t *)a;
    size_t right = *(const size_t *)b;
    if (left < right) {
        return -1;
    }
    return left > right ? 1 : 0;
}

BT_TEST(pyra_oracle, solved_grid_shape_and_colors) {
    bt_pyra_state state;
    bt_pyra_solved_state(&state);
    bt_pz_grid grid;
    bt_pyra_facelets(&state, &grid);
    BT_CHECK_EQ((long long)grid.face_count, 4);
    for (size_t face = 0; face < grid.face_count; face++) {
        size_t widths[3];
        for (size_t row = 0; row < grid.faces[face].row_count && row < 3; row++) {
            widths[row] = grid.faces[face].rows[row].cols;
        }
        BT_CHECK_EQ((long long)grid.faces[face].row_count, 3);
        BT_CHECK(widths[0] == 1 && widths[1] == 3 && widths[2] == 5);
        for (size_t row = 0; row < grid.faces[face].row_count; row++) {
            for (size_t col = 0; col < grid.faces[face].rows[row].cols; col++) {
                BT_CHECK_EQ(grid.faces[face].rows[row].cells[col], (uint8_t)face);
            }
        }
    }
    BT_CHECK(bt_pyra_is_solved(&state));
}

BT_TEST(pyra_oracle, vertex_turns_pin_derived_facelets) {
    const char *texts[4] = {"U", "L", "R", "B"};
    const uint8_t *expected[4] = {U_AFTER, L_AFTER, R_AFTER, B_AFTER};
    for (size_t i = 0; i < 4; i++) {
        uint8_t after[BT_PYRA_FACELET_COUNT];
        colors_of(texts[i], after);
        BT_CHECK(memcmp(after, expected[i], BT_PYRA_FACELET_COUNT) == 0);
    }
}

BT_TEST(pyra_oracle, counter_turns_equal_double_clockwise) {
    const char *pairs[8][2] = {
        {"U", "U'"}, {"L", "L'"}, {"R", "R'"}, {"B", "B'"},
        {"u", "u'"}, {"l", "l'"}, {"r", "r'"}, {"b", "b'"},
    };
    for (size_t i = 0; i < 8; i++) {
        char doubled[8];
        snprintf(doubled, sizeof doubled, "%s %s", pairs[i][0], pairs[i][0]);
        uint8_t counter[BT_PYRA_FACELET_COUNT];
        uint8_t twice[BT_PYRA_FACELET_COUNT];
        colors_of(pairs[i][1], counter);
        colors_of(doubled, twice);
        BT_CHECK(memcmp(counter, twice, BT_PYRA_FACELET_COUNT) == 0);
    }
}

BT_TEST(pyra_oracle, every_move_has_order_three) {
    for (size_t i = 0; i < 16; i++) {
        char thrice[16];
        snprintf(thrice, sizeof thrice, "%s %s %s", TOKENS[i], TOKENS[i], TOKENS[i]);
        uint8_t after[BT_PYRA_FACELET_COUNT];
        uint8_t base[BT_PYRA_FACELET_COUNT];
        colors_of(thrice, after);
        solved_colors(base);
        BT_CHECK(memcmp(after, base, BT_PYRA_FACELET_COUNT) == 0);
    }
}

BT_TEST(pyra_oracle, vertex_turns_move_exactly_two_layer_bands) {
    const size_t u0[4] = {0, 1, 2, 3};
    const size_t l0[4] = {1, 4, 5, 6};
    const size_t r0[4] = {3, 6, 7, 8};
    const char *texts[4] = {"U", "L", "R", "B"};
    size_t bands[4][12];
    size_t counts[4][3];
    counts[0][0] = band_slots(0, u0, 4, bands[0]);
    counts[0][1] = band_slots(1, u0, 4, bands[0] + 4);
    counts[0][2] = band_slots(2, u0, 4, bands[0] + 8);
    counts[1][0] = band_slots(0, l0, 4, bands[1]);
    counts[1][1] = band_slots(3, r0, 4, bands[1] + 4);
    counts[1][2] = band_slots(1, r0, 4, bands[1] + 8);
    counts[2][0] = band_slots(0, r0, 4, bands[2]);
    counts[2][1] = band_slots(3, l0, 4, bands[2] + 4);
    counts[2][2] = band_slots(2, l0, 4, bands[2] + 8);
    counts[3][0] = band_slots(3, u0, 4, bands[3]);
    counts[3][1] = band_slots(1, l0, 4, bands[3] + 4);
    counts[3][2] = band_slots(2, r0, 4, bands[3] + 8);
    for (size_t i = 0; i < 4; i++) {
        size_t expected[12];
        size_t total = 0;
        for (size_t j = 0; j < 3; j++) {
            for (size_t k = 0; k < counts[i][j]; k++) {
                expected[total] = bands[i][j * 4 + k];
                total++;
            }
        }
        qsort(expected, total, sizeof expected[0], cmp_size);
        size_t moved[BT_PYRA_FACELET_COUNT];
        size_t moved_count = changed_positions(texts[i], moved);
        BT_CHECK(moved_count == total);
        for (size_t k = 0; k < total && k < moved_count; k++) {
            BT_CHECK(moved[k] == expected[k]);
        }
    }
}

BT_TEST(pyra_oracle, tip_turns_touch_only_tip_stickers) {
    const char *texts[4] = {"u", "l", "r", "b"};
    const size_t tips[4][3] = {{0, 9, 18}, {4, 35, 17}, {8, 31, 22}, {27, 26, 13}};
    for (size_t i = 0; i < 4; i++) {
        size_t moved[BT_PYRA_FACELET_COUNT];
        size_t moved_count = changed_positions(texts[i], moved);
        BT_CHECK(moved_count == 3);
        size_t expected[3];
        for (size_t k = 0; k < 3; k++) {
            expected[k] = tips[i][k];
        }
        qsort(expected, 3, sizeof expected[0], cmp_size);
        for (size_t k = 0; k < 3; k++) {
            BT_CHECK(moved[k] == expected[k]);
        }
    }
}

BT_TEST(pyra_oracle, tip_turns_pin_derived_facelets) {
    const char *texts[4] = {"u", "l", "r", "b"};
    const size_t tips[4][3] = {{0, 9, 18}, {4, 35, 17}, {8, 31, 22}, {27, 26, 13}};
    const uint8_t values[4][3] = {
        {2, 0, 1},
        {1, 0, 3},
        {2, 0, 3},
        {1, 3, 2},
    };
    for (size_t i = 0; i < 4; i++) {
        uint8_t after[BT_PYRA_FACELET_COUNT];
        colors_of(texts[i], after);
        for (size_t k = 0; k < 3; k++) {
            BT_CHECK_EQ(after[tips[i][k]], values[i][k]);
        }
    }
}

BT_TEST(pyra_oracle, vertex_turn_carries_its_tip_along) {
    const char *twins[4] = {"U", "L", "R", "B"};
    const char *tips[4] = {"u", "l", "r", "b"};
    const size_t corners[4][3] = {{0, 9, 18}, {4, 35, 17}, {8, 31, 22}, {27, 26, 13}};
    for (size_t i = 0; i < 4; i++) {
        uint8_t wide[BT_PYRA_FACELET_COUNT];
        uint8_t narrow[BT_PYRA_FACELET_COUNT];
        colors_of(twins[i], wide);
        colors_of(tips[i], narrow);
        for (size_t k = 0; k < 3; k++) {
            BT_CHECK_EQ(wide[corners[i][k]], narrow[corners[i][k]]);
        }
    }
}

BT_TEST(pyra_oracle, single_moves_break_and_inverse_restores_solved) {
    for (size_t i = 0; i < 16; i++) {
        bt_pyra_state state;
        applied(TOKENS[i], &state);
        BT_CHECK(!bt_pyra_is_solved(&state));
        void *alg = bt_pz_alg_create();
        void *inverse = bt_pz_alg_create();
        bt_pz_error err;
        if (!bt_pyra_parse_alg(TOKENS[i], strlen(TOKENS[i]), alg, &err)) {
            BT_ABORT("parse %s failed: %s", TOKENS[i], err.token);
        }
        bt_pyra_invert_alg(alg, inverse);
        if (!bt_pyra_apply_alg(&state, inverse, &err)) {
            BT_ABORT("apply inverse failed: %s", err.token);
        }
        BT_CHECK(bt_pyra_is_solved(&state));
        bt_pz_alg_destroy(inverse);
        bt_pz_alg_destroy(alg);
    }
}

BT_TEST(pyra_oracle, parses_every_12e_token) {
    const bt_pyra_move expected[16] = {
        {BT_PYRA_VERTEX_U, BT_PYRA_LAYERS_TWIN, BT_PYRA_SENSE_CLOCK},
        {BT_PYRA_VERTEX_U, BT_PYRA_LAYERS_TWIN, BT_PYRA_SENSE_COUNTER},
        {BT_PYRA_VERTEX_U, BT_PYRA_LAYERS_TIP, BT_PYRA_SENSE_CLOCK},
        {BT_PYRA_VERTEX_U, BT_PYRA_LAYERS_TIP, BT_PYRA_SENSE_COUNTER},
        {BT_PYRA_VERTEX_L, BT_PYRA_LAYERS_TWIN, BT_PYRA_SENSE_CLOCK},
        {BT_PYRA_VERTEX_L, BT_PYRA_LAYERS_TWIN, BT_PYRA_SENSE_COUNTER},
        {BT_PYRA_VERTEX_L, BT_PYRA_LAYERS_TIP, BT_PYRA_SENSE_CLOCK},
        {BT_PYRA_VERTEX_L, BT_PYRA_LAYERS_TIP, BT_PYRA_SENSE_COUNTER},
        {BT_PYRA_VERTEX_R, BT_PYRA_LAYERS_TWIN, BT_PYRA_SENSE_CLOCK},
        {BT_PYRA_VERTEX_R, BT_PYRA_LAYERS_TWIN, BT_PYRA_SENSE_COUNTER},
        {BT_PYRA_VERTEX_R, BT_PYRA_LAYERS_TIP, BT_PYRA_SENSE_CLOCK},
        {BT_PYRA_VERTEX_R, BT_PYRA_LAYERS_TIP, BT_PYRA_SENSE_COUNTER},
        {BT_PYRA_VERTEX_B, BT_PYRA_LAYERS_TWIN, BT_PYRA_SENSE_CLOCK},
        {BT_PYRA_VERTEX_B, BT_PYRA_LAYERS_TWIN, BT_PYRA_SENSE_COUNTER},
        {BT_PYRA_VERTEX_B, BT_PYRA_LAYERS_TIP, BT_PYRA_SENSE_CLOCK},
        {BT_PYRA_VERTEX_B, BT_PYRA_LAYERS_TIP, BT_PYRA_SENSE_COUNTER},
    };
    const char *tokens[16] = {
        "U", "U'", "u", "u'", "L", "L'", "l", "l'", "R", "R'", "r", "r'", "B", "B'", "b", "b'",
    };
    for (size_t i = 0; i < 16; i++) {
        bt_pyra_move move;
        BT_CHECK(bt_pyra_parse_move(tokens[i], strlen(tokens[i]), &move, NULL));
        BT_CHECK(bt_pyra_move_eq(move, expected[i]));
    }
}

BT_TEST(pyra_oracle, rejects_tokens_outside_12e) {
    const char *tokens[] = {
        "F",   "F'", "D", "d", "U2", "u2", "U2'",      "'U", "UU", "U''", "(", "(R",
        "R')", "x",  "y", "M", "e",  "s",  "\xc3\xbc", "U ", " U", "uB",  "0", "-",
    };
    for (size_t i = 0; i < sizeof tokens / sizeof tokens[0]; i++) {
        bt_pyra_move move;
        BT_CHECK(!bt_pyra_parse_move(tokens[i], strlen(tokens[i]), &move, NULL));
    }
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    BT_CHECK(!bt_pyra_parse_alg("(R U' R')", strlen("(R U' R')"), alg, &err));
    BT_CHECK(!bt_pyra_parse_alg("U 2", strlen("U 2"), alg, &err));
    BT_CHECK(!bt_pyra_parse_alg("R2", strlen("R2"), alg, &err));
    bt_pz_alg_destroy(alg);
}

BT_TEST(pyra_oracle, formats_and_inverts) {
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    if (!bt_pyra_parse_alg("U u' L", strlen("U u' L"), alg, &err)) {
        BT_ABORT("parse failed: %s", err.token);
    }
    bt_strbuf text;
    bt_strbuf_init(&text, 32);
    bt_pyra_format_alg(alg, &text);
    BT_CHECK_EQ(text.data, "U u' L");
    void *inverse = bt_pz_alg_create();
    bt_pyra_invert_alg(alg, inverse);
    bt_strbuf inverted;
    bt_strbuf_init(&inverted, 32);
    bt_pyra_format_alg(inverse, &inverted);
    BT_CHECK_EQ(inverted.data, "L' u U'");
    void *empty = bt_pz_alg_create();
    if (!bt_pyra_parse_alg("", 0, empty, &err)) {
        BT_ABORT("parse empty failed: %s", err.token);
    }
    bt_strbuf empty_text;
    bt_strbuf_init(&empty_text, 8);
    bt_pyra_format_alg(empty, &empty_text);
    BT_CHECK_EQ(empty_text.data, "");
    BT_CHECK_EQ(bt_pz_alg_len(empty), 0U);
    void *inverse_empty = bt_pz_alg_create();
    bt_pyra_invert_alg(empty, inverse_empty);
    BT_CHECK_EQ(bt_pz_alg_len(inverse_empty), 0U);
    void *reparsed = bt_pz_alg_create();
    if (!bt_pyra_parse_alg(text.data, text.len, reparsed, &err)) {
        BT_ABORT("reparse failed: %s", err.token);
    }
    bt_pz_engine eng = bt_pyra_engine();
    BT_CHECK_EQ(bt_pz_alg_len(reparsed), bt_pz_alg_len(alg));
    for (size_t i = 0; i < bt_pz_alg_len(alg); i++) {
        bt_pyra_move a = *(const bt_pyra_move *)bt_pz_alg_at(&eng, alg, i);
        bt_pyra_move b = *(const bt_pyra_move *)bt_pz_alg_at(&eng, reparsed, i);
        BT_CHECK(bt_pyra_move_eq(a, b));
    }
    bt_pz_alg_destroy(reparsed);
    bt_pz_alg_destroy(inverse_empty);
    bt_strbuf_free(&empty_text);
    bt_pz_alg_destroy(empty);
    bt_strbuf_free(&inverted);
    bt_pz_alg_destroy(inverse);
    bt_strbuf_free(&text);
    bt_pz_alg_destroy(alg);
}

BT_TEST(pyra_oracle, apply_always_succeeds) {
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    if (!bt_pyra_parse_alg("U' b L R' u", strlen("U' b L R' u"), alg, &err)) {
        BT_ABORT("parse failed: %s", err.token);
    }
    bt_pyra_state state;
    bt_pyra_solved_state(&state);
    BT_CHECK(bt_pyra_apply_alg(&state, alg, &err));
    bt_pz_alg_destroy(alg);
}

BT_TEST(pyra_oracle, apply_rejects_hand_built_out_of_range_fields) {
    bt_pz_engine eng = bt_pyra_engine();
    const bt_pyra_move illegal[4] = {
        {4, 0, 0},
        {255, 1, 1},
        {0, 2, 0},
        {1, 0, 2},
    };
    for (size_t i = 0; i < 4; i++) {
        void *alg = bt_pz_alg_create();
        bt_pz_alg_push(&eng, alg, &illegal[i]);
        bt_pyra_state state;
        bt_pyra_solved_state(&state);
        bt_pz_error err;
        BT_CHECK(!bt_pyra_apply_alg(&state, alg, &err));
        BT_CHECK_EQ(err.kind, (int)BT_PZ_ERR_APPLY);
        bt_strbuf message;
        bt_strbuf_init(&message, 64);
        bt_pz_error_message(&err, &message);
        BT_CHECK(strstr(message.data, "vertex 4") != NULL ||
                 strstr(message.data, "vertex 255") != NULL ||
                 strstr(message.data, "vertex 0") != NULL ||
                 strstr(message.data, "vertex 1") != NULL);
        bt_strbuf_free(&message);
        bt_pz_alg_destroy(alg);
    }
}

static uint64_t xorshift(uint64_t *seed) {
    uint64_t x = *seed;
    x ^= x << 13U;
    x ^= x >> 7U;
    x ^= x << 17U;
    *seed = x;
    return x;
}

BT_TEST(pyra_oracle, random_algs_round_trip) {
    uint64_t seed = 0x5eed1234abcdf00dULL;
    bt_pz_error err;
    bt_pz_engine eng = bt_pyra_engine();
    for (size_t iteration = 0; iteration < 1000; iteration++) {
        size_t len = (size_t)(xorshift(&seed) % 21);
        char text[512];
        size_t at = 0;
        for (size_t i = 0; i < len; i++) {
            size_t pick = (size_t)(xorshift(&seed) % 16);
            at += (size_t)snprintf(text + at, sizeof text - at, "%s ", TOKENS[pick]);
        }
        if (at > 0) {
            text[at - 1] = '\0';
        } else {
            text[0] = '\0';
        }
        if (!tier1_ok(text)) {
            BT_ABORT("tier1 failed for %s", text);
        }
        void *alg = bt_pz_alg_create();
        if (!bt_pyra_parse_alg(text, strlen(text), alg, &err)) {
            BT_ABORT("parse %s failed: %s", text, err.token);
        }
        bt_strbuf formatted;
        bt_strbuf_init(&formatted, 64);
        bt_pyra_format_alg(alg, &formatted);
        void *reparsed = bt_pz_alg_create();
        if (!bt_pyra_parse_alg(formatted.data, formatted.len, reparsed, &err)) {
            BT_ABORT("reparse %s failed: %s", formatted.data, err.token);
        }
        if (bt_pz_alg_len(reparsed) != bt_pz_alg_len(alg)) {
            BT_ABORT("format round trip length differs for %s", text);
        }
        for (size_t i = 0; i < bt_pz_alg_len(alg); i++) {
            bt_pyra_move a = *(const bt_pyra_move *)bt_pz_alg_at(&eng, alg, i);
            bt_pyra_move b = *(const bt_pyra_move *)bt_pz_alg_at(&eng, reparsed, i);
            if (!bt_pyra_move_eq(a, b)) {
                BT_ABORT("format round trip differs for %s", text);
            }
        }
        void *inverse = bt_pz_alg_create();
        void *twice = bt_pz_alg_create();
        bt_pyra_invert_alg(alg, inverse);
        bt_pyra_invert_alg(inverse, twice);
        if (bt_pz_alg_len(twice) != bt_pz_alg_len(alg)) {
            BT_ABORT("double inverse length differs for %s", text);
        }
        for (size_t i = 0; i < bt_pz_alg_len(alg); i++) {
            bt_pyra_move a = *(const bt_pyra_move *)bt_pz_alg_at(&eng, alg, i);
            bt_pyra_move b = *(const bt_pyra_move *)bt_pz_alg_at(&eng, twice, i);
            if (!bt_pyra_move_eq(a, b)) {
                BT_ABORT("double inverse differs for %s", text);
            }
        }
        bt_pyra_state state;
        bt_pyra_solved_state(&state);
        if (!bt_pyra_apply_alg(&state, alg, &err)) {
            BT_ABORT("apply %s failed: %s", text, err.token);
        }
        if (!bt_pyra_apply_alg(&state, inverse, &err)) {
            BT_ABORT("apply inverse %s failed: %s", text, err.token);
        }
        if (!bt_pyra_is_solved(&state)) {
            BT_ABORT("forward round trip leaves unsolved for %s", text);
        }
        bt_pz_alg_destroy(twice);
        bt_pz_alg_destroy(inverse);
        bt_pz_alg_destroy(reparsed);
        bt_strbuf_free(&formatted);
        bt_pz_alg_destroy(alg);
    }
}

BT_TEST(pyra_oracle, pack_rows_pass_tier1) {
    char *text = pz_read_pack("pyraminx_l4e.tsv");
    size_t rows = 0;
    char unique[64][16];
    size_t unique_count = 0;
    pz_pack_rows walker;
    pz_pack_rows_init(&walker, text);
    char *fields[8];
    size_t field_count = 0;
    bool is_header = false;
    while (pz_pack_rows_next(&walker, fields, 8, &field_count, &is_header)) {
        if (is_header) {
            BT_CHECK(strcmp(fields[0], "case_key") == 0);
            BT_CHECK(strcmp(fields[1], "alg") == 0);
            continue;
        }
        BT_CHECK(field_count >= 2);
        char alg[512];
        size_t alg_at = 0;
        for (char *scan = fields[1]; *scan != '\0'; scan++) {
            alg[alg_at] = *scan == '(' || *scan == ')' ? ' ' : *scan;
            alg_at++;
        }
        alg[alg_at] = '\0';
        if (!tier1_ok(alg)) {
            BT_ABORT("tier1 failed for row %s", fields[0]);
        }
        char *save = NULL;
        for (char *token = pz_split_space(alg, &save); token != NULL;
             token = pz_split_space(NULL, &save)) {
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
    BT_CHECK_EQ((long long)rows, 35);
    BT_CHECK_EQ((long long)unique_count, 6);
    const char *pointers[64];
    for (size_t i = 0; i < unique_count; i++) {
        pointers[i] = unique[i];
    }
    qsort(pointers, unique_count, sizeof pointers[0], pz_cmp_string);
    const char *want[6] = {"L", "L'", "R", "R'", "U", "U'"};
    for (size_t i = 0; i < 6; i++) {
        BT_CHECK_EQ(pointers[i], want[i]);
    }
    free(text);
}
