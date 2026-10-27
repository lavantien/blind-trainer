#include "data/tsv.h"

#include "common/bt_trace.h"

#include <stdlib.h>
#include <string.h>

enum {
    BT_TSV_CELL_SLOTS = 64,
};

static const char *HEADER_CELLS[BT_TSV_COLUMNS] = {
    "Position", "Alg", "Commutator", "Reconstruction Link", "Youtube Link",
};

static char *cell_dup(const char *text, size_t len) {
    char *copy = malloc(len + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, len);
    copy[len] = '\0';
    return copy;
}

static int is_space(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

static size_t trim_len(const char *text) {
    size_t len = strlen(text);
    while (len > 0 && is_space(text[len - 1])) {
        len -= 1;
    }
    size_t start = 0;
    while (start < len && is_space(text[start])) {
        start += 1;
    }
    return len - start;
}

static int starts_with(const char *text, const char *prefix) {
    size_t prefix_len = strlen(prefix);
    return strncmp(text, prefix, prefix_len) == 0;
}

static int cell_equals_trimmed(const char *cell, const char *want) {
    while (is_space(*cell)) {
        cell += 1;
    }
    size_t want_len = strlen(want);
    if (strncmp(cell, want, want_len) != 0) {
        return 0;
    }
    const char *tail = cell + want_len;
    while (*tail != '\0' && is_space(*tail)) {
        tail += 1;
    }
    return *tail == '\0';
}

void bt_tsv_error_string(const bt_tsv_error *err, char *out, size_t out_len) {
    BT_TRACE_ENTER("tsv.error_string");
    switch (err->kind) {
    case BT_TSV_ERR_BAD_HEADER:
        snprintf(
            out, out_len,
            "line %zu: header must be Position/Alg/Commutator/Reconstruction Link/Youtube Link",
            err->line);
        break;
    case BT_TSV_ERR_COLUMN_COUNT:
        snprintf(out, out_len, "line %zu: expected 5 columns, found %zu", err->line, err->found);
        break;
    case BT_TSV_ERR_EMPTY_FIELD:
        snprintf(out, out_len, "line %zu: %s column is empty", err->line, err->column);
        break;
    }
    BT_TRACE_EXIT("tsv.error_string");
}

void bt_tsv_rows_free(bt_tsv_rows *rows) {
    BT_TRACE_ENTER("tsv.rows_free");
    for (size_t i = 0; i < rows->count; i++) {
        free(rows->items[i].row.position);
        free(rows->items[i].row.alg);
        free(rows->items[i].row.commutator);
        free(rows->items[i].row.recon_link);
        free(rows->items[i].row.youtube_link);
    }
    free(rows->items);
    rows->items = NULL;
    rows->count = 0;
    BT_TRACE_EXIT("tsv.rows_free");
}

int bt_sheet_row_eq(const bt_sheet_row *a, const bt_sheet_row *b) {
    return strcmp(a->position, b->position) == 0 && strcmp(a->alg, b->alg) == 0 &&
           strcmp(a->commutator, b->commutator) == 0 && strcmp(a->recon_link, b->recon_link) == 0 &&
           strcmp(a->youtube_link, b->youtube_link) == 0;
}

static int row_push(bt_tsv_rows *out, size_t line_number, char *cells[BT_TSV_COLUMNS]) {
    bt_tsv_line *grown = realloc(out->items, (out->count + 1) * sizeof *grown);
    if (grown == NULL) {
        return -1;
    }
    out->items = grown;
    bt_tsv_line *slot = &out->items[out->count];
    slot->rowid = line_number;
    slot->row.position = cells[0];
    slot->row.alg = cells[1];
    slot->row.commutator = cells[2];
    slot->row.recon_link = cells[3];
    slot->row.youtube_link = cells[4];
    out->count += 1;
    return 0;
}

static int validate_header(char *line, size_t line_number, bt_tsv_error *err) {
    BT_TRACE_ENTER("tsv.validate_header");
    char *cells[BT_TSV_CELL_SLOTS];
    size_t count = 0;
    char *cursor = line;
    cells[count++] = cursor;
    while ((cursor = strchr(cursor, '\t')) != NULL && count < BT_TSV_CELL_SLOTS) {
        *cursor = '\0';
        cursor += 1;
        cells[count++] = cursor;
    }
    const char *head = cells[0];
    while (*head != '\0' && is_space(*head)) {
        head += 1;
    }
    if (!starts_with(head, "Position")) {
        err->kind = BT_TSV_ERR_BAD_HEADER;
        err->line = line_number;
        BT_TRACE_FAIL("tsv.validate_header", "header does not start with Position at line %zu",
                      line_number);
        return -1;
    }
    while (count > BT_TSV_COLUMNS && trim_len(cells[count - 1]) == 0) {
        count -= 1;
    }
    if (count < BT_TSV_COLUMNS) {
        err->kind = BT_TSV_ERR_COLUMN_COUNT;
        err->line = line_number;
        err->found = count;
        BT_TRACE_FAIL("tsv.validate_header", "header holds %zu columns at line %zu", count,
                      line_number);
        return -1;
    }
    for (size_t i = 1; i < BT_TSV_COLUMNS; i++) {
        if (!cell_equals_trimmed(cells[i], HEADER_CELLS[i])) {
            err->kind = BT_TSV_ERR_BAD_HEADER;
            err->line = line_number;
            BT_TRACE_FAIL("tsv.validate_header", "header cell %zu mismatch at line %zu", i,
                          line_number);
            return -1;
        }
    }
    BT_TRACE_EXIT("tsv.validate_header");
    return 0;
}

int bt_tsv_parse_sheet(const char *text, bt_tsv_rows *out, bt_tsv_error *err) {
    BT_TRACE_ENTER("tsv.parse_sheet");
    out->items = NULL;
    out->count = 0;
    int saw_header = 0;
    size_t index = 0;
    const char *cursor = text;
    while (1) {
        const char *line = cursor;
        const char *newline = strchr(cursor, '\n');
        size_t raw_len = newline == NULL ? strlen(cursor) : (size_t)(newline - cursor);
        cursor = newline == NULL ? cursor + raw_len : newline + 1;
        size_t len = raw_len;
        if (len > 0 && line[len - 1] == '\r') {
            len -= 1;
        }
        char buffer[8192];
        if (len >= sizeof buffer) {
            bt_tsv_rows_free(out);
            BT_TRACE_FAIL("tsv.parse_sheet", "line %zu exceeds the buffer", index + 1);
            return -1;
        }
        memcpy(buffer, line, len);
        buffer[len] = '\0';
        if (trim_len(buffer) == 0) {
            index += 1;
            if (newline == NULL) {
                break;
            }
            continue;
        }
        size_t line_number = index + 1;
        if (!saw_header) {
            if (validate_header(buffer, line_number, err) != 0) {
                bt_tsv_rows_free(out);
                return -1;
            }
            saw_header = 1;
            index += 1;
            if (newline == NULL) {
                break;
            }
            continue;
        }
        char *cells[BT_TSV_CELL_SLOTS];
        size_t count = 0;
        char *scan = buffer;
        cells[count++] = scan;
        while ((scan = strchr(scan, '\t')) != NULL && count < BT_TSV_CELL_SLOTS) {
            *scan = '\0';
            scan += 1;
            cells[count++] = scan;
        }
        while (count > BT_TSV_COLUMNS && trim_len(cells[count - 1]) == 0) {
            count -= 1;
        }
        if (count != BT_TSV_COLUMNS) {
            err->kind = BT_TSV_ERR_COLUMN_COUNT;
            err->line = line_number;
            err->found = count;
            bt_tsv_rows_free(out);
            BT_TRACE_FAIL("tsv.parse_sheet", "line %zu holds %zu columns", line_number, count);
            return -1;
        }
        for (size_t i = 0; i < BT_TSV_COLUMNS; i++) {
            if ((i == 0 || i == 1) && trim_len(cells[i]) == 0) {
                err->kind = BT_TSV_ERR_EMPTY_FIELD;
                err->line = line_number;
                snprintf(err->column, sizeof err->column, "%s", HEADER_CELLS[i]);
                bt_tsv_rows_free(out);
                BT_TRACE_FAIL("tsv.parse_sheet", "%s column empty at line %zu", HEADER_CELLS[i],
                              line_number);
                return -1;
            }
        }
        char *owned[BT_TSV_COLUMNS];
        for (size_t i = 0; i < BT_TSV_COLUMNS; i++) {
            owned[i] = cell_dup(cells[i], strlen(cells[i]));
            if (owned[i] == NULL) {
                for (size_t k = 0; k < i; k++) {
                    free(owned[k]);
                }
                bt_tsv_rows_free(out);
                BT_TRACE_FAIL("tsv.parse_sheet", "allocation failed at line %zu", line_number);
                return -1;
            }
        }
        if (row_push(out, line_number, owned) != 0) {
            for (size_t i = 0; i < BT_TSV_COLUMNS; i++) {
                free(owned[i]);
            }
            bt_tsv_rows_free(out);
            BT_TRACE_FAIL("tsv.parse_sheet", "row push failed at line %zu", line_number);
            return -1;
        }
        index += 1;
        if (newline == NULL) {
            break;
        }
    }
    BT_TRACE_EXIT("tsv.parse_sheet");
    return 0;
}
