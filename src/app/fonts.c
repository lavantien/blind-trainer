#include "app/fonts.h"

#include "common/bt_trace.h"

#include <string.h>

static const int BT_APP_FONTS_DEFAULT_RANGES[][2] = {
    {0x20, 0x7E}, {0xA0, 0xFF}, {0x102, 0x103}, {0x110, 0x111}, {0x1A0, 0x1B0}, {0x1EA0, 0x1EF9},
};

enum {
    BT_APP_FONTS_DEFAULT_RANGE_COUNT = 6,
};
#define BT_APP_FONTS_BITMAP_BYTES (BT_APP_FONTS_CODEPOINT_LIMIT / 8)

static unsigned char bt_app_fonts_seen[BT_APP_FONTS_BITMAP_BYTES];

static void mark_codepoint(int value) {
    if (value < 0 || value >= BT_APP_FONTS_CODEPOINT_LIMIT) {
        return;
    }
    bt_app_fonts_seen[value / 8] |= (unsigned char)(1U << (unsigned int)(value % 8));
}

static void mark_range(int low, int high) {
    for (int value = low; value <= high && value < BT_APP_FONTS_CODEPOINT_LIMIT; value++) {
        mark_codepoint(value);
    }
}

static void mark_text(const char *text) {
    if (text == NULL) {
        return;
    }
    const unsigned char *cursor = (const unsigned char *)text;
    while (*cursor != 0) {
        unsigned int value = 0;
        int extra = 0;
        if ((*cursor & 0x80U) == 0) {
            value = *cursor;
            extra = 0;
        } else if ((*cursor & 0xE0U) == 0xC0U) {
            value = *cursor & 0x1FU;
            extra = 1;
        } else if ((*cursor & 0xF0U) == 0xE0U) {
            value = *cursor & 0x0FU;
            extra = 2;
        } else if ((*cursor & 0xF8U) == 0xF0U) {
            value = *cursor & 0x07U;
            extra = 3;
        } else {
            cursor++;
            continue;
        }
        cursor++;
        int valid = 1;
        for (int i = 0; i < extra; i++) {
            if ((*cursor & 0xC0U) != 0x80U) {
                valid = 0;
                break;
            }
            value = value * 64U + ((unsigned int)*cursor & 0x3FU);
            cursor++;
        }
        if (valid) {
            mark_codepoint((int)value);
        }
    }
}

static void mark_config_ranges(const bt_json_value *config) {
    const bt_json_value *app = bt_json_get(config, "app");
    const bt_json_value *fonts = app == NULL ? NULL : bt_json_get(app, "fonts");
    const bt_json_value *ranges = fonts == NULL ? NULL : bt_json_get(fonts, "codepoint_ranges");
    if (ranges == NULL || ranges->type != BT_JSON_ARRAY || ranges->as.array.count == 0) {
        for (int i = 0; i < BT_APP_FONTS_DEFAULT_RANGE_COUNT; i++) {
            mark_range(BT_APP_FONTS_DEFAULT_RANGES[i][0], BT_APP_FONTS_DEFAULT_RANGES[i][1]);
        }
        return;
    }
    for (size_t i = 0; i < ranges->as.array.count; i++) {
        const bt_json_value *pair = bt_json_at(ranges, i);
        if (pair == NULL || pair->type != BT_JSON_ARRAY || pair->as.array.count != 2) {
            continue;
        }
        const bt_json_value *low = bt_json_at(pair, 0);
        const bt_json_value *high = bt_json_at(pair, 1);
        if (low == NULL || high == NULL || low->type != BT_JSON_NUMBER ||
            high->type != BT_JSON_NUMBER || !bt_json_is_integer(low) || !bt_json_is_integer(high)) {
            continue;
        }
        mark_range((int)low->integer, (int)high->integer);
    }
}

int bt_app_fonts_codepoints(const bt_json_value *config, const bt_corpus *corpus,
                            const char *const *words, size_t word_count, int *out, size_t cap) {
    BT_TRACE_ENTER("app.fonts_codepoints");
    memset(bt_app_fonts_seen, 0, sizeof bt_app_fonts_seen);
    mark_config_ranges(config);
    for (int letter = 'A'; letter <= 'X'; letter++) {
        mark_codepoint(letter);
    }
    if (corpus != NULL) {
        for (size_t i = 0; i < corpus->count; i++) {
            mark_text(corpus->rows[i].position);
            mark_text(corpus->rows[i].alg_text);
            mark_text(corpus->rows[i].commutator);
        }
    }
    for (size_t i = 0; i < word_count; i++) {
        mark_text(words[i]);
    }
    size_t count = 0;
    for (int value = 0; value < BT_APP_FONTS_CODEPOINT_LIMIT; value++) {
        if ((bt_app_fonts_seen[value / 8] & (1U << (unsigned int)(value % 8))) != 0) {
            if (count >= cap) {
                BT_TRACE_FAIL("app.fonts_codepoints", "capacity %zu too small", cap);
                BT_TRACE_EXIT("app.fonts_codepoints");
                return -1;
            }
            out[count++] = value;
        }
    }
    BT_TRACE_EXIT("app.fonts_codepoints");
    return (int)count;
}
