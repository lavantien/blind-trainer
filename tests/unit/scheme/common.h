#ifndef BT_SCHEME_TEST_COMMON_H
#define BT_SCHEME_TEST_COMMON_H

#include "common/bt_harness.h"
#include "common/paths.h"
#include "common/strbuf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline char *bt_test_read_file(const char *path) {
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

typedef struct {
    char *data;
    char *cursor;
} bt_test_lines;

static inline void bt_test_lines_begin(bt_test_lines *lines, const char *path) {
    lines->data = bt_test_read_file(path);
    lines->cursor = lines->data;
}

static inline char *bt_test_lines_next(bt_test_lines *lines) {
    if (*lines->cursor == '\0') {
        return NULL;
    }
    char *line = lines->cursor;
    char *newline = strchr(line, '\n');
    if (newline != NULL) {
        *newline = '\0';
        lines->cursor = newline + 1;
    } else {
        lines->cursor = line + strlen(line);
    }
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\r') {
        line[len - 1] = '\0';
    }
    return line;
}

static inline void bt_test_lines_end(bt_test_lines *lines) {
    free(lines->data);
}

static inline void bt_test_ref_path(char out[BT_PATH_MAX], const char *relative) {
    const char *raw = getenv("BT_REF_GROUND");
    if (raw == NULL || *raw == '\0') {
        BT_ABORT("BT_REF_GROUND is unset: run tests through bt.py");
    }
    snprintf(out, BT_PATH_MAX, "%s/%s", raw, relative);
}

static inline long long bt_test_parse_int(const char *text) {
    char *end = NULL;
    long long value = strtoll(text, &end, 10);
    if (end == text) {
        BT_ABORT("not a number: %s", text);
    }
    return value;
}

#endif
