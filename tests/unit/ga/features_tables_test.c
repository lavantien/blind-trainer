#include "ga/features.h"
#include "ga/inventory.h"

#include "common/bt_harness.h"

#include "ga_test_util.h"

BT_TEST(features, every_rime_row_parses_back_to_itself) {
    size_t count = 0;
    const bt_ga_rime_row *rimes = bt_ga_rimes_table(&count);
    BT_CHECK(count == 95);
    for (size_t i = 0; i < count; i++) {
        bt_ga_features parsed = bt_ga_parse_or_abort(rimes[i].letters);
        BT_CHECK(!parsed.has_onset);
        BT_CHECK_EQ((long long)parsed.nucleus, (long long)rimes[i].nucleus);
        BT_CHECK_EQ((long long)parsed.coda, (long long)rimes[i].coda);
        BT_CHECK_EQ((long long)parsed.tone, (long long)BT_GA_TONE_NGANG);
    }
}

BT_TEST(features, every_tone_marked_row_is_detected_through_parse) {
    size_t count = 0;
    const bt_ga_tone_marked_row *rows = bt_ga_tone_marked_table(&count);
    BT_CHECK(count == 60);
    for (size_t i = 0; i < count; i++) {
        int open = strcmp(rows[i].base, "\xc4\x83") != 0 && strcmp(rows[i].base, "\xc3\xa2") != 0;
        char text[16];
        if (open) {
            snprintf(text, sizeof text, "m%s", rows[i].marked);
        } else {
            snprintf(text, sizeof text, "m%sn", rows[i].marked);
        }
        bt_ga_features parsed;
        if (!bt_ga_parse(text, &parsed)) {
            BT_ABORT("parse failed for %s", text);
        }
        BT_CHECK_EQ((long long)parsed.tone, (long long)rows[i].tone);
        BT_CHECK_EQ((long long)parsed.coda, (long long)(open ? BT_GA_CODA_OPEN : BT_GA_CODA_N));
        BT_CHECK(parsed.has_onset);
        BT_CHECK_EQ((long long)parsed.onset.manner, (long long)BT_GA_MANNER_NASAL);
    }
}

BT_TEST(features, confusion_bounds_hold_across_a_parsed_corpus) {
    const char *words[] = {"không", "của", "tía", "anh",  "mút",  "đặt",  "trăng",
                           "y",     "òn",  "gì",  "xinh", "vững", "được", "ngăn",
                           "ma",    "má",  "en",  "ôn",   "áp",   "ác"};
    bt_ga_features feats[20];
    for (int i = 0; i < 20; i++) {
        feats[i] = bt_ga_parse_or_abort(words[i]);
    }
    for (int i = 0; i < 20; i++) {
        BT_CHECK_EQ(bt_ga_confusion(&feats[i], &feats[i]), 1.0f);
        for (int j = 0; j < 20; j++) {
            float got = bt_ga_confusion(&feats[i], &feats[j]);
            BT_CHECK(got > 0.0f && got <= 1.0f);
        }
    }
}
