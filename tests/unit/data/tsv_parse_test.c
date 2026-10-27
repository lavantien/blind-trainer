#include "common.h"
#include "data/tsv.h"

#include <string.h>

BT_TEST(tsv_parse, all_twelve_sheets_parse_with_expected_row_counts) {
    for (size_t i = 0; i < 12; i++) {
        char *text = td_sheet_text(TD_SHEETS[i].solver_dir, TD_SHEETS[i].file);
        bt_tsv_rows rows;
        bt_tsv_error err;
        if (bt_tsv_parse_sheet(text, &rows, &err) != 0) {
            return;
        }
        BT_CHECK(rows.count == TD_SHEETS[i].expected);
        bt_tsv_rows_free(&rows);
        free(text);
    }
}

BT_TEST(tsv_parse, rowids_are_physical_line_numbers_starting_at_two) {
    char *text = td_sheet_text("cherry", "edge.tsv");
    bt_tsv_rows rows;
    bt_tsv_error err;
    if (bt_tsv_parse_sheet(text, &rows, &err) != 0) {
        return;
    }
    for (size_t offset = 0; offset < rows.count; offset++) {
        BT_CHECK(rows.items[offset].rowid == offset + 2);
    }
    bt_tsv_rows_free(&rows);
    free(text);
    char *empty_text = td_sheet_text("eggins", "flips.tsv");
    bt_tsv_rows empty;
    BT_CHECK(bt_tsv_parse_sheet(empty_text, &empty, &err) == 0);
    BT_CHECK(empty.count == 0);
    bt_tsv_rows_free(&empty);
    free(empty_text);
}

BT_TEST(tsv_parse, cherry_edge_first_row_columns_extract_verbatim) {
    char *text = td_sheet_text("cherry", "edge.tsv");
    bt_tsv_rows rows;
    bt_tsv_error err;
    if (bt_tsv_parse_sheet(text, &rows, &err) != 0) {
        return;
    }
    bt_sheet_row *row = &rows.items[0].row;
    BT_CHECK_EQ(row->position, "UF-UB-UL");
    BT_CHECK_EQ(row->alg, "U M2 U M' U2 M U M2 U'");
    BT_CHECK_EQ(row->commutator, "U M2 U:[M',U2]");
    BT_CHECK_EQ(row->recon_link, "https://bit.ly/2ZWpEPq");
    BT_CHECK_EQ(row->youtube_link, "3BLD: 16.04 Former WB Ao12");
    bt_tsv_rows_free(&rows);
    free(text);
}

BT_TEST(tsv_parse, special_class_headers_and_positions_extract) {
    char *twists_text = td_sheet_text("cherry", "twists.tsv");
    bt_tsv_rows twists;
    bt_tsv_error err;
    if (bt_tsv_parse_sheet(twists_text, &twists, &err) != 0) {
        free(twists_text);
        return;
    }
    BT_CHECK_EQ(twists.items[0].row.position, "UFR & UBR");
    BT_CHECK_EQ(twists.items[0].row.alg, "R' D' R D R' D' R U R' D R D' R' D R U'");
    bt_tsv_rows_free(&twists);
    free(twists_text);
    char *parity_text = td_sheet_text("cherry", "parity.tsv");
    bt_tsv_rows parity;
    if (bt_tsv_parse_sheet(parity_text, &parity, &err) != 0) {
        free(parity_text);
        return;
    }
    BT_CHECK_EQ(parity.items[0].row.position, "UF-UL, UFR-UFL");
    bt_tsv_rows_free(&parity);
    free(parity_text);
    char *ltct_text = td_sheet_text("cherry", "ltct.tsv");
    bt_tsv_rows ltct;
    if (bt_tsv_parse_sheet(ltct_text, &ltct, &err) != 0) {
        free(ltct_text);
        return;
    }
    BT_CHECK_EQ(ltct.items[0].row.position, "UF-UR, UBR-LUB[FUR]");
    BT_CHECK_EQ(ltct.items[0].row.commutator, "Not found.");
    bt_tsv_rows_free(&ltct);
    free(ltct_text);
}

BT_TEST(tsv_parse, crlf_input_parses_identically) {
    char *lf = td_sheet_text("cherry", "flips.tsv");
    size_t len = strlen(lf);
    char *crlf = malloc(len * 2 + 2);
    size_t used = 0;
    for (size_t i = 0; i < len; i++) {
        if (lf[i] == '\n') {
            crlf[used++] = '\r';
        }
        crlf[used++] = lf[i];
    }
    crlf[used] = '\0';
    bt_tsv_rows a;
    bt_tsv_rows b;
    bt_tsv_error err;
    if (bt_tsv_parse_sheet(lf, &a, &err) != 0) {
        return;
    }
    if (bt_tsv_parse_sheet(crlf, &b, &err) != 0) {
        return;
    }
    BT_CHECK(a.count == b.count);
    for (size_t i = 0; i < a.count && i < b.count; i++) {
        BT_CHECK(a.items[i].rowid == b.items[i].rowid);
        BT_CHECK(bt_sheet_row_eq(&a.items[i].row, &b.items[i].row) == 1);
    }
    bt_tsv_rows_free(&a);
    bt_tsv_rows_free(&b);
    free(lf);
    free(crlf);
}

BT_TEST(tsv_parse, header_padding_and_trailing_empty_columns_are_tolerated) {
    const char *text =
        "Position          \tAlg         \tCommutator\tReconstruction Link\tYoutube Link\t\t\n"
        "A-B-C\tU R U'\tNot found.\thttps://x\tnote\t\n";
    bt_tsv_rows rows;
    bt_tsv_error err;
    if (bt_tsv_parse_sheet(text, &rows, &err) != 0) {
        return;
    }
    BT_CHECK(rows.count == 1);
    BT_CHECK_EQ(rows.items[0].row.position, "A-B-C");
    BT_CHECK_EQ(rows.items[0].row.alg, "U R U'");
    bt_tsv_rows_free(&rows);
}

BT_TEST(tsv_parse, readme_content_is_rejected_as_a_bad_header) {
    char *text = td_sheet_text("cherry", "Readme.tsv");
    bt_tsv_rows rows;
    bt_tsv_error err;
    BT_CHECK(bt_tsv_parse_sheet(text, &rows, &err) != 0);
    BT_CHECK(err.kind == BT_TSV_ERR_BAD_HEADER);
    BT_CHECK(err.line == 1);
    free(text);
}

BT_TEST(tsv_parse, short_row_reports_its_line_number) {
    char *base = td_sheet_text("cherry", "edge.tsv");
    size_t len = strlen(base);
    char *joined = malloc(len + 32);
    memcpy(joined, base, len + 1);
    size_t line = 0;
    size_t line_start = 0;
    for (size_t i = 0; i <= len; i++) {
        if (joined[i] == '\n' || joined[i] == '\0') {
            if (line == 3) {
                size_t line_len = i - line_start;
                const char *short_row = "UF-UB-UR\tR2 U' S";
                size_t short_len = strlen(short_row);
                memmove(joined + line_start + short_len, joined + i, len - i + 1);
                memcpy(joined + line_start, short_row, short_len);
                (void)line_len;
                break;
            }
            line += 1;
            line_start = i + 1;
            if (joined[i] == '\0') {
                break;
            }
        }
    }
    BT_CHECK(line == 3);
    bt_tsv_rows rows;
    bt_tsv_error err;
    BT_CHECK(bt_tsv_parse_sheet(joined, &rows, &err) != 0);
    BT_CHECK(err.kind == BT_TSV_ERR_COLUMN_COUNT);
    BT_CHECK(err.line == 4);
    BT_CHECK(err.found == 2);
    free(base);
    free(joined);
}

BT_TEST(tsv_parse, extra_nonempty_column_is_rejected) {
    const char *text = "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link\n"
                       "A-B-C\tU R U'\tNot found.\thttps://x\tnote\textra\n";
    bt_tsv_rows rows;
    bt_tsv_error err;
    BT_CHECK(bt_tsv_parse_sheet(text, &rows, &err) != 0);
    BT_CHECK(err.kind == BT_TSV_ERR_COLUMN_COUNT);
    BT_CHECK(err.line == 2);
    BT_CHECK(err.found == 6);
}

BT_TEST(tsv_parse, empty_position_or_alg_reports_field_and_line) {
    const char *header = "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link";
    const char *no_position = "\tU R U'\tNot found.\thttps://x\tnote\n";
    const char *no_alg = "A-B-C\t\tNot found.\thttps://x\tnote\n";
    char text[256];
    bt_tsv_rows rows;
    bt_tsv_error err;
    snprintf(text, sizeof text, "%s\n%s", header, no_position);
    BT_CHECK(bt_tsv_parse_sheet(text, &rows, &err) != 0);
    BT_CHECK(err.kind == BT_TSV_ERR_EMPTY_FIELD);
    BT_CHECK(err.line == 2);
    BT_CHECK_EQ(err.column, "Position");
    snprintf(text, sizeof text, "%s\n%s", header, no_alg);
    BT_CHECK(bt_tsv_parse_sheet(text, &rows, &err) != 0);
    BT_CHECK(err.kind == BT_TSV_ERR_EMPTY_FIELD);
    BT_CHECK(err.line == 2);
    BT_CHECK_EQ(err.column, "Alg");
}

BT_TEST(tsv_parse, blank_lines_are_skipped_and_empty_input_is_empty) {
    const char *text = "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link\n"
                       "\n"
                       "A-B-C\tU R U'\tNot found.\thttps://x\tnote\n"
                       "\n";
    bt_tsv_rows rows;
    bt_tsv_error err;
    if (bt_tsv_parse_sheet(text, &rows, &err) != 0) {
        return;
    }
    BT_CHECK(rows.count == 1);
    BT_CHECK(rows.items[0].rowid == 3);
    bt_tsv_rows_free(&rows);
    BT_CHECK(bt_tsv_parse_sheet("", &rows, &err) == 0);
    BT_CHECK(rows.count == 0);
}

BT_TEST(tsv_parse, sheet_row_is_cloneable_and_comparable) {
    char *text = td_sheet_text("eggins", "parity.tsv");
    bt_tsv_rows rows;
    bt_tsv_error err;
    if (bt_tsv_parse_sheet(text, &rows, &err) != 0) {
        return;
    }
    char *owned[5];
    const char **fields[5] = {
        (const char **)&rows.items[0].row.position,
        (const char **)&rows.items[0].row.alg,
        (const char **)&rows.items[0].row.commutator,
        (const char **)&rows.items[0].row.recon_link,
        (const char **)&rows.items[0].row.youtube_link,
    };
    for (int i = 0; i < 5; i++) {
        size_t len = strlen(*fields[i]);
        owned[i] = malloc(len + 1);
        BT_CHECK(owned[i] != NULL);
        memcpy(owned[i], *fields[i], len + 1);
    }
    bt_sheet_row copy = rows.items[0].row;
    copy.position = owned[0];
    copy.alg = owned[1];
    copy.commutator = owned[2];
    copy.recon_link = owned[3];
    copy.youtube_link = owned[4];
    BT_CHECK(bt_sheet_row_eq(&rows.items[0].row, &copy) == 1);
    for (int i = 0; i < 5; i++) {
        free(owned[i]);
    }
    bt_tsv_rows_free(&rows);
    free(text);
}
