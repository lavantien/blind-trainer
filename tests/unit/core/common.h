#ifndef BT_CORE_TEST_COMMON_H
#define BT_CORE_TEST_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_harness.h"
#include "common/bt_trace.h"
#include "common/paths.h"
#include "common/rng.h"
#include "core/alg.h"
#include "core/cube.h"

#define TC_MAX_LINES 1200
#define TC_LINE_MAX 512
#define TC_MAX_ROWS 1077
#define TC_TEXT_MAX 512
#define TC_NAME_MAX 8

typedef struct {
    size_t count;
    char lines[TC_MAX_LINES][TC_LINE_MAX];
} tc_lines;

typedef struct {
    size_t count;
    char rel[TC_MAX_ROWS][32];
    char text[TC_MAX_ROWS][TC_TEXT_MAX];
} tc_vendor_rows;

typedef struct {
    size_t count;
    char cols[TC_MAX_ROWS][8][TC_TEXT_MAX];
} tc_simcheck_rows;

typedef struct {
    char name_of_sticker[48][TC_NAME_MAX];
    uint8_t piece_of_sticker[48];
    char sticker_keys[48][TC_NAME_MAX];
} tc_name_index;

static const char *TC_SHEETS[12][2] = {
    {"cherry/edge.tsv", "551"},  {"cherry/corner.tsv", "328"}, {"cherry/flips.tsv", "25"},
    {"cherry/twists.tsv", "20"}, {"cherry/parity.tsv", "26"},  {"cherry/ltct.tsv", "8"},
    {"eggins/edge.tsv", "71"},   {"eggins/corner.tsv", "38"},  {"eggins/flips.tsv", "0"},
    {"eggins/twists.tsv", "5"},  {"eggins/parity.tsv", "2"},   {"eggins/ltct.tsv", "3"},
};

static inline void tc_ref_ground(char out[BT_PATH_MAX]) {
    const char *raw = getenv("BT_REF_GROUND");
    if (raw == NULL || raw[0] == '\0') {
        BT_ABORT("BT_REF_GROUND is unset: run tests through bt.py");
    }
    if (raw[0] == '/' || (raw[1] == ':' && (raw[2] == '\\' || raw[2] == '/'))) {
        snprintf(out, BT_PATH_MAX, "%s", raw);
        return;
    }
    char root[BT_PATH_MAX];
    bt_paths_workspace_root(root);
    snprintf(out, BT_PATH_MAX, "%s\\%s", root, raw);
}

static inline void tc_sheet_lines(const char *rel, tc_lines *out) {
    BT_TRACE_ENTER("tc_sheet_lines");
    static tc_lines storage;
    char base[BT_PATH_MAX];
    tc_ref_ground(base);
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s\\%s", base, rel);
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        BT_ABORT("read %s failed", path);
    }
    storage.count = 0;
    char buffer[TC_LINE_MAX];
    bool ended_with_newline = false;
    while (fgets(buffer, sizeof buffer, file) != NULL) {
        size_t len = strlen(buffer);
        ended_with_newline = len > 0 && buffer[len - 1] == '\n';
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r')) {
            buffer[--len] = '\0';
        }
        if (storage.count >= TC_MAX_LINES) {
            BT_ABORT("sheet %s exceeds %d lines", rel, TC_MAX_LINES);
        }
        snprintf(storage.lines[storage.count], TC_LINE_MAX, "%s", buffer);
        storage.count += 1;
    }
    if (ended_with_newline && storage.count < TC_MAX_LINES) {
        storage.lines[storage.count][0] = '\0';
        storage.count += 1;
    }
    fclose(file);
    *out = storage;
    BT_TRACE_EXIT("tc_sheet_lines");
}

static inline size_t tc_split(char *line, char *fields[], size_t max_fields) {
    size_t count = 0;
    char *cursor = line;
    while (count < max_fields) {
        fields[count++] = cursor;
        char *tab = strchr(cursor, '\t');
        if (tab == NULL) {
            break;
        }
        *tab = '\0';
        cursor = tab + 1;
    }
    return count;
}

static inline size_t tc_split_ws(char *text, char *fields[], size_t cap) {
    size_t count = 0;
    char *cursor = text;
    while (*cursor != '\0' && count < cap) {
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        fields[count++] = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
            cursor++;
        }
        if (*cursor != '\0') {
            *cursor++ = '\0';
        }
    }
    return count;
}

static inline size_t tc_split_on(char *text, char sep, char *fields[], size_t cap) {
    size_t count = 0;
    char *cursor = text;
    while (count < cap) {
        fields[count++] = cursor;
        char *at = strchr(cursor, sep);
        if (at == NULL) {
            break;
        }
        *at = '\0';
        cursor = at + 1;
    }
    return count;
}

static inline void tc_load_vendor_rows(tc_vendor_rows *out) {
    BT_TRACE_ENTER("tc_vendor_rows");
    out->count = 0;
    for (size_t s = 0; s < 12; s++) {
        static tc_lines lines;
        tc_sheet_lines(TC_SHEETS[s][0], &lines);
        if (lines.count == 0 || strncmp(lines.lines[0], "Position", 8) != 0) {
            BT_ABORT("%s header: %s", TC_SHEETS[s][0], lines.count ? lines.lines[0] : "(empty)");
        }
        size_t expected = (size_t)strtoul(TC_SHEETS[s][1], NULL, 10);
        size_t count = 0;
        for (size_t i = 1; i < lines.count; i++) {
            if (lines.lines[i][0] == '\0') {
                continue;
            }
            char copy[TC_LINE_MAX];
            snprintf(copy, sizeof copy, "%s", lines.lines[i]);
            char *fields[8];
            size_t nfields = tc_split(copy, fields, 8);
            if (nfields != 5) {
                BT_ABORT("%s: %s", TC_SHEETS[s][0], lines.lines[i]);
            }
            if (out->count >= TC_MAX_ROWS) {
                BT_ABORT("vendor rows exceed %d", TC_MAX_ROWS);
            }
            snprintf(out->rel[out->count], 32, "%s", TC_SHEETS[s][0]);
            snprintf(out->text[out->count], TC_TEXT_MAX, "%s", fields[1]);
            out->count += 1;
            count += 1;
        }
        if (count != expected) {
            BT_ABORT("%s: %zu rows, want %zu", TC_SHEETS[s][0], count, expected);
        }
    }
    BT_TRACE_EXIT("tc_vendor_rows");
}

static inline void tc_load_simcheck_rows(tc_simcheck_rows *out) {
    BT_TRACE_ENTER("tc_simcheck_rows");
    static tc_lines lines;
    tc_sheet_lines("simcheck.tsv", &lines);
    if (lines.count == 0) {
        BT_ABORT("simcheck.tsv is empty");
    }
    char header[TC_LINE_MAX];
    snprintf(header, sizeof header, "%s", lines.lines[0]);
    char *fields[8];
    size_t nfields = tc_split(header, fields, 8);
    const char *want[8] = {"solver", "sheet", "rowid", "position", "moves", "qtm", "match", "note"};
    if (nfields != 8) {
        BT_ABORT("simcheck header has %zu columns", nfields);
    }
    for (size_t i = 0; i < 8; i++) {
        if (strcmp(fields[i], want[i]) != 0) {
            BT_ABORT("simcheck header column %zu: %s", i, fields[i]);
        }
    }
    out->count = 0;
    for (size_t i = 1; i < lines.count; i++) {
        if (lines.lines[i][0] == '\0') {
            continue;
        }
        char copy[TC_LINE_MAX];
        snprintf(copy, sizeof copy, "%s", lines.lines[i]);
        char *cols[8];
        size_t n = tc_split(copy, cols, 8);
        if (n != 8) {
            BT_ABORT("simcheck row has %zu columns: %s", n, lines.lines[i]);
        }
        if (out->count >= TC_MAX_ROWS) {
            BT_ABORT("simcheck rows exceed %d", TC_MAX_ROWS);
        }
        for (size_t c = 0; c < 8; c++) {
            snprintf(out->cols[out->count][c], TC_TEXT_MAX, "%s", cols[c]);
        }
        out->count += 1;
    }
    BT_TRACE_EXIT("tc_simcheck_rows");
}

static inline void tc_sheet_alg(const char *solver, const char *sheet, size_t rowid, char *out,
                                size_t out_size) {
    static char cached_rel[64];
    static tc_lines cached;
    static bool cached_valid;
    char rel[64];
    snprintf(rel, sizeof rel, "%s/%s.tsv", solver, sheet);
    if (!cached_valid || strcmp(cached_rel, rel) != 0) {
        tc_sheet_lines(rel, &cached);
        snprintf(cached_rel, sizeof cached_rel, "%s", rel);
        cached_valid = true;
    }
    if (rowid < 1 || rowid >= cached.count) {
        BT_ABORT("%s row %zu out of range", rel, rowid);
    }
    char copy[TC_LINE_MAX];
    snprintf(copy, sizeof copy, "%s", cached.lines[rowid - 1]);
    char *fields[8];
    tc_split(copy, fields, 8);
    snprintf(out, out_size, "%s", fields[1]);
}

static inline void tc_position_names(tc_name_index *out) {
    BT_TRACE_ENTER("tc_position_names");
    static tc_lines lines;
    tc_sheet_lines("position_map.tsv", &lines);
    if (lines.count == 0) {
        BT_ABORT("position_map.tsv is empty");
    }
    char header[TC_LINE_MAX];
    snprintf(header, sizeof header, "%s", lines.lines[0]);
    char *fields[8];
    size_t nfields = tc_split(header, fields, 8);
    const char *want[8] = {"orbit", "sticker", "letter", "name", "face", "row", "col", "piece"};
    if (nfields != 8) {
        BT_ABORT("position_map header has %zu columns", nfields);
    }
    for (size_t i = 0; i < 8; i++) {
        if (strcmp(fields[i], want[i]) != 0) {
            BT_ABORT("position_map header column %zu: %s", i, fields[i]);
        }
    }
    for (size_t i = 0; i < 48; i++) {
        out->piece_of_sticker[i] = 255;
    }
    size_t names = 0;
    for (size_t i = 1; i < lines.count; i++) {
        if (lines.lines[i][0] == '\0') {
            continue;
        }
        char copy[TC_LINE_MAX];
        snprintf(copy, sizeof copy, "%s", lines.lines[i]);
        char *cols[8];
        size_t n = tc_split(copy, cols, 8);
        if (n != 8) {
            BT_ABORT("position_map row: %s", lines.lines[i]);
        }
        size_t sticker = (size_t)strtoul(cols[1], NULL, 10);
        uint8_t piece = (uint8_t)strtoul(cols[7], NULL, 10);
        bool is_corner = strcmp(cols[0], "corner") == 0;
        if (is_corner) {
            BT_CHECK(sticker < 24 && piece < 8);
        } else {
            BT_CHECK(strcmp(cols[0], "edge") == 0);
            BT_CHECK(sticker >= 24 && sticker < 48 && piece < 12);
        }
        snprintf(out->name_of_sticker[sticker], TC_NAME_MAX, "%s", cols[3]);
        snprintf(out->sticker_keys[sticker], TC_NAME_MAX, "%s", cols[3]);
        out->piece_of_sticker[sticker] = piece;
        names += 1;
    }
    BT_CHECK_EQ(names, (size_t)48);
    for (size_t i = 0; i < 48; i++) {
        BT_CHECK(out->piece_of_sticker[i] != 255);
    }
    BT_TRACE_EXIT("tc_position_names");
}

static inline uint8_t tc_sticker_of_name(const tc_name_index *names, const char *name) {
    for (size_t i = 0; i < 48; i++) {
        if (strcmp(names->sticker_keys[i], name) == 0) {
            return (uint8_t)i;
        }
    }
    BT_ABORT("unknown sticker name %s", name);
    abort();
}

static inline size_t tc_alg_pool(char pool[][8], size_t cap) {
    size_t n = 0;
    const char *faces = "UDLRFB";
    const char *suffixes[4] = {"", "'", "2", "2'"};
    const char *extra = "MESxyz";
    for (size_t f = 0; f < 6; f++) {
        for (size_t s = 0; s < 4; s++) {
            if (n < cap)
                snprintf(pool[n++], 8, "%c%s", faces[f], suffixes[s]);
            if (n < cap)
                snprintf(pool[n++], 8, "2%c%s", faces[f], suffixes[s]);
            if (n < cap) {
                snprintf(pool[n++], 8, "%c%s", (char)(faces[f] | 0x20), suffixes[s]);
            }
        }
    }
    for (size_t e = 0; e < 6; e++) {
        for (size_t s = 0; s < 4; s++) {
            if (n < cap)
                snprintf(pool[n++], 8, "%c%s", extra[e], suffixes[s]);
        }
    }
    return n;
}

static inline void tc_alg_text(bt_rng *rng, char *out, size_t out_size) {
    char pool[96][8];
    size_t pool_len = tc_alg_pool(pool, 96);
    size_t count = (size_t)bt_rng_below(rng, 24);
    size_t used = 0;
    out[0] = '\0';
    for (size_t i = 0; i < count; i++) {
        size_t pick = (size_t)bt_rng_below(rng, (uint64_t)pool_len);
        int written = snprintf(out + used, out_size - used, "%s%s", i == 0 ? "" : " ", pool[pick]);
        if (written < 0 || (size_t)written >= out_size - used) {
            break;
        }
        used += (size_t)written;
    }
}

#endif
