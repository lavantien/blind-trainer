#include "common.h"
#include "common/bt_harness.h"
#include "data/seedpack/normalizer.h"
#include "data/seedpack/parse.h"
#include "data/seedpack/registry.h"

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

static char *raw_alg(const char *file, const char *key) {
    char packs[BT_PATH_MAX];
    td_packs_root(packs);
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/%s", packs, file);
    char *text = td_read_file(path);
    char *cursor = text;
    bool first = true;
    while (*cursor != '\0') {
        char *line = cursor;
        char *newline = strchr(line, '\n');
        if (newline != NULL) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor = line + strlen(line);
        }
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\r') {
            line[len - 1] = '\0';
        }
        if (first) {
            first = false;
            continue;
        }
        if (line[0] == '\0') {
            continue;
        }
        char *tab = strchr(line, '\t');
        if (tab == NULL) {
            continue;
        }
        *tab = '\0';
        if (strcmp(line, key) == 0) {
            char *alg_start = tab + 1;
            char *next_tab = strchr(alg_start, '\t');
            if (next_tab != NULL) {
                *next_tab = '\0';
            }
            char *out = malloc(strlen(alg_start) + 1);
            strcpy(out, alg_start);
            free(text);
            return out;
        }
    }
    free(text);
    BT_ABORT("%s not found in %s", key, file);
    return NULL;
}

static bt_seed_rows pack_rows(const char *file) {
    char packs[BT_PATH_MAX];
    td_packs_root(packs);
    bt_pack_entry *entries = NULL;
    size_t count = 0;
    bt_seed_error err;
    if (bt_seedpack_load_registry(packs, &entries, &count, &err) != 0) {
        BT_ABORT("registry failed");
    }
    bt_pack_entry entry;
    bool found = false;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(entries[i].file, file) == 0) {
            entry = entries[i];
            found = true;
            break;
        }
    }
    bt_seedpack_free_entries(entries);
    if (!found) {
        BT_ABORT("registry holds no %s", file);
    }
    bt_seed_rows rows;
    if (bt_seedpack_load_pack(packs, &entry, &rows, &err) != 0) {
        char text[768];
        bt_seed_error_string(&err, text, sizeof text);
        BT_ABORT("pack %s failed: %s", file, text);
    }
    return rows;
}

static const bt_seed_row *find_row(const bt_seed_rows *rows, const char *case_key) {
    for (size_t i = 0; i < rows->count; i++) {
        if (strcmp(rows->items[i].case_key, case_key) == 0) {
            return &rows->items[i];
        }
    }
    BT_ABORT("%s not found", case_key);
    return NULL;
}

BT_TEST(seedpack_normalize, sq1_fused_chains_normalize_to_the_engine_canonical_spacing) {
    struct {
        const char *key;
        const char *file;
        const char *expected;
    } cases[3] = {
        {"co.01", "sq1_co.tsv", "(1,0) / (3,0) / (-1,0)"},
        {"co.03", "sq1_co.tsv", "(1,0) / (-1,0)"},
        {
            "ep.90",
            "sq1_ep.tsv",
            "/ (0,3) / (1,0) / (2,-2) / (4,0) / (4,0) / (-3,2) / (-1,0) / (0,-3) / (4,0) / (-1,2) / "
            "(3,-2)",
        },
    };
    for (int i = 0; i < 3; i++) {
        char *raw = raw_alg(cases[i].file, cases[i].key);
        BT_CHECK(strstr(raw, ")/") != NULL);
        bt_normalized out = normalize_ok("sq1", raw);
        BT_CHECK_EQ(out.alg, cases[i].expected);
        bt_normalized fixpoint = normalize_ok("sq1", out.alg);
        BT_CHECK_EQ(fixpoint.alg, cases[i].expected);
        BT_CHECK(fixpoint.note_count == 0);
        bt_seed_rows rows = pack_rows(cases[i].file);
        const bt_seed_row *stored = find_row(&rows, cases[i].key);
        BT_CHECK_EQ(stored->alg, cases[i].expected);
        bt_seed_rows_free(&rows);
        bt_normalized_free(&out);
        bt_normalized_free(&fixpoint);
        free(raw);
    }
}

BT_TEST(seedpack_normalize, sq1_hazard_tails_stay_verbatim_in_loader_notes) {
    bt_seed_rows rows = pack_rows("sq1_ep.tsv");
    const bt_seed_row *ep90 = find_row(&rows, "ep.90");
    BT_CHECK_EQ(ep90->front_json,
                "{\"case\":\"W on D\",\"loader_notes\":[\"equator marker stripped: *\","
                "\"auf prefix stripped: (D')\"],\"parity\":true}");
    bt_seed_rows_free(&rows);
}

static bool sq1_engine_token(const char *token) {
    if (strcmp(token, "/") == 0) {
        return true;
    }
    if (token[0] != '(') {
        return false;
    }
    char inner[32];
    snprintf(inner, sizeof inner, "%s", token + 1);
    size_t len = strlen(inner);
    if (len == 0 || inner[len - 1] != ')') {
        return false;
    }
    inner[len - 1] = '\0';
    char *comma = strchr(inner, ',');
    if (comma == NULL) {
        return false;
    }
    *comma = '\0';
    char *end = NULL;
    long long top = strtoll(inner, &end, 10);
    if (end == NULL || *end != '\0') {
        return false;
    }
    long long bottom = strtoll(comma + 1, &end, 10);
    if (end == NULL || *end != '\0') {
        return false;
    }
    bool in_range = (top >= -5 && top <= 6 && bottom >= -5 && bottom <= 6) != 0;
    return (in_range && (top != 0 || bottom != 0)) != 0;
}

BT_TEST(seedpack_normalize, sq1_pack_rows_stay_inside_the_engine_twist_domain) {
    const char *files[5] = {
        "sq1_cubeshape.tsv", "sq1_co.tsv", "sq1_eo.tsv", "sq1_cp.tsv", "sq1_ep.tsv",
    };
    for (int f = 0; f < 5; f++) {
        bt_seed_rows rows = pack_rows(files[f]);
        for (size_t r = 0; r < rows.count; r++) {
            BT_CHECK(rows.items[r].alg[0] != '\0');
            BT_CHECK(strchr(rows.items[r].alg, '*') == NULL);
            BT_CHECK(strchr(rows.items[r].alg, 'U') == NULL);
            BT_CHECK(strchr(rows.items[r].alg, 'D') == NULL);
            char *copy = malloc(strlen(rows.items[r].alg) + 1);
            strcpy(copy, rows.items[r].alg);
            char *cursor = copy;
            while (*cursor != '\0') {
                while (*cursor == ' ') {
                    cursor += 1;
                }
                if (*cursor == '\0') {
                    break;
                }
                char *start = cursor;
                while (*cursor != '\0' && *cursor != ' ') {
                    cursor += 1;
                }
                if (*cursor != '\0') {
                    *cursor++ = '\0';
                }
                BT_CHECK(sq1_engine_token(start));
            }
            free(copy);
        }
        bt_seed_rows_free(&rows);
    }
}

BT_TEST(seedpack_normalize, sq1_minus_six_turns_wrap_to_six_with_a_verbatim_note) {
    bt_normalized out = normalize_ok("sq1", "(1,0) / (0,-6)");
    BT_CHECK_EQ(out.alg, "(1,0) / (0,6)");
    BT_CHECK(out.note_count == 1);
    BT_CHECK_EQ(out.notes[0], "wraparound: (0,-6) -> (0,6)");
    bt_normalized_free(&out);
    bt_normalized bare = normalize_ok("sq1", "/ -6,0 /");
    BT_CHECK_EQ(bare.alg, "/ (6,0) /");
    BT_CHECK(bare.note_count == 1);
    BT_CHECK_EQ(bare.notes[0], "wraparound: (-6,0) -> (6,0)");
    bt_normalized_free(&bare);
    bt_seed_rows rows = pack_rows("sq1_ep.tsv");
    const bt_seed_row *ep96 = find_row(&rows, "ep.96");
    BT_CHECK_EQ(ep96->alg,
                "/ (0,-3) / (3,0) / (0,-3) / (0,-3) / (1,-2) / (-2,0) / (-4,2) / (4,-2) / (0,-2) / "
                "(-2,1) / (3,3) / (0,6)");
    BT_CHECK_EQ(ep96->front_json,
                "{\"case\":\"H-W\",\"loader_notes\":[\"auf prefix stripped: (D2)\","
                "\"wraparound: (0,-6) -> (0,6)\"],\"parity\":true}");
    bt_seed_rows_free(&rows);
}

static const char *MEGA_PINNED_TOKENS[16] = {
    "U", "U'", "U2", "U2'", "R", "R'", "R2", "R2'", "L", "L'", "F", "F'", "BR", "BR'", "y", "y'",
};

static bool mega_token_ok(const char *token) {
    for (int i = 0; i < 16; i++) {
        if (strcmp(token, MEGA_PINNED_TOKENS[i]) == 0) {
            return true;
        }
    }
    return false;
}

BT_TEST(seedpack_normalize, megaminx_4lll_33_bracket_repeat_expands_into_the_pinned_engine_tokens) {
    char *raw = raw_alg("megaminx_4lll.tsv", "4lll.33");
    BT_CHECK(strchr(raw, '[') != NULL && strstr(raw, "*2") != NULL);
    bt_seed_rows rows = pack_rows("megaminx_4lll.tsv");
    const bt_seed_row *row = find_row(&rows, "4lll.33");
    BT_CHECK_EQ(row->alg, "R U R' U R' U' R2 U' R' U R' U R U R U R' U R' U' R2 U' R' U R' U R U");
    char *copy = malloc(strlen(row->alg) + 1);
    strcpy(copy, row->alg);
    char *cursor = copy;
    while (*cursor != '\0') {
        while (*cursor == ' ') {
            cursor += 1;
        }
        if (*cursor == '\0') {
            break;
        }
        char *start = cursor;
        while (*cursor != '\0' && *cursor != ' ') {
            cursor += 1;
        }
        if (*cursor != '\0') {
            *cursor++ = '\0';
        }
        BT_CHECK(mega_token_ok(start));
    }
    free(copy);
    BT_CHECK(strstr(row->front_json, "loader_notes") == NULL);
    bt_normalized fixpoint = normalize_ok("megaminx", row->alg);
    BT_CHECK_EQ(fixpoint.alg, row->alg);
    bt_normalized_free(&fixpoint);
    bt_seed_rows_free(&rows);
    free(raw);
}

BT_TEST(seedpack_normalize, megaminx_pack_normalizes_inside_the_pinned_engine_grammar) {
    bt_seed_rows rows = pack_rows("megaminx_4lll.tsv");
    BT_CHECK(rows.count == 39);
    for (size_t r = 0; r < rows.count; r++) {
        char *copy = malloc(strlen(rows.items[r].alg) + 1);
        strcpy(copy, rows.items[r].alg);
        char *cursor = copy;
        while (*cursor != '\0') {
            while (*cursor == ' ') {
                cursor += 1;
            }
            if (*cursor == '\0') {
                break;
            }
            char *start = cursor;
            while (*cursor != '\0' && *cursor != ' ') {
                cursor += 1;
            }
            if (*cursor != '\0') {
                *cursor++ = '\0';
            }
            BT_CHECK(mega_token_ok(start));
        }
        free(copy);
    }
    bt_seed_rows_free(&rows);
}

static bool nxn_engine_token(const char *token) {
    size_t len = strlen(token);
    size_t index = 0;
    bool digit_prefixed = (len > 1 && token[0] >= '2' && token[0] <= '7') != 0;
    if (digit_prefixed) {
        index = 1;
    }
    if (index >= len) {
        return false;
    }
    char head = token[index];
    index += 1;
    bool upper_face = (head == 'U' || head == 'D' || head == 'L' || head == 'R' || head == 'F' ||
                       head == 'B') != 0;
    bool lower_face = (head == 'u' || head == 'd' || head == 'f' || head == 'r' || head == 'l' ||
                       head == 'b') != 0;
    if (upper_face && index < len && token[index] == 'w') {
        index += 1;
    }
    bool bare_head = (head == 'M' || head == 'E' || head == 'S' || head == 'x' || head == 'y' ||
                      head == 'z') != 0;
    if (!upper_face && !lower_face && !bare_head) {
        return false;
    }
    if (bare_head && digit_prefixed) {
        return false;
    }
    bool prime = false;
    bool two = false;
    while (index < len) {
        if (token[index] == '\'' && !prime) {
            prime = true;
        } else if (token[index] == '2' && !two) {
            two = true;
        } else {
            return false;
        }
        index += 1;
    }
    return true;
}

BT_TEST(seedpack_normalize, bigcube_parity_oll4_maps_the_literal_x_rotation_into_the_nxn_grammar) {
    char *raw = raw_alg("bigcube_parity.tsv", "parity.oll4");
    BT_CHECK(strchr(raw, 'X') != NULL);
    bt_seed_rows rows = pack_rows("bigcube_parity.tsv");
    const bt_seed_row *row = find_row(&rows, "parity.oll4");
    BT_CHECK_EQ(row->alg, "Rw U2 x Rw U2 Rw U2 Rw' U2 Lw U2 Rw' U2 Rw U2 Rw' U2 Rw'");
    BT_CHECK(strchr(row->alg, 'X') == NULL);
    BT_CHECK(strstr(row->front_json, "mapped X to x") != NULL);
    bt_seed_rows_free(&rows);
    free(raw);
}

BT_TEST(seedpack_normalize, bigcube_packs_normalize_inside_the_nxn_engine_grammar) {
    BT_CHECK(nxn_engine_token("3Rw'") == true);
    BT_CHECK(nxn_engine_token("Rw2'") == true);
    BT_CHECK(nxn_engine_token("u2") == true);
    BT_CHECK(nxn_engine_token("X") == false);
    BT_CHECK(nxn_engine_token("wR") == false);
    BT_CHECK(nxn_engine_token("rw") == false);
    BT_CHECK(nxn_engine_token("2M") == false);
    const char *files[2] = {"bigcube_parity.tsv", "bigcube_l2e.tsv"};
    for (int f = 0; f < 2; f++) {
        bt_seed_rows rows = pack_rows(files[f]);
        for (size_t r = 0; r < rows.count; r++) {
            char *copy = malloc(strlen(rows.items[r].alg) + 1);
            strcpy(copy, rows.items[r].alg);
            char *cursor = copy;
            while (*cursor != '\0') {
                while (*cursor == ' ') {
                    cursor += 1;
                }
                if (*cursor == '\0') {
                    break;
                }
                char *start = cursor;
                while (*cursor != '\0' && *cursor != ' ') {
                    cursor += 1;
                }
                if (*cursor != '\0') {
                    *cursor++ = '\0';
                }
                BT_CHECK(nxn_engine_token(start));
            }
            free(copy);
        }
        bt_seed_rows_free(&rows);
    }
}

BT_TEST(seedpack_normalize, group_repeat_rows_expand_from_the_real_packs) {
    bt_seed_rows f2l = pack_rows("cfop_f2l.tsv");
    struct {
        const char *slot;
        const char *face;
    } slots[4] = {{"fr", "R"}, {"fl", "F"}, {"br", "B"}, {"bl", "L"}};
    for (int s = 0; s < 4; s++) {
        char key[32];
        snprintf(key, sizeof key, "f2l.%s.29", slots[s].slot);
        char *raw = raw_alg("cfop_f2l.tsv", key);
        BT_CHECK(strstr(raw, ")3") != NULL);
        const bt_seed_row *row = find_row(&f2l, key);
        char want[128];
        char group[32];
        snprintf(group, sizeof group, "U %s U' %s'", slots[s].face, slots[s].face);
        snprintf(want, sizeof want, "%s %s %s", group, group, group);
        BT_CHECK_EQ(row->alg, want);
        BT_CHECK(strstr(row->front_json, "loader_notes") == NULL);
        free(raw);
    }
    bt_seed_rows_free(&f2l);
    bt_seed_rows sarah_rows = pack_rows("skewb_sarah.tsv");
    const bt_seed_row *sarah = find_row(&sarah_rows, "sarah.05");
    BT_CHECK_EQ(sarah->alg, "y2 F' L F L' F' L F L'");
    bt_seed_rows_free(&sarah_rows);
}
