#include "common.h"
#include "core/analysis.h"
#include "core/cube.h"

enum { DS_NONE = 127 };

typedef struct {
    bool has_flip[12];
    bool flipped[12];
    bool has_twist[8];
    int8_t twist[8];
} defects_map;

typedef struct {
    char from[TC_NAME_MAX];
    char to[TC_NAME_MAX];
} name_pair;

typedef struct {
    char piece[TC_NAME_MAX];
    char slot[TC_NAME_MAX];
    int8_t dir;
    name_pair moves[4];
    size_t moves_len;
} ltct_corner;

static void record_defect(const tc_name_index *names, defects_map *map, const char *name,
                          const char *value) {
    uint8_t sticker = tc_sticker_of_name(names, name);
    bt_piece piece = bt_piece_of(sticker);
    bool is_flip = strcmp(value, "ok") == 0;
    if (!is_flip) {
        is_flip = strcmp(value, "flip") == 0;
    }
    if (is_flip && piece.kind != BT_PIECE_EDGE) {
        BT_ABORT("flip defect %s names a non-edge piece", name);
    }
    if (!is_flip && piece.kind != BT_PIECE_CORNER) {
        BT_ABORT("twist defect %s names a non-corner piece", name);
    }
    if (strcmp(value, "ok") == 0) {
        map->has_flip[piece.index] = true;
        map->flipped[piece.index] = false;
    } else if (strcmp(value, "flip") == 0) {
        map->has_flip[piece.index] = true;
        map->flipped[piece.index] = true;
    } else if (strcmp(value, "+0") == 0 || strcmp(value, "-0") == 0) {
        map->has_twist[piece.index] = true;
        map->twist[piece.index] = 0;
    } else if (strcmp(value, "+1") == 0) {
        map->has_twist[piece.index] = true;
        map->twist[piece.index] = 1;
    } else if (strcmp(value, "-1") == 0) {
        map->has_twist[piece.index] = true;
        map->twist[piece.index] = -1;
    } else {
        BT_ABORT("unknown defect value %s", value);
    }
}

static void parse_defects(const tc_name_index *names, const char *note, defects_map *map) {
    for (size_t i = 0; i < 12; i++) {
        map->has_flip[i] = false;
    }
    for (size_t i = 0; i < 8; i++) {
        map->has_twist[i] = false;
    }
    const char *rest = note;
    if (strncmp(rest, "defects:", 8) == 0) {
        rest += 8;
    }
    char work[TC_LINE_MAX];
    snprintf(work, sizeof work, "%s", rest);
    char *tokens[64];
    size_t token_len = tc_split_ws(work, tokens, 64);
    for (size_t t = 0; t < token_len; t++) {
        if (strcmp(tokens[t], "clean") == 0) {
            continue;
        }
        char *entries[16];
        size_t entry_len = tc_split_on(tokens[t], ',', entries, 16);
        for (size_t e = 0; e < entry_len; e++) {
            if (entries[e][0] == '\0') {
                continue;
            }
            char *eq = strchr(entries[e], '=');
            if (eq == NULL) {
                BT_ABORT("defect entry %s lacks =", entries[e]);
            }
            *eq = '\0';
            record_defect(names, map, entries[e], eq + 1);
        }
    }
}

static void assert_defects_match(const defects_map *map, const bt_cube *cube,
                                 const tc_name_index *names, const char *ctx) {
    bt_state_summary summary = bt_summarize(cube);
    for (size_t slot = 0; slot < 8; slot++) {
        uint8_t piece = summary.corners[slot].piece;
        int8_t expected = map->has_twist[piece] ? map->twist[piece] : 0;
        if (summary.corners[slot].twist != expected) {
            BT_ABORT("%s corner slot %zu piece %s", ctx, slot,
                     names->name_of_sticker[bt_head_sticker((bt_piece){BT_PIECE_CORNER, piece})]);
        }
    }
    for (size_t slot = 0; slot < 12; slot++) {
        uint8_t piece = summary.edges[slot].piece;
        bool expected = false;
        if (map->has_flip[piece]) {
            expected = map->flipped[piece];
        }
        if (summary.edges[slot].flipped != expected) {
            BT_ABORT("%s edge slot %zu piece %s", ctx, slot,
                     names->name_of_sticker[bt_head_sticker((bt_piece){BT_PIECE_EDGE, piece})]);
        }
    }
}

static size_t parse_bracket_pairs(const char *inner, name_pair *out, size_t cap) {
    size_t count = 0;
    char work[TC_LINE_MAX];
    snprintf(work, sizeof work, "%s", inner);
    char *tokens[8];
    size_t token_len = tc_split_ws(work, tokens, 8);
    for (size_t t = 0; t < token_len; t++) {
        char *token = tokens[t];
        char *arrow = strstr(token, ">");
        if (arrow == NULL) {
            BT_ABORT("pair token %s lacks >", token);
        }
        *arrow = '\0';
        if (count < cap) {
            snprintf(out[count].from, TC_NAME_MAX, "%s", token);
            snprintf(out[count].to, TC_NAME_MAX, "%s", arrow + 1);
        }
        count += 1;
    }
    return count;
}

static const char *after_token(const char *tail) {
    while (*tail != ' ' && *tail != '\t' && *tail != '\0') {
        tail++;
    }
    while (*tail == ' ' || *tail == '\t') {
        tail++;
    }
    return tail;
}

static size_t parse_twists_note(const char *note, ltct_corner *out, size_t cap) {
    size_t count = 0;
    const char *rest = note;
    while (*rest != '\0') {
        const char *open = strchr(rest, '[');
        if (open == NULL) {
            break;
        }
        const char *close = strchr(open, ']');
        if (close == NULL) {
            BT_ABORT("twists note %s lacks ]", note);
        }
        char inner[TC_LINE_MAX];
        size_t inner_len = (size_t)(close - open - 1);
        if (inner_len >= sizeof inner) {
            BT_ABORT("twists bracket too long");
        }
        memcpy(inner, open + 1, inner_len);
        inner[inner_len] = '\0';
        const char *after = close + 1;
        const char *dir_at = strchr(after, 'd');
        if (dir_at == NULL) {
            BT_ABORT("twists note lacks d");
        }
        const char *tail = dir_at;
        const char *eq = strchr(tail, '=');
        if (eq == NULL) {
            BT_ABORT("twists note lacks =");
        }
        tail = eq + 1;
        long value = strtol(tail, NULL, 10);
        if (count < cap) {
            out[count].moves_len = parse_bracket_pairs(inner, out[count].moves, 4);
            if (out[count].moves_len != 3) {
                BT_ABORT("twists cycle needs 3 pairs: %s", inner);
            }
            out[count].dir = (int8_t)value;
            out[count].piece[0] = '\0';
            out[count].slot[0] = '\0';
        }
        count += 1;
        rest = after_token(tail);
    }
    return count;
}

typedef struct {
    name_pair edges[4];
    size_t edges_len;
    ltct_corner corners[3];
    size_t corners_len;
} ltct_note;

static void parse_ltct_note(const char *note, ltct_note *out) {
    char parts[3][TC_LINE_MAX];
    size_t part = 0;
    size_t fill = 0;
    for (const char *cursor = note; *cursor != '\0'; cursor++) {
        if (cursor[0] == ' ' && cursor[1] == '|' && cursor[2] == ' ') {
            parts[part][fill] = '\0';
            part += 1;
            fill = 0;
            cursor += 2;
            if (part >= 3) {
                BT_ABORT("ltct note %s has too many parts", note);
            }
            continue;
        }
        if (fill + 1 >= TC_LINE_MAX) {
            BT_ABORT("ltct part too long");
        }
        parts[part][fill++] = *cursor;
    }
    parts[part][fill] = '\0';
    if (part != 2) {
        BT_ABORT("ltct note %s needs 3 parts", note);
    }
    const char *edges_part = parts[1];
    if (strncmp(edges_part, "edges: ", 7) != 0) {
        BT_ABORT("ltct note %s lacks edges part", note);
    }
    edges_part += 7;
    out->edges_len = 0;
    char work[TC_LINE_MAX];
    snprintf(work, sizeof work, "%s", edges_part);
    char *tokens[8];
    size_t token_len = tc_split_ws(work, tokens, 8);
    for (size_t t = 0; t < token_len; t++) {
        char *token = tokens[t];
        char from[TC_NAME_MAX];
        char to[TC_NAME_MAX];
        char *arrow = strstr(token, "->");
        if (arrow == NULL) {
            BT_ABORT("edge token %s lacks ->", token);
        }
        *arrow = '\0';
        snprintf(from, sizeof from, "%s", token);
        snprintf(to, sizeof to, "%s", arrow + 2);
        if (out->edges_len < 4) {
            snprintf(out->edges[out->edges_len].from, TC_NAME_MAX, "%s", from);
            snprintf(out->edges[out->edges_len].to, TC_NAME_MAX, "%s", to);
        }
        out->edges_len += 1;
    }
    const char *corners_part = parts[2];
    if (strncmp(corners_part, "corners: ", 9) != 0) {
        BT_ABORT("ltct note %s lacks corners part", note);
    }
    corners_part += 9;
    out->corners_len = 0;
    const char *rest = corners_part;
    while (*rest != '\0') {
        const char *arrow = strchr(rest, '-');
        if (arrow == NULL || arrow[1] != '>') {
            BT_ABORT("ltct corner %s lacks ->", rest);
        }
        const char *open = strchr(arrow, '[');
        if (open == NULL) {
            BT_ABORT("ltct corner %s lacks [", rest);
        }
        ltct_corner corner;
        size_t piece_len = (size_t)(arrow - rest);
        while (piece_len > 0 && (rest[piece_len - 1] == ' ' || rest[piece_len - 1] == '\t')) {
            piece_len -= 1;
        }
        snprintf(corner.piece, TC_NAME_MAX, "%.*s", (int)piece_len, rest);
        size_t slot_len = (size_t)(open - arrow - 2);
        while (slot_len > 0 &&
               (arrow[2 + slot_len - 1] == ' ' || arrow[2 + slot_len - 1] == '\t')) {
            slot_len -= 1;
        }
        snprintf(corner.slot, TC_NAME_MAX, "%.*s", (int)slot_len, arrow + 2);
        const char *close = strchr(open, ']');
        if (close == NULL) {
            BT_ABORT("ltct corner %s lacks ]", rest);
        }
        char inner[TC_LINE_MAX];
        size_t inner_len = (size_t)(close - open - 1);
        memcpy(inner, open + 1, inner_len);
        inner[inner_len] = '\0';
        corner.moves_len = parse_bracket_pairs(inner, corner.moves, 4);
        const char *after = close + 1;
        const char *tw_at = strstr(after, "tw=");
        if (tw_at == NULL) {
            BT_ABORT("ltct corner %s lacks tw=", rest);
        }
        const char *tail = tw_at + 3;
        corner.dir = (int8_t)strtol(tail, NULL, 10);
        if (out->corners_len < 3) {
            out->corners[out->corners_len] = corner;
        }
        out->corners_len += 1;
        rest = after_token(tail);
    }
}

static bt_cube apply(const char *text) {
    bt_alg alg;
    bt_alg_init(&alg);
    char err[64];
    if (!bt_alg_parse(text, &alg, err, sizeof err)) {
        BT_ABORT("parse failed for %s: %s", text, err);
    }
    bt_cube cube = bt_cube_solved();
    bt_cube_apply_alg(&cube, &alg);
    bt_alg_free(&alg);
    return cube;
}

BT_TEST(analysis_sheets, all_vendor_algs_keep_flip_parity_even_and_twist_sum_zero) {
    static tc_simcheck_rows rows;
    tc_load_simcheck_rows(&rows);
    BT_CHECK_EQ(rows.count, (size_t)1077);
    for (size_t i = 0; i < rows.count; i++) {
        size_t rowid = (size_t)strtoul(rows.cols[i][2], NULL, 10);
        char alg_text[TC_TEXT_MAX];
        tc_sheet_alg(rows.cols[i][0], rows.cols[i][1], rowid, alg_text, sizeof alg_text);
        bt_cube cube = apply(alg_text);
        bt_state_summary summary = bt_summarize(&cube);
        size_t flips = 0;
        for (size_t e = 0; e < 12; e++) {
            if (summary.edges[e].flipped) {
                flips += 1;
            }
        }
        if (flips % 2 != 0) {
            BT_ABORT("%s %s row %s: odd flips", rows.cols[i][0], rows.cols[i][1], rows.cols[i][2]);
        }
        int32_t twist_sum = 0;
        for (size_t c = 0; c < 8; c++) {
            twist_sum += summary.corners[c].twist;
        }
        if (((twist_sum % 3) + 3) % 3 != 0) {
            BT_ABORT("%s %s row %s: twist sum %d", rows.cols[i][0], rows.cols[i][1],
                     rows.cols[i][2], twist_sum);
        }
    }
}

BT_TEST(analysis_sheets, flips_rows_flip_exactly_the_two_named_edges_in_place) {
    tc_name_index names;
    tc_position_names(&names);
    static tc_simcheck_rows rows;
    tc_load_simcheck_rows(&rows);
    size_t checked = 0;
    for (size_t i = 0; i < rows.count; i++) {
        if (strcmp(rows.cols[i][1], "flips") != 0) {
            continue;
        }
        size_t rowid = (size_t)strtoul(rows.cols[i][2], NULL, 10);
        char alg_text[TC_TEXT_MAX];
        tc_sheet_alg(rows.cols[i][0], rows.cols[i][1], rowid, alg_text, sizeof alg_text);
        bt_cube cube = apply(alg_text);
        BT_CHECK(bt_cube_centers_fixed(&cube));
        char pair_work[TC_TEXT_MAX];
        snprintf(pair_work, sizeof pair_work, "%s", rows.cols[i][3]);
        char *pair[2] = {NULL, NULL};
        size_t pair_len = 0;
        char *cursor = pair_work;
        while (pair_len < 2) {
            pair[pair_len++] = cursor;
            char *amp = strstr(cursor, " & ");
            if (amp == NULL) {
                break;
            }
            *amp = '\0';
            cursor = amp + 3;
        }
        BT_CHECK_EQ(pair_len, (size_t)2);
        uint8_t first = bt_piece_of(tc_sticker_of_name(&names, pair[0])).index;
        uint8_t second = bt_piece_of(tc_sticker_of_name(&names, pair[1])).index;
        BT_CHECK(first != second);
        bt_state_summary summary = bt_summarize(&cube);
        for (size_t slot = 0; slot < 12; slot++) {
            BT_CHECK_EQ(summary.edges[slot].piece, (uint8_t)slot);
            bool expected = slot == first;
            if (!expected) {
                expected = slot == second;
            }
            BT_CHECK_EQ(summary.edges[slot].flipped, expected);
        }
        for (size_t slot = 0; slot < 8; slot++) {
            BT_CHECK_EQ(summary.corners[slot].piece, (uint8_t)slot);
            BT_CHECK_EQ(summary.corners[slot].twist, 0);
        }
        checked += 1;
    }
    BT_CHECK_EQ(checked, (size_t)25);
}

BT_TEST(analysis_sheets, twists_rows_match_simcheck_signed_directions) {
    tc_name_index names;
    tc_position_names(&names);
    static tc_simcheck_rows rows;
    tc_load_simcheck_rows(&rows);
    size_t checked = 0;
    for (size_t i = 0; i < rows.count; i++) {
        if (strcmp(rows.cols[i][1], "twists") != 0) {
            continue;
        }
        size_t rowid = (size_t)strtoul(rows.cols[i][2], NULL, 10);
        char alg_text[TC_TEXT_MAX];
        tc_sheet_alg(rows.cols[i][0], rows.cols[i][1], rowid, alg_text, sizeof alg_text);
        bt_cube cube = apply(alg_text);
        BT_CHECK(bt_cube_centers_fixed(&cube));
        bt_state_summary summary = bt_summarize(&cube);
        for (size_t slot = 0; slot < 8; slot++) {
            BT_CHECK_EQ(summary.corners[slot].piece, (uint8_t)slot);
        }
        for (size_t slot = 0; slot < 12; slot++) {
            BT_CHECK_EQ(summary.edges[slot].piece, (uint8_t)slot);
            BT_CHECK(!summary.edges[slot].flipped);
        }
        ltct_corner cycles[2];
        size_t cycle_len = parse_twists_note(rows.cols[i][7], cycles, 2);
        BT_CHECK_EQ(cycle_len, (size_t)2);
        BT_CHECK_EQ(cycles[0].dir, (int8_t)(-cycles[1].dir));
        size_t twisted_slots[2];
        for (size_t k = 0; k < 2; k++) {
            size_t slot = bt_piece_of(tc_sticker_of_name(&names, cycles[k].moves[0].from)).index;
            BT_CHECK_EQ(summary.corners[slot].twist, cycles[k].dir);
            twisted_slots[k] = slot;
            for (size_t p = 0; p < cycles[k].moves_len; p++) {
                BT_CHECK_EQ(
                    bt_cube_position_of(&cube, tc_sticker_of_name(&names, cycles[k].moves[p].from)),
                    tc_sticker_of_name(&names, cycles[k].moves[p].to));
            }
        }
        BT_CHECK(twisted_slots[0] != twisted_slots[1]);
        for (size_t slot = 0; slot < 8; slot++) {
            if (slot != twisted_slots[0] && slot != twisted_slots[1]) {
                BT_CHECK_EQ(summary.corners[slot].twist, 0);
            }
        }
        checked += 1;
    }
    BT_CHECK_EQ(checked, (size_t)25);
}

BT_TEST(analysis_sheets, defect_notes_agree_with_orientation_rules) {
    tc_name_index names;
    tc_position_names(&names);
    static tc_simcheck_rows rows;
    tc_load_simcheck_rows(&rows);
    size_t checked = 0;
    for (size_t i = 0; i < rows.count; i++) {
        bool relevant_sheet = strcmp(rows.cols[i][1], "edge") == 0;
        if (!relevant_sheet) {
            relevant_sheet = strcmp(rows.cols[i][1], "corner") == 0;
        }
        if (!relevant_sheet) {
            relevant_sheet = strcmp(rows.cols[i][1], "parity") == 0;
        }
        if (!relevant_sheet || strncmp(rows.cols[i][7], "defects", 6) != 0) {
            continue;
        }
        size_t rowid = (size_t)strtoul(rows.cols[i][2], NULL, 10);
        char alg_text[TC_TEXT_MAX];
        tc_sheet_alg(rows.cols[i][0], rows.cols[i][1], rowid, alg_text, sizeof alg_text);
        bt_cube cube = apply(alg_text);
        defects_map map;
        parse_defects(&names, rows.cols[i][7], &map);
        char ctx[256];
        snprintf(ctx, sizeof ctx, "%s %s row %s note %s", rows.cols[i][0], rows.cols[i][1],
                 rows.cols[i][2], rows.cols[i][7]);
        assert_defects_match(&map, &cube, &names, ctx);
        checked += 1;
    }
    BT_CHECK_EQ(checked, (size_t)(366 + 622 + 28));
}

BT_TEST(analysis_sheets, ltct_notes_agree_with_orientation_rules) {
    tc_name_index names;
    tc_position_names(&names);
    static tc_simcheck_rows rows;
    tc_load_simcheck_rows(&rows);
    size_t checked = 0;
    for (size_t i = 0; i < rows.count; i++) {
        if (strcmp(rows.cols[i][1], "ltct") != 0) {
            continue;
        }
        size_t rowid = (size_t)strtoul(rows.cols[i][2], NULL, 10);
        char alg_text[TC_TEXT_MAX];
        tc_sheet_alg(rows.cols[i][0], rows.cols[i][1], rowid, alg_text, sizeof alg_text);
        bt_cube cube = apply(alg_text);
        BT_CHECK(bt_cube_centers_fixed(&cube));
        bt_state_summary summary = bt_summarize(&cube);
        ltct_note note;
        parse_ltct_note(rows.cols[i][7], &note);
        BT_CHECK_EQ(note.edges_len, (size_t)2);
        BT_CHECK_EQ(note.corners_len, (size_t)3);
        int expected_pieces[12];
        for (size_t slot = 0; slot < 12; slot++) {
            expected_pieces[slot] = DS_NONE;
        }
        for (size_t e = 0; e < note.edges_len; e++) {
            uint8_t to = bt_piece_of(tc_sticker_of_name(&names, note.edges[e].to)).index;
            uint8_t from = bt_piece_of(tc_sticker_of_name(&names, note.edges[e].from)).index;
            expected_pieces[to] = from;
        }
        for (size_t slot = 0; slot < 12; slot++) {
            int expected = expected_pieces[slot] == DS_NONE ? (int)slot : expected_pieces[slot];
            BT_CHECK_EQ(summary.edges[slot].piece, (uint8_t)expected);
            BT_CHECK(!summary.edges[slot].flipped);
        }
        int32_t twist_sum = 0;
        for (size_t c = 0; c < note.corners_len; c++) {
            twist_sum += note.corners[c].dir;
        }
        BT_CHECK_EQ(((twist_sum % 3) + 3) % 3, 0);
        size_t twisted_slots[3];
        for (size_t c = 0; c < note.corners_len; c++) {
            uint8_t piece = bt_piece_of(tc_sticker_of_name(&names, note.corners[c].piece)).index;
            uint8_t slot = bt_piece_of(tc_sticker_of_name(&names, note.corners[c].slot)).index;
            BT_CHECK_EQ(summary.corners[slot].piece, piece);
            BT_CHECK_EQ(summary.corners[slot].twist, note.corners[c].dir);
            twisted_slots[c] = slot;
            for (size_t p = 0; p < note.corners[c].moves_len; p++) {
                BT_CHECK_EQ(bt_cube_position_of(
                                &cube, tc_sticker_of_name(&names, note.corners[c].moves[p].from)),
                            tc_sticker_of_name(&names, note.corners[c].moves[p].to));
            }
        }
        for (size_t slot = 0; slot < 8; slot++) {
            bool twisted = slot == twisted_slots[0];
            if (!twisted) {
                twisted = slot == twisted_slots[1];
            }
            if (!twisted) {
                twisted = slot == twisted_slots[2];
            }
            if (!twisted) {
                BT_CHECK_EQ(summary.corners[slot].piece, (uint8_t)slot);
                BT_CHECK_EQ(summary.corners[slot].twist, 0);
            }
        }
        checked += 1;
    }
    BT_CHECK_EQ(checked, (size_t)11);
}

BT_TEST(analysis_sheets, parse_twists_handles_the_note_shapes) {
    const char *note = "cyc1=[UFR>FUR FUR>RUF RUF>UFR] d1=+1 cyc2=[UBR>BUR RUB>UBR BUR>RUB] d2=-1";
    ltct_corner parsed[2];
    size_t count = parse_twists_note(note, parsed, 2);
    BT_CHECK_EQ(count, (size_t)2);
    BT_CHECK_EQ(parsed[0].dir, 1);
    BT_CHECK_EQ(parsed[1].dir, -1);
    BT_CHECK_EQ(parsed[0].moves_len, (size_t)3);
    BT_CHECK(strcmp(parsed[0].moves[0].from, "UFR") == 0);
    BT_CHECK(strcmp(parsed[0].moves[0].to, "FUR") == 0);

    const char *ltct = "named e=UF/UR c=UBR/LUB[FUR] | edges: UR->UF UF->UR | corners: UBL->BUR "
                       "[UBL>BUR LUB>RUB BUL>UBR] tw=-1 UFR->LUB [UFR>LUB FUR>UBL RUF>BUL] tw=-1 "
                       "UFR->RUF [UFR>RUF FUR>UFR RUF>FUR] tw=-1";
    ltct_note parsed_ltct;
    parse_ltct_note(ltct, &parsed_ltct);
    BT_CHECK_EQ(parsed_ltct.edges_len, (size_t)2);
    BT_CHECK_EQ(parsed_ltct.corners_len, (size_t)3);
    BT_CHECK(strcmp(parsed_ltct.corners[0].piece, "UBL") == 0);
    BT_CHECK(strcmp(parsed_ltct.corners[0].slot, "BUR") == 0);
    BT_CHECK_EQ(parsed_ltct.corners[0].dir, -1);
    BT_CHECK_EQ(parsed_ltct.corners[2].dir, -1);
    BT_CHECK_EQ(parsed_ltct.corners[0].moves_len, (size_t)3);
}
