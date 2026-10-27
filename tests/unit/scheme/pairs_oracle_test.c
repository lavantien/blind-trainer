#include "common.h"
#include "scheme/pairs.h"
#include "scheme/scheme.h"

#include "common/bt_harness.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t t1;
    uint8_t t2;
    char letters[3];
} bt_pairs_row;

static size_t load_rows(const char *file, bt_pairs_row *rows, size_t cap) {
    char path[BT_PATH_MAX];
    bt_test_ref_path(path, file);
    bt_test_lines lines;
    bt_test_lines_begin(&lines, path);
    char *header = bt_test_lines_next(&lines);
    BT_CHECK(header != NULL);
    BT_CHECK_EQ(header, "t1\tt2\tletters");
    size_t count = 0;
    char *line;
    while ((line = bt_test_lines_next(&lines)) != NULL) {
        if (*line == '\0') {
            continue;
        }
        char *cols[3];
        if (bt_tsv_split(line, cols, 3) != 3) {
            BT_ABORT("row must have 3 columns: %s", line);
        }
        if (count >= cap) {
            BT_ABORT("pairs dump row budget exceeded");
        }
        rows[count].t1 = (uint8_t)bt_test_parse_int(cols[0]);
        rows[count].t2 = (uint8_t)bt_test_parse_int(cols[1]);
        snprintf(rows[count].letters, 3, "%s", cols[2]);
        count++;
    }
    bt_test_lines_end(&lines);
    return count;
}

static int compare_pairs(const void *a, const void *b) {
    const bt_pairs_row *first = a;
    const bt_pairs_row *second = b;
    if (first->t1 != second->t1) {
        return (int)first->t1 - (int)second->t1;
    }
    return (int)first->t2 - (int)second->t2;
}

static void check_against_dump(bt_orbit orbit, const char *file, size_t expected_count,
                               uint8_t buffer) {
    bt_pairs_row rows[BT_PAIRS_MAX];
    size_t row_count = load_rows(file, rows, BT_PAIRS_MAX);
    BT_CHECK_EQ(row_count, expected_count);
    bt_case cases[BT_PAIRS_MAX];
    size_t count = bt_scheme_enumerate_cases(orbit, cases);
    BT_CHECK_EQ(count, expected_count);

    for (size_t i = 0; i < count; i++) {
        BT_CHECK_EQ(cases[i].t1, rows[i].t1);
        BT_CHECK_EQ(cases[i].t2, rows[i].t2);
    }

    bt_pairs_row *sorted = malloc(count * sizeof *sorted);
    memcpy(sorted, rows, count * sizeof *sorted);
    qsort(sorted, count, sizeof *sorted, compare_pairs);
    for (size_t i = 1; i < count; i++) {
        BT_CHECK(compare_pairs(&sorted[i - 1], &sorted[i]) != 0);
    }
    free(sorted);

    for (size_t i = 0; i < count; i++) {
        const bt_case *case_id = &cases[i];
        BT_CHECK_EQ(case_id->t1, rows[i].t1);
        BT_CHECK_EQ(case_id->t2, rows[i].t2);
        char letters[2];
        bt_scheme_case_letters(case_id, letters);
        char joined[3] = {letters[0], letters[1], '\0'};
        BT_CHECK_EQ(joined, rows[i].letters);
        BT_CHECK_EQ(letters[0], bt_scheme_letter_of(case_id->t1));
        BT_CHECK_EQ(letters[1], bt_scheme_letter_of(case_id->t2));
        BT_CHECK(case_id->t1 >= bt_scheme_orbit_lo(orbit));
        BT_CHECK(case_id->t1 < bt_scheme_orbit_hi(orbit));
        BT_CHECK(case_id->t2 >= bt_scheme_orbit_lo(orbit));
        BT_CHECK(case_id->t2 < bt_scheme_orbit_hi(orbit));
        BT_CHECK(!bt_scheme_same_piece(case_id->t1, buffer));
        BT_CHECK(!bt_scheme_same_piece(case_id->t2, buffer));
        BT_CHECK(!bt_scheme_same_piece(case_id->t1, case_id->t2));
    }
}

BT_TEST(pairs_oracle, edge_pairs_match_pairs_edge2_exactly) {
    check_against_dump(BT_ORBIT_EDGE, "pairs_edge2.tsv", 440, BT_SCHEME_EDGE_BUFFER_STICKER);
}

BT_TEST(pairs_oracle, corner_pairs_match_pairs_corner2_exactly) {
    check_against_dump(BT_ORBIT_CORNER, "pairs_corner2.tsv", 378, BT_SCHEME_CORNER_BUFFER_STICKER);
}
