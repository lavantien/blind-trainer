#ifndef BT_DATA_TSV_H
#define BT_DATA_TSV_H

#include <stddef.h>

#define BT_TSV_COLUMNS 5
#define BT_TSV_COLUMN_MAX 24

typedef struct {
    char *position;
    char *alg;
    char *commutator;
    char *recon_link;
    char *youtube_link;
} bt_sheet_row;

typedef enum {
    BT_TSV_ERR_BAD_HEADER,
    BT_TSV_ERR_COLUMN_COUNT,
    BT_TSV_ERR_EMPTY_FIELD,
} bt_tsv_error_kind;

typedef struct {
    bt_tsv_error_kind kind;
    size_t line;
    size_t found;
    char column[BT_TSV_COLUMN_MAX];
} bt_tsv_error;

typedef struct {
    size_t rowid;
    bt_sheet_row row;
} bt_tsv_line;

typedef struct {
    bt_tsv_line *items;
    size_t count;
} bt_tsv_rows;

void bt_tsv_error_string(const bt_tsv_error *err, char *out, size_t out_len);
int bt_tsv_parse_sheet(const char *text, bt_tsv_rows *out, bt_tsv_error *err);
void bt_tsv_rows_free(bt_tsv_rows *rows);
int bt_sheet_row_eq(const bt_sheet_row *a, const bt_sheet_row *b);

#endif
