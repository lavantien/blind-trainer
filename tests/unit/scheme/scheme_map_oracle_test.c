#include "common.h"
#include "scheme/scheme.h"

#include "common/bt_harness.h"

#include <stdlib.h>
#include <string.h>

static const int CORNER_GRID[4][2] = {{0, 0}, {0, 2}, {2, 2}, {2, 0}};
static const int EDGE_GRID[4][2] = {{0, 1}, {1, 2}, {2, 1}, {1, 0}};

static int compare_names(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

BT_TEST(scheme_map, position_map_tsv_matches_scheme_all_48) {
    char path[BT_PATH_MAX];
    bt_test_ref_path(path, "position_map.tsv");
    bt_test_lines lines;
    bt_test_lines_begin(&lines, path);
    char *header = bt_test_lines_next(&lines);
    BT_CHECK(header != NULL);
    BT_CHECK_EQ(header, "orbit\tsticker\tletter\tname\tface\trow\tcol\tpiece");

    uint8_t pieces[48];
    memset(pieces, 255, sizeof pieces);
    bool letters_seen[26] = {false};
    const char *names_seen[48];
    size_t rows = 0;

    char *line;
    while ((line = bt_test_lines_next(&lines)) != NULL) {
        if (*line == '\0') {
            continue;
        }
        char *cols[8];
        if (bt_tsv_split(line, cols, 8) != 8) {
            BT_ABORT("row must have 8 columns: %s", line);
        }
        bt_orbit orbit = BT_ORBIT_CORNER;
        if (strcmp(cols[0], "corner") == 0) {
            orbit = BT_ORBIT_CORNER;
        } else if (strcmp(cols[0], "edge") == 0) {
            orbit = BT_ORBIT_EDGE;
        } else {
            BT_ABORT("unknown orbit token: %s", cols[0]);
        }
        long long sticker_raw = bt_test_parse_int(cols[1]);
        if (sticker_raw < 0 || sticker_raw >= 48) {
            BT_ABORT("sticker out of range: %s", cols[1]);
        }
        uint8_t sticker = (uint8_t)sticker_raw;
        BT_CHECK(strlen(cols[2]) == 1);
        char letter = cols[2][0];
        if (letter < 'A' || letter > 'X') {
            BT_ABORT("letter out of range: %s", cols[2]);
        }
        const char *name = cols[3];
        uint8_t face = (uint8_t)bt_test_parse_int(cols[4]);
        uint8_t row = (uint8_t)bt_test_parse_int(cols[5]);
        uint8_t col = (uint8_t)bt_test_parse_int(cols[6]);
        uint8_t piece = (uint8_t)bt_test_parse_int(cols[7]);

        BT_CHECK_EQ(bt_scheme_letter_of(sticker), letter);
        BT_CHECK_EQ(bt_scheme_name_of(sticker), name);
        BT_CHECK_EQ(bt_scheme_sticker_of(orbit, letter), sticker);
        BT_CHECK_EQ(face, (uint8_t)((sticker % 24) / 4));
        const int (*grid)[2] = orbit == BT_ORBIT_CORNER ? CORNER_GRID : EDGE_GRID;
        BT_CHECK_EQ(row, (uint8_t)grid[sticker % 4][0]);
        BT_CHECK_EQ(col, (uint8_t)grid[sticker % 4][1]);

        BT_CHECK(sticker >= bt_scheme_orbit_lo(orbit));
        BT_CHECK(sticker < bt_scheme_orbit_hi(orbit));
        if (rows >= 48) {
            BT_ABORT("position_map row budget exceeded");
        }
        pieces[sticker] = piece;
        letters_seen[letter - 'A'] = true;
        names_seen[rows] = name;
        rows++;
    }
    bt_test_lines_end(&lines);

    BT_CHECK_EQ(rows, (size_t)48);
    int letters = 0;
    for (int i = 0; i < 26; i++) {
        letters += letters_seen[i] ? 1 : 0;
    }
    BT_CHECK_EQ(letters, 24);
    qsort(names_seen, 48, sizeof names_seen[0], compare_names);
    int names = 1;
    for (int i = 1; i < 48; i++) {
        if (strcmp(names_seen[i - 1], names_seen[i]) != 0) {
            names++;
        }
    }
    BT_CHECK_EQ(names, 48);

    bt_orbit orbits[2] = {BT_ORBIT_EDGE, BT_ORBIT_CORNER};
    for (int i = 0; i < 2; i++) {
        int expected = orbits[i] == BT_ORBIT_EDGE ? 12 : 8;
        bool ids[48] = {false};
        uint8_t lo = 255;
        uint8_t hi = 0;
        for (int s = bt_scheme_orbit_lo(orbits[i]); s < bt_scheme_orbit_hi(orbits[i]); s++) {
            uint8_t id = pieces[s];
            ids[id] = true;
            if (id < lo) {
                lo = id;
            }
            if (id > hi) {
                hi = id;
            }
        }
        int distinct = 0;
        for (int id = 0; id < 48; id++) {
            distinct += ids[id] ? 1 : 0;
        }
        BT_CHECK_EQ(distinct, expected);
        BT_CHECK_EQ(lo, 0);
        BT_CHECK_EQ(hi, (uint8_t)(expected - 1));
    }

    for (uint8_t a = 0; a < 48; a++) {
        for (uint8_t b = 0; b < 48; b++) {
            bool grouped = ((a < 24) == (b < 24) && pieces[a] == pieces[b]) != 0;
            BT_CHECK_EQ(grouped, bt_scheme_same_piece(a, b));
        }
    }
}
