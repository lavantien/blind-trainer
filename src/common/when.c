#include "common/when.h"

#include <stdio.h>
#include <time.h>

#include "common/bt_trace.h"

static void civil_from_days(long long days, long long *year, unsigned *month, unsigned *day) {
    long long shifted = days + 719468;
    long long era = (shifted >= 0 ? shifted : shifted - 146096) / 146097;
    unsigned long long day_of_era = (unsigned long long)(shifted - era * 146097);
    unsigned long long year_of_era =
        (day_of_era - day_of_era / 1460 + day_of_era / 36524 - day_of_era / 146096) / 365;
    long long y = (long long)year_of_era + era * 400;
    unsigned long long day_of_year =
        day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    unsigned long long month_prime = (5 * day_of_year + 2) / 153;
    *day = (unsigned)(day_of_year - (153 * month_prime + 2) / 5 + 1);
    *month = (unsigned)(month_prime < 10 ? month_prime + 3 : month_prime - 9);
    *year = *month <= 2 ? y + 1 : y;
}

void bt_when_iso8601(uint64_t unix_secs, char *out, size_t out_len) {
    BT_TRACE_ENTER("when.iso8601");
    long long days = (long long)(unix_secs / 86400);
    uint64_t secs = unix_secs % 86400;
    long long year;
    unsigned month;
    unsigned day;
    civil_from_days(days, &year, &month, &day);
    snprintf(out, out_len, "%04lld-%02u-%02uT%02llu:%02llu:%02lluZ", year, month, day,
             (unsigned long long)(secs / 3600), (unsigned long long)(secs % 3600 / 60),
             (unsigned long long)(secs % 60));
    BT_TRACE_EXIT("when.iso8601");
}

void bt_when_now(char *out, size_t out_len) {
    BT_TRACE_ENTER("when.now");
    time_t now = time(NULL);
    uint64_t secs = now > 0 ? (uint64_t)now : 0;
    bt_when_iso8601(secs, out, out_len);
    BT_TRACE_EXIT("when.now");
}
