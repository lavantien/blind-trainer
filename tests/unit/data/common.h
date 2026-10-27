#ifndef BT_DATA_TEST_COMMON_H
#define BT_DATA_TEST_COMMON_H

#include "common/bt_harness.h"
#include "common/paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TD_TEXT_MAX 8192
#define TD_NAME_MAX 8

typedef struct {
    const char *solver_dir;
    const char *file;
    size_t expected;
} td_sheet_pin;

static const td_sheet_pin TD_SHEETS[12] = {
    {"cherry", "edge.tsv", 551},  {"cherry", "corner.tsv", 328}, {"cherry", "flips.tsv", 25},
    {"cherry", "twists.tsv", 20}, {"cherry", "parity.tsv", 26},  {"cherry", "ltct.tsv", 8},
    {"eggins", "edge.tsv", 71},   {"eggins", "corner.tsv", 38},  {"eggins", "flips.tsv", 0},
    {"eggins", "twists.tsv", 5},  {"eggins", "parity.tsv", 2},   {"eggins", "ltct.tsv", 3},
};

static inline void td_ref_ground(char out[BT_PATH_MAX]) {
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

static inline char *td_read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        BT_ABORT("cannot open %s", path);
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *buffer = malloc((size_t)size + 1);
    if (buffer == NULL) {
        BT_ABORT("out of memory reading %s", path);
    }
    size_t got = fread(buffer, 1, (size_t)size, file);
    fclose(file);
    buffer[got] = '\0';
    return buffer;
}

static inline char *td_sheet_text(const char *solver_dir, const char *file) {
    char root[BT_PATH_MAX];
    td_ref_ground(root);
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s\\%s\\%s", root, solver_dir, file);
    return td_read_file(path);
}

static inline void td_packs_root(char out[BT_PATH_MAX]) {
    td_ref_ground(out);
    size_t len = strlen(out);
    snprintf(out + len, BT_PATH_MAX - len, "%s", "\\packs");
}

static inline char *td_ref_file(const char *relative) {
    char root[BT_PATH_MAX];
    td_ref_ground(root);
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s\\%s", root, relative);
    return td_read_file(path);
}

typedef struct {
    size_t count;
    char cols[1077][8][TD_TEXT_MAX];
} td_simcheck_rows;

static inline void td_load_simcheck(td_simcheck_rows *out) {
    char *text = td_ref_file("simcheck.tsv");
    out->count = 0;
    char *cursor = text;
    bool header = true;
    while (*cursor != '\0') {
        char *line = cursor;
        char *newline = strchr(line, '\n');
        if (newline != NULL) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor = line + strlen(line);
        }
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\r') {
            line[len - 1] = '\0';
        }
        bool blank = true;
        for (char *p = line; *p != '\0'; p++) {
            if (*p != ' ' && *p != '\t') {
                blank = false;
            }
        }
        if (blank) {
            continue;
        }
        if (header) {
            header = false;
            continue;
        }
        char copy[TD_TEXT_MAX];
        snprintf(copy, sizeof copy, "%s", line);
        size_t col = 0;
        char *cell = copy;
        out->cols[out->count][col][0] = '\0';
        for (char *p = copy;; p++) {
            if (*p == '\t' || *p == '\0') {
                bool end = *p == '\0';
                *p = '\0';
                snprintf(out->cols[out->count][col], TD_TEXT_MAX, "%s", cell);
                col += 1;
                if (end || col >= 8) {
                    break;
                }
                cell = p + 1;
                out->cols[out->count][col][0] = '\0';
            }
        }
        out->count += 1;
    }
    free(text);
}

#endif
