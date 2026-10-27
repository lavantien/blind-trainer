#include "app/fonts.h"

#include "common.h"

#include "data/corpus.h"
#include "data/paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *text_dup(const char *text) {
    size_t len = strlen(text);
    char *copy = malloc(len + 1);
    memcpy(copy, text, len + 1);
    return copy;
}

static bt_json_value *config_with_ranges(void) {
    const char *text = "{\"app\":{\"fonts\":{\"codepoint_ranges\":[[32,126],[160,255],"
                       "[7840,7843]]}},\"paths\":{\"fonts\":\"assets/fonts\"}}";
    bt_json_value *value = NULL;
    bt_json_error error;
    if (bt_json_parse(text, strlen(text), &value, &error) != 0) {
        BT_ABORT("ranges json: %s", error.message);
    }
    return value;
}

static bt_corpus sample_corpus(void) {
    bt_corpus corpus;
    corpus.rows = calloc(3, sizeof *corpus.rows);
    corpus.count = 3;
    corpus.rows[0].position = text_dup("UF-UB");
    corpus.rows[0].alg_text = text_dup("R2 U' S R2 S' R2 U R2");
    corpus.rows[0].commutator = text_dup("[S,R2]");
    corpus.rows[1].position = text_dup("Vietnamese ấ");
    corpus.rows[1].alg_text = text_dup("M2");
    corpus.rows[1].commutator = NULL;
    corpus.rows[2].position = text_dup("x");
    corpus.rows[2].alg_text = text_dup("U D");
    corpus.rows[2].commutator = text_dup("ơ ư đ");
    return corpus;
}

static int contains(const int *codepoints, int count, int want) {
    for (int i = 0; i < count; i++) {
        if (codepoints[i] == want) {
            return 1;
        }
    }
    return 0;
}

BT_TEST(fonts, codepoint_set_covers_config_ranges_corpus_and_words) {
    bt_json_value *config = config_with_ranges();
    bt_corpus corpus = sample_corpus();
    const char *words[3] = {"Quail Man", "thử", "Ếch"};
    int codepoints[BT_APP_FONTS_MAX];
    int count = bt_app_fonts_codepoints(config, &corpus, words, 3, codepoints, BT_APP_FONTS_MAX);
    BT_CHECK(count > 0);
    int previous = -1;
    for (int i = 0; i < count; i++) {
        BT_CHECK(codepoints[i] > previous);
        previous = codepoints[i];
    }
    BT_CHECK(contains(codepoints, count, 'A') && contains(codepoints, count, 'z'));
    BT_CHECK(contains(codepoints, count, ' ') && contains(codepoints, count, '-'));
    BT_CHECK(contains(codepoints, count, '2') && contains(codepoints, count, '\''));
    BT_CHECK(contains(codepoints, count, 0xE1));
    BT_CHECK(contains(codepoints, count, 0x1EA5));
    BT_CHECK(contains(codepoints, count, 0x1EBE));
    bt_corpus_free(&corpus);
    bt_json_free(config);
}

BT_TEST(fonts, default_ranges_cover_latin1_latin_ext_and_vietnamese) {
    bt_json_value *config = config_with_ranges();
    bt_json_value *app = bt_json_get(config, "app");
    bt_json_object_set(app, "fonts", bt_json_new_null());
    bt_corpus corpus;
    bt_corpus_from_rows(NULL, 0, &corpus);
    int codepoints[BT_APP_FONTS_MAX];
    int count = bt_app_fonts_codepoints(config, &corpus, NULL, 0, codepoints, BT_APP_FONTS_MAX);
    BT_CHECK(count > 0);
    BT_CHECK(contains(codepoints, count, 0x20) && contains(codepoints, count, 0x7E));
    BT_CHECK(contains(codepoints, count, 0xA0) && contains(codepoints, count, 0xFF));
    BT_CHECK(contains(codepoints, count, 0x102) && contains(codepoints, count, 0x103));
    BT_CHECK(contains(codepoints, count, 0x110) && contains(codepoints, count, 0x111));
    BT_CHECK(contains(codepoints, count, 0x1A0) && contains(codepoints, count, 0x1AF));
    BT_CHECK(contains(codepoints, count, 0x1B0));
    BT_CHECK(contains(codepoints, count, 0x1EA0) && contains(codepoints, count, 0x1EF9));
    bt_corpus_free(&corpus);
    bt_json_free(config);
}

BT_TEST(fonts, memo_letters_join_the_set_from_the_scheme) {
    bt_json_value *config = config_with_ranges();
    bt_corpus corpus;
    bt_corpus_from_rows(NULL, 0, &corpus);
    int codepoints[BT_APP_FONTS_MAX];
    int count = bt_app_fonts_codepoints(config, &corpus, NULL, 0, codepoints, BT_APP_FONTS_MAX);
    for (int letter = 'A'; letter <= 'X'; letter++) {
        BT_CHECK(contains(codepoints, count, letter));
    }
    bt_corpus_free(&corpus);
    bt_json_free(config);
}

BT_TEST(fonts, capacity_overflow_is_reported_not_truncated_silently) {
    bt_json_value *config = config_with_ranges();
    bt_corpus corpus;
    bt_corpus_from_rows(NULL, 0, &corpus);
    int codepoints[8];
    int count = bt_app_fonts_codepoints(config, &corpus, NULL, 0, codepoints, 8);
    BT_CHECK_EQ(count, -1);
    bt_corpus_free(&corpus);
    bt_json_free(config);
}
