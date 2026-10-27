#include "verify_test_util.h"

#include "verify/when.h"

#include <string.h>

BT_TEST(verify_when, epoch_is_1970_01_01) {
    char out[VERIFY_ISO8601_LEN];
    verify_when_iso8601(0, out, sizeof out);
    BT_CHECK_EQ(out, "1970-01-01T00:00:00Z");
    verify_when_iso8601(86399, out, sizeof out);
    BT_CHECK_EQ(out, "1970-01-01T23:59:59Z");
    verify_when_iso8601(86400, out, sizeof out);
    BT_CHECK_EQ(out, "1970-01-02T00:00:00Z");
}

BT_TEST(verify_when, known_unix_stamps_render_exactly) {
    char out[VERIFY_ISO8601_LEN];
    verify_when_iso8601(951782400, out, sizeof out);
    BT_CHECK_EQ(out, "2000-02-29T00:00:00Z");
    verify_when_iso8601(951868800, out, sizeof out);
    BT_CHECK_EQ(out, "2000-03-01T00:00:00Z");
    verify_when_iso8601(1000000000, out, sizeof out);
    BT_CHECK_EQ(out, "2001-09-09T01:46:40Z");
    verify_when_iso8601(1791763200, out, sizeof out);
    BT_CHECK_EQ(out, "2026-10-12T00:00:00Z");
    verify_when_iso8601(1791849599, out, sizeof out);
    BT_CHECK_EQ(out, "2026-10-12T23:59:59Z");
}

BT_TEST(verify_when, years_advance_through_leap_days) {
    char out[VERIFY_ISO8601_LEN];
    verify_when_iso8601(2ULL * 365 * 86400, out, sizeof out);
    BT_CHECK_EQ(out, "1972-01-01T00:00:00Z");
    verify_when_iso8601(3ULL * 365 * 86400, out, sizeof out);
    BT_CHECK_EQ(out, "1972-12-31T00:00:00Z");
}

BT_TEST(verify_when, now_is_parseable_utc_shape) {
    char out[VERIFY_ISO8601_LEN];
    verify_when_now(out, sizeof out);
    BT_CHECK_EQ(strlen(out), 20);
    BT_CHECK_EQ(out[4], '-');
    BT_CHECK_EQ(out[10], 'T');
    BT_CHECK(out[19] == 'Z');
}
