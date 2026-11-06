#include <intrin.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "common/alloc.h"

enum { TRACE_CAP = 1U << 22U };

typedef struct {
    size_t index;
    size_t size;
    void *site;
} trace_row;

static trace_row rows[TRACE_CAP];
static size_t row_count = 0;
static size_t alloc_index = 0;

static bool armed = false;
static size_t ok_left = 0;
static size_t failures = 0;

static bool alloc_allowed(void) {
    if (!armed) {
        return true;
    }
    if (ok_left == 0) {
        failures++;
        return false;
    }
    ok_left--;
    return true;
}

static void trace_row_push(size_t size) {
    if (row_count >= TRACE_CAP) {
        return;
    }
    rows[row_count].index = alloc_index;
    rows[row_count].size = size;
    rows[row_count].site = _ReturnAddress();
    row_count++;
}

void *bt_malloc(size_t size) {
    trace_row_push(size);
    alloc_index++;
    if (!alloc_allowed()) {
        return NULL;
    }
    return malloc(size);
}

void *bt_calloc(size_t count, size_t size) {
    size_t bytes = count * size;
    trace_row_push(bytes);
    alloc_index++;
    if (!alloc_allowed()) {
        return NULL;
    }
    return calloc(count, size);
}

void *bt_realloc(void *ptr, size_t size) {
    trace_row_push(size);
    alloc_index++;
    if (!alloc_allowed()) {
        return NULL;
    }
    return realloc(ptr, size);
}

void bt_alloc_fail_arm(size_t ok_count) {
    armed = true;
    ok_left = ok_count;
    failures = 0;
}

void bt_alloc_fail_disarm(void) {
    armed = false;
    ok_left = 0;
}

size_t bt_alloc_fail_count(void) {
    return failures;
}

size_t bt_alloc_trace_total(void) {
    return row_count;
}

void bt_alloc_trace_dump(const char *path) {
    FILE *out = fopen(path, "w");
    if (out == NULL) {
        fprintf(stderr, "trace dump open failed: %s\n", path);
        exit(2);
    }
    for (size_t i = 0; i < row_count; i++) {
        fprintf(out, "%zu %zu %p\n", rows[i].index, rows[i].size, rows[i].site);
    }
    fclose(out);
}
