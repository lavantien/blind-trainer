#include "policy/constants.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "policy/lib.h"

static int cscan_is_word(unsigned char byte) {
    return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
           (byte >= '0' && byte <= '9') || byte == '_';
}

static int is_digit(unsigned char byte) {
    return byte >= '0' && byte <= '9';
}

static int cscan_is_hex_digit(unsigned char byte) {
    return (byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'f') ||
           (byte >= 'A' && byte <= 'F');
}

static int is_digit_or_sep(unsigned char byte, policy_lang lang) {
    if (is_digit(byte)) {
        return 1;
    }
    return lang == POLICY_LANG_C ? byte == '\'' : byte == '_';
}

static int is_sep(unsigned char byte, policy_lang lang) {
    return lang == POLICY_LANG_C ? byte == '\'' : byte == '_';
}

static int radix_digit(unsigned char radix, unsigned char byte) {
    if (radix == 'x' || radix == 'X') {
        return cscan_is_hex_digit(byte);
    }
    if (radix == 'o' || radix == 'O') {
        return byte >= '0' && byte <= '7';
    }
    return byte == '0' || byte == '1';
}

static int radix_prefix(const char *text, size_t len, size_t at, unsigned char *radix_out) {
    if (at + 2 >= len) {
        return 0;
    }
    unsigned char radix = (unsigned char)text[at + 1];
    unsigned char first = (unsigned char)text[at + 2];
    if (text[at] != '0') {
        return 0;
    }
    int known = radix == 'x' || radix == 'X' || radix == 'o' || radix == 'O' || radix == 'b' ||
                radix == 'B';
    if (known && (radix_digit(radix, first) || first == '_')) {
        *radix_out = radix;
        return 1;
    }
    return 0;
}

static int suffix_ok(const char *text, size_t from, size_t to, policy_lang lang) {
    if (to <= from) {
        return 0;
    }
    for (size_t at = from; at < to; at++) {
        unsigned char byte = (unsigned char)text[at];
        int ok = lang == POLICY_LANG_C ? (byte == 'u' || byte == 'U' || byte == 'l' ||
                                          byte == 'L' || byte == 'f' || byte == 'F')
                                       : (byte == 'j' || byte == 'J');
        if (!ok) {
            return 0;
        }
    }
    return 1;
}

int policy_constants_from_config(const bt_json_value *config, policy_policy_config *out, char *err,
                                 size_t err_len) {
    BT_TRACE_ENTER("policy.constants_from_config");
    memset(out, 0, sizeof *out);
    const bt_json_value *section = config == NULL ? NULL : bt_json_get(config, "policy");
    if (section == NULL) {
        snprintf(err, err_len, "config section missing: policy");
        BT_TRACE_FAIL("policy.constants_from_config", "policy section missing");
        BT_TRACE_EXIT("policy.constants_from_config");
        return -1;
    }
    const bt_json_value *raw = bt_json_get(section, "literal_recurrence");
    if (raw == NULL) {
        snprintf(err, err_len, "config section missing: policy.literal_recurrence");
        BT_TRACE_FAIL("policy.constants_from_config", "literal_recurrence missing");
        BT_TRACE_EXIT("policy.constants_from_config");
        return -1;
    }
    if (raw->type != BT_JSON_NUMBER) {
        snprintf(err, err_len, "config value malformed: policy.literal_recurrence");
        BT_TRACE_FAIL("policy.constants_from_config", "literal_recurrence is not a number");
        BT_TRACE_EXIT("policy.constants_from_config");
        return -1;
    }
    unsigned long long recurrence = 0;
    if (!bt_json_is_integer(raw)) {
        snprintf(err, err_len, "config value malformed: policy.literal_recurrence");
        BT_TRACE_FAIL("policy.constants_from_config", "literal_recurrence is not an integer");
        BT_TRACE_EXIT("policy.constants_from_config");
        return -1;
    }
    long long exact = bt_json_integer_or(raw, 0);
    if (exact < 0) {
        snprintf(err, err_len, "config value malformed: policy.literal_recurrence");
        BT_TRACE_FAIL("policy.constants_from_config", "literal_recurrence below zero");
        BT_TRACE_EXIT("policy.constants_from_config");
        return -1;
    }
    if (exact < 1) {
        snprintf(err, err_len, "policy.literal_recurrence must be >= 1");
        BT_TRACE_FAIL("policy.constants_from_config", "literal_recurrence below 1");
        BT_TRACE_EXIT("policy.constants_from_config");
        return -1;
    }
    recurrence = (unsigned long long)exact;
    const bt_json_value *list = bt_json_get(section, "small_literal_allow");
    if (list == NULL) {
        snprintf(err, err_len, "config section missing: policy.small_literal_allow");
        BT_TRACE_FAIL("policy.constants_from_config", "small_literal_allow missing");
        BT_TRACE_EXIT("policy.constants_from_config");
        return -1;
    }
    if (list->type != BT_JSON_ARRAY) {
        snprintf(err, err_len, "config value malformed: policy.small_literal_allow");
        BT_TRACE_FAIL("policy.constants_from_config", "small_literal_allow is not an array");
        BT_TRACE_EXIT("policy.constants_from_config");
        return -1;
    }
    out->small_literal_allow =
        policy_xalloc(list->as.array.count * sizeof *out->small_literal_allow);
    for (size_t at = 0; at < list->as.array.count; at++) {
        const bt_json_value *entry = bt_json_at(list, at);
        if (entry == NULL || entry->type != BT_JSON_STRING) {
            policy_policy_config_free(out);
            snprintf(err, err_len, "config value malformed: policy.small_literal_allow");
            BT_TRACE_FAIL("policy.constants_from_config", "small_literal_allow entry not a "
                                                          "string");
            BT_TRACE_EXIT("policy.constants_from_config");
            return -1;
        }
        out->small_literal_allow[out->allow_count] = policy_xstrdup(entry->as.string);
        out->allow_count++;
    }
    out->literal_recurrence = recurrence;
    BT_TRACE_EXIT("policy.constants_from_config");
    return 0;
}

void policy_policy_config_free(policy_policy_config *config) {
    for (size_t at = 0; at < config->allow_count; at++) {
        free(config->small_literal_allow[at]);
    }
    free(config->small_literal_allow);
    config->small_literal_allow = NULL;
    config->allow_count = 0;
}

static int core_integer_value(const char *core, policy_lang lang, unsigned long long *out) {
    int radix = 10;
    size_t at = 0;
    if (core[0] == '\0') {
        return 0;
    }
    if (core[0] == '0' && core[1] != '\0') {
        if (core[1] == 'x' || core[1] == 'X') {
            radix = 16;
            at = 2;
        } else if (core[1] == 'b' || core[1] == 'B') {
            radix = 2;
            at = 2;
        } else if (core[1] == 'o' || core[1] == 'O') {
            radix = 8;
            at = 2;
        } else if (lang == POLICY_LANG_C) {
            radix = 8;
            at = 1;
        } else {
            at = 1;
        }
    }
    unsigned long long value = 0;
    int digits_seen = 0;
    for (; core[at] != '\0'; at++) {
        unsigned char byte = (unsigned char)core[at];
        int digit;
        if (byte >= '0' && byte <= '9') {
            digit = byte - '0';
        } else if (byte >= 'a' && byte <= 'f') {
            digit = byte - 'a' + 10;
        } else if (byte >= 'A' && byte <= 'F') {
            digit = byte - 'A' + 10;
        } else {
            return 0;
        }
        if (digit >= radix) {
            return 0;
        }
        if (value > (ULLONG_MAX - (unsigned long long)digit) / (unsigned long long)radix) {
            return 0;
        }
        value = value * (unsigned long long)radix + (unsigned long long)digit;
        digits_seen++;
    }
    if (digits_seen == 0) {
        return 0;
    }
    *out = value;
    return 1;
}

policy_occurrence *policy_scan_literals(const char *masked_text, size_t len, policy_lang lang,
                                        size_t *count) {
    BT_TRACE_ENTER("policy.scan_literals");
    const char *text = masked_text;
    policy_occurrence *hits = NULL;
    size_t hit_count = 0;
    size_t hit_cap = 0;
    size_t i = 0;
    while (i < len) {
        if (!is_digit((unsigned char)text[i])) {
            i++;
            continue;
        }
        int leading_dot = 0;
        int excluded = 0;
        if (i > 0) {
            unsigned char prev = (unsigned char)text[i - 1];
            unsigned char before = i >= 2 ? (unsigned char)text[i - 2] : 0;
            int prev_dots_range = prev == '.' && before == '.';
            int before_is_name = cscan_is_word(before) || (before & 0xC0U) == 0x80U;
            if (prev_dots_range) {
                excluded = 0;
            } else if (prev == '.') {
                if (before_is_name) {
                    excluded = 1;
                } else {
                    leading_dot = 1;
                }
            } else if (cscan_is_word(prev) || prev == '$' || (prev & 0xC0U) == 0x80U) {
                excluded = 1;
            }
        }
        if (excluded) {
            i++;
            continue;
        }
        size_t start = leading_dot ? i - 1 : i;
        size_t j = i;
        unsigned char radix = 0;
        if (radix_prefix(text, len, j, &radix)) {
            j += 2;
            while (j < len && (radix_digit(radix, (unsigned char)text[j]) ||
                               is_sep((unsigned char)text[j], lang))) {
                j++;
            }
            if (radix == 'x' || radix == 'X') {
                if (j < len && text[j] == '.' && !(j + 1 < len && text[j + 1] == '.')) {
                    j++;
                    while (j < len && (cscan_is_hex_digit((unsigned char)text[j]) ||
                                       is_sep((unsigned char)text[j], lang))) {
                        j++;
                    }
                }
                if (j < len && (text[j] == 'p' || text[j] == 'P')) {
                    size_t k = j + 1;
                    if (k < len && (text[k] == '+' || text[k] == '-')) {
                        k++;
                    }
                    if (k < len && is_digit_or_sep((unsigned char)text[k], lang)) {
                        j = k;
                        while (j < len && is_digit_or_sep((unsigned char)text[j], lang)) {
                            j++;
                        }
                    }
                }
            }
        } else {
            while (j < len && is_digit_or_sep((unsigned char)text[j], lang)) {
                j++;
            }
            if (j < len && text[j] == '.' && !(j + 1 < len && text[j + 1] == '.')) {
                j++;
                while (j < len && is_digit_or_sep((unsigned char)text[j], lang)) {
                    j++;
                }
            }
            if (j < len && (text[j] == 'e' || text[j] == 'E')) {
                size_t k = j + 1;
                if (k < len && (text[k] == '+' || text[k] == '-')) {
                    k++;
                }
                if (k < len && is_digit_or_sep((unsigned char)text[k], lang)) {
                    j = k;
                    while (j < len && is_digit_or_sep((unsigned char)text[j], lang)) {
                        j++;
                    }
                }
            }
        }
        size_t s = j;
        while (s < len && cscan_is_word((unsigned char)text[s])) {
            s++;
        }
        size_t end = (s > j && suffix_ok(text, j, s, lang)) ? s : j;
        int boundary_ok = end >= len;
        if (!boundary_ok) {
            unsigned char after = (unsigned char)text[end];
            if (cscan_is_word(after) || after == '.') {
                boundary_ok = after == '.' && end + 1 < len && text[end + 1] == '.';
            } else {
                boundary_ok = 1;
            }
        }
        if (boundary_ok) {
            char *core = policy_xalloc(j - start + 1);
            size_t used = 0;
            for (size_t at = start; at < j; at++) {
                if (text[at] != '_' && text[at] != '\'') {
                    core[used++] = text[at];
                }
            }
            core[used] = '\0';
            unsigned long long value = 0;
            if (core_integer_value(core, lang, &value)) {
                char canon[32];
                int wrote = snprintf(canon, sizeof canon, "%llu", value);
                if (wrote > 0 && (size_t)wrote < sizeof canon) {
                    free(core);
                    core = policy_xstrdup(canon);
                }
            }
            if (hit_count == hit_cap) {
                hit_cap = hit_cap == 0 ? 16 : hit_cap * 2;
                policy_occurrence *grown = realloc(hits, hit_cap * sizeof *grown);
                if (grown == NULL) {
                    BT_TRACE_FAIL("policy.scan_literals", "hit growth failed at %zu", hit_cap);
                    fputs("bt-policy: out of memory\n", stderr);
                    exit(2);
                }
                hits = grown;
            }
            hits[hit_count].line = policy_line_of(masked_text, start);
            hits[hit_count].core = core;
            hit_count++;
        }
        i = end;
    }
    *count = hit_count;
    BT_TRACE_EXIT("policy.scan_literals");
    return hits;
}

void policy_occurrences_free(policy_occurrence *hits, size_t count) {
    for (size_t at = 0; at < count; at++) {
        free(hits[at].core);
    }
    free(hits);
}

typedef struct {
    char *core;
    size_t count;
    size_t line;
} policy_core_count;

static int recurring_cmp(const void *left, const void *right) {
    const policy_recurring *a = left;
    const policy_recurring *b = right;
    if (a->line != b->line) {
        return a->line < b->line ? -1 : 1;
    }
    return strcmp(a->core, b->core);
}

policy_recurring *policy_recurring_literals(const char *masked_text, size_t len, policy_lang lang,
                                            unsigned long long threshold, size_t *count) {
    BT_TRACE_ENTER("policy.recurring_literals");
    size_t occurrence_count = 0;
    policy_occurrence *occurrences =
        policy_scan_literals(masked_text, len, lang, &occurrence_count);
    policy_core_count *counts = NULL;
    size_t count_len = 0;
    size_t count_cap = 0;
    for (size_t at = 0; at < occurrence_count; at++) {
        size_t slot = count_len;
        for (size_t probe = 0; probe < count_len; probe++) {
            if (strcmp(counts[probe].core, occurrences[at].core) == 0) {
                slot = probe;
                break;
            }
        }
        if (slot == count_len) {
            if (count_len == count_cap) {
                count_cap = count_cap == 0 ? 16 : count_cap * 2;
                policy_core_count *grown = realloc(counts, count_cap * sizeof *grown);
                if (grown == NULL) {
                    BT_TRACE_FAIL("policy.recurring_literals", "count growth failed at %zu",
                                  count_cap);
                    fputs("bt-policy: out of memory\n", stderr);
                    exit(2);
                }
                counts = grown;
            }
            counts[slot].core = policy_xstrdup(occurrences[at].core);
            counts[slot].count = 0;
            counts[slot].line = occurrences[at].line;
            count_len++;
        }
        counts[slot].count++;
    }
    policy_recurring *hits = NULL;
    size_t hit_count = 0;
    for (size_t at = 0; at < count_len; at++) {
        if (counts[at].count >= threshold) {
            hit_count++;
        }
    }
    if (hit_count > 0) {
        hits = policy_xalloc(hit_count * sizeof *hits);
        size_t used = 0;
        for (size_t at = 0; at < count_len; at++) {
            if (counts[at].count >= threshold) {
                hits[used].core = policy_xstrdup(counts[at].core);
                hits[used].count = counts[at].count;
                hits[used].line = counts[at].line;
                used++;
            }
        }
        qsort(hits, hit_count, sizeof *hits, recurring_cmp);
    }
    for (size_t at = 0; at < count_len; at++) {
        free(counts[at].core);
    }
    free(counts);
    policy_occurrences_free(occurrences, occurrence_count);
    *count = hit_count;
    BT_TRACE_EXIT("policy.recurring_literals");
    return hits;
}

void policy_recurring_free(policy_recurring *hits, size_t count) {
    for (size_t at = 0; at < count; at++) {
        free(hits[at].core);
    }
    free(hits);
}
