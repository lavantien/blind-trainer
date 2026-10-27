#ifndef BT_APP_FONTS_H
#define BT_APP_FONTS_H

#include "common/json.h"
#include "data/corpus.h"

#include <stddef.h>

#define BT_APP_FONTS_MAX 2048
#define BT_APP_FONTS_CODEPOINT_LIMIT 0x110000

int bt_app_fonts_codepoints(const bt_json_value *config, const bt_corpus *corpus,
                            const char *const *words, size_t word_count, int *out, size_t cap);

#endif
