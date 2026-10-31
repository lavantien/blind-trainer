#include "ga/inventory.h"

#include "common/bt_harness.h"
#include "common/bt_trace.h"

#include "common/config.h"
#include "common/space.h"
#include "ga/alloc.h"
#include "ga/features.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

BT_NORETURN static void bt_ga_inventory_abort(const char *label, const char *why, ...) {
    char text[128];
    va_list args;
    va_start(args, why);
    vsnprintf(text, sizeof text, why, args);
    va_end(args);
    BT_TRACE_FAIL("ga.build_pool", "ga abort: %s", text);
    bt_harness_fail("src/ga/inventory.c", 0, "abort: %s: %s", label, text);
    bt_harness_exit_failure();
}

size_t bt_ga_case_count(void) {
    BT_TRACE_ENTER("ga.case_count");
    BT_TRACE_EXIT("ga.case_count");
    return 818;
}

static char *bt_ga_read_file(const char *path, size_t *len, char err[BT_GA_ERR_MAX]) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        snprintf(err, BT_GA_ERR_MAX, "read %s", path);
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0) {
        fclose(file);
        snprintf(err, BT_GA_ERR_MAX, "read %s", path);
        return NULL;
    }
    char *text = bt_ga_alloc((size_t)size + 1, "ga.read_file.buffer");
    if (text == NULL) {
        fclose(file);
        snprintf(err, BT_GA_ERR_MAX, "allocation failed reading %s", path);
        BT_TRACE_FAIL("ga.read_file", "allocation failed reading %s", path);
        return NULL;
    }
    size_t got = fread(text, 1, (size_t)size, file);
    fclose(file);
    text[got] = '\0';
    *len = got;
    return text;
}

static char *bt_ga_trim(char *text) {
    size_t len = strlen(text);
    size_t at = 0;
    while (at < len) {
        size_t space = bt_space_len(text, at, len);
        if (space == 0) {
            break;
        }
        at += space;
    }
    size_t end = len;
    while (end > at) {
        size_t start = end - 1;
        while (start > at && ((unsigned char)text[start] & 0xC0U) == 0x80U) {
            start--;
        }
        if (bt_space_len(text, start, end) != end - start) {
            break;
        }
        end = start;
    }
    if (end < len) {
        text[end] = '\0';
    }
    return text + at;
}

static int bt_ga_strset_push(bt_ga_strset *set, const char *text) {
    for (size_t i = 0; i < set->count; i++) {
        if (strcmp(set->items[i], text) == 0) {
            return 1;
        }
    }
    char **grown = bt_ga_alloc((set->count + 1) * sizeof *grown, "ga.strset.items");
    if (grown == NULL) {
        return 0;
    }
    if (set->count > 0) {
        memcpy(grown, set->items, set->count * sizeof *grown);
    }
    size_t bytes = strlen(text) + 1;
    char *copy = bt_ga_alloc(bytes, "ga.strset.entry");
    if (copy == NULL) {
        bt_ga_free(grown);
        return 0;
    }
    memcpy(copy, text, bytes);
    bt_ga_free(set->items);
    set->items = grown;
    set->items[set->count] = copy;
    set->count++;
    return 1;
}

void bt_ga_strset_free(bt_ga_strset *set) {
    BT_TRACE_ENTER("ga.strset_free");
    for (size_t i = 0; i < set->count; i++) {
        bt_ga_free(set->items[i]);
    }
    bt_ga_free(set->items);
    set->items = NULL;
    set->count = 0;
    BT_TRACE_EXIT("ga.strset_free");
}

int bt_ga_strset_contains(const bt_ga_strset *set, const char *text) {
    BT_TRACE_ENTER("ga.strset_contains");
    for (size_t i = 0; i < set->count; i++) {
        if (strcmp(set->items[i], text) == 0) {
            BT_TRACE_EXIT("ga.strset_contains");
            return 1;
        }
    }
    BT_TRACE_EXIT("ga.strset_contains");
    return 0;
}

int bt_ga_load_wordlist(const char *path, bt_ga_strset *out, char err[BT_GA_ERR_MAX]) {
    BT_TRACE_ENTER("ga.load_wordlist");
    out->items = NULL;
    out->count = 0;
    size_t len = 0;
    char *text = bt_ga_read_file(path, &len, err);
    if (text == NULL) {
        BT_TRACE_FAIL("ga.load_wordlist", "read failed for %s", path);
        return 0;
    }
    if (!bt_ga_utf8_valid(text)) {
        bt_ga_free(text);
        snprintf(err, BT_GA_ERR_MAX, "utf8 %s", path);
        BT_TRACE_FAIL("ga.load_wordlist", "utf8 failed for %s", path);
        return 0;
    }
    char *cursor = text;
    while (*cursor != '\0') {
        char *line = cursor;
        char *end = strchr(cursor, '\n');
        if (end != NULL) {
            *end = '\0';
            cursor = end + 1;
        } else {
            cursor = line + strlen(line);
        }
        size_t raw = strlen(line);
        if (raw > 0 && line[raw - 1] == '\r') {
            line[raw - 1] = '\0';
        }
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        char *syllable = bt_ga_trim(line);
        if (syllable[0] == '\0') {
            continue;
        }
        if (!bt_ga_is_nfc(syllable)) {
            snprintf(err, BT_GA_ERR_MAX, "combining mark in wordlist entry \"%s\"", syllable);
            bt_ga_free(text);
            bt_ga_strset_free(out);
            BT_TRACE_FAIL("ga.load_wordlist", "combining mark in %s", syllable);
            return 0;
        }
        if (!bt_ga_strset_push(out, syllable)) {
            snprintf(err, BT_GA_ERR_MAX, "wordlist allocation failed at entry %zu", out->count + 1);
            bt_ga_free(text);
            bt_ga_strset_free(out);
            BT_TRACE_FAIL("ga.load_wordlist", "allocation failed at entry %zu", out->count + 1);
            return 0;
        }
    }
    bt_ga_free(text);
    BT_TRACE_EXIT("ga.load_wordlist");
    return 1;
}

static int bt_ga_freqs_push(bt_ga_freqs *freqs, const char *key, uint64_t value) {
    for (size_t i = 0; i < freqs->count; i++) {
        if (strcmp(freqs->keys[i], key) == 0) {
            freqs->values[i] += value;
            return 1;
        }
    }
    char **grown_keys = bt_ga_alloc((freqs->count + 1) * sizeof *grown_keys, "ga.freqs.keys");
    uint64_t *grown_values =
        bt_ga_alloc((freqs->count + 1) * sizeof *grown_values, "ga.freqs.values");
    size_t bytes = strlen(key) + 1;
    char *copy = bt_ga_alloc(bytes, "ga.freqs.entry");
    if (grown_keys == NULL || grown_values == NULL || copy == NULL) {
        bt_ga_free(grown_keys);
        bt_ga_free(grown_values);
        bt_ga_free(copy);
        return 0;
    }
    if (freqs->count > 0) {
        memcpy(grown_keys, freqs->keys, freqs->count * sizeof *grown_keys);
        memcpy(grown_values, freqs->values, freqs->count * sizeof *grown_values);
    }
    memcpy(copy, key, bytes);
    bt_ga_free(freqs->keys);
    bt_ga_free(freqs->values);
    freqs->keys = grown_keys;
    freqs->values = grown_values;
    freqs->keys[freqs->count] = copy;
    freqs->values[freqs->count] = value;
    freqs->count++;
    return 1;
}

void bt_ga_freqs_free(bt_ga_freqs *freqs) {
    BT_TRACE_ENTER("ga.freqs_free");
    for (size_t i = 0; i < freqs->count; i++) {
        bt_ga_free(freqs->keys[i]);
    }
    bt_ga_free(freqs->keys);
    bt_ga_free(freqs->values);
    freqs->keys = NULL;
    freqs->values = NULL;
    freqs->count = 0;
    BT_TRACE_EXIT("ga.freqs_free");
}

uint64_t bt_ga_freqs_get(const bt_ga_freqs *freqs, const char *key) {
    BT_TRACE_ENTER("ga.freqs_get");
    for (size_t i = 0; i < freqs->count; i++) {
        if (strcmp(freqs->keys[i], key) == 0) {
            BT_TRACE_EXIT("ga.freqs_get");
            return freqs->values[i];
        }
    }
    BT_TRACE_EXIT("ga.freqs_get");
    return 0;
}

static int bt_ga_freqs_has(const bt_ga_freqs *freqs, const char *key) {
    for (size_t i = 0; i < freqs->count; i++) {
        if (strcmp(freqs->keys[i], key) == 0) {
            return 1;
        }
    }
    return 0;
}

static int bt_ga_parse_u64(const char *text, uint64_t *out) {
    const char *digits = text;
    if (digits[0] == '+') {
        digits++;
    }
    if (digits[0] == '\0') {
        return 0;
    }
    uint64_t value = 0;
    for (const char *p = digits; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            return 0;
        }
        uint64_t digit = (uint64_t)(*p - '0');
        if (value > (UINT64_MAX - digit) / 10) {
            return 0;
        }
        value = value * 10 + digit;
    }
    *out = value;
    return 1;
}

int bt_ga_load_syllable_frequencies(const char *path, bt_ga_freqs *out, char err[BT_GA_ERR_MAX]) {
    BT_TRACE_ENTER("ga.load_syllable_frequencies");
    out->keys = NULL;
    out->values = NULL;
    out->count = 0;
    size_t len = 0;
    char *text = bt_ga_read_file(path, &len, err);
    if (text == NULL) {
        BT_TRACE_FAIL("ga.load_syllable_frequencies", "read failed for %s", path);
        return 0;
    }
    if (!bt_ga_utf8_valid(text)) {
        bt_ga_free(text);
        snprintf(err, BT_GA_ERR_MAX, "utf8 %s", path);
        BT_TRACE_FAIL("ga.load_syllable_frequencies", "utf8 failed for %s", path);
        return 0;
    }
    size_t line_number = 0;
    char *cursor = text;
    while (*cursor != '\0') {
        char *line = cursor;
        char *end = strchr(cursor, '\n');
        if (end != NULL) {
            *end = '\0';
            cursor = end + 1;
        } else {
            cursor = line + strlen(line);
        }
        line_number++;
        size_t raw = strlen(line);
        if (raw > 0 && line[raw - 1] == '\r') {
            line[raw - 1] = '\0';
        }
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        char *columns[3] = {NULL, NULL, NULL};
        size_t column_count = 0;
        char *field = line;
        for (char *p = line;; p++) {
            if (*p == '\t' || *p == '\0') {
                int last = *p == '\0';
                *p = '\0';
                if (column_count < 3) {
                    columns[column_count] = field;
                }
                column_count++;
                field = p + 1;
                if (last) {
                    break;
                }
            }
        }
        char *count_text = columns[1] != NULL ? columns[1] : "";
        char *count_trimmed = bt_ga_trim(count_text);
        uint64_t parsed_count = 0;
        if (!bt_ga_parse_u64(count_trimmed, &parsed_count)) {
            snprintf(err, BT_GA_ERR_MAX, "freq row %zu has non numeric count \"%s\"", line_number,
                     count_text);
            bt_ga_free(text);
            bt_ga_freqs_free(out);
            BT_TRACE_FAIL("ga.load_syllable_frequencies", "non numeric count row %zu", line_number);
            return 0;
        }
        char *word = columns[2] != NULL ? columns[2] : "";
        char *saved = NULL;
        for (char *token = strtok_s(word, " \t\r\n\v\f", &saved); token != NULL;
             token = strtok_s(NULL, " \t\r\n\v\f", &saved)) {
            if (!bt_ga_is_nfc(token)) {
                snprintf(err, BT_GA_ERR_MAX, "combining mark in freq token \"%s\"", token);
                bt_ga_free(text);
                bt_ga_freqs_free(out);
                BT_TRACE_FAIL("ga.load_syllable_frequencies", "combining mark in %s", token);
                return 0;
            }
            if (!bt_ga_freqs_push(out, token, parsed_count)) {
                snprintf(err, BT_GA_ERR_MAX, "frequency table allocation failed at row %zu",
                         line_number);
                bt_ga_free(text);
                bt_ga_freqs_free(out);
                BT_TRACE_FAIL("ga.load_syllable_frequencies", "allocation failed at row %zu",
                              line_number);
                return 0;
            }
        }
    }
    bt_ga_free(text);
    BT_TRACE_EXIT("ga.load_syllable_frequencies");
    return 1;
}

static const bt_ga_onset BT_GA_ONSETS[21] = {
    BT_GA_ONSET_B, BT_GA_ONSET_DD, BT_GA_ONSET_T,    BT_GA_ONSET_TH, BT_GA_ONSET_CH, BT_GA_ONSET_TR,
    BT_GA_ONSET_K, BT_GA_ONSET_M,  BT_GA_ONSET_N,    BT_GA_ONSET_NH, BT_GA_ONSET_NG, BT_GA_ONSET_PH,
    BT_GA_ONSET_X, BT_GA_ONSET_S,  BT_GA_ONSET_KH,   BT_GA_ONSET_H,  BT_GA_ONSET_V,  BT_GA_ONSET_Z,
    BT_GA_ONSET_G, BT_GA_ONSET_L,  BT_GA_ONSET_ZERO,
};

static const bt_ga_nucleus BT_GA_NUCLEI[14] = {
    BT_GA_NUC_I,  BT_GA_NUC_UW, BT_GA_NUC_U,  BT_GA_NUC_EH, BT_GA_NUC_OR,
    BT_GA_NUC_OH, BT_GA_NUC_E,  BT_GA_NUC_AH, BT_GA_NUC_A,  BT_GA_NUC_AB,
    BT_GA_NUC_O,  BT_GA_NUC_IA, BT_GA_NUC_WA, BT_GA_NUC_UA,
};

static const bt_ga_coda BT_GA_CODAS[7] = {
    BT_GA_CODA_OPEN, BT_GA_CODA_P, BT_GA_CODA_T,  BT_GA_CODA_K,
    BT_GA_CODA_M,    BT_GA_CODA_N, BT_GA_CODA_NG,
};

static const bt_ga_tone BT_GA_TONES[6] = {
    BT_GA_TONE_NGANG, BT_GA_TONE_HUYEN, BT_GA_TONE_SAC,
    BT_GA_TONE_HOI,   BT_GA_TONE_NGA,   BT_GA_TONE_NANG,
};

static int bt_ga_is_front(bt_ga_nucleus nucleus) {
    return nucleus == BT_GA_NUC_I || nucleus == BT_GA_NUC_E || nucleus == BT_GA_NUC_EH ||
           nucleus == BT_GA_NUC_IA;
}

static int bt_ga_takes_velar_coda(bt_ga_nucleus nucleus) {
    return nucleus == BT_GA_NUC_I || nucleus == BT_GA_NUC_E || nucleus == BT_GA_NUC_EH;
}

static int bt_ga_takes_k_n_coda(bt_ga_nucleus nucleus) {
    return nucleus != BT_GA_NUC_OR;
}

static int bt_ga_takes_m_n_coda(bt_ga_nucleus nucleus) {
    return nucleus != BT_GA_NUC_UW;
}

static int bt_ga_coda_is_stop(bt_ga_coda coda) {
    return coda == BT_GA_CODA_P || coda == BT_GA_CODA_T || coda == BT_GA_CODA_K;
}

static const char *bt_ga_coda_letters(bt_ga_coda coda, bt_ga_nucleus nucleus) {
    switch (coda) {
    case BT_GA_CODA_OPEN:
        return "";
    case BT_GA_CODA_P:
        return "p";
    case BT_GA_CODA_T:
        return "t";
    case BT_GA_CODA_M:
        return "m";
    case BT_GA_CODA_N:
        return "n";
    case BT_GA_CODA_K:
        return bt_ga_takes_velar_coda(nucleus) ? "ch" : "c";
    case BT_GA_CODA_NG:
        return bt_ga_takes_velar_coda(nucleus) ? "nh" : "ng";
    }
    return "";
}

int bt_ga_nucleus_rime(bt_ga_nucleus nucleus, bt_ga_coda coda, const char **rime, size_t *mark_at) {
    BT_TRACE_ENTER("ga.nucleus_rime");
    if (coda == BT_GA_CODA_OPEN) {
        const char *open_rime = NULL;
        switch (nucleus) {
        case BT_GA_NUC_I:
            open_rime = "i";
            break;
        case BT_GA_NUC_UW:
            open_rime = "\xc6\xb0";
            break;
        case BT_GA_NUC_U:
            open_rime = "u";
            break;
        case BT_GA_NUC_EH:
            open_rime = "\xc3\xaa";
            break;
        case BT_GA_NUC_OR:
            open_rime = "\xc6\xa1";
            break;
        case BT_GA_NUC_OH:
            open_rime = "\xc3\xb4";
            break;
        case BT_GA_NUC_E:
            open_rime = "e";
            break;
        case BT_GA_NUC_A:
            open_rime = "a";
            break;
        case BT_GA_NUC_O:
            open_rime = "o";
            break;
        case BT_GA_NUC_IA:
            open_rime = "ia";
            break;
        case BT_GA_NUC_WA:
            open_rime = "\xc6\xb0"
                        "a";
            break;
        case BT_GA_NUC_UA:
            open_rime = "ua";
            break;
        default:
            BT_TRACE_EXIT("ga.nucleus_rime");
            return 0;
        }
        *rime = open_rime;
        *mark_at = 0;
        BT_TRACE_EXIT("ga.nucleus_rime");
        return 1;
    }
    if (!bt_ga_takes_m_n_coda(nucleus) && (coda == BT_GA_CODA_M || coda == BT_GA_CODA_N)) {
        BT_TRACE_EXIT("ga.nucleus_rime");
        return 0;
    }
    if (!bt_ga_takes_k_n_coda(nucleus) && (coda == BT_GA_CODA_NG || coda == BT_GA_CODA_K)) {
        BT_TRACE_EXIT("ga.nucleus_rime");
        return 0;
    }
    if (nucleus == BT_GA_NUC_UA && coda == BT_GA_CODA_P) {
        BT_TRACE_EXIT("ga.nucleus_rime");
        return 0;
    }
    const char *body = NULL;
    switch (nucleus) {
    case BT_GA_NUC_I:
        body = "i";
        break;
    case BT_GA_NUC_UW:
        body = "\xc6\xb0";
        break;
    case BT_GA_NUC_U:
        body = "u";
        break;
    case BT_GA_NUC_EH:
        body = "\xc3\xaa";
        break;
    case BT_GA_NUC_OR:
        body = "\xc6\xa1";
        break;
    case BT_GA_NUC_OH:
        body = "\xc3\xb4";
        break;
    case BT_GA_NUC_E:
        body = (coda == BT_GA_CODA_NG || coda == BT_GA_CODA_K) ? "a" : "e";
        break;
    case BT_GA_NUC_AH:
        body = "\xc3\xa2";
        break;
    case BT_GA_NUC_A:
        body = "a";
        break;
    case BT_GA_NUC_AB:
        body = "\xc4\x83";
        break;
    case BT_GA_NUC_O:
        body = "o";
        break;
    case BT_GA_NUC_IA:
        body = "i\xc3\xaa";
        break;
    case BT_GA_NUC_WA:
        body = "\xc6\xb0\xc6\xa1";
        break;
    case BT_GA_NUC_UA:
        body = "u\xc3\xb4";
        break;
    }
    *rime = body;
    *mark_at =
        (nucleus == BT_GA_NUC_IA || nucleus == BT_GA_NUC_WA || nucleus == BT_GA_NUC_UA) ? 1 : 0;
    BT_TRACE_EXIT("ga.nucleus_rime");
    return 1;
}

const char *bt_ga_tone_char(const char *base, bt_ga_tone tone) {
    BT_TRACE_ENTER("ga.tone_char");
    size_t count = 0;
    const bt_ga_tone_marked_row *rows = bt_ga_tone_marked_table(&count);
    for (size_t i = 0; i < count; i++) {
        if (strcmp(rows[i].base, base) == 0 && rows[i].tone == tone) {
            BT_TRACE_EXIT("ga.tone_char");
            return rows[i].marked;
        }
    }
    BT_TRACE_EXIT("ga.tone_char");
    return base;
}

void bt_ga_onset_spellings(bt_ga_onset onset, bt_ga_nucleus nucleus,
                           bt_ga_onset_spelling_list *out) {
    BT_TRACE_ENTER("ga.onset_spellings");
    int front = bt_ga_is_front(nucleus);
    out->count = 0;
    switch (onset) {
    case BT_GA_ONSET_B:
        out->items[out->count++] = "b";
        break;
    case BT_GA_ONSET_DD:
        out->items[out->count++] = "\xc4\x91";
        break;
    case BT_GA_ONSET_T:
        out->items[out->count++] = "t";
        break;
    case BT_GA_ONSET_TH:
        out->items[out->count++] = "th";
        break;
    case BT_GA_ONSET_CH:
        out->items[out->count++] = "ch";
        break;
    case BT_GA_ONSET_TR:
        out->items[out->count++] = "tr";
        break;
    case BT_GA_ONSET_K:
        if (front) {
            out->items[out->count++] = "k";
        } else {
            out->items[out->count++] = "c";
            out->items[out->count++] = "k";
        }
        break;
    case BT_GA_ONSET_M:
        out->items[out->count++] = "m";
        break;
    case BT_GA_ONSET_N:
        out->items[out->count++] = "n";
        break;
    case BT_GA_ONSET_NH:
        out->items[out->count++] = "nh";
        break;
    case BT_GA_ONSET_NG:
        out->items[out->count++] = front ? "ngh" : "ng";
        break;
    case BT_GA_ONSET_PH:
        out->items[out->count++] = "ph";
        break;
    case BT_GA_ONSET_X:
        out->items[out->count++] = "x";
        break;
    case BT_GA_ONSET_S:
        out->items[out->count++] = "s";
        break;
    case BT_GA_ONSET_KH:
        out->items[out->count++] = "kh";
        break;
    case BT_GA_ONSET_H:
        out->items[out->count++] = "h";
        break;
    case BT_GA_ONSET_V:
        out->items[out->count++] = "v";
        break;
    case BT_GA_ONSET_Z:
        out->items[out->count++] = "d";
        out->items[out->count++] = "r";
        if (nucleus == BT_GA_NUC_A || nucleus == BT_GA_NUC_AB || nucleus == BT_GA_NUC_AH ||
            nucleus == BT_GA_NUC_I || nucleus == BT_GA_NUC_IA || nucleus == BT_GA_NUC_WA) {
            out->items[out->count++] = "gi";
        }
        break;
    case BT_GA_ONSET_G:
        out->items[out->count++] = front ? "gh" : "g";
        break;
    case BT_GA_ONSET_L:
        out->items[out->count++] = "l";
        break;
    case BT_GA_ONSET_ZERO:
        out->items[out->count++] = "";
        break;
    }
    BT_TRACE_EXIT("ga.onset_spellings");
}

int bt_ga_render(const char *spelling, bt_ga_nucleus nucleus, bt_ga_coda coda, bt_ga_tone tone,
                 char out[BT_GA_TEXT_MAX]) {
    BT_TRACE_ENTER("ga.render");
    const char *rime = NULL;
    size_t mark_at = 0;
    if (!bt_ga_nucleus_rime(nucleus, coda, &rime, &mark_at)) {
        BT_TRACE_EXIT("ga.render");
        return 0;
    }
    const char *coda_text = bt_ga_coda_letters(coda, nucleus);
    const char *prefix = spelling;
    if (strcmp(spelling, "gi") == 0 && rime[0] == 'i') {
        prefix = "g";
    }
    size_t len = 0;
    const char *p = prefix;
    while (*p != '\0') {
        out[len++] = *p++;
    }
    size_t char_index = 0;
    for (const unsigned char *r = (const unsigned char *)rime; *r != '\0';) {
        uint32_t cp = 0;
        size_t width = 1;
        if (*r < 0x80) {
            cp = *r;
        } else {
            if ((*r & 0xE0U) == 0xC0U) {
                cp = ((uint32_t)(*r & 0x1FU) << 6U) | (uint32_t)(r[1] & 0x3FU);
                width = 2;
            } else {
                cp = ((uint32_t)(*r & 0x0FU) << 12U) | (((uint32_t)(r[1] & 0x3FU)) << 6U) |
                     (uint32_t)(r[2] & 0x3FU);
                width = 3;
            }
        }
        char base[5];
        if (width == 1) {
            base[0] = (char)cp;
            base[1] = '\0';
        } else {
            for (size_t k = 0; k < width; k++) {
                base[k] = (char)r[k];
            }
            base[width] = '\0';
        }
        const char *piece = (char_index == mark_at) ? bt_ga_tone_char(base, tone) : base;
        const char *q = piece;
        while (*q != '\0') {
            out[len++] = *q++;
        }
        r += width;
        char_index++;
    }
    const char *c = coda_text;
    while (*c != '\0') {
        out[len++] = *c++;
    }
    out[len] = '\0';
    BT_TRACE_EXIT("ga.render");
    return 1;
}

void bt_ga_zero_onset_variants(bt_ga_nucleus nucleus, bt_ga_coda coda, bt_ga_tone tone,
                               bt_ga_form_list *out) {
    BT_TRACE_ENTER("ga.zero_onset_variants");
    out->count = 0;
    if (bt_ga_render("", nucleus, coda, tone, out->items[out->count])) {
        out->count++;
    }
    char extra[BT_GA_TEXT_MAX];
    if (nucleus == BT_GA_NUC_I && coda == BT_GA_CODA_OPEN) {
        snprintf(out->items[out->count], BT_GA_TEXT_MAX, "%s", bt_ga_tone_char("y", tone));
        out->count++;
    } else if (nucleus == BT_GA_NUC_IA && coda == BT_GA_CODA_OPEN) {
        snprintf(out->items[out->count], BT_GA_TEXT_MAX, "y%s", bt_ga_tone_char("a", tone));
        out->count++;
    } else if (nucleus == BT_GA_NUC_I && (coda == BT_GA_CODA_M || coda == BT_GA_CODA_P ||
                                          coda == BT_GA_CODA_K || coda == BT_GA_CODA_NG)) {
        snprintf(extra, BT_GA_TEXT_MAX, "%s%s", bt_ga_tone_char("y", tone),
                 bt_ga_coda_letters(coda, nucleus));
        snprintf(out->items[out->count], BT_GA_TEXT_MAX, "%s", extra);
        out->count++;
    }
    BT_TRACE_EXIT("ga.zero_onset_variants");
}

static int bt_ga_sorted_contains(const char *const *sorted, size_t count, const char *text) {
    size_t low = 0;
    size_t high = count;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        int cmp = strcmp(sorted[mid], text);
        if (cmp == 0) {
            return 1;
        }
        if (cmp < 0) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    return 0;
}

static int bt_ga_ptr_cmp(const void *a, const void *b) {
    const char *left = *(const char *const *)a;
    const char *right = *(const char *const *)b;
    return strcmp(left, right);
}

static int bt_ga_entry_cmp(const void *a, const void *b) {
    const bt_ga_inventory_entry *left = a;
    const bt_ga_inventory_entry *right = b;
    int cmp = strcmp(left->text, right->text);
    if (cmp != 0) {
        return cmp;
    }
    if (left->onset != right->onset) {
        return left->onset < right->onset ? -1 : 1;
    }
    if (left->nucleus != right->nucleus) {
        return left->nucleus < right->nucleus ? -1 : 1;
    }
    if (left->coda != right->coda) {
        return left->coda < right->coda ? -1 : 1;
    }
    if (left->tone != right->tone) {
        return left->tone < right->tone ? -1 : 1;
    }
    return 0;
}

static int bt_ga_cover_word(char ***covered, size_t *covered_size, size_t *covered_capacity,
                            const char *text, char err[BT_GA_ERR_MAX]) {
    for (size_t c = 0; c < *covered_size; c++) {
        if (strcmp((*covered)[c], text) == 0) {
            return 1;
        }
    }
    if (*covered_size == *covered_capacity) {
        *covered_capacity *= 2;
        char **grown = bt_ga_alloc(*covered_capacity * sizeof *grown, "ga.build_inventory.covered");
        if (grown == NULL) {
            snprintf(err, BT_GA_ERR_MAX, "inventory covered allocation failed");
            BT_TRACE_FAIL("ga.build_inventory", "covered allocation failed");
            return 0;
        }
        if (*covered_size > 0) {
            memcpy(grown, *covered, *covered_size * sizeof *grown);
        }
        bt_ga_free(*covered);
        *covered = grown;
    }
    size_t bytes = strlen(text) + 1;
    char *copy = bt_ga_alloc(bytes, "ga.build_inventory.word_copy");
    if (copy == NULL) {
        snprintf(err, BT_GA_ERR_MAX, "inventory word copy allocation failed");
        BT_TRACE_FAIL("ga.build_inventory", "word copy allocation failed");
        return 0;
    }
    memcpy(copy, text, bytes);
    (*covered)[(*covered_size)++] = copy;
    return 1;
}

static int bt_ga_grow_list(bt_ga_inventory_entry **list, size_t *capacity, size_t size,
                           char err[BT_GA_ERR_MAX]) {
    if (size < *capacity) {
        return 1;
    }
    *capacity *= 2;
    bt_ga_inventory_entry *grown =
        bt_ga_alloc(*capacity * sizeof *grown, "ga.build_inventory.list");
    if (grown == NULL) {
        snprintf(err, BT_GA_ERR_MAX, "inventory list allocation failed");
        BT_TRACE_FAIL("ga.build_inventory", "list allocation failed");
        return 0;
    }
    if (size > 0) {
        memcpy(grown, *list, size * sizeof *grown);
    }
    bt_ga_free(*list);
    *list = grown;
    return 1;
}

int bt_ga_build_inventory(const bt_ga_strset *wordlist, bt_ga_inventory_entry **entries,
                          size_t *count, bt_ga_inventory_report *report, char err[BT_GA_ERR_MAX]) {
    BT_TRACE_ENTER("ga.build_inventory");
    int ok = 1;
    size_t covered_capacity = 64;
    size_t covered_size = 0;
    char **covered = NULL;
    bt_ga_inventory_entry *list = NULL;
    const char **sorted =
        bt_ga_alloc(wordlist->count * sizeof *sorted, "ga.build_inventory.sorted");
    if (sorted == NULL) {
        snprintf(err, BT_GA_ERR_MAX, "inventory sorted allocation failed");
        BT_TRACE_FAIL("ga.build_inventory", "sorted allocation failed");
        ok = 0;
        goto fail;
    }
    for (size_t i = 0; i < wordlist->count; i++) {
        sorted[i] = wordlist->items[i];
    }
    qsort(sorted, wordlist->count, sizeof *sorted, bt_ga_ptr_cmp);

    size_t capacity = 64;
    size_t size = 0;
    list = bt_ga_alloc(capacity * sizeof *list, "ga.build_inventory.list");
    if (list == NULL) {
        snprintf(err, BT_GA_ERR_MAX, "inventory list allocation failed");
        BT_TRACE_FAIL("ga.build_inventory", "list allocation failed");
        ok = 0;
        goto fail;
    }
    size_t generated = 0;
    covered = bt_ga_alloc(covered_capacity * sizeof *covered, "ga.build_inventory.covered");
    if (covered == NULL) {
        snprintf(err, BT_GA_ERR_MAX, "inventory covered allocation failed");
        BT_TRACE_FAIL("ga.build_inventory", "covered allocation failed");
        ok = 0;
        goto fail;
    }

    for (size_t oi = 0; oi < sizeof BT_GA_ONSETS / sizeof BT_GA_ONSETS[0]; oi++) {
        bt_ga_onset onset = BT_GA_ONSETS[oi];
        for (size_t ni = 0; ni < sizeof BT_GA_NUCLEI / sizeof BT_GA_NUCLEI[0]; ni++) {
            bt_ga_nucleus nucleus = BT_GA_NUCLEI[ni];
            for (size_t ci = 0; ci < sizeof BT_GA_CODAS / sizeof BT_GA_CODAS[0]; ci++) {
                bt_ga_coda coda = BT_GA_CODAS[ci];
                for (size_t ti = 0; ti < sizeof BT_GA_TONES / sizeof BT_GA_TONES[0]; ti++) {
                    bt_ga_tone tone = BT_GA_TONES[ti];
                    if (bt_ga_coda_is_stop(coda) && tone != BT_GA_TONE_SAC &&
                        tone != BT_GA_TONE_NANG) {
                        continue;
                    }
                    if (onset == BT_GA_ONSET_ZERO) {
                        bt_ga_form_list forms;
                        bt_ga_zero_onset_variants(nucleus, coda, tone, &forms);
                        for (size_t f = 0; f < forms.count; f++) {
                            generated++;
                            if (!bt_ga_sorted_contains(sorted, wordlist->count, forms.items[f])) {
                                continue;
                            }
                            if (!bt_ga_cover_word(&covered, &covered_size, &covered_capacity,
                                                  forms.items[f], err)) {
                                ok = 0;
                                goto fail;
                            }
                            if (!bt_ga_grow_list(&list, &capacity, size, err)) {
                                ok = 0;
                                goto fail;
                            }
                            strcpy(list[size].text, forms.items[f]);
                            list[size].onset = onset;
                            list[size].nucleus = nucleus;
                            list[size].coda = coda;
                            list[size].tone = tone;
                            size++;
                        }
                    } else {
                        bt_ga_onset_spelling_list spellings;
                        bt_ga_onset_spellings(onset, nucleus, &spellings);
                        for (size_t s = 0; s < spellings.count; s++) {
                            char text[BT_GA_TEXT_MAX];
                            if (!bt_ga_render(spellings.items[s], nucleus, coda, tone, text)) {
                                continue;
                            }
                            generated++;
                            if (!bt_ga_sorted_contains(sorted, wordlist->count, text)) {
                                continue;
                            }
                            if (!bt_ga_cover_word(&covered, &covered_size, &covered_capacity, text,
                                                  err)) {
                                ok = 0;
                                goto fail;
                            }
                            if (!bt_ga_grow_list(&list, &capacity, size, err)) {
                                ok = 0;
                                goto fail;
                            }
                            strcpy(list[size].text, text);
                            list[size].onset = onset;
                            list[size].nucleus = nucleus;
                            list[size].coda = coda;
                            list[size].tone = tone;
                            size++;
                        }
                    }
                }
            }
        }
    }

    qsort(list, size, sizeof *list, bt_ga_entry_cmp);
    size_t kept = 0;
    for (size_t i = 0; i < size; i++) {
        if (kept > 0 && strcmp(list[i].text, list[kept - 1].text) == 0) {
            continue;
        }
        list[kept++] = list[i];
    }

    report->generated = generated;
    report->kept = kept;
    report->dropped_not_in_wordlist = generated - kept;
    report->wordlist_size = wordlist->count;
    report->wordlist_covered = covered_size;
    *entries = list;
    *count = kept;

fail:
    for (size_t c = 0; c < covered_size; c++) {
        bt_ga_free(covered[c]);
    }
    bt_ga_free(covered);
    bt_ga_free(sorted);
    if (!ok) {
        bt_ga_free(list);
        *entries = NULL;
        *count = 0;
        memset(report, 0, sizeof *report);
        BT_TRACE_FAIL("ga.build_inventory", "allocation failed: %s", err);
        return 0;
    }
    BT_TRACE_EXIT("ga.build_inventory");
    return 1;
}

void bt_ga_inventory_free(bt_ga_inventory_entry *entries) {
    BT_TRACE_ENTER("ga.inventory_free");
    bt_ga_free(entries);
    BT_TRACE_EXIT("ga.inventory_free");
}

static int bt_ga_pool_cmp(const void *a, const void *b) {
    const bt_ga_pool_entry *left = a;
    const bt_ga_pool_entry *right = b;
    if (left->frequency != right->frequency) {
        return left->frequency > right->frequency ? -1 : 1;
    }
    return strcmp(left->text, right->text);
}

void bt_ga_build_pool(const bt_ga_inventory_entry *entries, size_t count, const bt_ga_freqs *freqs,
                      size_t top_k, bt_ga_pool_entry **pool, size_t *pool_count) {
    BT_TRACE_ENTER("ga.build_pool");
    size_t size = 0;
    bt_ga_pool_entry *ranked =
        bt_ga_alloc((count > 0 ? count : 1) * sizeof *ranked, "ga.build_pool.ranked");
    if (ranked == NULL) {
        bt_ga_inventory_abort("ga build_pool", "allocation failed at ga.build_pool.ranked");
    }
    for (size_t i = 0; i < count; i++) {
        if (!bt_ga_freqs_has(freqs, entries[i].text)) {
            continue;
        }
        uint64_t frequency = bt_ga_freqs_get(freqs, entries[i].text);
        strcpy(ranked[size].text, entries[i].text);
        ranked[size].frequency = frequency;
        size++;
    }
    qsort(ranked, size, sizeof *ranked, bt_ga_pool_cmp);
    if (size > top_k) {
        size = top_k;
    }
    *pool = ranked;
    *pool_count = size;
    BT_TRACE_EXIT("ga.build_pool");
}

void bt_ga_pool_free(bt_ga_pool_entry *pool) {
    BT_TRACE_ENTER("ga.pool_free");
    bt_ga_free(pool);
    BT_TRACE_EXIT("ga.pool_free");
}

int bt_ga_config_pool_top_k(const bt_json_value *config, size_t *out, char err[BT_GA_ERR_MAX]) {
    BT_TRACE_ENTER("ga.config_pool_top_k");
    const bt_json_value *value = bt_config_value(config, "ga.pool_top_k");
    if (value == NULL || !bt_json_is_integer(value) || value->integer < 0) {
        snprintf(err, BT_GA_ERR_MAX, "config section missing: ga.pool_top_k");
        BT_TRACE_FAIL("ga.config_pool_top_k", "ga.pool_top_k missing");
        return 0;
    }
    *out = (size_t)value->integer;
    BT_TRACE_EXIT("ga.config_pool_top_k");
    return 1;
}
