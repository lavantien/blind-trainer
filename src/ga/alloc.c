#include "ga/alloc.h"

#include "common/bt_trace.h"

#include <stdlib.h>

static bt_ga_alloc_hook bt_ga_alloc_active;
static bt_ga_free_hook bt_ga_free_active;
static void *bt_ga_alloc_user;

static void *bt_ga_alloc_libc(size_t bytes, const char *site, void *user) {
    (void)site;
    (void)user;
    return malloc(bytes);
}

static void bt_ga_free_libc(void *block, void *user) {
    (void)user;
    free(block);
}

void bt_ga_alloc_install(bt_ga_alloc_hook alloc, bt_ga_free_hook release, void *user) {
    BT_TRACE_ENTER("ga.alloc_install");
    bt_ga_alloc_active = alloc;
    bt_ga_free_active = release;
    bt_ga_alloc_user = user;
    BT_TRACE_EXIT("ga.alloc_install");
}

void bt_ga_alloc_reset(void) {
    BT_TRACE_ENTER("ga.alloc_reset");
    bt_ga_alloc_install(bt_ga_alloc_libc, bt_ga_free_libc, NULL);
    BT_TRACE_EXIT("ga.alloc_reset");
}

void *bt_ga_alloc(size_t bytes, const char *site) {
    if (bt_ga_alloc_active == NULL) {
        bt_ga_alloc_reset();
    }
    return bt_ga_alloc_active(bytes, site, bt_ga_alloc_user);
}

void bt_ga_free(void *block) {
    if (bt_ga_free_active == NULL) {
        bt_ga_free_libc(block, NULL);
        return;
    }
    bt_ga_free_active(block, bt_ga_alloc_user);
}
