#include "common/when.h"

#include <string.h>

#include "common/bt_harness.h"

BT_TEST(gendata_when, epoch_is_1970_01_01) {
    char stamp[21];
    bt_when_iso8601(0, stamp, sizeof stamp);
    BT_CHECK_EQ(strcmp(stamp, "1970-01-01T00:00:00Z"), 0);
    bt_when_iso8601(86399, stamp, sizeof stamp);
    BT_CHECK_EQ(strcmp(stamp, "1970-01-01T23:59:59Z"), 0);
    bt_when_iso8601(86400, stamp, sizeof stamp);
    BT_CHECK_EQ(strcmp(stamp, "1970-01-02T00:00:00Z"), 0);
}

BT_TEST(gendata_when, known_unix_stamps_render_exactly) {
    char stamp[21];
    bt_when_iso8601(951782400, stamp, sizeof stamp);
    BT_CHECK_EQ(strcmp(stamp, "2000-02-29T00:00:00Z"), 0);
    bt_when_iso8601(951868800, stamp, sizeof stamp);
    BT_CHECK_EQ(strcmp(stamp, "2000-03-01T00:00:00Z"), 0);
    bt_when_iso8601(1000000000, stamp, sizeof stamp);
    BT_CHECK_EQ(strcmp(stamp, "2001-09-09T01:46:40Z"), 0);
    bt_when_iso8601(1791763200, stamp, sizeof stamp);
    BT_CHECK_EQ(strcmp(stamp, "2026-10-12T00:00:00Z"), 0);
    bt_when_iso8601(1791849599, stamp, sizeof stamp);
    BT_CHECK_EQ(strcmp(stamp, "2026-10-12T23:59:59Z"), 0);
}

BT_TEST(gendata_when, years_advance_through_leap_days) {
    char stamp[21];
    bt_when_iso8601(2ULL * 365ULL * 86400ULL, stamp, sizeof stamp);
    BT_CHECK_EQ(strcmp(stamp, "1972-01-01T00:00:00Z"), 0);
    bt_when_iso8601(3ULL * 365ULL * 86400ULL, stamp, sizeof stamp);
    BT_CHECK_EQ(strcmp(stamp, "1972-12-31T00:00:00Z"), 0);
}

BT_TEST(gendata_when, now_is_parseable_utc_shape) {
    char stamp[21];
    bt_when_now(stamp, sizeof stamp);
    BT_CHECK_EQ(strlen(stamp), 20);
    BT_CHECK_EQ(stamp[4], '-');
    BT_CHECK_EQ(stamp[10], 'T');
    BT_CHECK_EQ(stamp[19], 'Z');
}
