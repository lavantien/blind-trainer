#include "common.h"
#include "scheme/memo.h"
#include "scheme/pairs.h"
#include "scheme/scheme.h"
#include "scheme/scramble.h"

#include "common/bt_harness.h"
#include "common/rng.h"

#include <stdlib.h>
#include <string.h>

enum { MO_SIM_MAX = 1100 };

typedef struct {
    char solver[32];
    char sheet[16];
    char rowid[16];
    char position[96];
    char match[24];
    char note[512];
} mo_sim_row;

static mo_sim_row mo_sim[MO_SIM_MAX];
static size_t mo_sim_count;

static void mo_load_simcheck(void) {
    mo_sim_count = 0;
    char path[BT_PATH_MAX];
    bt_test_ref_path(path, "simcheck.tsv");
    bt_test_lines lines;
    bt_test_lines_begin(&lines, path);
    char *line = bt_test_lines_next(&lines);
    BT_CHECK(line != NULL);
    while ((line = bt_test_lines_next(&lines)) != NULL) {
        if (*line == '\0') {
            continue;
        }
        char *cols[8];
        BT_CHECK_EQ(bt_tsv_split(line, cols, 8), (size_t)8);
        BT_CHECK(mo_sim_count < MO_SIM_MAX);
        mo_sim_row *row = &mo_sim[mo_sim_count++];
        snprintf(row->solver, sizeof row->solver, "%s", cols[0]);
        snprintf(row->sheet, sizeof row->sheet, "%s", cols[1]);
        snprintf(row->rowid, sizeof row->rowid, "%s", cols[2]);
        snprintf(row->position, sizeof row->position, "%s", cols[3]);
        snprintf(row->match, sizeof row->match, "%s", cols[6]);
        snprintf(row->note, sizeof row->note, "%s", cols[7]);
    }
    bt_test_lines_end(&lines);
}

static void mo_sticker_of_name_init(char names[48][8], uint8_t stickers[48]) {
    char path[BT_PATH_MAX];
    bt_test_ref_path(path, "position_map.tsv");
    bt_test_lines lines;
    bt_test_lines_begin(&lines, path);
    char *line = bt_test_lines_next(&lines);
    BT_CHECK(line != NULL);
    size_t count = 0;
    while ((line = bt_test_lines_next(&lines)) != NULL) {
        if (*line == '\0') {
            continue;
        }
        char *cols[8];
        BT_CHECK_EQ(bt_tsv_split(line, cols, 8), (size_t)8);
        snprintf(names[count], 8, "%s", cols[3]);
        stickers[count] = (uint8_t)bt_test_parse_int(cols[1]);
        count++;
    }
    bt_test_lines_end(&lines);
    BT_CHECK_EQ(count, (size_t)48);
}

static uint8_t mo_sticker_of_name(const char names[48][8], const uint8_t stickers[48],
                                  const char *name) {
    for (size_t i = 0; i < 48; i++) {
        if (strcmp(names[i], name) == 0) {
            return stickers[i];
        }
    }
    BT_ABORT("unknown piece name %s", name);
    return 0;
}

static void mo_parse_alg(const char *text, bt_alg *alg) {
    bt_alg_init(alg);
    char err[128];
    if (!bt_alg_parse(text, alg, err, sizeof err)) {
        BT_ABORT("parse %s failed: %s", text, err);
    }
}

static void mo_alg_of_row(const char *solver, const char *sheet, long long rowid, bt_alg *alg) {
    char relative[BT_PATH_MAX];
    snprintf(relative, BT_PATH_MAX, "%s/%s.tsv", solver, sheet);
    char path[BT_PATH_MAX];
    bt_test_ref_path(path, relative);
    bt_test_lines lines;
    bt_test_lines_begin(&lines, path);
    char kept[512];
    kept[0] = '\0';
    for (long long i = 0; i < rowid; i++) {
        char *line = bt_test_lines_next(&lines);
        BT_CHECK(line != NULL);
        if (i == rowid - 1) {
            snprintf(kept, sizeof kept, "%s", line);
        }
    }
    bt_test_lines_end(&lines);
    char *cols[8];
    BT_CHECK(bt_tsv_split(kept, cols, 8) >= 2);
    mo_parse_alg(cols[1], alg);
}

static void mo_forward_state(const bt_alg *alg, bt_cube *cube) {
    *cube = bt_cube_solved();
    bt_cube_apply_alg(cube, alg);
}

static void mo_solved_by_state(const bt_alg *alg, bt_cube *cube) {
    *cube = bt_cube_solved();
    bt_alg inverse = bt_alg_inverse(alg);
    bt_cube_apply_alg(cube, &inverse);
    bt_alg_free(&inverse);
}

static void mo_random_cube(uint64_t seed, bt_cube *cube) {
    bt_rng rng;
    bt_rng_seed(&rng, seed);
    const char *faces[6] = {"U", "D", "L", "R", "F", "B"};
    const char *suffixes[3] = {"", "'", "2"};
    size_t len = 8 + (size_t)bt_rng_below(&rng, 17);
    char text[256];
    size_t at = 0;
    size_t last = 6;
    for (size_t i = 0; i < len; i++) {
        size_t f = (size_t)bt_rng_below(&rng, 6);
        while (f == last) {
            f = (size_t)bt_rng_below(&rng, 6);
        }
        last = f;
        at += (size_t)snprintf(text + at, sizeof text - at, "%s%s ", faces[f],
                               suffixes[bt_rng_below(&rng, 3)]);
    }
    if (at > 0) {
        text[at - 1] = '\0';
    }
    bt_alg alg;
    mo_parse_alg(text, &alg);
    mo_forward_state(&alg, cube);
    bt_alg_free(&alg);
}

static void mo_assert_solves(const bt_cube *cube, bt_memo *memo) {
    bt_scheme_memo(cube, memo);
    uint8_t work[54];
    bt_scheme_state_array(cube, work);
    bt_scheme_apply_memo(work, memo);
    for (int i = 0; i < 54; i++) {
        BT_CHECK_EQ(work[i], (uint8_t)i);
    }
}

static void mo_piece_of_sticker(uint8_t sticker, bt_orbit *orbit, uint8_t *piece) {
    bt_piece keyed = bt_piece_of(sticker);
    *orbit = keyed.kind == BT_PIECE_CORNER ? BT_ORBIT_CORNER : BT_ORBIT_EDGE;
    *piece = keyed.index;
}

static bool mo_covered_contains(const bool covered[2][12], bt_orbit orbit, uint8_t piece) {
    return covered[orbit == BT_ORBIT_EDGE ? 0 : 1][piece];
}

static void mo_covered_pieces(const bt_memo *memo, bool covered[2][12]) {
    memset(covered, 0, sizeof(bool[2][12]));
    for (size_t i = 0; i < memo->edge_pair_count; i++) {
        bt_orbit orbit;
        uint8_t piece;
        mo_piece_of_sticker(memo->edge_pairs[i].t1, &orbit, &piece);
        covered[orbit == BT_ORBIT_EDGE ? 0 : 1][piece] = true;
        mo_piece_of_sticker(memo->edge_pairs[i].t2, &orbit, &piece);
        covered[orbit == BT_ORBIT_EDGE ? 0 : 1][piece] = true;
    }
    for (size_t i = 0; i < memo->corner_pair_count; i++) {
        bt_orbit orbit;
        uint8_t piece;
        mo_piece_of_sticker(memo->corner_pairs[i].t1, &orbit, &piece);
        covered[orbit == BT_ORBIT_EDGE ? 0 : 1][piece] = true;
        mo_piece_of_sticker(memo->corner_pairs[i].t2, &orbit, &piece);
        covered[orbit == BT_ORBIT_EDGE ? 0 : 1][piece] = true;
    }
    for (size_t i = 0; i < memo->edge_buffer_count; i++) {
        bt_orbit orbit;
        uint8_t piece;
        mo_piece_of_sticker(memo->edge_buffers[i], &orbit, &piece);
        covered[orbit == BT_ORBIT_EDGE ? 0 : 1][piece] = true;
    }
    for (size_t i = 0; i < memo->corner_buffer_count; i++) {
        bt_orbit orbit;
        uint8_t piece;
        mo_piece_of_sticker(memo->corner_buffers[i], &orbit, &piece);
        covered[orbit == BT_ORBIT_EDGE ? 0 : 1][piece] = true;
    }
    if (memo->residue.has_parity) {
        const uint8_t *pair = memo->residue.parity.edge;
        const uint8_t *corner_pair = memo->residue.parity.corner;
        for (int k = 0; k < 2; k++) {
            bt_orbit orbit;
            uint8_t piece;
            mo_piece_of_sticker(pair[k], &orbit, &piece);
            covered[orbit == BT_ORBIT_EDGE ? 0 : 1][piece] = true;
            mo_piece_of_sticker(corner_pair[k], &orbit, &piece);
            covered[orbit == BT_ORBIT_EDGE ? 0 : 1][piece] = true;
        }
    }
    for (uint8_t i = 0; i < memo->residue.flip_count; i++) {
        bt_orbit orbit;
        uint8_t piece;
        mo_piece_of_sticker(memo->residue.flips[i], &orbit, &piece);
        covered[orbit == BT_ORBIT_EDGE ? 0 : 1][piece] = true;
    }
    for (uint8_t i = 0; i < memo->residue.twist_count; i++) {
        bt_orbit orbit;
        uint8_t piece;
        mo_piece_of_sticker(memo->residue.twists[i].corner, &orbit, &piece);
        covered[orbit == BT_ORBIT_EDGE ? 0 : 1][piece] = true;
    }
    covered[0][bt_piece_of(bt_scheme_sticker_of(BT_ORBIT_EDGE, 'C')).index] = true;
    covered[1][bt_piece_of(bt_scheme_sticker_of(BT_ORBIT_CORNER, 'C')).index] = true;
}

static bool mo_case_set_contains(const bt_case *cases, size_t count, uint8_t t1, uint8_t t2) {
    for (size_t i = 0; i < count; i++) {
        if (cases[i].t1 == t1 && cases[i].t2 == t2) {
            return true;
        }
    }
    return false;
}

static void mo_check_partition(const bt_cube *cube, const bt_memo *memo) {
    bt_case edge_cases[BT_PAIRS_MAX];
    bt_case corner_cases[BT_PAIRS_MAX];
    size_t edge_count = bt_scheme_enumerate_cases(BT_ORBIT_EDGE, edge_cases);
    size_t corner_count = bt_scheme_enumerate_cases(BT_ORBIT_CORNER, corner_cases);
    bool covered[2][12];
    mo_covered_pieces(memo, covered);
    const size_t *letters[2] = {&memo->edge_letter_count, &memo->corner_letter_count};
    const size_t *pair_counts[2] = {&memo->edge_pair_count, &memo->corner_pair_count};
    for (int orbit_index = 0; orbit_index < 2; orbit_index++) {
        size_t slack = *letters[orbit_index] - 2 * *pair_counts[orbit_index];
        BT_CHECK_EQ(slack, *letters[orbit_index] % 2);
        const size_t *buffer_counts[2] = {&memo->edge_buffer_count, &memo->corner_buffer_count};
        BT_CHECK_EQ(*pair_counts[orbit_index], *buffer_counts[orbit_index]);
    }
    for (size_t i = 0; i < memo->edge_pair_count; i++) {
        BT_CHECK(mo_case_set_contains(edge_cases, edge_count, memo->edge_pairs[i].t1,
                                      memo->edge_pairs[i].t2));
    }
    for (size_t i = 0; i < memo->corner_pair_count; i++) {
        BT_CHECK(mo_case_set_contains(corner_cases, corner_count, memo->corner_pairs[i].t1,
                                      memo->corner_pairs[i].t2));
    }
    if (memo->edge_letter_count % 2 == 1) {
        char letter = memo->edge_letters[memo->edge_letter_count - 1];
        uint8_t last = bt_scheme_sticker_of(BT_ORBIT_EDGE, letter);
        uint8_t head = bt_head_sticker(bt_piece_of(last));
        bool owned = memo->residue.has_parity;
        for (uint8_t i = 0; i < memo->residue.flip_count && !owned; i++) {
            owned = memo->residue.flips[i] == head;
        }
        for (uint8_t i = 0; i < memo->residue.twist_count && !owned; i++) {
            owned = memo->residue.twists[i].corner == head;
        }
        BT_CHECK(owned);
    }
    if (memo->corner_letter_count % 2 == 1) {
        char letter = memo->corner_letters[memo->corner_letter_count - 1];
        uint8_t last = bt_scheme_sticker_of(BT_ORBIT_CORNER, letter);
        uint8_t head = bt_head_sticker(bt_piece_of(last));
        bool owned = memo->residue.has_parity;
        for (uint8_t i = 0; i < memo->residue.flip_count && !owned; i++) {
            owned = memo->residue.flips[i] == head;
        }
        for (uint8_t i = 0; i < memo->residue.twist_count && !owned; i++) {
            owned = memo->residue.twists[i].corner == head;
        }
        BT_CHECK(owned);
    }
    for (uint8_t sticker = 0; sticker < 48; sticker++) {
        if (bt_cube_position_of(cube, sticker) != sticker) {
            bt_orbit orbit;
            uint8_t piece;
            mo_piece_of_sticker(sticker, &orbit, &piece);
            BT_CHECK(mo_covered_contains(covered, orbit, piece));
        }
    }
}

BT_TEST(memo_oracle, memo_round_trips_and_partitions_random_states) {
    for (uint64_t seed = 0; seed < 2000; seed++) {
        bt_cube cube;
        mo_random_cube(seed, &cube);
        bt_memo memo;
        mo_assert_solves(&cube, &memo);
        bt_memo again;
        bt_scheme_memo(&cube, &again);
        BT_CHECK(bt_scheme_memo_eq(&memo, &again));
        mo_check_partition(&cube, &memo);
    }
}

BT_TEST(memo_oracle, prop_memo_round_trips_partitions_and_is_deterministic) {
    const char *tokens[18] = {
        "U", "U'", "U2", "D", "D'", "D2", "L", "L'", "L2",
        "R", "R'", "R2", "F", "F'", "F2", "B", "B'", "B2",
    };
    for (uint64_t case_index = 0; case_index < 512; case_index++) {
        bt_rng rng;
        bt_rng_seed(&rng, 20261012 + case_index);
        size_t len = 8 + (size_t)bt_rng_below(&rng, 16);
        char text[256];
        size_t at = 0;
        for (size_t i = 0; i < len; i++) {
            at += (size_t)snprintf(text + at, sizeof text - at, "%s ",
                                   tokens[bt_rng_below(&rng, 18)]);
        }
        if (at > 0) {
            text[at - 1] = '\0';
        }
        bt_alg alg;
        mo_parse_alg(text, &alg);
        bt_cube cube;
        mo_forward_state(&alg, &cube);
        bt_memo memo;
        mo_assert_solves(&cube, &memo);
        bt_memo again;
        bt_scheme_memo(&cube, &again);
        BT_CHECK(bt_scheme_memo_eq(&memo, &again));
        mo_check_partition(&cube, &memo);
        bt_alg_free(&alg);
    }
}

BT_TEST(memo_oracle, clean_vendor_rows_match_first_pair_and_ideal_cycle) {
    mo_load_simcheck();
    char names[48][8];
    uint8_t stickers[48];
    mo_sticker_of_name_init(names, stickers);
    size_t checked = 0;
    for (size_t i = 0; i < mo_sim_count; i++) {
        const mo_sim_row *row = &mo_sim[i];
        if (strcmp(row->sheet, "edge") != 0 && strcmp(row->sheet, "corner") != 0) {
            continue;
        }
        if (strcmp(row->match, "CYCLE_A") != 0 || strstr(row->note, "clean") == NULL) {
            continue;
        }
        bt_orbit orbit = strcmp(row->sheet, "edge") == 0 ? BT_ORBIT_EDGE : BT_ORBIT_CORNER;
        long long rowid = bt_test_parse_int(row->rowid);
        bt_alg alg;
        mo_alg_of_row(row->solver, row->sheet, rowid, &alg);
        char position_copy[96];
        snprintf(position_copy, sizeof position_copy, "%s", row->position);
        char *tokens[3];
        char *cursor = position_copy;
        for (int t = 0; t < 3; t++) {
            tokens[t] = cursor;
            char *dash = strchr(cursor, '-');
            if (dash != NULL && t < 2) {
                *dash = '\0';
                cursor = dash + 1;
            }
        }
        uint8_t h = mo_sticker_of_name(names, stickers, tokens[0]);
        uint8_t t1 = mo_sticker_of_name(names, stickers, tokens[1]);
        uint8_t t2 = mo_sticker_of_name(names, stickers, tokens[2]);
        bt_cube state;
        mo_solved_by_state(&alg, &state);
        bt_memo memo;
        bt_scheme_memo(&state, &memo);
        if (orbit == BT_ORBIT_EDGE) {
            BT_CHECK_EQ(memo.edge_pair_count, (size_t)1);
            BT_CHECK_EQ(memo.edge_pairs[0].t1, t1);
            BT_CHECK_EQ(memo.edge_pairs[0].t2, t2);
            BT_CHECK_EQ(memo.edge_buffers[0], h);
            BT_CHECK_EQ(memo.edge_letter_count, (size_t)2);
            BT_CHECK_EQ(memo.edge_letters[0], bt_scheme_letter_of(t1));
            BT_CHECK_EQ(memo.edge_letters[1], bt_scheme_letter_of(t2));
            BT_CHECK_EQ(memo.corner_pair_count, (size_t)0);
            BT_CHECK_EQ(memo.corner_letter_count, (size_t)0);
        } else {
            BT_CHECK_EQ(memo.corner_pair_count, (size_t)1);
            BT_CHECK_EQ(memo.corner_pairs[0].t1, t1);
            BT_CHECK_EQ(memo.corner_pairs[0].t2, t2);
            BT_CHECK_EQ(memo.corner_buffers[0], h);
            BT_CHECK_EQ(memo.corner_letter_count, (size_t)2);
            BT_CHECK_EQ(memo.corner_letters[0], bt_scheme_letter_of(t1));
            BT_CHECK_EQ(memo.corner_letters[1], bt_scheme_letter_of(t2));
            BT_CHECK_EQ(memo.edge_pair_count, (size_t)0);
            BT_CHECK_EQ(memo.edge_letter_count, (size_t)0);
        }
        bt_residue empty;
        memset(&empty, 0, sizeof empty);
        BT_CHECK(bt_scheme_residue_eq(&memo.residue, &empty));
        bt_scheme_cycles cycles;
        bt_scheme_ideal_cycle_at(orbit, h, t1, t2, &cycles);
        bt_cube forward;
        mo_forward_state(&alg, &forward);
        bool affected[48] = {false};
        for (uint8_t c = 0; c < cycles.count; c++) {
            for (uint8_t k = 0; k < 3; k++) {
                affected[cycles.items[c][k]] = true;
            }
            uint8_t a = cycles.items[c][0];
            uint8_t b = cycles.items[c][1];
            uint8_t e = cycles.items[c][2];
            BT_CHECK_EQ(bt_cube_position_of(&forward, a), b);
            BT_CHECK_EQ(bt_cube_position_of(&forward, b), e);
            BT_CHECK_EQ(bt_cube_position_of(&forward, e), a);
        }
        for (uint8_t sticker = 0; sticker < 48; sticker++) {
            if (!affected[sticker]) {
                BT_CHECK_EQ(bt_cube_position_of(&forward, sticker), sticker);
            }
        }
        mo_assert_solves(&state, &memo);
        bt_alg_free(&alg);
        checked++;
    }
    BT_CHECK_EQ(checked, (size_t)208);
}

static int mo_compare_u8(const void *a, const void *b) {
    return (int)*(const uint8_t *)a - (int)*(const uint8_t *)b;
}

BT_TEST(memo_oracle, flips_sheet_rows_leave_flip_pair_residue) {
    mo_load_simcheck();
    size_t checked = 0;
    for (size_t i = 0; i < mo_sim_count; i++) {
        const mo_sim_row *row = &mo_sim[i];
        if (strcmp(row->sheet, "flips") != 0) {
            continue;
        }
        long long rowid = bt_test_parse_int(row->rowid);
        bt_alg alg;
        mo_alg_of_row(row->solver, row->sheet, rowid, &alg);
        bt_cube state;
        mo_forward_state(&alg, &state);
        bt_memo memo;
        mo_assert_solves(&state, &memo);
        char position_copy[96];
        snprintf(position_copy, sizeof position_copy, "%s", row->position);
        char *parts[2];
        char *cursor = position_copy;
        parts[0] = cursor;
        char *amp = strstr(cursor, " & ");
        BT_CHECK(amp != NULL);
        *amp = '\0';
        parts[1] = amp + 3;
        uint8_t expected[2] = {bt_scheme_head_sticker(parts[0]), bt_scheme_head_sticker(parts[1])};
        qsort(expected, 2, 1, mo_compare_u8);
        uint8_t flips[BT_MEMO_MAX_FLIPS];
        memcpy(flips, memo.residue.flips, memo.residue.flip_count);
        qsort(flips, memo.residue.flip_count, 1, mo_compare_u8);
        BT_CHECK_EQ(memo.residue.flip_count, (uint8_t)2);
        BT_CHECK_EQ(flips[0], expected[0]);
        BT_CHECK_EQ(flips[1], expected[1]);
        BT_CHECK_EQ(memo.edge_pair_count, (size_t)0);
        BT_CHECK_EQ(memo.corner_pair_count, (size_t)0);
        BT_CHECK(!memo.residue.has_parity);
        BT_CHECK_EQ(memo.residue.twist_count, 0);
        bt_alg_free(&alg);
        checked++;
    }
    BT_CHECK_EQ(checked, (size_t)25);
}

static void mo_face_key(const char *name, uint8_t out[3]) {
    out[0] = 0;
    out[1] = 0;
    out[2] = 0;
    for (size_t i = 0; i < 3 && name[i] != '\0'; i++) {
        out[i] = (uint8_t)name[i];
    }
    for (int i = 0; i < 3; i++) {
        for (size_t j = i; j > 0 && out[j - 1] > out[j]; j--) {
            uint8_t tmp = out[j - 1];
            out[j - 1] = out[j];
            out[j] = tmp;
        }
    }
}

static bool mo_same_family(const char *a, const char *b) {
    uint8_t key_a[3];
    uint8_t key_b[3];
    mo_face_key(a, key_a);
    mo_face_key(b, key_b);
    return (key_a[0] == key_b[0] && key_a[1] == key_b[1] && key_a[2] == key_b[2]) != 0;
}

static int mo_twist_sign(const char *note, const char *tag) {
    char marker[8];
    snprintf(marker, sizeof marker, "%s", tag);
    const char *at = strstr(note, marker);
    BT_CHECK(at != NULL);
    if (at == NULL) {
        return 0;
    }
    at += strlen(tag);
    char signed_value[8];
    size_t len = 0;
    signed_value[len++] = at[0];
    at++;
    while (*at >= '0' && *at <= '9') {
        signed_value[len++] = *at++;
    }
    signed_value[len] = '\0';
    return (int)bt_test_parse_int(signed_value);
}

BT_TEST(memo_oracle, twists_sheet_rows_leave_signed_twist_pair_residue) {
    mo_load_simcheck();
    char names[48][8];
    uint8_t stickers[48];
    mo_sticker_of_name_init(names, stickers);
    size_t checked = 0;
    for (size_t i = 0; i < mo_sim_count; i++) {
        const mo_sim_row *row = &mo_sim[i];
        if (strcmp(row->sheet, "twists") != 0) {
            continue;
        }
        long long rowid = bt_test_parse_int(row->rowid);
        bt_alg alg;
        mo_alg_of_row(row->solver, row->sheet, rowid, &alg);
        bt_cube state;
        mo_forward_state(&alg, &state);
        bt_memo memo;
        mo_assert_solves(&state, &memo);
        char position_copy[96];
        snprintf(position_copy, sizeof position_copy, "%s", row->position);
        char *parts[2];
        char *cursor = position_copy;
        parts[0] = cursor;
        char *amp = strstr(cursor, " & ");
        BT_CHECK(amp != NULL);
        *amp = '\0';
        parts[1] = amp + 3;
        uint8_t expected_heads[2];
        uint8_t expected_quarters[2];
        for (int p = 0; p < 2; p++) {
            char cyc_tag[8];
            snprintf(cyc_tag, sizeof cyc_tag, "cyc%d=[", p + 1);
            const char *cyc = strstr(row->note, cyc_tag);
            BT_CHECK(cyc != NULL);
            if (cyc == NULL) {
                continue;
            }
            cyc += strlen(cyc_tag);
            char corner[8];
            size_t len = 0;
            while (cyc[len] != '\0' && cyc[len] != '>') {
                corner[len] = cyc[len];
                len++;
            }
            corner[len] = '\0';
            BT_CHECK(mo_same_family(corner, parts[p]));
            char d_tag[8];
            snprintf(d_tag, sizeof d_tag, "d%d=", p + 1);
            int d = mo_twist_sign(row->note, d_tag);
            expected_heads[p] = mo_sticker_of_name(names, stickers, parts[p]);
            expected_quarters[p] = d == 1 ? 2 : 1;
        }
        BT_CHECK_EQ(memo.residue.twist_count, (uint8_t)2);
        bool matched[BT_MEMO_MAX_TWISTS] = {false};
        for (int p = 0; p < 2; p++) {
            bool found = false;
            for (uint8_t t = 0; t < memo.residue.twist_count && !found; t++) {
                if (!matched[t] && memo.residue.twists[t].corner == expected_heads[p] &&
                    memo.residue.twists[t].quarter == expected_quarters[p]) {
                    matched[t] = true;
                    found = true;
                }
            }
            BT_CHECK(found);
        }
        BT_CHECK_EQ(memo.edge_pair_count, (size_t)0);
        BT_CHECK_EQ(memo.corner_pair_count, (size_t)0);
        BT_CHECK(!memo.residue.has_parity);
        BT_CHECK_EQ(memo.residue.flip_count, 0);
        bt_alg_free(&alg);
        checked++;
    }
    BT_CHECK_EQ(checked, (size_t)25);
}

BT_TEST(memo_oracle, parity_sheet_rows_leave_parity_residue) {
    mo_load_simcheck();
    size_t checked = 0;
    for (size_t i = 0; i < mo_sim_count; i++) {
        const mo_sim_row *row = &mo_sim[i];
        if (strcmp(row->sheet, "parity") != 0) {
            continue;
        }
        long long rowid = bt_test_parse_int(row->rowid);
        bt_alg alg;
        mo_alg_of_row(row->solver, row->sheet, rowid, &alg);
        bt_cube state;
        mo_forward_state(&alg, &state);
        bt_memo memo;
        mo_assert_solves(&state, &memo);
        char position_copy[96];
        snprintf(position_copy, sizeof position_copy, "%s", row->position);
        char *halves[2];
        char *cursor = position_copy;
        halves[0] = cursor;
        char *comma = strstr(cursor, ", ");
        BT_CHECK(comma != NULL);
        *comma = '\0';
        halves[1] = comma + 2;
        char *edge_tokens[2];
        edge_tokens[0] = halves[0];
        char *dash = strchr(halves[0], '-');
        BT_CHECK(dash != NULL);
        *dash = '\0';
        edge_tokens[1] = dash + 1;
        char *corner_tokens[2];
        corner_tokens[0] = halves[1];
        dash = strchr(halves[1], '-');
        BT_CHECK(dash != NULL);
        *dash = '\0';
        corner_tokens[1] = dash + 1;
        BT_CHECK(memo.residue.has_parity);
        BT_CHECK_EQ(memo.residue.parity.edge[0], bt_scheme_head_sticker(edge_tokens[0]));
        BT_CHECK_EQ(memo.residue.parity.edge[1], bt_scheme_head_sticker(edge_tokens[1]));
        BT_CHECK_EQ(memo.residue.parity.corner[0], bt_scheme_head_sticker(corner_tokens[0]));
        BT_CHECK_EQ(memo.residue.parity.corner[1], bt_scheme_head_sticker(corner_tokens[1]));
        BT_CHECK_EQ(memo.edge_pair_count, (size_t)0);
        BT_CHECK_EQ(memo.corner_pair_count, (size_t)0);
        BT_CHECK(memo.edge_letter_count <= 1);
        BT_CHECK(memo.corner_letter_count <= 1);
        if (strstr(row->note, "clean") != NULL) {
            BT_CHECK_EQ(memo.residue.flip_count, 0);
            BT_CHECK_EQ(memo.residue.twist_count, 0);
            bt_residue_class class = bt_scheme_residue_class(&memo.residue);
            BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_PARITY);
        }
        bt_alg_free(&alg);
        checked++;
    }
    BT_CHECK_EQ(checked, (size_t)28);
}

BT_TEST(memo_oracle, ltct_sheet_rows_leave_ltct_residue) {
    mo_load_simcheck();
    size_t checked = 0;
    for (size_t i = 0; i < mo_sim_count; i++) {
        const mo_sim_row *row = &mo_sim[i];
        if (strcmp(row->sheet, "ltct") != 0) {
            continue;
        }
        long long rowid = bt_test_parse_int(row->rowid);
        bt_alg alg;
        mo_alg_of_row(row->solver, row->sheet, rowid, &alg);
        bt_cube state;
        mo_forward_state(&alg, &state);
        bt_memo memo;
        mo_assert_solves(&state, &memo);
        char position_copy[96];
        snprintf(position_copy, sizeof position_copy, "%s", row->position);
        char *halves[2];
        char *cursor = position_copy;
        halves[0] = cursor;
        char *comma = strstr(cursor, ", ");
        BT_CHECK(comma != NULL);
        *comma = '\0';
        halves[1] = comma + 2;
        char *edge_tokens[2];
        edge_tokens[0] = halves[0];
        char *dash = strchr(halves[0], '-');
        BT_CHECK(dash != NULL);
        *dash = '\0';
        edge_tokens[1] = dash + 1;
        char *bracket_start = strchr(halves[1], '[');
        BT_CHECK(bracket_start != NULL);
        char *close = strchr(bracket_start, ']');
        BT_CHECK(close != NULL);
        *bracket_start = '\0';
        *close = '\0';
        const char *bracket = bracket_start + 1;
        char *corner_tokens[2];
        corner_tokens[0] = halves[1];
        dash = strchr(halves[1], '-');
        BT_CHECK(dash != NULL);
        *dash = '\0';
        corner_tokens[1] = dash + 1;
        BT_CHECK(memo.residue.has_parity);
        BT_CHECK_EQ(memo.residue.parity.edge[0], bt_scheme_head_sticker(edge_tokens[0]));
        BT_CHECK_EQ(memo.residue.parity.edge[1], bt_scheme_head_sticker(edge_tokens[1]));
        BT_CHECK_EQ(memo.residue.parity.corner[0], bt_scheme_head_sticker(corner_tokens[0]));
        BT_CHECK_EQ(memo.residue.parity.corner[1], bt_scheme_head_sticker(corner_tokens[1]));
        BT_CHECK(memo.residue.twist_count == 2 || memo.residue.twist_count == 3);
        bool bracket_twisted = false;
        for (uint8_t t = 0; t < memo.residue.twist_count; t++) {
            if (memo.residue.twists[t].corner == bt_scheme_head_sticker(bracket)) {
                bracket_twisted = true;
            }
        }
        BT_CHECK(bracket_twisted);
        BT_CHECK_EQ(memo.residue.flip_count, 0);
        BT_CHECK_EQ(memo.edge_pair_count, (size_t)0);
        BT_CHECK_EQ(memo.corner_pair_count, (size_t)0);
        bt_residue_class class = bt_scheme_residue_class(&memo.residue);
        BT_CHECK_EQ((int)class.kind, (int)BT_RESIDUE_LTCT);
        bt_alg_free(&alg);
        checked++;
    }
    BT_CHECK_EQ(checked, (size_t)11);
}
