#include "common.h"

typedef struct {
    tc_name_index names;
    int piece[54];
} name_map;

static void sticker_names(name_map *map) {
    tc_position_names(&map->names);
    for (size_t i = 0; i < 54; i++) {
        map->piece[i] = 200 + (int)i;
    }
    static tc_lines lines;
    tc_sheet_lines("position_map.tsv", &lines);
    for (size_t i = 1; i < lines.count; i++) {
        if (lines.lines[i][0] == '\0') {
            continue;
        }
        char copy[TC_LINE_MAX];
        snprintf(copy, sizeof copy, "%s", lines.lines[i]);
        char *cols[8];
        size_t n = tc_split(copy, cols, 8);
        BT_CHECK_EQ(n, (size_t)8);
        size_t sticker = (size_t)strtoul(cols[1], NULL, 10);
        int piece = (int)strtoul(cols[7], NULL, 10);
        map->piece[sticker] = strcmp(cols[0], "corner") == 0 ? piece : 100 + piece;
    }
}

BT_TEST(vendor_cycles, clean_cycle_rows_are_pure_piece_three_cycles) {
    static name_map map;
    sticker_names(&map);
    static tc_simcheck_rows rows;
    tc_load_simcheck_rows(&rows);
    size_t checked = 0;
    for (size_t i = 0; i < rows.count; i++) {
        if (strcmp(rows.cols[i][6], "CYCLE_A") != 0 || strstr(rows.cols[i][7], "clean") == NULL) {
            continue;
        }
        size_t rowid = (size_t)strtoul(rows.cols[i][2], NULL, 10);
        char alg_text[TC_TEXT_MAX];
        tc_sheet_alg(rows.cols[i][0], rows.cols[i][1], rowid, alg_text, sizeof alg_text);
        bt_alg alg;
        bt_alg_init(&alg);
        char err[64];
        if (!bt_alg_parse(alg_text, &alg, err, sizeof err)) {
            BT_ABORT("%s/%s row %zu: %s", rows.cols[i][0], rows.cols[i][1], rowid, err);
        }
        char position[TC_TEXT_MAX];
        snprintf(position, sizeof position, "%s", rows.cols[i][3]);
        char *tokens[3] = {NULL, NULL, NULL};
        size_t ntokens = 0;
        char *cursor = position;
        while (ntokens < 3) {
            tokens[ntokens++] = cursor;
            char *dash = strchr(cursor, '-');
            if (dash == NULL) {
                break;
            }
            *dash = '\0';
            cursor = dash + 1;
        }
        BT_CHECK_EQ(ntokens, (size_t)3);
        size_t a = tc_sticker_of_name(&map.names, tokens[0]);
        size_t b = tc_sticker_of_name(&map.names, tokens[1]);
        size_t c = tc_sticker_of_name(&map.names, tokens[2]);
        int pa = map.piece[a];
        int pb = map.piece[b];
        int pc = map.piece[c];
        BT_CHECK(pa != pb && pb != pc && pa != pc);
        bt_cube cube = bt_cube_solved();
        bt_cube_apply_alg(&cube, &alg);
        BT_CHECK_EQ(bt_cube_position_of(&cube, (uint8_t)a), (uint8_t)b);
        BT_CHECK_EQ(bt_cube_position_of(&cube, (uint8_t)b), (uint8_t)c);
        BT_CHECK_EQ(bt_cube_position_of(&cube, (uint8_t)c), (uint8_t)a);
        for (size_t sticker = 0; sticker < 54; sticker++) {
            size_t dest = bt_cube_position_of(&cube, (uint8_t)sticker);
            int expected_piece;
            if (map.piece[sticker] == pa) {
                expected_piece = pb;
            } else if (map.piece[sticker] == pb) {
                expected_piece = pc;
            } else if (map.piece[sticker] == pc) {
                expected_piece = pa;
            } else {
                if (dest != sticker) {
                    BT_ABORT("%s/%s row %zu %s: sticker %zu moved", rows.cols[i][0],
                             rows.cols[i][1], rowid, alg_text, sticker);
                }
                continue;
            }
            if (map.piece[dest] != expected_piece) {
                BT_ABORT("%s/%s row %zu %s: piece mapping broke", rows.cols[i][0], rows.cols[i][1],
                         rowid, alg_text);
            }
        }
        bt_alg_free(&alg);
        checked += 1;
    }
    BT_CHECK_EQ(checked, (size_t)208);
}
