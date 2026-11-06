#include "common/alloc.h"
#include "common/bt_harness.h"
#include "common/bt_trace.h"
#include "unit/core/death_test.h"

#include <stdio.h>
#include <string.h>

BT_TEST(trace, macros_write_enter_exit_and_fail_lines) {
    FILE *sink = bt_test_open_scratch("bt_trace_sink");
    BT_CHECK(sink != NULL);
    BT_CHECK_EQ(bt_trace_init(4096, 2, 256), 0);
    bt_trace_enter("alpha");
    bt_trace_exit("alpha");
    BT_TRACE_FAIL("alpha", "boom %d", 7);
    bt_trace_dump_to(sink);
    bt_trace_shutdown();
    char buffer[8192];
    memset(buffer, 0, sizeof buffer);
    fseek(sink, 0, SEEK_SET);
    size_t got = fread(buffer, 1, sizeof buffer - 1, sink);
    fclose(sink);
    BT_CHECK(got > 0);
    BT_CHECK(strstr(buffer, "enter alpha") != NULL);
    BT_CHECK(strstr(buffer, "exit alpha") != NULL);
    BT_CHECK(strstr(buffer, "fail alpha") != NULL);
    BT_CHECK(strstr(buffer, "boom 7") != NULL);
}

BT_TEST(trace, ring_keeps_newest_entries) {
    FILE *sink = bt_test_open_scratch("bt_trace_sink");
    BT_CHECK(sink != NULL);
    BT_CHECK_EQ(bt_trace_init(96, 1, 256), 0);
    for (int i = 0; i < 8; i++) {
        char unit[32];
        snprintf(unit, sizeof unit, "unit_%d_name_padding", i);
        bt_trace_enter(unit);
    }
    bt_trace_dump_to(sink);
    bt_trace_shutdown();
    char buffer[8192];
    memset(buffer, 0, sizeof buffer);
    fseek(sink, 0, SEEK_SET);
    size_t got = fread(buffer, 1, sizeof buffer - 1, sink);
    fclose(sink);
    BT_CHECK(got > 0);
    BT_CHECK(got < sizeof buffer - 1);
    BT_CHECK(strstr(buffer, "unit_7_name_padding") != NULL);
    BT_CHECK(strstr(buffer, "unit_6_name_padding") != NULL);
    BT_CHECK(strstr(buffer, "unit_0_name_padding") == NULL);
    BT_CHECK(strstr(buffer, "unit_1_name_padding") == NULL);
}

BT_TEST(trace, second_init_is_rejected_and_keeps_first_ring) {
    FILE *sink = bt_test_open_scratch("bt_trace_sink");
    BT_CHECK(sink != NULL);
    BT_CHECK_EQ(bt_trace_init(4096, 1, 256), 0);
    bt_trace_enter("first");
    BT_CHECK(bt_trace_init(1024, 1, 256) != 0);
    bt_trace_dump_to(sink);
    bt_trace_shutdown();
    char buffer[8192];
    memset(buffer, 0, sizeof buffer);
    fseek(sink, 0, SEEK_SET);
    size_t got = fread(buffer, 1, sizeof buffer - 1, sink);
    fclose(sink);
    BT_CHECK(got > 0);
    BT_CHECK(strstr(buffer, "enter first") != NULL);
}

BT_TEST(trace, dump_without_init_is_a_noop) {
    FILE *sink = bt_test_open_scratch("bt_trace_sink");
    BT_CHECK(sink != NULL);
    bt_trace_enter("noinit");
    bt_trace_dump_to(sink);
    bt_trace_shutdown();
    fseek(sink, 0, SEEK_END);
    BT_CHECK_EQ(ftell(sink), 0);
    fclose(sink);
}

BT_TEST(trace, shutdown_allows_reinit) {
    BT_CHECK_EQ(bt_trace_init(512, 1, 256), 0);
    bt_trace_enter("cycle");
    bt_trace_shutdown();
    FILE *sink = bt_test_open_scratch("bt_trace_sink");
    BT_CHECK(sink != NULL);
    BT_CHECK_EQ(bt_trace_init(512, 1, 256), 0);
    bt_trace_enter("second_cycle");
    bt_trace_dump_to(sink);
    bt_trace_shutdown();
    char buffer[4096];
    memset(buffer, 0, sizeof buffer);
    fseek(sink, 0, SEEK_SET);
    size_t got = fread(buffer, 1, sizeof buffer - 1, sink);
    fclose(sink);
    BT_CHECK(got > 0);
    BT_CHECK(strstr(buffer, "enter second_cycle") != NULL);
    BT_CHECK(strstr(buffer, "enter cycle") == NULL);
}

BT_TEST(trace, long_lines_are_clamped_to_line_max) {
    FILE *sink = bt_test_open_scratch("bt_trace_sink");
    BT_CHECK(sink != NULL);
    BT_CHECK_EQ(bt_trace_init(8192, 1, 64), 0);
    bt_trace_fail("wide", "%s",
                  "0123456789012345678901234567890123456789012345678901234567890123456789");
    bt_trace_dump_to(sink);
    bt_trace_shutdown();
    fseek(sink, 0, SEEK_END);
    long size = ftell(sink);
    fclose(sink);
    BT_CHECK(size > 0);
    BT_CHECK(size < 256);
}

BT_TEST(trace, crash_handler_installs_and_removes) {
    BT_CHECK_EQ(bt_trace_install_crash_handler(), 0);
    BT_CHECK_EQ(bt_trace_remove_crash_handler(), 0);
}

BT_TEST(trace, init_rejects_thread_counts_out_of_range) {
    BT_CHECK(bt_trace_init(4096, 0, 256) != 0);
    BT_CHECK(bt_trace_init(4096, BT_TRACE_MAX_SLOTS + 1, 256) != 0);
    bt_trace_shutdown();
}

BT_TEST(trace, init_failure_frees_the_rings_allocated_so_far) {
    bt_alloc_fail_arm(1);
    BT_CHECK(bt_trace_init(4096, 3, 256) != 0);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(bt_alloc_fail_count(), (size_t)1);
    BT_CHECK_EQ(bt_trace_init(4096, 3, 256), 0);
    bt_trace_shutdown();
}

BT_TEST(trace, a_line_wider_than_the_ring_is_dropped) {
    FILE *sink = bt_test_open_scratch("bt_trace_sink");
    BT_CHECK(sink != NULL);
    BT_CHECK_EQ(bt_trace_init(8, 1, 256), 0);
    bt_trace_enter("toolong");
    bt_trace_dump_to(sink);
    bt_trace_shutdown();
    fseek(sink, 0, SEEK_END);
    BT_CHECK_EQ(ftell(sink), 0);
    fclose(sink);
}

BT_TEST(trace, format_errors_fail_closed_silently) {
    BT_CHECK_EQ(bt_trace_init(4096, 1, 256), 0);
    bt_trace_fail("unit", "%");
    bt_trace_fail_at("unit", __FILE__, __LINE__, "%");
    bt_trace_enter("short");
    bt_trace_shutdown();
}

BT_TEST(trace, a_unit_longer_than_the_line_stack_is_clamped) {
    BT_CHECK_EQ(bt_trace_init(4096, 1, 600), 0);
    char unit[600];
    memset(unit, 'u', sizeof unit - 1);
    unit[sizeof unit - 1] = '\0';
    bt_trace_enter(unit);
    bt_trace_shutdown();
}

static DWORD WINAPI trace_extra_thread(void *arg) {
    (void)arg;
    bt_trace_enter("other_thread");
    bt_trace_exit("other_thread");
    return 0;
}

BT_TEST(trace, threads_beyond_the_slot_budget_emit_nothing) {
    BT_CHECK_EQ(bt_trace_init(4096, 1, 256), 0);
    bt_trace_enter("main_thread");
    HANDLE thread = CreateThread(NULL, 0, trace_extra_thread, NULL, 0, NULL);
    BT_CHECK(thread != NULL);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    FILE *sink = bt_test_open_scratch("bt_trace_sink");
    BT_CHECK(sink != NULL);
    bt_trace_dump_to(sink);
    bt_trace_shutdown();
    char buffer[4096];
    memset(buffer, 0, sizeof buffer);
    fseek(sink, 0, SEEK_SET);
    size_t got = fread(buffer, 1, sizeof buffer - 1, sink);
    fclose(sink);
    BT_CHECK(got > 0);
    BT_CHECK(strstr(buffer, "main_thread") != NULL);
    BT_CHECK(strstr(buffer, "other_thread") == NULL);
}

static void trace_death_null_write(void) {
    bt_trace_enter("doomed");
    volatile int *bad = NULL;
    *bad = 1;
}

BT_TEST(trace, the_crash_filter_dumps_the_ring_on_a_fatal_fault) {
    bt_death_expect_code("trace.fault_dump", "doomed", -1073741819LL, trace_death_null_write);
}
