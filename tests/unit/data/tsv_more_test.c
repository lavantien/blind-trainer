#include "common/bt_harness.h"
#include "data/tsv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char HEADER[] = "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link";

static void make_row(char *out, size_t out_size, const char *position, const char *alg,
                     const char *commutator, const char *recon, const char *youtube) {
    snprintf(out, out_size, "%s\t%s\t%s\t%s\t%s", position, alg, commutator, recon, youtube);
}

BT_TEST(tsv_more, quoted_cells_pass_through_verbatim) {
    char row[256];
    make_row(row, sizeof row, "UF-UB-UL", "U M2 U \"M'\" U2", "say \"setup:insert\" here",
             "https://x?q=\"1 2\"", "\"quoted\" note");
    char text[512];
    snprintf(text, sizeof text, "%s\n%s\n", HEADER, row);
    bt_tsv_rows rows;
    bt_tsv_error err;
    if (bt_tsv_parse_sheet(text, &rows, &err) != 0) {
        return;
    }
    BT_CHECK(rows.count == 1);
    bt_sheet_row *parsed = &rows.items[0].row;
    BT_CHECK_EQ(parsed->position, "UF-UB-UL");
    BT_CHECK_EQ(parsed->alg, "U M2 U \"M'\" U2");
    BT_CHECK_EQ(parsed->commutator, "say \"setup:insert\" here");
    BT_CHECK_EQ(parsed->recon_link, "https://x?q=\"1 2\"");
    BT_CHECK_EQ(parsed->youtube_link, "\"quoted\" note");
    bt_tsv_rows_free(&rows);
}

BT_TEST(tsv_more, ragged_rows_report_their_found_width) {
    const char *cells[3] = {"UF-UB-UL", "UF-UB-UL\tU M2 U'", "UF-UB-UL\tU M2 U'\tx"};
    const size_t found[3] = {1, 2, 3};
    for (int i = 0; i < 3; i++) {
        char text[256];
        snprintf(text, sizeof text, "%s\n%s\n", HEADER, cells[i]);
        bt_tsv_rows rows;
        bt_tsv_error err;
        BT_CHECK(bt_tsv_parse_sheet(text, &rows, &err) != 0);
        BT_CHECK(err.kind == BT_TSV_ERR_COLUMN_COUNT);
        BT_CHECK(err.line == 2);
        BT_CHECK(err.found == found[i]);
    }
}

BT_TEST(tsv_more, wrong_middle_header_cell_is_a_bad_header) {
    const char *headers[4] = {
        "Position\tMove\tCommutator\tReconstruction Link\tYoutube Link",
        "Position\tAlg\tCommutators\tReconstruction Link\tYoutube Link",
        "Position\tAlg\tCommutator\tReconstruction\tYoutube Link",
        "Position\tAlg\tCommutator\tReconstruction Link\tLink",
    };
    for (int i = 0; i < 4; i++) {
        char text[256];
        snprintf(text, sizeof text, "%s\nA-B-C\tR U R'\t\t\t\n", headers[i]);
        bt_tsv_rows rows;
        bt_tsv_error err;
        BT_CHECK(bt_tsv_parse_sheet(text, &rows, &err) != 0);
        BT_CHECK(err.kind == BT_TSV_ERR_BAD_HEADER);
        BT_CHECK(err.line == 1);
    }
}

BT_TEST(tsv_more, padded_header_cells_are_tolerated) {
    const char *header = " Position \t Alg \t Commutator \t Reconstruction Link \t Youtube Link ";
    char text[256];
    snprintf(text, sizeof text, "%s\nA-B-C\tR U R'\t\t\t\n", header);
    bt_tsv_rows rows;
    bt_tsv_error err;
    if (bt_tsv_parse_sheet(text, &rows, &err) != 0) {
        return;
    }
    if (rows.count == 1) {
        BT_CHECK_EQ(rows.items[0].row.alg, "R U R'");
    }
    bt_tsv_rows_free(&rows);
}

BT_TEST(tsv_more, leading_blank_lines_shift_the_header_line_number) {
    const char *text = "\n \nnope\tAlg\tCommutator\tReconstruction Link\tYoutube Link\n";
    bt_tsv_rows rows;
    bt_tsv_error err;
    BT_CHECK(bt_tsv_parse_sheet(text, &rows, &err) != 0);
    BT_CHECK(err.kind == BT_TSV_ERR_BAD_HEADER);
    BT_CHECK(err.line == 3);
}

BT_TEST(tsv_more, header_only_file_yields_no_rows) {
    char text[256];
    bt_tsv_rows rows;
    bt_tsv_error err;
    BT_CHECK(bt_tsv_parse_sheet(HEADER, &rows, &err) == 0);
    BT_CHECK(rows.count == 0);
    snprintf(text, sizeof text, "%s\n", HEADER);
    if (bt_tsv_parse_sheet(text, &rows, &err) != 0) {
        return;
    }
    BT_CHECK(rows.count == 0);
    snprintf(text, sizeof text, "%s\n\t\n \n", HEADER);
    if (bt_tsv_parse_sheet(text, &rows, &err) != 0) {
        return;
    }
    BT_CHECK(rows.count == 0);
}

BT_TEST(tsv_more, crlf_rows_report_physical_line_numbers) {
    char first[128];
    char second[128];
    make_row(first, sizeof first, "A-B-C", "R U R'", "", "", "");
    make_row(second, sizeof second, "D-E-F", "R2", "", "", "");
    char text[384];
    snprintf(text, sizeof text, "%s\r\n%s\r\n\r\n%s\r\n", HEADER, first, second);
    bt_tsv_rows rows;
    bt_tsv_error err;
    if (bt_tsv_parse_sheet(text, &rows, &err) != 0) {
        return;
    }
    BT_CHECK(rows.count == 2);
    BT_CHECK(rows.items[0].rowid == 2);
    BT_CHECK(rows.items[1].rowid == 4);
    BT_CHECK_EQ(rows.items[1].row.position, "D-E-F");
    bt_tsv_rows_free(&rows);
}

BT_TEST(tsv_more, trailing_empty_columns_on_data_rows_are_trimmed) {
    char row[128];
    make_row(row, sizeof row, "A-B-C", "R U R'", "", "", "");
    char text[256];
    snprintf(text, sizeof text, "%s\n%s\t\t\n", HEADER, row);
    bt_tsv_rows rows;
    bt_tsv_error err;
    if (bt_tsv_parse_sheet(text, &rows, &err) != 0) {
        return;
    }
    BT_CHECK(rows.count == 1);
    BT_CHECK_EQ(rows.items[0].row.youtube_link, "");
    bt_tsv_rows_free(&rows);
}

BT_TEST(tsv_more, whitespace_only_position_or_alg_is_still_empty) {
    char row[128];
    bt_tsv_rows rows;
    bt_tsv_error err;
    char text[256];
    make_row(row, sizeof row, "   ", "R U R'", "", "", "");
    snprintf(text, sizeof text, "%s\n%s\n", HEADER, row);
    BT_CHECK(bt_tsv_parse_sheet(text, &rows, &err) != 0);
    BT_CHECK(err.kind == BT_TSV_ERR_EMPTY_FIELD);
    BT_CHECK(err.line == 2);
    BT_CHECK_EQ(err.column, "Position");
    make_row(row, sizeof row, "A-B-C", " \t ", "", "", "");
    snprintf(text, sizeof text, "%s\n%s\n", HEADER, row);
    BT_CHECK(bt_tsv_parse_sheet(text, &rows, &err) != 0);
    BT_CHECK(err.kind == BT_TSV_ERR_EMPTY_FIELD);
    BT_CHECK(err.line == 2);
    BT_CHECK_EQ(err.column, "Alg");
}

BT_TEST(tsv_more, later_bad_row_stops_the_parse_at_its_line) {
    char good[128];
    char late[128];
    make_row(good, sizeof good, "A-B-C", "R U R'", "", "", "");
    make_row(late, sizeof late, "D-E-F", "R2", "", "", "");
    char text[384];
    snprintf(text, sizeof text, "%s\n%s\nnope\n%s\n", HEADER, good, late);
    bt_tsv_rows rows;
    bt_tsv_error err;
    BT_CHECK(bt_tsv_parse_sheet(text, &rows, &err) != 0);
    BT_CHECK(err.kind == BT_TSV_ERR_COLUMN_COUNT);
    BT_CHECK(err.line == 3);
    BT_CHECK(err.found == 1);
}
