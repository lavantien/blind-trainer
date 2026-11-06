#include <stdlib.h>
#include <string.h>

#include "common/alloc.h"
#include "common/bt_harness.h"

BT_TEST(alloc, default_path_allocates_writable_memory) {
    char *block = bt_malloc(16);
    BT_CHECK(block != NULL);
    memset(block, 7, 16);
    BT_CHECK_EQ((int)block[15], 7);
    free(block);
    char *zeroed = bt_calloc(4, 8);
    BT_CHECK(zeroed != NULL);
    for (size_t i = 0; i < 32; i++) {
        BT_CHECK_EQ((int)zeroed[i], 0);
    }
    char *grown = bt_realloc(zeroed, 64);
    BT_CHECK(grown != NULL);
    grown[63] = 3;
    BT_CHECK_EQ((int)grown[63], 3);
    free(grown);
}

BT_TEST(alloc, armed_zero_fails_the_next_allocation_and_counts) {
    bt_alloc_fail_arm(0);
    BT_CHECK(bt_malloc(8) == NULL);
    BT_CHECK(bt_calloc(2, 4) == NULL);
    BT_CHECK_EQ(bt_alloc_fail_count(), (size_t)2);
    bt_alloc_fail_disarm();
    char *after = bt_malloc(8);
    BT_CHECK(after != NULL);
    free(after);
}

BT_TEST(alloc, armed_count_lets_that_many_pass_then_fails) {
    bt_alloc_fail_arm(2);
    void *first = bt_malloc(4);
    void *second = bt_calloc(1, 4);
    BT_CHECK(first != NULL && second != NULL);
    BT_CHECK(bt_malloc(4) == NULL);
    BT_CHECK_EQ(bt_alloc_fail_count(), (size_t)1);
    bt_alloc_fail_disarm();
    char *third = bt_malloc(4);
    BT_CHECK(third != NULL);
    free(third);
    free(first);
    free(second);
}

BT_TEST(alloc, realloc_failure_leaves_the_original_block_valid) {
    char *block = bt_malloc(8);
    BT_CHECK(block != NULL);
    memset(block, 9, 8);
    bt_alloc_fail_arm(0);
    char *grown = bt_realloc(block, 64);
    BT_CHECK(grown == NULL);
    BT_CHECK_EQ((int)block[7], 9);
    bt_alloc_fail_disarm();
    free(block);
}

BT_TEST(alloc, disarm_stops_injection_and_resets_the_counter_on_rearm) {
    bt_alloc_fail_arm(1);
    char *counted = bt_malloc(4);
    BT_CHECK(counted != NULL);
    bt_alloc_fail_disarm();
    char *plain = bt_malloc(4);
    BT_CHECK(plain != NULL);
    BT_CHECK_EQ(bt_alloc_fail_count(), (size_t)0);
    free(counted);
    free(plain);
    bt_alloc_fail_arm(0);
    BT_CHECK(bt_malloc(4) == NULL);
    BT_CHECK_EQ(bt_alloc_fail_count(), (size_t)1);
    bt_alloc_fail_disarm();
}

BT_TEST(alloc, harness_wrappers_control_the_same_hook) {
    bt_harness_alloc_arm(0);
    BT_CHECK(bt_malloc(4) == NULL);
    BT_CHECK_EQ(bt_harness_alloc_failures(), (size_t)1);
    bt_harness_alloc_disarm();
    char *after = bt_malloc(4);
    BT_CHECK(after != NULL);
    free(after);
}

BT_TEST(alloc, oversized_requests_report_real_allocation_failure) {
    BT_CHECK(bt_malloc(SIZE_MAX) == NULL);
    BT_CHECK(bt_calloc(SIZE_MAX, 2) == NULL);
    void *block = bt_malloc(8);
    BT_CHECK(block != NULL);
    BT_CHECK(bt_realloc(block, SIZE_MAX) == NULL);
    free(block);
}
