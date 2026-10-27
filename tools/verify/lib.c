#include "verify/lib.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/strbuf.h"

const verify_sheet_pin VERIFY_SHEET_EXPECTED_ROWS[12] = {
    {"cherry", "edge", 551},  {"cherry", "corner", 328}, {"cherry", "flips", 25},
    {"cherry", "twists", 20}, {"cherry", "parity", 26},  {"cherry", "ltct", 8},
    {"eggins", "edge", 71},   {"eggins", "corner", 38},  {"eggins", "flips", 0},
    {"eggins", "twists", 5},  {"eggins", "parity", 2},   {"eggins", "ltct", 3},
};

const size_t VERIFY_SHEET_PIN_COUNT = 12;

void verify_sim_map_init(verify_sim_map *map) {
    map->items = NULL;
    map->count = 0;
    map->cap = 0;
}

void verify_sim_map_free(verify_sim_map *map) {
    for (size_t at = 0; at < map->count; at++) {
        free(map->items[at].row.position);
    }
    free(map->items);
    map->items = NULL;
    map->count = 0;
    map->cap = 0;
}

void verify_sim_map_insert(verify_sim_map *map, bt_solver solver, bt_sheet_class sheet,
                           size_t rowid, const char *position, uint32_t qtm) {
    BT_TRACE_ENTER("verify.sim_map_insert");
    for (size_t at = 0; at < map->count; at++) {
        verify_sim_entry *entry = &map->items[at];
        if (entry->solver == solver && entry->sheet == sheet && entry->rowid == rowid) {
            free(entry->row.position);
            entry->row.position = verify_xstrdup(position);
            entry->row.qtm = qtm;
            BT_TRACE_EXIT("verify.sim_map_insert");
            return;
        }
    }
    if (map->count == map->cap) {
        size_t cap = map->cap == 0 ? 64 : map->cap * 2;
        verify_sim_entry *items = realloc(map->items, cap * sizeof *items);
        if (items == NULL) {
            BT_TRACE_FAIL("verify.sim_map_insert", "oom at %zu", map->count);
            BT_TRACE_EXIT("verify.sim_map_insert");
            return;
        }
        map->items = items;
        map->cap = cap;
    }
    verify_sim_entry *entry = &map->items[map->count];
    entry->solver = solver;
    entry->sheet = sheet;
    entry->rowid = rowid;
    entry->row.position = verify_xstrdup(position);
    entry->row.qtm = qtm;
    map->count += 1;
    BT_TRACE_EXIT("verify.sim_map_insert");
}

const verify_sim_row *verify_sim_map_get(const verify_sim_map *map, bt_solver solver,
                                         bt_sheet_class sheet, size_t rowid) {
    for (size_t at = 0; at < map->count; at++) {
        const verify_sim_entry *entry = &map->items[at];
        if (entry->solver == solver && entry->sheet == sheet && entry->rowid == rowid) {
            return &entry->row;
        }
    }
    return NULL;
}

static bool parse_index(const char *text, size_t *out) {
    const char *cursor = text;
    if (*cursor == '+') {
        cursor += 1;
    }
    if (*cursor == '\0') {
        return false;
    }
    size_t value = 0;
    for (; *cursor != '\0'; cursor++) {
        if (*cursor < '0' || *cursor > '9') {
            return false;
        }
        if (value > (SIZE_MAX - (size_t)(*cursor - '0')) / 10) {
            return false;
        }
        value = value * 10 + (size_t)(*cursor - '0');
    }
    *out = value;
    return true;
}

static bool parse_qtm(const char *text, uint32_t *out) {
    const char *cursor = text;
    if (*cursor == '+') {
        cursor += 1;
    }
    if (*cursor == '\0') {
        return false;
    }
    uint64_t value = 0;
    for (; *cursor != '\0'; cursor++) {
        if (*cursor < '0' || *cursor > '9') {
            return false;
        }
        if (value > (UINT64_MAX - (uint64_t)(*cursor - '0')) / 10) {
            return false;
        }
        value = value * 10 + (uint64_t)(*cursor - '0');
    }
    if (value > UINT32_MAX) {
        return false;
    }
    *out = (uint32_t)value;
    return true;
}

int verify_load_simcheck(const char *root, verify_sim_map *out, char *err, size_t err_len) {
    BT_TRACE_ENTER("verify.load_simcheck");
    verify_sim_map_init(out);
    char path[VERIFY_ERR_MAX * 2];
    snprintf(path, sizeof path, "%s/simcheck.tsv", root);
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        snprintf(err, err_len, "read %s: %s", path, strerror(errno));
        BT_TRACE_FAIL("verify.load_simcheck", "open failed %s", path);
        BT_TRACE_EXIT("verify.load_simcheck");
        return 1;
    }
    char line[VERIFY_LINE_MAX];
    bool header = true;
    while (fgets(line, sizeof line, file) != NULL) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        bool blank = true;
        for (size_t at = 0; at < len; at++) {
            if (line[at] != ' ' && line[at] != '\t') {
                blank = false;
            }
        }
        if (blank) {
            continue;
        }
        if (header) {
            header = false;
            if (strcmp(line, VERIFY_SIMCHECK_HEADER) != 0) {
                snprintf(err, err_len, "simcheck.tsv header changed: %s", line);
                fclose(file);
                BT_TRACE_FAIL("verify.load_simcheck", "header changed");
                BT_TRACE_EXIT("verify.load_simcheck");
                return 1;
            }
            continue;
        }
        char *cols[9];
        size_t count = bt_tsv_split(line, cols, 9);
        if (count != 8) {
            snprintf(err, err_len, "simcheck row has %zu columns: %s", count, line);
            fclose(file);
            BT_TRACE_FAIL("verify.load_simcheck", "%zu columns", count);
            BT_TRACE_EXIT("verify.load_simcheck");
            return 1;
        }
        bt_solver solver;
        if (!bt_solver_parse(cols[0], &solver)) {
            snprintf(err, err_len, "simcheck solver %s is unknown", cols[0]);
            fclose(file);
            BT_TRACE_FAIL("verify.load_simcheck", "solver %s", cols[0]);
            BT_TRACE_EXIT("verify.load_simcheck");
            return 1;
        }
        bt_sheet_class sheet;
        if (!bt_sheet_class_parse(cols[1], &sheet)) {
            snprintf(err, err_len, "simcheck sheet %s is unknown", cols[1]);
            fclose(file);
            BT_TRACE_FAIL("verify.load_simcheck", "sheet %s", cols[1]);
            BT_TRACE_EXIT("verify.load_simcheck");
            return 1;
        }
        size_t rowid;
        if (!parse_index(cols[2], &rowid)) {
            snprintf(err, err_len, "simcheck rowid %s is not a number", cols[2]);
            fclose(file);
            BT_TRACE_FAIL("verify.load_simcheck", "rowid %s", cols[2]);
            BT_TRACE_EXIT("verify.load_simcheck");
            return 1;
        }
        uint32_t qtm;
        if (!parse_qtm(cols[5], &qtm)) {
            snprintf(err, err_len, "simcheck qtm %s is not a number", cols[5]);
            fclose(file);
            BT_TRACE_FAIL("verify.load_simcheck", "qtm %s", cols[5]);
            BT_TRACE_EXIT("verify.load_simcheck");
            return 1;
        }
        verify_sim_map_insert(out, solver, sheet, rowid, cols[3], qtm);
    }
    if (header) {
        snprintf(err, err_len, "simcheck.tsv is empty");
        fclose(file);
        BT_TRACE_FAIL("verify.load_simcheck", "empty");
        BT_TRACE_EXIT("verify.load_simcheck");
        return 1;
    }
    fclose(file);
    BT_TRACE_EXIT("verify.load_simcheck");
    return 0;
}

const char *verify_crate_id(void) {
    return "bt-verify";
}

char *verify_xstrdup(const char *text) {
    size_t len = strlen(text) + 1;
    char *copy = malloc(len);
    if (copy != NULL) {
        memcpy(copy, text, len);
    }
    return copy;
}
