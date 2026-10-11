use std::time::{SystemTime, UNIX_EPOCH};

pub fn now_iso8601() -> String {
    let secs = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_secs())
        .unwrap_or(0);
    iso8601(secs)
}

pub fn iso8601(unix: u64) -> String {
    let days = (unix / 86400) as i64;
    let secs = unix % 86400;
    let (year, month, day) = civil_from_days(days);
    format!(
        "{year:04}-{month:02}-{day:02}T{:02}:{:02}:{:02}Z",
        secs / 3600,
        secs % 3600 / 60,
        secs % 60
    )
}

fn civil_from_days(days: i64) -> (i64, u32, u32) {
    let shifted = days + 719468;
    let era = if shifted >= 0 {
        shifted
    } else {
        shifted - 146096
    } / 146097;
    let day_of_era = (shifted - era * 146097) as u64;
    let year_of_era =
        (day_of_era - day_of_era / 1460 + day_of_era / 36524 - day_of_era / 146096) / 365;
    let year = year_of_era as i64 + era * 400;
    let day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    let month_prime = (5 * day_of_year + 2) / 153;
    let day = (day_of_year - (153 * month_prime + 2) / 5 + 1) as u32;
    let month = if month_prime < 10 {
        month_prime + 3
    } else {
        month_prime - 9
    } as u32;
    (if month <= 2 { year + 1 } else { year }, month, day)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn epoch_is_1970_01_01() {
        assert_eq!(iso8601(0), "1970-01-01T00:00:00Z");
        assert_eq!(iso8601(86399), "1970-01-01T23:59:59Z");
        assert_eq!(iso8601(86400), "1970-01-02T00:00:00Z");
    }

    #[test]
    fn known_unix_stamps_render_exactly() {
        assert_eq!(iso8601(951782400), "2000-02-29T00:00:00Z");
        assert_eq!(iso8601(951868800), "2000-03-01T00:00:00Z");
        assert_eq!(iso8601(1000000000), "2001-09-09T01:46:40Z");
        assert_eq!(iso8601(1791763200), "2026-10-12T00:00:00Z");
        assert_eq!(iso8601(1791849599), "2026-10-12T23:59:59Z");
    }

    #[test]
    fn years_advance_through_leap_days() {
        assert_eq!(iso8601(2 * 365 * 86400), "1972-01-01T00:00:00Z");
        assert_eq!(iso8601(3 * 365 * 86400), "1972-12-31T00:00:00Z");
    }

    #[test]
    fn now_is_parseable_utc_shape() {
        let stamp = now_iso8601();
        assert_eq!(stamp.len(), 20);
        assert_eq!(&stamp[4..5], "-");
        assert_eq!(&stamp[10..11], "T");
        assert!(stamp.ends_with('Z'));
    }
}
