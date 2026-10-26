#include "common/bt_harness.h"
#include "common/bt_trace.h"

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
