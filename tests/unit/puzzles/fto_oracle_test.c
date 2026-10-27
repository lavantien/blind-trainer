#include "puzzles/fto.h"
#include "puzzles/tier1.h"

#include "common/bt_harness.h"
#include "common/rng.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void applied(const bt_fto *fto, const char *text, bt_fto_state *out) {
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    if (!bt_fto_parse_alg(fto, text, strlen(text), alg, &err)) {
        BT_ABORT("parse %s failed: %s", text, err.token);
    }
    bt_fto_solved_state(fto, out);
    if (!bt_fto_apply_alg(fto, out, alg, &err)) {
        BT_ABORT("apply %s failed: %s", text, err.token);
    }
    bt_pz_alg_destroy(alg);
}

static void after_text(const bt_fto *fto, const char *text, uint8_t out[BT_FTO_FACELET_COUNT]) {
    bt_fto_state state;
    applied(fto, text, &state);
    memcpy(out, state.facelets, BT_FTO_FACELET_COUNT);
}

static bool tier1_ok(const bt_fto *fto, const char *text) {
    bt_pz_engine eng = bt_fto_engine(fto);
    bt_tier1_error err;
    return bt_verify_tier1(&eng, text, &err) == BT_TIER1_OK;
}

static char *read_pack(void) {
    const char *root = getenv("BT_REF_GROUND");
    if (root == NULL || root[0] == '\0') {
        root = "ref_ground";
    }
    char path[512];
    snprintf(path, sizeof path, "%s/packs/fto_l4t_redux.tsv", root);
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

BT_TEST(fto_oracle, adjacency_closes_over_the_octahedron) {
    bt_fto fto = bt_fto_new();
    const uint8_t expected[8][3] = {
        {1, 3, 5}, {0, 2, 6}, {1, 3, 7}, {0, 2, 4}, {3, 5, 7}, {0, 4, 6}, {1, 5, 7}, {2, 4, 6},
    };
    for (uint8_t face = 0; face < 8; face++) {
        uint8_t neighbors[3];
        bt_fto_neighbors(&fto, face, neighbors);
        for (size_t k = 0; k < 3; k++) {
            BT_CHECK_EQ(neighbors[k], expected[face][k]);
        }
        for (size_t k = 0; k < 3; k++) {
            uint8_t back[3];
            bt_fto_neighbors(&fto, neighbors[k], back);
            bool contains = false;
            for (size_t j = 0; j < 3; j++) {
                contains = contains || back[j] == face;
            }
            BT_CHECK(contains);
            BT_CHECK(bt_fto_opposite(&fto, face) != neighbors[k]);
        }
    }
    const uint8_t pairs[4][2] = {{0, 7}, {1, 4}, {2, 5}, {3, 6}};
    for (size_t i = 0; i < 4; i++) {
        BT_CHECK_EQ(bt_fto_opposite(&fto, pairs[i][0]), pairs[i][1]);
        BT_CHECK_EQ(bt_fto_opposite(&fto, pairs[i][1]), pairs[i][0]);
    }
}

BT_TEST(fto_oracle, every_turn_is_a_bijective_order_three_seven_three_cycle_permutation) {
    bt_fto fto = bt_fto_new();
    for (uint8_t face = 0; face < 8; face++) {
        for (uint8_t quarter = 1; quarter <= 2; quarter++) {
            uint8_t table[BT_FTO_FACELET_COUNT];
            bt_fto_turn_table(&fto, face, quarter, table);
            uint8_t sorted[BT_FTO_FACELET_COUNT];
            memcpy(sorted, table, sizeof sorted);
            for (size_t i = 1; i < BT_FTO_FACELET_COUNT; i++) {
                uint8_t key = sorted[i];
                size_t j = i;
                while (j > 0 && sorted[j - 1] > key) {
                    sorted[j] = sorted[j - 1];
                    j--;
                }
                sorted[j] = key;
            }
            for (size_t i = 0; i < BT_FTO_FACELET_COUNT; i++) {
                BT_CHECK_EQ(sorted[i], (uint8_t)i);
            }
            for (size_t i = 0; i < BT_FTO_FACELET_COUNT; i++) {
                uint8_t doubled = table[table[i]];
                BT_CHECK_EQ(table[doubled], (uint8_t)i);
            }
            bool seen[BT_FTO_FACELET_COUNT] = {false};
            size_t fixed = 0;
            size_t threes = 0;
            for (size_t i = 0; i < BT_FTO_FACELET_COUNT; i++) {
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
                    BT_ABORT("face %u quarter %u cycle len %zu", face, quarter, len);
                }
            }
            BT_CHECK_EQ((long long)fixed, 51);
            BT_CHECK_EQ((long long)threes, 7);
        }
    }
}

BT_TEST(fto_oracle, turn_recolors_only_neighbor_and_opposite_neighbor_faces) {
    bt_fto fto = bt_fto_new();
    bt_pz_engine eng = bt_fto_engine(&fto);
    bt_pz_error err;
    for (uint8_t face = 0; face < 8; face++) {
        uint8_t far = bt_fto_opposite(&fto, face);
        uint8_t landing[6];
        bt_fto_neighbors(&fto, face, landing);
        bt_fto_neighbors(&fto, far, landing + 3);
        for (uint8_t quarter = 1; quarter <= 2; quarter++) {
            void *alg = bt_pz_alg_create();
            bt_fto_move move = {face, quarter};
            bt_pz_alg_push(&eng, alg, &move);
            bt_fto_state state;
            bt_fto_solved_state(&fto, &state);
            if (!bt_fto_apply_alg(&fto, &state, alg, &err)) {
                BT_ABORT("apply failed: %s", err.token);
            }
            size_t changed = 0;
            for (size_t i = 0; i < BT_FTO_FACELET_COUNT; i++) {
                uint8_t home = (uint8_t)(i / 9);
                if (state.facelets[i] != home) {
                    changed++;
                    bool allowed = false;
                    for (size_t k = 0; k < 6; k++) {
                        allowed = allowed || landing[k] == home;
                    }
                    BT_CHECK(allowed);
                }
            }
            for (size_t slot = 0; slot < 9; slot++) {
                BT_CHECK_EQ(state.facelets[(size_t)face * 9 + slot], face);
                BT_CHECK_EQ(state.facelets[(size_t)far * 9 + slot], far);
            }
            BT_CHECK_EQ((long long)changed, 12);
            bt_pz_alg_destroy(alg);
        }
    }
}

BT_TEST(fto_oracle, opposite_face_turns_commute) {
    bt_fto fto = bt_fto_new();
    const uint8_t pairs[4][2] = {{0, 7}, {1, 4}, {2, 5}, {3, 6}};
    bt_pz_error err;
    for (size_t i = 0; i < 4; i++) {
        const char *a_name = BT_FTO_FACE_NAMES[pairs[i][0]];
        const char *b_name = BT_FTO_FACE_NAMES[pairs[i][1]];
        void *a_alg = bt_pz_alg_create();
        void *b_alg = bt_pz_alg_create();
        if (!bt_fto_parse_alg(&fto, a_name, strlen(a_name), a_alg, &err) ||
            !bt_fto_parse_alg(&fto, b_name, strlen(b_name), b_alg, &err)) {
            BT_ABORT("parse failed: %s", err.token);
        }
        bt_fto_state ab;
        bt_fto_state ba;
        bt_fto_solved_state(&fto, &ab);
        bt_fto_solved_state(&fto, &ba);
        if (!bt_fto_apply_alg(&fto, &ab, a_alg, &err) ||
            !bt_fto_apply_alg(&fto, &ab, b_alg, &err) ||
            !bt_fto_apply_alg(&fto, &ba, b_alg, &err) ||
            !bt_fto_apply_alg(&fto, &ba, a_alg, &err)) {
            BT_ABORT("apply failed: %s", err.token);
        }
        BT_CHECK(bt_fto_state_eq(&ab, &ba));
        bt_pz_alg_destroy(b_alg);
        bt_pz_alg_destroy(a_alg);
    }
}

BT_TEST(fto_oracle, double_turn_token_equals_two_quarter_turns) {
    bt_fto fto = bt_fto_new();
    for (size_t face = 0; face < 8; face++) {
        const char *name = BT_FTO_FACE_NAMES[face];
        char doubled[16];
        char tripled[24];
        char tokened[16];
        snprintf(doubled, sizeof doubled, "%s %s", name, name);
        snprintf(tripled, sizeof tripled, "%s %s %s", name, name, name);
        snprintf(tokened, sizeof tokened, "%s2", name);
        uint8_t twice[BT_FTO_FACELET_COUNT];
        uint8_t token[BT_FTO_FACELET_COUNT];
        uint8_t thrice[BT_FTO_FACELET_COUNT];
        after_text(&fto, doubled, twice);
        after_text(&fto, tokened, token);
        after_text(&fto, tripled, thrice);
        BT_CHECK(memcmp(twice, token, BT_FTO_FACELET_COUNT) == 0);
        uint8_t solved[BT_FTO_FACELET_COUNT];
        bt_fto_state solved_state;
        bt_fto_solved_state(&fto, &solved_state);
        memcpy(solved, solved_state.facelets, BT_FTO_FACELET_COUNT);
        BT_CHECK(memcmp(thrice, solved, BT_FTO_FACELET_COUNT) == 0);
    }
}

BT_TEST(fto_oracle, br_parity_row_parses_and_applies) {
    bt_fto fto = bt_fto_new();
    char *text = read_pack();
    bool found = false;
    char alg_text[512] = "";
    char *cursor = text;
    bool first = true;
    while (*cursor != '\0' && !found) {
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
        if (line[0] == '\0' || first) {
            first = false;
            continue;
        }
        char *key = line;
        char *alg = strchr(line, '\t');
        if (alg == NULL) {
            continue;
        }
        *alg = '\0';
        alg++;
        char *rest = strchr(alg, '\t');
        if (rest != NULL) {
            *rest = '\0';
        }
        if (strcmp(key, "l4t.36") == 0) {
            snprintf(alg_text, sizeof alg_text, "%s", alg);
            found = true;
        }
    }
    free(text);
    BT_CHECK(found);
    BT_CHECK_EQ(alg_text, "U BR U BR' F U F'");
    bt_fto_move move;
    BT_CHECK(bt_fto_parse_move("BR", strlen("BR"), &move, NULL));
    BT_CHECK(bt_fto_parse_move("BR'", strlen("BR'"), &move, NULL));
    BT_CHECK(tier1_ok(&fto, alg_text));
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    if (!bt_fto_parse_alg(&fto, alg_text, strlen(alg_text), alg, &err)) {
        BT_ABORT("parse failed: %s", err.token);
    }
    bt_fto_state state;
    bt_fto_solved_state(&fto, &state);
    if (!bt_fto_apply_alg(&fto, &state, alg, &err)) {
        BT_ABORT("apply failed: %s", err.token);
    }
    BT_CHECK(!bt_fto_is_solved(&fto, &state));
    bt_pz_alg_destroy(alg);
}

BT_TEST(fto_oracle, tier1_green_over_every_pack_row) {
    bt_fto fto = bt_fto_new();
    char *text = read_pack();
    size_t rows = 0;
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
        char *key = line;
        char *alg = strchr(line, '\t');
        if (alg == NULL) {
            BT_ABORT("row without alg column: %s", key);
        }
        *alg = '\0';
        alg++;
        char *rest = strchr(alg, '\t');
        if (rest != NULL) {
            *rest = '\0';
        }
        if (!tier1_ok(&fto, alg)) {
            BT_ABORT("tier1 failed for %s: %s", key, alg);
        }
        rows++;
    }
    free(text);
    BT_CHECK_EQ((long long)rows, 36);
}

BT_TEST(fto_oracle, grammar_accepts_exactly_the_pinned_token_inventory) {
    bt_fto fto = bt_fto_new();
    const char *accepted[] = {
        "BR", "BR'", "F", "F'", "L", "L'", "L2", "L2'", "R", "R'", "R2'", "U", "U'",
    };
    for (size_t i = 0; i < sizeof accepted / sizeof accepted[0]; i++) {
        bt_fto_move move;
        BT_CHECK(bt_fto_parse_move(accepted[i], strlen(accepted[i]), &move, NULL));
    }
    const char *suffixes[4] = {"", "'", "2", "2'"};
    for (size_t face = 0; face < 8; face++) {
        for (size_t s = 0; s < 4; s++) {
            char token[16];
            snprintf(token, sizeof token, "%s%s", BT_FTO_FACE_NAMES[face], suffixes[s]);
            bt_fto_move move;
            BT_CHECK(bt_fto_parse_move(token, strlen(token), &move, NULL));
        }
    }
    bt_pz_error err;
    const char *singles[3] = {"L2", "L2'", "R2'"};
    const char *wanted[3] = {"L'", "L", "R"};
    for (size_t i = 0; i < 3; i++) {
        void *alg = bt_pz_alg_create();
        if (!bt_fto_parse_alg(&fto, singles[i], strlen(singles[i]), alg, &err)) {
            BT_ABORT("parse %s failed: %s", singles[i], err.token);
        }
        bt_strbuf text;
        bt_strbuf_init(&text, 16);
        bt_fto_format_alg(&fto, alg, &text);
        BT_CHECK_EQ(text.data, wanted[i]);
        bt_strbuf_free(&text);
        bt_pz_alg_destroy(alg);
    }
    bt_pz_engine eng = bt_fto_engine(&fto);
    void *alg = bt_pz_alg_create();
    if (!bt_fto_parse_alg(&fto, "U BR U BR' F U F'", strlen("U BR U BR' F U F'"), alg, &err)) {
        BT_ABORT("parse failed: %s", err.token);
    }
    bt_strbuf text;
    bt_strbuf_init(&text, 64);
    bt_fto_format_alg(&fto, alg, &text);
    void *reparsed = bt_pz_alg_create();
    if (!bt_fto_parse_alg(&fto, text.data, text.len, reparsed, &err)) {
        BT_ABORT("reparse failed: %s", err.token);
    }
    BT_CHECK_EQ(bt_pz_alg_len(reparsed), bt_pz_alg_len(alg));
    for (size_t i = 0; i < bt_pz_alg_len(alg); i++) {
        bt_fto_move a = *(const bt_fto_move *)bt_pz_alg_at(&eng, alg, i);
        bt_fto_move b = *(const bt_fto_move *)bt_pz_alg_at(&eng, reparsed, i);
        BT_CHECK(bt_fto_move_eq(a, b));
    }
    bt_pz_alg_destroy(reparsed);
    bt_pz_alg_destroy(alg);
    bt_strbuf_free(&text);
}

BT_TEST(fto_oracle, rejects_garbage_and_cube_slice_tokens) {
    const char *tokens[] = {
        "M",  "E",  "S",  "x",  "y", "z", "u",    "r",    "Uw", "BRw",
        "3U", "U3", "U4", "UU", "'", "2", "U2''", "F2'2", "",
    };
    for (size_t i = 0; i < sizeof tokens / sizeof tokens[0]; i++) {
        bt_fto_move move;
        BT_CHECK(!bt_fto_parse_move(tokens[i], strlen(tokens[i]), &move, NULL));
    }
    bt_fto fto = bt_fto_new();
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    BT_CHECK(!bt_fto_parse_alg(&fto, "U M", strlen("U M"), alg, &err));
    BT_CHECK_EQ(err.kind, (int)BT_PZ_ERR_PARSE);
    BT_CHECK_EQ(err.token, "M");
    bt_pz_alg_destroy(alg);
}

BT_TEST(fto_oracle, solved_grid_renders_eight_triforce_faces) {
    bt_fto fto = bt_fto_new();
    bt_fto_state state;
    bt_fto_solved_state(&fto, &state);
    bt_pz_grid grid;
    bt_fto_facelets(&fto, &state, &grid);
    BT_CHECK_EQ((long long)grid.face_count, 8);
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
    BT_CHECK(bt_fto_is_solved(&fto, &state));
}

BT_TEST(fto_oracle, inverse_round_trips_through_solved) {
    bt_fto fto = bt_fto_new();
    bt_pz_engine eng = bt_fto_engine(&fto);
    bt_rng rng;
    bt_rng_seed(&rng, 20261012);
    bt_pz_error err;
    for (size_t iteration = 0; iteration < 256; iteration++) {
        size_t length = (size_t)bt_rng_below(&rng, 24);
        void *alg = bt_pz_alg_create();
        for (size_t i = 0; i < length; i++) {
            bt_fto_move move = {(uint8_t)bt_rng_below(&rng, 8),
                                (uint8_t)(bt_rng_below(&rng, 2) + 1)};
            bt_pz_alg_push(&eng, alg, &move);
        }
        void *inverse = bt_pz_alg_create();
        bt_fto_invert_alg(&fto, alg, inverse);
        bt_fto_state forward;
        bt_fto_state reverse;
        bt_fto_solved_state(&fto, &forward);
        bt_fto_solved_state(&fto, &reverse);
        if (!bt_fto_apply_alg(&fto, &forward, alg, &err) ||
            !bt_fto_apply_alg(&fto, &forward, inverse, &err) || !bt_fto_is_solved(&fto, &forward)) {
            BT_ABORT("forward round trip failed");
        }
        if (!bt_fto_apply_alg(&fto, &reverse, inverse, &err) ||
            !bt_fto_apply_alg(&fto, &reverse, alg, &err) || !bt_fto_is_solved(&fto, &reverse)) {
            BT_ABORT("reverse round trip failed");
        }
        bt_strbuf text;
        bt_strbuf_init(&text, 256);
        bt_fto_format_alg(&fto, alg, &text);
        void *reparsed = bt_pz_alg_create();
        if (!bt_fto_parse_alg(&fto, text.data, text.len, reparsed, &err) ||
            bt_pz_alg_len(reparsed) != bt_pz_alg_len(alg)) {
            BT_ABORT("format round trip failed for %s", text.data);
        }
        for (size_t i = 0; i < bt_pz_alg_len(alg); i++) {
            bt_fto_move a = *(const bt_fto_move *)bt_pz_alg_at(&eng, alg, i);
            bt_fto_move b = *(const bt_fto_move *)bt_pz_alg_at(&eng, reparsed, i);
            if (!bt_fto_move_eq(a, b)) {
                BT_ABORT("format round trip differs for %s", text.data);
            }
        }
        bt_strbuf inverse_text;
        bt_strbuf_init(&inverse_text, 256);
        bt_fto_format_alg(&fto, inverse, &inverse_text);
        void *reparsed_inverse = bt_pz_alg_create();
        if (!bt_fto_parse_alg(&fto, inverse_text.data, inverse_text.len, reparsed_inverse, &err) ||
            bt_pz_alg_len(reparsed_inverse) != bt_pz_alg_len(inverse)) {
            BT_ABORT("inverse format round trip failed for %s", inverse_text.data);
        }
        void *twice = bt_pz_alg_create();
        bt_fto_invert_alg(&fto, inverse, twice);
        if (bt_pz_alg_len(twice) != bt_pz_alg_len(alg)) {
            BT_ABORT("double inverse length differs");
        }
        for (size_t i = 0; i < bt_pz_alg_len(alg); i++) {
            bt_fto_move a = *(const bt_fto_move *)bt_pz_alg_at(&eng, alg, i);
            bt_fto_move b = *(const bt_fto_move *)bt_pz_alg_at(&eng, twice, i);
            if (!bt_fto_move_eq(a, b)) {
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

BT_TEST(fto_oracle, quarter_one_direction_is_pinned_per_face) {
    bt_fto fto = bt_fto_new();
    const char *names[8] = {"U", "F", "R", "BR", "B", "BL", "L", "D"};
    const size_t positions[8] = {9, 0, 1, 1, 18, 0, 0, 11};
    const uint8_t values[8] = {3, 6, 6, 2, 6, 4, 4, 5};
    for (size_t index = 0; index < 8; index++) {
        bt_fto_state state;
        applied(&fto, names[index], &state);
        BT_CHECK_EQ(state.facelets[positions[index]], values[index]);
        uint8_t table[BT_FTO_FACELET_COUNT];
        bt_fto_turn_table(&fto, (uint8_t)index, 1, table);
        BT_CHECK_EQ((size_t)table[positions[index]] / 9, (size_t)values[index]);
    }
}

BT_TEST(fto_oracle, apply_rejects_hand_built_illegal_quarters) {
    bt_fto fto = bt_fto_new();
    bt_pz_engine eng = bt_fto_engine(&fto);
    const uint8_t quarters[2] = {0, 3};
    for (size_t i = 0; i < 2; i++) {
        void *alg = bt_pz_alg_create();
        bt_fto_move move = {0, quarters[i]};
        bt_pz_alg_push(&eng, alg, &move);
        bt_fto_state state;
        bt_fto_solved_state(&fto, &state);
        bt_pz_error err;
        BT_CHECK(!bt_fto_apply_alg(&fto, &state, alg, &err));
        BT_CHECK_EQ(err.kind, (int)BT_PZ_ERR_APPLY);
        bt_strbuf message;
        bt_strbuf_init(&message, 32);
        bt_pz_error_message(&err, &message);
        BT_CHECK(strstr(message.data, "illegal") != NULL);
        bt_strbuf_free(&message);
        bt_pz_alg_destroy(alg);
    }
}

BT_TEST(fto_inline, solved_state_is_solved) {
    bt_fto fto = bt_fto_new();
    bt_fto_state state;
    bt_fto_solved_state(&fto, &state);
    BT_CHECK(bt_fto_is_solved(&fto, &state));
}

BT_TEST(fto_inline, every_single_turn_breaks_and_reverts) {
    bt_fto fto = bt_fto_new();
    bt_pz_engine eng = bt_fto_engine(&fto);
    bt_pz_error err;
    for (uint8_t face = 0; face < 8; face++) {
        for (uint8_t quarter = 1; quarter <= 2; quarter++) {
            void *alg = bt_pz_alg_create();
            bt_fto_move move = {face, quarter};
            bt_pz_alg_push(&eng, alg, &move);
            bt_fto_state state;
            bt_fto_solved_state(&fto, &state);
            if (!bt_fto_apply_alg(&fto, &state, alg, &err)) {
                BT_ABORT("apply failed: %s", err.token);
            }
            BT_CHECK(!bt_fto_is_solved(&fto, &state));
            void *inverse = bt_pz_alg_create();
            bt_fto_invert_alg(&fto, alg, inverse);
            if (!bt_fto_apply_alg(&fto, &state, inverse, &err)) {
                BT_ABORT("apply inverse failed: %s", err.token);
            }
            BT_CHECK(bt_fto_is_solved(&fto, &state));
            bt_pz_alg_destroy(inverse);
            bt_pz_alg_destroy(alg);
        }
    }
}

static uint64_t xorshift(uint64_t *seed) {
    uint64_t x = *seed;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *seed = x;
    return x;
}

BT_TEST(fto_inline, inverse_round_trips_a_deterministic_alg_sweep) {
    bt_fto fto = bt_fto_new();
    bt_pz_engine eng = bt_fto_engine(&fto);
    uint64_t seed = 0x2545f4914f6cdd1dULL;
    bt_pz_error err;
    for (size_t iteration = 0; iteration < 4096; iteration++) {
        size_t length = (size_t)(xorshift(&seed) % 24);
        void *alg = bt_pz_alg_create();
        for (size_t i = 0; i < length; i++) {
            bt_fto_move move = {(uint8_t)(xorshift(&seed) % 8), (uint8_t)(xorshift(&seed) % 2 + 1)};
            bt_pz_alg_push(&eng, alg, &move);
        }
        void *inverse = bt_pz_alg_create();
        bt_fto_invert_alg(&fto, alg, inverse);
        bt_fto_state forward;
        bt_fto_state reverse;
        bt_fto_solved_state(&fto, &forward);
        bt_fto_solved_state(&fto, &reverse);
        if (!bt_fto_apply_alg(&fto, &forward, alg, &err) ||
            !bt_fto_apply_alg(&fto, &forward, inverse, &err) || !bt_fto_is_solved(&fto, &forward)) {
            BT_ABORT("forward round trip failed");
        }
        if (!bt_fto_apply_alg(&fto, &reverse, inverse, &err) ||
            !bt_fto_apply_alg(&fto, &reverse, alg, &err) || !bt_fto_is_solved(&fto, &reverse)) {
            BT_ABORT("reverse round trip failed");
        }
        bt_strbuf text;
        bt_strbuf_init(&text, 256);
        bt_fto_format_alg(&fto, alg, &text);
        void *reparsed = bt_pz_alg_create();
        if (!bt_fto_parse_alg(&fto, text.data, text.len, reparsed, &err) ||
            bt_pz_alg_len(reparsed) != bt_pz_alg_len(alg)) {
            BT_ABORT("format round trip failed for %s", text.data);
        }
        for (size_t i = 0; i < bt_pz_alg_len(alg); i++) {
            bt_fto_move a = *(const bt_fto_move *)bt_pz_alg_at(&eng, alg, i);
            bt_fto_move b = *(const bt_fto_move *)bt_pz_alg_at(&eng, reparsed, i);
            if (!bt_fto_move_eq(a, b)) {
                BT_ABORT("format round trip differs for %s", text.data);
            }
        }
        void *twice = bt_pz_alg_create();
        bt_fto_invert_alg(&fto, inverse, twice);
        if (bt_pz_alg_len(twice) != bt_pz_alg_len(alg)) {
            BT_ABORT("double inverse length differs");
        }
        if (!tier1_ok(&fto, text.data)) {
            BT_ABORT("tier1 failed for %s", text.data);
        }
        bt_pz_alg_destroy(twice);
        bt_pz_alg_destroy(reparsed);
        bt_strbuf_free(&text);
        bt_pz_alg_destroy(inverse);
        bt_pz_alg_destroy(alg);
    }
}
