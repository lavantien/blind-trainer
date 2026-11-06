#include "common/alloc.h"
#include "common/bt_harness.h"
#include "data/seedpack/normalizer.h"
#include "data/seedpack/parse.h"
#include "unit/data/test_util.h"

#include <stdio.h>
#include <string.h>

static const char *HEADER = "case_key\talg\tfront_json\tsource_url\tsource_at";

static bt_pack_entry entry_of(const char *event, size_t rows) {
    bt_pack_entry entry;
    memset(&entry, 0, sizeof entry);
    snprintf(entry.file, sizeof entry.file, "%s", "probe.tsv");
    snprintf(entry.event, sizeof entry.event, "%s", event);
    snprintf(entry.method, sizeof entry.method, "%s", "cfop");
    snprintf(entry.stage, sizeof entry.stage, "%s", "pll");
    entry.expected_rows = rows;
    return entry;
}

BT_TEST(seedpack_oom, error_string_names_the_chain_and_empty_rejections) {
    bt_normalized normalized;
    bt_normalize_error err;
    char text[128];
    BT_CHECK(bt_seedpack_normalize_alg("3x3", "-> note", &normalized, &err) == false);
    BT_CHECK(err.kind == BT_NORMALIZE_ERR_BAD_CHAIN);
    bt_normalize_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "chain marker") != NULL);
    BT_CHECK(strstr(text, "-> note") != NULL);
    BT_CHECK(bt_seedpack_normalize_alg("3x3", "   ", &normalized, &err) == false);
    BT_CHECK(err.kind == BT_NORMALIZE_ERR_EMPTY_ALG);
    bt_normalize_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "empty") != NULL);
}

BT_TEST(seedpack_oom, normalize_cube_fails_closed_under_each_allocation_failure) {
    for (size_t ok = 0; ok < 16; ok++) {
        bt_normalized normalized;
        bt_normalize_error err;
        bt_alloc_fail_arm(ok);
        bool good = bt_seedpack_normalize_alg("3x3", "[R U]*2 -> drill note", &normalized, &err);
        bt_alloc_fail_disarm();
        if (good) {
            BT_CHECK_EQ(normalized.alg, "R U R U");
            BT_CHECK((int)normalized.note_count > 0);
        }
        bt_normalized_free(&normalized);
    }
    bt_normalized normalized;
    bt_normalize_error err;
    BT_CHECK(bt_seedpack_normalize_alg("3x3", "[R U]*2 -> drill note", &normalized, &err));
    BT_CHECK_EQ(normalized.alg, "R U R U");
    BT_CHECK((int)normalized.note_count > 0);
    bt_normalized_free(&normalized);
}

BT_TEST(seedpack_oom, normalize_sq1_fails_closed_under_each_allocation_failure) {
    for (size_t ok = 0; ok < 12; ok++) {
        bt_normalized normalized;
        bt_normalize_error err;
        bt_alloc_fail_arm(ok);
        bool good = bt_seedpack_normalize_alg("sq1", "(1,0) / (3,3)", &normalized, &err);
        bt_alloc_fail_disarm();
        if (good) {
            BT_CHECK_EQ(normalized.alg, "(1,0) / (3,3)");
        }
        bt_normalized_free(&normalized);
    }
    bt_normalized normalized;
    bt_normalize_error err;
    BT_CHECK(bt_seedpack_normalize_alg("sq1", "(1,0) / (3,3)", &normalized, &err));
    BT_CHECK_EQ(normalized.alg, "(1,0) / (3,3)");
    BT_CHECK_EQ((int)normalized.note_count, 0);
    bt_normalized_free(&normalized);
}

BT_TEST(seedpack_oom, parse_pack_fails_closed_under_each_allocation_failure) {
    char text[512];
    snprintf(text, sizeof text, "%s\nT\tR U R'\t{}\thttps://x\t2026-01-01\n", HEADER);
    bt_pack_entry entry = entry_of("3x3", 1);
    for (size_t ok = 0; ok < 40; ok++) {
        bt_seed_rows rows;
        bt_seed_error err;
        bt_alloc_fail_arm(ok);
        int rc = bt_seedpack_parse_pack(&entry, text, &rows, &err);
        bt_alloc_fail_disarm();
        if (rc == 0) {
            BT_CHECK_EQ((int)rows.count, 1);
            BT_CHECK_EQ(rows.items[0].alg, "R U R'");
        }
        bt_seed_rows_free(&rows);
    }
    bt_seed_rows rows;
    bt_seed_error err;
    BT_CHECK(bt_seedpack_parse_pack(&entry, text, &rows, &err) == 0);
    BT_CHECK_EQ((int)rows.count, 1);
    bt_seed_rows_free(&rows);
}

BT_TEST(seedpack_oom, parse_pack_skips_whitespace_only_lines_between_rows) {
    char text[512];
    snprintf(text, sizeof text, "%s\n \t \nT\tR U R'\t{}\thttps://x\t2026-01-01\n\n   \n", HEADER);
    bt_pack_entry entry = entry_of("3x3", 1);
    bt_seed_rows rows;
    bt_seed_error err;
    BT_CHECK(bt_seedpack_parse_pack(&entry, text, &rows, &err) == 0);
    BT_CHECK_EQ((int)rows.count, 1);
    BT_CHECK_EQ(rows.items[0].case_key, "T");
    bt_seed_rows_free(&rows);
}

BT_TEST(seedpack_oom, group_repeat_expansion_fails_closed_under_each_allocation_failure) {
    for (size_t ok = 0; ok < 18; ok++) {
        bt_normalized normalized;
        bt_normalize_error err;
        bt_alloc_fail_arm(ok);
        bool good = bt_seedpack_normalize_alg("3x3", "(R U)3", &normalized, &err);
        bt_alloc_fail_disarm();
        if (good) {
            BT_CHECK_EQ(normalized.alg, "R U R U R U");
        }
        bt_normalized_free(&normalized);
    }
    bt_normalized normalized;
    bt_normalize_error err;
    BT_CHECK(bt_seedpack_normalize_alg("3x3", "(R U)3", &normalized, &err));
    BT_CHECK_EQ(normalized.alg, "R U R U R U");
    bt_normalized_free(&normalized);
}

BT_TEST(seedpack_oom, token_growth_and_the_x_note_fail_closed) {
    for (size_t ok = 0; ok < 24; ok++) {
        bt_normalized normalized;
        bt_normalize_error err;
        bt_alloc_fail_arm(ok);
        bool good = bt_seedpack_normalize_alg("3x3", "X R U R U R U R U R U R U R U R U R U",
                                              &normalized, &err);
        bt_alloc_fail_disarm();
        if (good) {
            BT_CHECK_EQ(normalized.alg, "x R U R U R U R U R U R U R U R U R U");
            BT_CHECK((int)normalized.note_count > 0);
        }
        bt_normalized_free(&normalized);
    }
    bt_normalized normalized;
    bt_normalize_error err;
    BT_CHECK(bt_seedpack_normalize_alg("3x3", "X R U R U R U R U R U R U R U R U R U", &normalized,
                                       &err));
    BT_CHECK_EQ(normalized.alg, "x R U R U R U R U R U R U R U R U R U");
    BT_CHECK((int)normalized.note_count > 0);
    bt_normalized_free(&normalized);
}

BT_TEST(seedpack_oom, chain_split_and_prose_paths_fail_closed) {
    for (size_t ok = 0; ok < 12; ok++) {
        bt_normalized normalized;
        bt_normalize_error err;
        bt_alloc_fail_arm(ok);
        bool good = bt_seedpack_normalize_alg("3x3", "R U -> drill note", &normalized, &err);
        bt_alloc_fail_disarm();
        if (good) {
            BT_CHECK_EQ(normalized.alg, "R U");
            BT_CHECK((int)normalized.note_count > 0);
        }
        bt_normalized_free(&normalized);
    }
    for (size_t ok = 0; ok < 8; ok++) {
        bt_normalized normalized;
        bt_normalize_error err;
        bt_alloc_fail_arm(ok);
        bool good = bt_seedpack_normalize_alg("3x3", "rotate the whole cube", &normalized, &err);
        bt_alloc_fail_disarm();
        if (good) {
            BT_CHECK_EQ(normalized.alg, "");
            BT_CHECK((int)normalized.note_count > 0);
        }
        bt_normalized_free(&normalized);
    }
    bt_normalized normalized;
    bt_normalize_error err;
    BT_CHECK(bt_seedpack_normalize_alg("3x3", "R U -> drill note", &normalized, &err));
    BT_CHECK_EQ(normalized.alg, "R U");
    bt_normalized_free(&normalized);
}

BT_TEST(seedpack_oom, sq1_notes_and_part_growth_fail_closed) {
    for (size_t ok = 0; ok < 15; ok++) {
        bt_normalized normalized;
        bt_normalize_error err;
        bt_alloc_fail_arm(ok);
        bool good = bt_seedpack_normalize_alg("sq1", "(U  D2) (0,-6)", &normalized, &err);
        bt_alloc_fail_disarm();
        if (good) {
            BT_CHECK_EQ(normalized.alg, "(0,6)");
            BT_CHECK((int)normalized.note_count > 0);
        }
        bt_normalized_free(&normalized);
    }
    for (size_t ok = 0; ok < 13; ok++) {
        bt_normalized normalized;
        bt_normalize_error err;
        bt_alloc_fail_arm(ok);
        bool good = bt_seedpack_normalize_alg("sq1", "0,-6 /", &normalized, &err);
        bt_alloc_fail_disarm();
        if (good) {
            BT_CHECK_EQ(normalized.alg, "(0,6) /");
            BT_CHECK((int)normalized.note_count > 0);
        }
        bt_normalized_free(&normalized);
    }
    for (size_t ok = 0; ok < 31; ok++) {
        bt_normalized normalized;
        bt_normalize_error err;
        bt_alloc_fail_arm(ok);
        bool good = bt_seedpack_normalize_alg("sq1", "////////////////////", &normalized, &err);
        bt_alloc_fail_disarm();
        if (good) {
            BT_CHECK_EQ(normalized.alg, "/ / / / / / / / / / / / / / / / / / / /");
        }
        bt_normalized_free(&normalized);
    }
    for (size_t ok = 0; ok < 6; ok++) {
        bt_normalized normalized;
        bt_normalize_error err;
        bt_alloc_fail_arm(ok);
        bool good = bt_seedpack_normalize_alg("sq1", "(1,0) *", &normalized, &err);
        bt_alloc_fail_disarm();
        if (good) {
            BT_CHECK_EQ(normalized.alg, "(1,0)");
            BT_CHECK((int)normalized.note_count > 0);
        }
        bt_normalized_free(&normalized);
    }
    bt_normalized normalized;
    bt_normalize_error err;
    BT_CHECK(bt_seedpack_normalize_alg("sq1", "0,-6 /", &normalized, &err));
    BT_CHECK_EQ(normalized.alg, "(0,6) /");
    bt_normalized_free(&normalized);
}
