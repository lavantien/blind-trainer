#include "common/alloc.h"
#include "common/bt_harness.h"
#include "common/strbuf.h"

BT_TEST(strbuf, reserve_doubles_until_a_wide_append_fits) {
    bt_strbuf sb;
    BT_CHECK_EQ(bt_strbuf_init(&sb, 16), 0);
    char wide[40];
    memset(wide, 'w', sizeof wide - 1);
    wide[sizeof wide - 1] = '\0';
    BT_CHECK_EQ(bt_strbuf_append_str(&sb, wide), 0);
    BT_CHECK_EQ(sb.cap, (size_t)64);
    BT_CHECK_EQ(sb.len, (size_t)39);
    bt_strbuf_free(&sb);
}

BT_TEST(strbuf, append_and_appendf_report_injected_allocation_failures) {
    bt_strbuf sb;
    BT_CHECK_EQ(bt_strbuf_init(&sb, 16), 0);
    char wide[40];
    memset(wide, 'w', sizeof wide - 1);
    wide[sizeof wide - 1] = '\0';
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_strbuf_append_str(&sb, wide), -1);
    BT_CHECK_EQ(bt_strbuf_appendf(&sb, "%s", wide), -1);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(sb.len, (size_t)0);
    bt_strbuf_free(&sb);
}

BT_TEST(strbuf, join_reports_separator_and_part_allocation_failures) {
    char wide[32];
    memset(wide, 'w', 31);
    wide[31] = '\0';
    const char *wide_parts[2] = {wide, "x"};
    bt_strbuf sb;
    BT_CHECK_EQ(bt_strbuf_init(&sb, 16), 0);
    bt_alloc_fail_arm(1);
    BT_CHECK_EQ(bt_strbuf_join(&sb, "-", wide_parts, 2), -1);
    bt_alloc_fail_disarm();
    bt_strbuf_free(&sb);
    BT_CHECK_EQ(bt_strbuf_init(&sb, 16), 0);
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_strbuf_join(&sb, "-", wide_parts, 2), -1);
    bt_alloc_fail_disarm();
    bt_strbuf_free(&sb);
}

BT_TEST(strbuf, append_grows_and_keeps_exact_content) {
    bt_strbuf sb;
    BT_CHECK_EQ(bt_strbuf_init(&sb, 1), 0);
    char chunk[65];
    memset(chunk, 'x', sizeof chunk - 1);
    chunk[sizeof chunk - 1] = '\0';
    for (int i = 0; i < 20; i++) {
        BT_CHECK_EQ(bt_strbuf_append_str(&sb, chunk), 0);
    }
    BT_CHECK_EQ(sb.len, 20ULL * 64ULL);
    BT_CHECK(sb.data[sb.len] == '\0');
    BT_CHECK(sb.data[0] == 'x');
    BT_CHECK(sb.data[sb.len - 1] == 'x');
    bt_strbuf_free(&sb);
    BT_CHECK(sb.data == NULL);
    BT_CHECK_EQ(sb.len, 0U);
}

BT_TEST(strbuf, append_bytes_and_format) {
    bt_strbuf sb;
    BT_CHECK_EQ(bt_strbuf_init(&sb, 16), 0);
    BT_CHECK_EQ(bt_strbuf_append(&sb, "ab", 2), 0);
    BT_CHECK_EQ(bt_strbuf_appendf(&sb, "-%d-%s-", 42, "mid"), 0);
    BT_CHECK_EQ(bt_strbuf_append_str(&sb, "end"), 0);
    BT_CHECK(strcmp(sb.data, "ab-42-mid-end") == 0);
    bt_strbuf_free(&sb);
}

BT_TEST(strbuf, format_longer_than_capacity_grows) {
    bt_strbuf sb;
    BT_CHECK_EQ(bt_strbuf_init(&sb, 4), 0);
    BT_CHECK_EQ(bt_strbuf_appendf(&sb, "%s", "0123456789ABCDEFGH"), 0);
    BT_CHECK(strcmp(sb.data, "0123456789ABCDEFGH") == 0);
    BT_CHECK_EQ(sb.len, 18U);
    bt_strbuf_free(&sb);
}

BT_TEST(strbuf, trim_strips_surrounding_whitespace) {
    bt_strbuf sb;
    BT_CHECK_EQ(bt_strbuf_init(&sb, 16), 0);
    BT_CHECK_EQ(bt_strbuf_append_str(&sb, "  \t hi there \r\n"), 0);
    bt_strbuf_trim(&sb);
    BT_CHECK(strcmp(sb.data, "hi there") == 0);
    BT_CHECK_EQ(sb.len, 8U);
    bt_strbuf_free(&sb);
}

BT_TEST(strbuf, trim_all_whitespace_leaves_empty) {
    bt_strbuf sb;
    BT_CHECK_EQ(bt_strbuf_init(&sb, 16), 0);
    BT_CHECK_EQ(bt_strbuf_append_str(&sb, " \t\r\n "), 0);
    bt_strbuf_trim(&sb);
    BT_CHECK_EQ(sb.len, 0U);
    BT_CHECK(sb.data[0] == '\0');
    bt_strbuf_free(&sb);
}

BT_TEST(strbuf, join_inserts_separator_between_parts) {
    bt_strbuf sb;
    BT_CHECK_EQ(bt_strbuf_init(&sb, 8), 0);
    const char *parts[3] = {"u", "v", "w"};
    BT_CHECK_EQ(bt_strbuf_join(&sb, "\t", parts, 3), 0);
    BT_CHECK(strcmp(sb.data, "u\tv\tw") == 0);
    bt_strbuf_free(&sb);
}

BT_TEST(tsv, splits_fields_and_keeps_trailing_empty) {
    char line[32];
    strcpy(line, "a\tb\t");
    char *fields[4];
    size_t count = bt_tsv_split(line, fields, 4);
    BT_CHECK_EQ(count, 3U);
    BT_CHECK(strcmp(fields[0], "a") == 0);
    BT_CHECK(strcmp(fields[1], "b") == 0);
    BT_CHECK(strcmp(fields[2], "") == 0);
}

BT_TEST(tsv, empty_line_is_one_empty_field) {
    char line[8];
    strcpy(line, "");
    char *fields[2];
    BT_CHECK_EQ(bt_tsv_split(line, fields, 2), 1U);
    BT_CHECK(strcmp(fields[0], "") == 0);
}

BT_TEST(tsv, single_field_has_no_split) {
    char line[16];
    strcpy(line, "solo");
    char *fields[2];
    BT_CHECK_EQ(bt_tsv_split(line, fields, 2), 1U);
    BT_CHECK(strcmp(fields[0], "solo") == 0);
}

BT_TEST(tsv, overflow_reports_size_max) {
    char line[32];
    strcpy(line, "a\tb\tc\td");
    char *fields[2];
    BT_CHECK_EQ(bt_tsv_split(line, fields, 2), BT_TSV_OVERFLOW);
}
