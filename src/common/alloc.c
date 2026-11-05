#include "common/alloc.h"

#include <stdbool.h>
#include <stdlib.h>

#include "common/bt_trace.h"

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

void *bt_malloc(size_t size) {
    BT_TRACE_ENTER("alloc.malloc");
    if (!alloc_allowed()) {
        BT_TRACE_FAIL("alloc.malloc", "injected failure");
        BT_TRACE_EXIT("alloc.malloc");
        return NULL;
    }
    void *block = malloc(size);
    if (block == NULL) {
        BT_TRACE_FAIL("alloc.malloc", "malloc of %zu failed", size);
    }
    BT_TRACE_EXIT("alloc.malloc");
    return block;
}

void *bt_calloc(size_t count, size_t size) {
    BT_TRACE_ENTER("alloc.calloc");
    if (!alloc_allowed()) {
        BT_TRACE_FAIL("alloc.calloc", "injected failure");
        BT_TRACE_EXIT("alloc.calloc");
        return NULL;
    }
    void *block = calloc(count, size);
    if (block == NULL) {
        BT_TRACE_FAIL("alloc.calloc", "calloc of %zu by %zu failed", count, size);
    }
    BT_TRACE_EXIT("alloc.calloc");
    return block;
}

void *bt_realloc(void *ptr, size_t size) {
    BT_TRACE_ENTER("alloc.realloc");
    if (!alloc_allowed()) {
        BT_TRACE_FAIL("alloc.realloc", "injected failure");
        BT_TRACE_EXIT("alloc.realloc");
        return NULL;
    }
    void *block = realloc(ptr, size);
    if (block == NULL) {
        BT_TRACE_FAIL("alloc.realloc", "realloc to %zu failed", size);
    }
    BT_TRACE_EXIT("alloc.realloc");
    return block;
}

void bt_alloc_fail_arm(size_t ok_count) {
    BT_TRACE_ENTER("alloc.arm");
    armed = true;
    ok_left = ok_count;
    failures = 0;
    BT_TRACE_EXIT("alloc.arm");
}

void bt_alloc_fail_disarm(void) {
    BT_TRACE_ENTER("alloc.disarm");
    armed = false;
    ok_left = 0;
    BT_TRACE_EXIT("alloc.disarm");
}

size_t bt_alloc_fail_count(void) {
    BT_TRACE_ENTER("alloc.count");
    BT_TRACE_EXIT("alloc.count");
    return failures;
}
