#include "common.h"
#include "scheme/classifier.h"
#include "scheme/memo.h"
#include "scheme/scheme.h"

#include "common/bt_harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { ORACLE_ROWS_MAX = 1100 };

typedef struct {
    char solver[32];
    char sheet[16];
    char rowid[16];
    char position[96];
    char alg[256];
    long long qtm;
    char note[512];
} bt_oracle_row;

static bt_oracle_row g_rows[ORACLE_ROWS_MAX];
static size_t g_row_count;

typedef struct {
    char solver[32];
    char sheet[16];
    char rowid[16];
    char position[96];
    long long qtm;
    char note[512];
    bool used;
} bt_simcheck_row;

static bt_simcheck_row g_sim[ORACLE_ROWS_MAX];
static size_t g_sim_count;

static void load_simcheck(void) {
    g_sim_count = 0;
    char path[BT_PATH_MAX];
    bt_test_ref_path(path, "simcheck.tsv");
    bt_test_lines lines;
    bt_test_lines_begin(&lines, path);
    char *header = bt_test_lines_next(&lines);
    BT_CHECK(header != NULL);
    BT_CHECK_EQ(header, "solver\tsheet\trowid\tposition\tmoves\tqtm\tmatch\tnote");
    char *line;
    while ((line = bt_test_lines_next(&lines)) != NULL) {
        if (*line == '\0') {
            continue;
        }
        char *cols[8];
        if (bt_tsv_split(line, cols, 8) != 8) {
            BT_ABORT("simcheck row width: %s", line);
        }
        if (g_sim_count >= ORACLE_ROWS_MAX) {
            BT_ABORT("simcheck row budget exceeded");
        }
        bt_simcheck_row *row = &g_sim[g_sim_count++];
        snprintf(row->solver, sizeof row->solver, "%s", cols[0]);
        snprintf(row->sheet, sizeof row->sheet, "%s", cols[1]);
        snprintf(row->rowid, sizeof row->rowid, "%s", cols[2]);
        snprintf(row->position, sizeof row->position, "%s", cols[3]);
        row->qtm = bt_test_parse_int(cols[5]);
        snprintf(row->note, sizeof row->note, "%s", cols[7]);
        row->used = false;
    }
    bt_test_lines_end(&lines);
}

static void load_rows(void) {
    load_simcheck();
    g_row_count = 0;
    char path[BT_PATH_MAX];
    bt_test_ref_path(path, "rows.tsv");
    bt_test_lines lines;
    bt_test_lines_begin(&lines, path);
    char *line;
    while ((line = bt_test_lines_next(&lines)) != NULL) {
        if (*line == '\0') {
            continue;
        }
        char *cols[5];
        if (bt_tsv_split(line, cols, 5) != 5) {
            BT_ABORT("rows row width: %s", line);
        }
        bt_simcheck_row *match = NULL;
        for (size_t i = 0; i < g_sim_count && match == NULL; i++) {
            if (strcmp(g_sim[i].solver, cols[0]) == 0 && strcmp(g_sim[i].sheet, cols[1]) == 0 &&
                strcmp(g_sim[i].rowid, cols[2]) == 0) {
                match = &g_sim[i];
            }
        }
        if (match == NULL) {
            BT_ABORT("simcheck row missing for %s", line);
        }
        match->used = true;
        BT_CHECK_EQ(match->position, cols[3]);
        if (g_row_count >= ORACLE_ROWS_MAX) {
            BT_ABORT("rows row budget exceeded");
        }
        bt_oracle_row *row = &g_rows[g_row_count++];
        snprintf(row->solver, sizeof row->solver, "%s", cols[0]);
        snprintf(row->sheet, sizeof row->sheet, "%s", cols[1]);
        snprintf(row->rowid, sizeof row->rowid, "%s", cols[2]);
        snprintf(row->position, sizeof row->position, "%s", cols[3]);
        snprintf(row->alg, sizeof row->alg, "%s", cols[4]);
        row->qtm = match->qtm;
        snprintf(row->note, sizeof row->note, "%s", match->note);
    }
    bt_test_lines_end(&lines);
    for (size_t i = 0; i < g_sim_count; i++) {
        BT_CHECK(g_sim[i].used);
    }
}

static void applied(const char *alg_text, uint8_t occ[54]) {
    bt_cube cube = bt_cube_solved();
    bt_alg alg;
    bt_alg_init(&alg);
    char err[128];
    if (!bt_alg_parse(alg_text, &alg, err, sizeof err)) {
        BT_ABORT("parse %s failed: %s", alg_text, err);
    }
    bt_cube_apply_alg(&cube, &alg);
    bt_alg_free(&alg);
    bt_scheme_state_array(&cube, occ);
}

static bool sheet_is(const bt_oracle_row *row, const char *sheet) {
    return strcmp(row->sheet, sheet) == 0;
}

BT_TEST(classifier_oracle, three_style_rows_reproduce_positions_exactly) {
    load_rows();
    size_t styled = 0;
    for (size_t i = 0; i < g_row_count; i++) {
        const bt_oracle_row *row = &g_rows[i];
        if (!sheet_is(row, "edge") && !sheet_is(row, "corner")) {
            continue;
        }
        styled++;
        uint8_t occ[54];
        applied(row->alg, occ);
        bt_report report;
        bt_scheme_classify(occ, &report);
        bt_orbit expected = sheet_is(row, "edge") ? BT_ORBIT_EDGE : BT_ORBIT_CORNER;
        BT_CHECK_EQ((int)report.class.kind, (int)BT_CLASS_THREE_STYLE);
        if (report.class.kind != BT_CLASS_THREE_STYLE) {
            continue;
        }
        BT_CHECK_EQ((int)report.class.orbit, (int)expected);
        BT_CHECK_EQ(report.class.case_count, (size_t)1);
        char position[BT_CLASS_POSITION_MAX];
        BT_CHECK(bt_scheme_report_position(&report, position));
        BT_CHECK_EQ(position, row->position);
    }
    BT_CHECK_EQ(styled, (size_t)988);
}

BT_TEST(classifier_oracle, endgame_rows_class_and_named_pieces_match) {
    load_rows();
    size_t endgame = 0;
    for (size_t i = 0; i < g_row_count; i++) {
        const bt_oracle_row *row = &g_rows[i];
        if (!sheet_is(row, "flips") && !sheet_is(row, "twists") && !sheet_is(row, "parity") &&
            !sheet_is(row, "ltct")) {
            continue;
        }
        endgame++;
        uint8_t occ[54];
        applied(row->alg, occ);
        bt_report report;
        bt_scheme_classify(occ, &report);
        char position[BT_CLASS_POSITION_MAX];
        if (sheet_is(row, "flips")) {
            BT_CHECK_EQ((int)report.class.kind, (int)BT_CLASS_FLIP_PAIR);
            if (report.class.kind != BT_CLASS_FLIP_PAIR) {
                continue;
            }
            char first[16];
            char second[16];
            snprintf(first, sizeof first, "%s", bt_scheme_name_of(report.class.first));
            snprintf(second, sizeof second, "%s", bt_scheme_name_of(report.class.second));
            char want_first[16];
            char want_second[16];
            BT_CHECK(sscanf(row->position, "%15s & %15s", want_first, want_second) == 2);
            BT_CHECK((strcmp(first, want_first) == 0 && strcmp(second, want_second) == 0) ||
                     (strcmp(first, want_second) == 0 && strcmp(second, want_first) == 0));
        } else if (sheet_is(row, "twists")) {
            BT_CHECK_EQ((int)report.class.kind, (int)BT_CLASS_TWIST_PAIR);
        } else {
            BT_CHECK(bt_scheme_report_position(&report, position));
            BT_CHECK_EQ(position, row->position);
        }
    }
    BT_CHECK_EQ(endgame, (size_t)89);
}

static size_t defect_tokens(const char *note, char keys[24][16], char vals[24][16]) {
    const char *body = strstr(note, "defects:");
    BT_CHECK(body != NULL);
    body += strlen("defects:");
    char clean[512];
    snprintf(clean, sizeof clean, "%s", body);
    size_t len = strlen(clean);
    if (len >= 6 && strcmp(clean + len - 6, " clean") == 0) {
        clean[len - 6] = '\0';
    }
    size_t count = 0;
    char *cursor = clean;
    while (*cursor != '\0') {
        while (*cursor == ',' || *cursor == ' ') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        char *token = cursor;
        while (*cursor != '\0' && *cursor != ',' && *cursor != ' ') {
            cursor++;
        }
        if (*cursor != '\0') {
            *cursor++ = '\0';
        }
        char *eq = strchr(token, '=');
        BT_CHECK(eq != NULL);
        if (eq == NULL) {
            continue;
        }
        *eq = '\0';
        snprintf(keys[count], 16, "%s", token);
        snprintf(vals[count], 16, "%s", eq + 1);
        count++;
    }
    return count;
}

static const char *token_value(const char *key, char keys[24][16], char vals[24][16], size_t count,
                               const char *fallback) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(keys[i], key) == 0) {
            return vals[i];
        }
    }
    return fallback;
}

BT_TEST(classifier_oracle, defect_notes_match_simcheck_on_carrying_rows) {
    load_rows();
    size_t carrying = 0;
    for (size_t i = 0; i < g_row_count; i++) {
        const bt_oracle_row *row = &g_rows[i];
        if (strncmp(row->note, "defects:", 8) != 0) {
            continue;
        }
        carrying++;
        uint8_t occ[54];
        applied(row->alg, occ);
        bt_report report;
        bt_scheme_classify(occ, &report);
        char keys[24][16];
        char vals[24][16];
        size_t count = defect_tokens(row->note, keys, vals);
        BT_CHECK(count > 0);
        for (int e = 0; e < 12; e++) {
            const char *piece = BT_SCHEME_EDGE_FLOATING[e];
            uint8_t head = bt_scheme_head_sticker(piece);
            const char *got = bt_scheme_defect_flip(&report.defects, head) ? "flip" : "ok";
            BT_CHECK_EQ(got, token_value(piece, keys, vals, count, "ok"));
        }
        for (int c = 0; c < 8; c++) {
            const char *piece = BT_SCHEME_CORNER_FLOATING[c];
            uint8_t head = bt_scheme_head_sticker(piece);
            char got[8];
            snprintf(got, sizeof got, "%+d", (int)bt_scheme_defect_twist(&report.defects, head));
            BT_CHECK_EQ(got, token_value(piece, keys, vals, count, "+0"));
        }
        for (size_t k = 0; k < count; k++) {
            bool known = false;
            for (int e = 0; e < 12; e++) {
                if (strcmp(BT_SCHEME_EDGE_FLOATING[e], keys[k]) == 0) {
                    known = true;
                }
            }
            for (int c = 0; c < 8; c++) {
                if (strcmp(BT_SCHEME_CORNER_FLOATING[c], keys[k]) == 0) {
                    known = true;
                }
            }
            BT_CHECK(known);
        }
    }
    BT_CHECK_EQ(carrying, (size_t)1016);
}

static int parse_signed(const char *text) {
    return (int)bt_test_parse_int(text);
}

BT_TEST(classifier_oracle, twist_pair_positions_and_signed_deltas_match_simcheck) {
    load_rows();
    size_t twists = 0;
    for (size_t i = 0; i < g_row_count; i++) {
        const bt_oracle_row *row = &g_rows[i];
        if (!sheet_is(row, "twists")) {
            continue;
        }
        twists++;
        uint8_t occ[54];
        applied(row->alg, occ);
        bt_report report;
        bt_scheme_classify(occ, &report);
        char position[BT_CLASS_POSITION_MAX];
        BT_CHECK(bt_scheme_report_position(&report, position));
        BT_CHECK_EQ(position, row->position);
        char first[16];
        char second[16];
        BT_CHECK(sscanf(row->position, "%15s & %15s", first, second) == 2);
        int deltas[2];
        const char *tags[2] = {"d1=", "d2="};
        for (int d = 0; d < 2; d++) {
            const char *at = strstr(row->note, tags[d]);
            BT_CHECK(at != NULL);
            if (at == NULL) {
                continue;
            }
            const char *tail = at + strlen(tags[d]);
            char signed_value[8];
            size_t at_out = 0;
            signed_value[at_out++] = tail[0];
            tail++;
            while (*tail >= '0' && *tail <= '9') {
                if (at_out + 1 >= sizeof signed_value) {
                    BT_ABORT("twist value too long in %s", row->note);
                }
                signed_value[at_out++] = *tail++;
            }
            signed_value[at_out] = '\0';
            deltas[d] = parse_signed(signed_value);
        }
        BT_CHECK_EQ((int)bt_scheme_defect_twist(&report.defects, bt_scheme_head_sticker(first)),
                    deltas[0]);
        BT_CHECK_EQ((int)bt_scheme_defect_twist(&report.defects, bt_scheme_head_sticker(second)),
                    deltas[1]);
        BT_CHECK_EQ(deltas[0] + deltas[1], 0);
    }
    BT_CHECK_EQ(twists, (size_t)25);
}

static char *next_space_token(char **cursor) {
    while (**cursor == ' ') {
        (*cursor)++;
    }
    if (**cursor == '\0') {
        return NULL;
    }
    char *token = *cursor;
    while (**cursor != '\0' && **cursor != ' ') {
        (*cursor)++;
    }
    if (**cursor != '\0') {
        *(*cursor)++ = '\0';
    }
    return token;
}

static void split_tw_chunk(const char *chunk, char *piece, size_t piece_cap, int *value,
                           const char *next_chunk) {
    char buffer[256];
    snprintf(buffer, sizeof buffer, "%s", chunk);
    char *cursor = buffer;
    char *found = NULL;
    char *token = next_space_token(&cursor);
    while (token != NULL) {
        if (found == NULL && strstr(token, "->") != NULL) {
            found = token;
        }
        token = next_space_token(&cursor);
    }
    BT_CHECK(found != NULL);
    if (found == NULL) {
        return;
    }
    char *arrow = strstr(found, "->");
    *arrow = '\0';
    snprintf(piece, piece_cap, "%s", found);
    char next[256];
    snprintf(next, sizeof next, "%s", next_chunk);
    char *next_cursor = next;
    char *first = next_space_token(&next_cursor);
    BT_CHECK(first != NULL);
    if (first == NULL) {
        return;
    }
    *value = parse_signed(first);
}

BT_TEST(classifier_oracle, ltct_corner_twists_match_note_and_law_holds) {
    load_rows();
    size_t ltct = 0;
    for (size_t i = 0; i < g_row_count; i++) {
        const bt_oracle_row *row = &g_rows[i];
        if (!sheet_is(row, "ltct")) {
            continue;
        }
        ltct++;
        uint8_t occ[54];
        applied(row->alg, occ);
        bt_report report;
        bt_scheme_classify(occ, &report);
        BT_CHECK_EQ((int)report.class.kind, (int)BT_CLASS_LTCT);
        if (report.class.kind != BT_CLASS_LTCT) {
            continue;
        }
        const char *corners_part = strstr(row->note, " | corners: ");
        BT_CHECK(corners_part != NULL);
        if (corners_part == NULL) {
            continue;
        }
        corners_part += strlen(" | corners: ");
        char chunks[5][192];
        size_t chunk_count = 0;
        const char *cursor = corners_part;
        while (chunk_count < 5) {
            const char *tw = strstr(cursor, " tw=");
            size_t len = tw != NULL ? (size_t)(tw - cursor) : strlen(cursor);
            snprintf(chunks[chunk_count], 192, "%.*s", (int)len, cursor);
            chunk_count++;
            if (tw == NULL) {
                break;
            }
            cursor = tw + 4;
        }
        BT_CHECK_EQ(chunk_count, (size_t)4);
        for (int c = 0; c < 3; c++) {
            char piece[16];
            int value = 0;
            split_tw_chunk(chunks[c], piece, sizeof piece, &value, chunks[c + 1]);
            BT_CHECK_EQ((int)bt_scheme_defect_twist(&report.defects, bt_scheme_head_sticker(piece)),
                        value);
        }
        int sum = 0;
        for (int c = 0; c < 8; c++) {
            sum += bt_scheme_defect_twist(&report.defects,
                                          bt_scheme_head_sticker(BT_SCHEME_CORNER_FLOATING[c]));
        }
        BT_CHECK_EQ(sum % 3, 0);
        BT_CHECK(bt_scheme_defect_twist(&report.defects, bt_scheme_head_sticker(bt_scheme_name_of(
                                                             report.class.bracket))) != 0);
        BT_CHECK(!bt_scheme_defect_flip(&report.defects, report.class.e1));
        BT_CHECK(!bt_scheme_defect_flip(&report.defects, report.class.e2));
        const char *edges_part = strstr(row->note, " | edges: ");
        BT_CHECK(edges_part != NULL);
        if (edges_part == NULL) {
            continue;
        }
        edges_part += strlen(" | edges: ");
        char edges[128];
        const char *bar = strstr(edges_part, " |");
        snprintf(edges, sizeof edges, "%.*s",
                 bar != NULL ? (int)(bar - edges_part) : (int)strlen(edges_part), edges_part);
        char e1_name[16];
        char e2_name[16];
        snprintf(e1_name, sizeof e1_name, "%s", bt_scheme_name_of(report.class.e1));
        snprintf(e2_name, sizeof e2_name, "%s", bt_scheme_name_of(report.class.e2));
        bool has_e1 = false;
        bool has_e2 = false;
        int token_count = 0;
        char *edges_cursor = edges;
        char *token = next_space_token(&edges_cursor);
        while (token != NULL) {
            token_count++;
            char *arrow = strstr(token, "->");
            BT_CHECK(arrow != NULL);
            if (arrow != NULL) {
                *arrow = '\0';
                if (strcmp(token, e1_name) == 0) {
                    has_e1 = true;
                }
                if (strcmp(token, e2_name) == 0) {
                    has_e2 = true;
                }
            }
            token = next_space_token(&edges_cursor);
        }
        BT_CHECK(has_e1 && has_e2 && token_count == 2);
    }
    BT_CHECK_EQ(ltct, (size_t)11);
}

BT_TEST(classifier_oracle, qtm_agrees_with_simcheck_on_every_row) {
    load_rows();
    BT_CHECK_EQ(g_row_count, (size_t)1077);
    for (size_t i = 0; i < g_row_count; i++) {
        const bt_oracle_row *row = &g_rows[i];
        bt_alg alg;
        bt_alg_init(&alg);
        char err[128];
        BT_CHECK(bt_alg_parse(row->alg, &alg, err, sizeof err));
        bt_report report;
        bt_scheme_classify_alg(&alg, &report);
        BT_CHECK(report.has_qtm);
        BT_CHECK_EQ((long long)report.qtm, row->qtm);
        bt_alg_free(&alg);
    }
}

BT_TEST(classifier_oracle, every_row_trace_stays_within_its_orbit) {
    load_rows();
    for (size_t i = 0; i < g_row_count; i++) {
        const bt_oracle_row *row = &g_rows[i];
        uint8_t occ[54];
        applied(row->alg, occ);
        bt_orbit orbits[2] = {BT_ORBIT_EDGE, BT_ORBIT_CORNER};
        for (int o = 0; o < 2; o++) {
            bt_trace trace;
            bt_scheme_trace(occ, orbits[o], &trace);
            for (size_t b = 0; b < trace.block_count; b++) {
                for (size_t s = 0; s < trace.block_len[b]; s++) {
                    uint8_t sticker = trace.blocks[b][s];
                    BT_CHECK(sticker >= bt_scheme_orbit_lo(orbits[o]));
                    BT_CHECK(sticker < bt_scheme_orbit_hi(orbits[o]));
                }
            }
        }
    }
}
