#ifndef BT_PUZZLES_TEST_COMMON_H
#define BT_PUZZLES_TEST_COMMON_H

#include "common/bt_harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline char *pz_read_pack(const char *leaf) {
    const char *root = getenv("BT_REF_GROUND");
    if (root == NULL || root[0] == '\0') {
        root = "ref_ground";
    }
    char path[512];
    snprintf(path, sizeof path, "%s/packs/%s", root, leaf);
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        BT_ABORT("pack unreadable: %s", path);
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = malloc((size_t)size + 1);
    if (text == NULL || fread(text, 1, (size_t)size, file) != (size_t)size) {
        BT_ABORT("pack unreadable: %s", path);
    }
    text[size] = '\0';
    fclose(file);
    return text;
}

typedef struct {
    char *cursor;
    bool first;
} pz_pack_rows;

static inline void pz_pack_rows_init(pz_pack_rows *rows, char *text) {
    rows->cursor = text;
    rows->first = true;
}

static inline bool pz_pack_rows_next(pz_pack_rows *rows, char *fields[], size_t max_fields,
                                     size_t *field_count, bool *is_header) {
    while (*rows->cursor != '\0') {
        char *line_end = strchr(rows->cursor, '\n');
        if (line_end == NULL) {
            line_end = rows->cursor + strlen(rows->cursor);
        }
        char *line = rows->cursor;
        bool last = *line_end == '\0';
        if (line_end > line && line_end[-1] == '\r') {
            line_end[-1] = '\0';
        }
        *line_end = '\0';
        rows->cursor = last ? line_end : line_end + 1;
        if (line[0] == '\0') {
            continue;
        }
        *is_header = rows->first;
        rows->first = false;
        size_t count = 0;
        char *field = line;
        for (char *scan = line;; scan++) {
            if (*scan == '\t' || *scan == '\0') {
                if (count < max_fields) {
                    fields[count] = field;
                }
                count++;
                bool done = *scan == '\0';
                *scan = '\0';
                if (done) {
                    break;
                }
                field = scan + 1;
            }
        }
        *field_count = count;
        return true;
    }
    return false;
}

static inline char *pz_split_space(char *str, char **save) {
    char *start = str != NULL ? str : *save;
    if (start == NULL) {
        return NULL;
    }
    while (*start == ' ') {
        start++;
    }
    if (*start == '\0') {
        *save = start;
        return NULL;
    }
    char *end = start;
    while (*end != '\0' && *end != ' ') {
        end++;
    }
    if (*end != '\0') {
        *end = '\0';
        end++;
    }
    *save = end;
    return start;
}

static inline uint64_t pz_xorshift(uint64_t *seed) {
    uint64_t x = *seed;
    x ^= x << 13U;
    x ^= x >> 7U;
    x ^= x << 17U;
    *seed = x;
    return x;
}

static inline int pz_cmp_size(const void *a, const void *b) {
    size_t left = *(const size_t *)a;
    size_t right = *(const size_t *)b;
    if (left < right) {
        return -1;
    }
    return left > right ? 1 : 0;
}

static inline int pz_cmp_string(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

#endif
