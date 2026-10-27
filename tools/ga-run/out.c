#include "ga-run/out.h"

#include "common/bt_trace.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    BT_GR_F32_PRECISION_MAX = 9,
    BT_GR_F64_PRECISION_MAX = 17,
    BT_GR_RYU_KK_LIMIT = 15,
    BT_GR_RYU_EXP_FLOOR = -6,
    BT_GR_DIGITS_MAX = 40,
};

static int shortest_digits(double value, int is_f32, char digits[BT_GR_DIGITS_MAX],
                           size_t *digits_count, int *exp10) {
    char buffer[64];
    int max_precision = is_f32 ? BT_GR_F32_PRECISION_MAX : BT_GR_F64_PRECISION_MAX;
    for (int precision = 1; precision <= max_precision; precision++) {
        snprintf(buffer, sizeof buffer, "%.*g", precision, value);
        double parsed = strtod(buffer, NULL);
        int same;
        if (is_f32) {
            uint32_t a_bits = 0;
            uint32_t b_bits = 0;
            float a = (float)parsed;
            float b = (float)value;
            memcpy(&a_bits, &a, sizeof a_bits);
            memcpy(&b_bits, &b, sizeof b_bits);
            same = a_bits == b_bits;
        } else {
            same = parsed == value;
        }
        if (same) {
            break;
        }
    }
    char raw[BT_GR_DIGITS_MAX];
    size_t raw_count = 0;
    int int_digits = 0;
    int seen_point = 0;
    const char *cursor = buffer;
    if (*cursor == '-') {
        cursor++;
    }
    while (*cursor != '\0' && *cursor != 'e' && *cursor != 'E') {
        if (*cursor >= '0' && *cursor <= '9') {
            if (raw_count < BT_GR_DIGITS_MAX) {
                raw[raw_count] = *cursor;
                raw_count++;
            }
            if (!seen_point) {
                int_digits++;
            }
        } else if (*cursor == '.') {
            seen_point = 1;
        }
        cursor++;
    }
    size_t first = raw_count;
    for (size_t i = 0; i < raw_count; i++) {
        if (raw[i] != '0') {
            first = i;
            break;
        }
    }
    if (first == raw_count) {
        digits[0] = '0';
        *digits_count = 1;
        *exp10 = 0;
        return 0;
    }
    size_t last = raw_count;
    while (last > first + 1 && raw[last - 1] == '0') {
        last--;
    }
    for (size_t i = first; i < last; i++) {
        digits[i - first] = raw[i];
    }
    *digits_count = last - first;
    if (*cursor == 'e' || *cursor == 'E') {
        *exp10 = (int)strtol(cursor + 1, NULL, 10);
    } else {
        *exp10 = int_digits - 1 - (int)first;
    }
    return 0;
}

static int append_zeros(bt_strbuf *out, int count) {
    for (int i = 0; i < count; i++) {
        if (bt_strbuf_append_str(out, "0") != 0) {
            return -1;
        }
    }
    return 0;
}

static int render_ryu(bt_strbuf *out, double value, int is_f32) {
    if (!isfinite(value)) {
        BT_TRACE_FAIL("gr.out.render_ryu", "non-finite value rejected");
        return -1;
    }
    char digits[BT_GR_DIGITS_MAX];
    size_t count = 0;
    int exp10 = 0;
    shortest_digits(fabs(value), is_f32, digits, &count, &exp10);
    if (signbit(value) && bt_strbuf_append_str(out, "-") != 0) {
        return -1;
    }
    int n = (int)count;
    if (exp10 >= n - 1 && exp10 <= BT_GR_RYU_KK_LIMIT) {
        if (bt_strbuf_append(out, digits, count) != 0 || append_zeros(out, exp10 - n + 1) != 0 ||
            bt_strbuf_append_str(out, ".0") != 0) {
            return -1;
        }
        return 0;
    }
    if (exp10 >= 0 && exp10 <= BT_GR_RYU_KK_LIMIT) {
        if (bt_strbuf_append(out, digits, (size_t)exp10 + 1) != 0 ||
            bt_strbuf_append_str(out, ".") != 0 ||
            bt_strbuf_append(out, digits + exp10 + 1, count - (size_t)exp10 + 1) != 0) {
            return -1;
        }
        return 0;
    }
    if (exp10 >= BT_GR_RYU_EXP_FLOOR && exp10 <= -1) {
        if (bt_strbuf_append_str(out, "0.") != 0 || append_zeros(out, -exp10 - 1) != 0 ||
            bt_strbuf_append(out, digits, count) != 0) {
            return -1;
        }
        return 0;
    }
    if (bt_strbuf_append(out, digits, 1) != 0) {
        return -1;
    }
    if (n > 1 && (bt_strbuf_append_str(out, ".") != 0 ||
                  bt_strbuf_append(out, digits + 1, count - 1) != 0)) {
        return -1;
    }
    char tail[16];
    snprintf(tail, sizeof tail, "e%d", exp10);
    return bt_strbuf_append_str(out, tail) == 0 ? 0 : -1;
}

static int emit_f32(bt_strbuf *out, float value) {
    return render_ryu(out, (double)value, 1);
}

static int emit_f64(bt_strbuf *out, double value) {
    return render_ryu(out, value, 0);
}

int bt_gr_display_f64(bt_strbuf *out, double value) {
    BT_TRACE_ENTER("gr.out.display_f64");
    if (!isfinite(value)) {
        BT_TRACE_FAIL("gr.out.display_f64", "non-finite value rejected");
        return -1;
    }
    char digits[BT_GR_DIGITS_MAX];
    size_t count = 0;
    int exp10 = 0;
    shortest_digits(fabs(value), 0, digits, &count, &exp10);
    int rc = 0;
    if (signbit(value)) {
        rc = bt_strbuf_append_str(out, "-");
    }
    int n = (int)count;
    if (rc == 0) {
        if (exp10 >= n - 1) {
            rc = bt_strbuf_append(out, digits, count) == 0 ? append_zeros(out, exp10 - n + 1) : -1;
        } else if (exp10 >= 0) {
            rc = bt_strbuf_append(out, digits, (size_t)exp10 + 1) == 0 &&
                         bt_strbuf_append_str(out, ".") == 0 &&
                         bt_strbuf_append(out, digits + exp10 + 1, count - (size_t)exp10 + 1) == 0
                     ? 0
                     : -1;
        } else {
            rc = bt_strbuf_append_str(out, "0.") == 0 && append_zeros(out, -exp10 - 1) == 0 &&
                         bt_strbuf_append(out, digits, count) == 0
                     ? 0
                     : -1;
        }
    }
    if (rc != 0) {
        BT_TRACE_FAIL("gr.out.display_f64", "rendering failed");
        return -1;
    }
    BT_TRACE_EXIT("gr.out.display_f64");
    return 0;
}

static int emit_string(bt_strbuf *out, const char *text) {
    if (bt_strbuf_append_str(out, "\"") != 0) {
        return -1;
    }
    for (const char *cursor = text; *cursor != '\0'; cursor++) {
        unsigned char c = (unsigned char)*cursor;
        const char *escape = NULL;
        if (c == '"') {
            escape = "\\\"";
        } else if (c == '\\') {
            escape = "\\\\";
        } else if (c < 0x20) {
            char buffer[7];
            snprintf(buffer, sizeof buffer, "\\u%04x", c);
            if (bt_strbuf_append_str(out, buffer) != 0) {
                return -1;
            }
            continue;
        }
        if (escape != NULL) {
            if (bt_strbuf_append_str(out, escape) != 0) {
                return -1;
            }
            continue;
        }
        if (bt_strbuf_append(out, cursor, 1) != 0) {
            return -1;
        }
    }
    return bt_strbuf_append_str(out, "\"") == 0 ? 0 : -1;
}

static int emit_indent(bt_strbuf *out, size_t depth) {
    for (size_t i = 0; i < depth; i++) {
        if (bt_strbuf_append_str(out, "  ") != 0) {
            return -1;
        }
    }
    return 0;
}

static int emit_u64(bt_strbuf *out, uint64_t value) {
    return bt_strbuf_appendf(out, "%llu", (unsigned long long)value) == 0 ? 0 : -1;
}

static int sorted_value(const bt_json_value *value, bt_strbuf *out, size_t depth) {
    if (value == NULL) {
        return -1;
    }
    switch (value->type) {
    case BT_JSON_NULL:
        return bt_strbuf_append_str(out, "null") == 0 ? 0 : -1;
    case BT_JSON_BOOL:
        return bt_strbuf_append_str(out, value->as.boolean ? "true" : "false") == 0 ? 0 : -1;
    case BT_JSON_NUMBER:
        if (value->is_integer) {
            return bt_strbuf_appendf(out, "%lld", value->integer) == 0 ? 0 : -1;
        }
        return emit_f64(out, value->as.number);
    case BT_JSON_STRING:
        return emit_string(out, value->as.string);
    case BT_JSON_ARRAY:
        if (value->as.array.count == 0) {
            return bt_strbuf_append_str(out, "[]") == 0 ? 0 : -1;
        }
        if (bt_strbuf_append_str(out, "[\n") != 0) {
            return -1;
        }
        for (size_t i = 0; i < value->as.array.count; i++) {
            if (emit_indent(out, depth + 1) != 0 ||
                sorted_value(value->as.array.items[i], out, depth + 1) != 0) {
                return -1;
            }
            if (i + 1 < value->as.array.count && bt_strbuf_append_str(out, ",") != 0) {
                return -1;
            }
            if (bt_strbuf_append_str(out, "\n") != 0) {
                return -1;
            }
        }
        return emit_indent(out, depth) == 0 && bt_strbuf_append_str(out, "]") == 0 ? 0 : -1;
    case BT_JSON_OBJECT: {
        if (value->as.object.count == 0) {
            return bt_strbuf_append_str(out, "{}") == 0 ? 0 : -1;
        }
        size_t inline_slots[16];
        size_t slot_count = value->as.object.count;
        int heap_slots = slot_count > 16;
        size_t *order = heap_slots ? malloc(slot_count * sizeof *order) : inline_slots;
        if (order == NULL) {
            return -1;
        }
        for (size_t i = 0; i < slot_count; i++) {
            order[i] = i;
        }
        for (size_t i = 1; i < slot_count; i++) {
            size_t key = order[i];
            size_t j = i;
            while (j > 0 &&
                   strcmp(value->as.object.keys[order[j - 1]], value->as.object.keys[key]) > 0) {
                order[j] = order[j - 1];
                j--;
            }
            order[j] = key;
        }
        if (bt_strbuf_append_str(out, "{\n") != 0) {
            if (heap_slots) {
                free(order);
            }
            return -1;
        }
        for (size_t i = 0; i < slot_count; i++) {
            size_t index = order[i];
            if (emit_indent(out, depth + 1) != 0 ||
                emit_string(out, value->as.object.keys[index]) != 0 ||
                bt_strbuf_append_str(out, ": ") != 0 ||
                sorted_value(value->as.object.values[index], out, depth + 1) != 0) {
                if (heap_slots) {
                    free(order);
                }
                return -1;
            }
            if (i + 1 < slot_count && bt_strbuf_append_str(out, ",") != 0) {
                if (heap_slots) {
                    free(order);
                }
                return -1;
            }
            if (bt_strbuf_append_str(out, "\n") != 0) {
                if (heap_slots) {
                    free(order);
                }
                return -1;
            }
        }
        int rc = emit_indent(out, depth) == 0 && bt_strbuf_append_str(out, "}") == 0 ? 0 : -1;
        if (heap_slots) {
            free(order);
        }
        return rc;
    }
    }
    return -1;
}

int bt_gr_json_value_text(const bt_json_value *value, bt_strbuf *out) {
    BT_TRACE_ENTER("gr.out.json_value_text");
    int rc = sorted_value(value, out, 0);
    if (rc != 0) {
        BT_TRACE_FAIL("gr.out.json_value_text", "sorted serialization failed");
        return -1;
    }
    BT_TRACE_EXIT("gr.out.json_value_text");
    return 0;
}

int bt_gr_cases_text_compact(const bt_gr_syllable_case *cases, size_t count, bt_strbuf *out) {
    BT_TRACE_ENTER("gr.out.cases_text_compact");
    if (bt_strbuf_append_str(out, "[") != 0) {
        BT_TRACE_FAIL("gr.out.cases_text_compact", "open bracket failed");
        return -1;
    }
    for (size_t i = 0; i < count; i++) {
        if (i > 0 && bt_strbuf_append_str(out, ",") != 0) {
            BT_TRACE_FAIL("gr.out.cases_text_compact", "separator failed at %zu", i);
            return -1;
        }
        int rc = bt_strbuf_append_str(out, "{\"orbit\":") == 0 &&
                 emit_string(out, cases[i].orbit) == 0 &&
                 bt_strbuf_append_str(out, ",\"letters\":") == 0 &&
                 emit_string(out, cases[i].letters) == 0 &&
                 bt_strbuf_append_str(out, ",\"syllable\":") == 0 &&
                 emit_string(out, cases[i].syllable) == 0 &&
                 bt_strbuf_append_str(out, ",\"frequency\":") == 0 &&
                 emit_u64(out, cases[i].frequency) == 0 && bt_strbuf_append_str(out, "}") == 0;
        if (!rc) {
            BT_TRACE_FAIL("gr.out.cases_text_compact", "row failed at %zu", i);
            return -1;
        }
    }
    if (bt_strbuf_append_str(out, "]") != 0) {
        BT_TRACE_FAIL("gr.out.cases_text_compact", "close bracket failed");
        return -1;
    }
    BT_TRACE_EXIT("gr.out.cases_text_compact");
    return 0;
}

static int cases_text_pretty(const bt_gr_syllable_case *cases, size_t count, bt_strbuf *out) {
    if (count == 0) {
        return bt_strbuf_append_str(out, "[]") == 0 ? 0 : -1;
    }
    if (bt_strbuf_append_str(out, "[\n") != 0) {
        return -1;
    }
    for (size_t i = 0; i < count; i++) {
        int rc = emit_indent(out, 2) == 0 && bt_strbuf_append_str(out, "{\n") == 0 &&
                 emit_indent(out, 3) == 0 && bt_strbuf_append_str(out, "\"orbit\": ") == 0 &&
                 emit_string(out, cases[i].orbit) == 0 && bt_strbuf_append_str(out, ",\n") == 0 &&
                 emit_indent(out, 3) == 0 && bt_strbuf_append_str(out, "\"letters\": ") == 0 &&
                 emit_string(out, cases[i].letters) == 0 && bt_strbuf_append_str(out, ",\n") == 0 &&
                 emit_indent(out, 3) == 0 && bt_strbuf_append_str(out, "\"syllable\": ") == 0 &&
                 emit_string(out, cases[i].syllable) == 0 &&
                 bt_strbuf_append_str(out, ",\n") == 0 && emit_indent(out, 3) == 0 &&
                 bt_strbuf_append_str(out, "\"frequency\": ") == 0 &&
                 emit_u64(out, cases[i].frequency) == 0 && bt_strbuf_append_str(out, "\n") == 0 &&
                 emit_indent(out, 2) == 0 && bt_strbuf_append_str(out, "}") == 0;
        if (!rc) {
            return -1;
        }
        if (i + 1 < count && bt_strbuf_append_str(out, ",") != 0) {
            return -1;
        }
        if (bt_strbuf_append_str(out, "\n") != 0) {
            return -1;
        }
    }
    return emit_indent(out, 1) == 0 && bt_strbuf_append_str(out, "]") == 0 ? 0 : -1;
}

int bt_gr_assignment_text(const bt_gr_syllable_assignment *assignment, bt_strbuf *out) {
    BT_TRACE_ENTER("gr.out.assignment_text");
    int rc =
        bt_strbuf_append_str(out, "{\n") == 0 && emit_indent(out, 1) == 0 &&
        bt_strbuf_append_str(out, "\"cases\": ") == 0 &&
        cases_text_pretty(assignment->cases, assignment->cases_count, out) == 0 &&
        bt_strbuf_append_str(out, ",\n") == 0 && emit_indent(out, 1) == 0 &&
        bt_strbuf_append_str(out, "\"pool_size\": ") == 0 &&
        emit_u64(out, (uint64_t)assignment->pool_size) == 0 &&
        bt_strbuf_append_str(out, ",\n") == 0 && emit_indent(out, 1) == 0 &&
        bt_strbuf_append_str(out, "\"seed\": ") == 0 && emit_u64(out, assignment->seed) == 0 &&
        bt_strbuf_append_str(out, ",\n") == 0 && emit_indent(out, 1) == 0 &&
        bt_strbuf_append_str(out, "\"best_fitness\": ") == 0 &&
        emit_f32(out, assignment->best_fitness) == 0 && bt_strbuf_append_str(out, ",\n") == 0 &&
        emit_indent(out, 1) == 0 && bt_strbuf_append_str(out, "\"freq_sha256\": ") == 0 &&
        emit_string(out, assignment->freq_sha256) == 0 && bt_strbuf_append_str(out, ",\n") == 0 &&
        emit_indent(out, 1) == 0 && bt_strbuf_append_str(out, "\"syllables_sha256\": ") == 0 &&
        emit_string(out, assignment->syllables_sha256) == 0 &&
        bt_strbuf_append_str(out, "\n}") == 0;
    if (!rc) {
        BT_TRACE_FAIL("gr.out.assignment_text", "assignment serialization failed");
        return -1;
    }
    BT_TRACE_EXIT("gr.out.assignment_text");
    return 0;
}

static int pair_rows_text(const bt_ga_pool_entry *rows, size_t count, bt_strbuf *out) {
    if (count == 0) {
        return bt_strbuf_append_str(out, "[]") == 0 ? 0 : -1;
    }
    if (bt_strbuf_append_str(out, "[\n") != 0) {
        return -1;
    }
    for (size_t i = 0; i < count; i++) {
        int rc = emit_indent(out, 2) == 0 && bt_strbuf_append_str(out, "[\n") == 0 &&
                 emit_indent(out, 3) == 0 && emit_string(out, rows[i].text) == 0 &&
                 bt_strbuf_append_str(out, ",\n") == 0 && emit_indent(out, 3) == 0 &&
                 emit_u64(out, rows[i].frequency) == 0 && bt_strbuf_append_str(out, "\n") == 0 &&
                 emit_indent(out, 2) == 0 && bt_strbuf_append_str(out, "]") == 0;
        if (!rc) {
            return -1;
        }
        if (i + 1 < count && bt_strbuf_append_str(out, ",") != 0) {
            return -1;
        }
        if (bt_strbuf_append_str(out, "\n") != 0) {
            return -1;
        }
    }
    return emit_indent(out, 1) == 0 && bt_strbuf_append_str(out, "]") == 0 ? 0 : -1;
}

static int float_array_text(const float *values, size_t count, bt_strbuf *out) {
    if (count == 0) {
        return bt_strbuf_append_str(out, "[]") == 0 ? 0 : -1;
    }
    if (bt_strbuf_append_str(out, "[\n") != 0) {
        return -1;
    }
    for (size_t i = 0; i < count; i++) {
        if (emit_indent(out, 2) != 0 || emit_f32(out, values[i]) != 0) {
            return -1;
        }
        if (i + 1 < count && bt_strbuf_append_str(out, ",") != 0) {
            return -1;
        }
        if (bt_strbuf_append_str(out, "\n") != 0) {
            return -1;
        }
    }
    return emit_indent(out, 1) == 0 && bt_strbuf_append_str(out, "]") == 0 ? 0 : -1;
}

int bt_gr_report_text(const bt_gr_ga_report *report, bt_strbuf *out) {
    BT_TRACE_ENTER("gr.out.report_text");
    int rc =
        bt_strbuf_append_str(out, "{\n") == 0 && emit_indent(out, 1) == 0 &&
        bt_strbuf_append_str(out, "\"run_config\": ") == 0 &&
        sorted_value(report->run_config, out, 1) == 0 && bt_strbuf_append_str(out, ",\n") == 0 &&
        emit_indent(out, 1) == 0 && bt_strbuf_append_str(out, "\"inventory\": ") == 0 &&
        bt_strbuf_append_str(out, "{\n") == 0 && emit_indent(out, 2) == 0 &&
        bt_strbuf_appendf(out, "\"generated\": %zu", report->inventory.generated) == 0 &&
        bt_strbuf_append_str(out, ",\n") == 0 && emit_indent(out, 2) == 0 &&
        bt_strbuf_appendf(out, "\"kept\": %zu", report->inventory.kept) == 0 &&
        bt_strbuf_append_str(out, ",\n") == 0 && emit_indent(out, 2) == 0 &&
        bt_strbuf_appendf(out, "\"dropped_not_in_wordlist\": %zu",
                          report->inventory.dropped_not_in_wordlist) == 0 &&
        bt_strbuf_append_str(out, ",\n") == 0 && emit_indent(out, 2) == 0 &&
        bt_strbuf_appendf(out, "\"wordlist_size\": %zu", report->inventory.wordlist_size) == 0 &&
        bt_strbuf_append_str(out, ",\n") == 0 && emit_indent(out, 2) == 0 &&
        bt_strbuf_appendf(out, "\"wordlist_covered\": %zu", report->inventory.wordlist_covered) ==
            0 &&
        bt_strbuf_append_str(out, "\n") == 0 && emit_indent(out, 1) == 0 &&
        bt_strbuf_append_str(out, "}") == 0 && bt_strbuf_append_str(out, ",\n") == 0 &&
        emit_indent(out, 1) == 0 && bt_strbuf_append_str(out, "\"pool_size\": ") == 0 &&
        emit_u64(out, (uint64_t)report->pool_size) == 0 && bt_strbuf_append_str(out, ",\n") == 0 &&
        emit_indent(out, 1) == 0 && bt_strbuf_append_str(out, "\"pool_top\": ") == 0 &&
        pair_rows_text(report->pool_top, report->pool_top_count, out) == 0 &&
        bt_strbuf_append_str(out, ",\n") == 0 && emit_indent(out, 1) == 0 &&
        bt_strbuf_append_str(out, "\"best_fitness\": ") == 0 &&
        emit_f32(out, report->best_fitness) == 0 && bt_strbuf_append_str(out, ",\n") == 0 &&
        emit_indent(out, 1) == 0 && bt_strbuf_append_str(out, "\"best_parts\": ") == 0 &&
        float_array_text(report->best_parts, 3, out) == 0 &&
        bt_strbuf_append_str(out, ",\n") == 0 && emit_indent(out, 1) == 0 &&
        bt_strbuf_append_str(out, "\"best_generation\": ") == 0 &&
        emit_u64(out, (uint64_t)report->best_generation) == 0 &&
        bt_strbuf_append_str(out, ",\n") == 0 && emit_indent(out, 1) == 0 &&
        bt_strbuf_append_str(out, "\"generations_run\": ") == 0 &&
        emit_u64(out, (uint64_t)report->generations_run) == 0 &&
        bt_strbuf_append_str(out, ",\n") == 0 && emit_indent(out, 1) == 0 &&
        bt_strbuf_append_str(out, "\"per_gen_best\": ") == 0 &&
        float_array_text(report->per_gen_best, report->per_gen_count, out) == 0 &&
        bt_strbuf_append_str(out, ",\n") == 0 && emit_indent(out, 1) == 0 &&
        bt_strbuf_appendf(out, "\"stagnation_tail\": %u", (unsigned)report->stagnation_tail) == 0 &&
        bt_strbuf_append_str(out, ",\n") == 0 && emit_indent(out, 1) == 0 &&
        bt_strbuf_appendf(out, "\"max_stagnation\": %u", (unsigned)report->max_stagnation) == 0 &&
        bt_strbuf_append_str(out, ",\n") == 0 && emit_indent(out, 1) == 0 &&
        bt_strbuf_append_str(out, "\"evaluations\": ") == 0 &&
        emit_u64(out, report->evaluations) == 0 && bt_strbuf_append_str(out, ",\n") == 0 &&
        emit_indent(out, 1) == 0 && bt_strbuf_append_str(out, "\"freq_sha256\": ") == 0 &&
        emit_string(out, report->freq_sha256) == 0 && bt_strbuf_append_str(out, ",\n") == 0 &&
        emit_indent(out, 1) == 0 && bt_strbuf_append_str(out, "\"syllables_sha256\": ") == 0 &&
        emit_string(out, report->syllables_sha256) == 0 && bt_strbuf_append_str(out, "\n}") == 0;
    if (!rc) {
        BT_TRACE_FAIL("gr.out.report_text", "report serialization failed");
        return -1;
    }
    BT_TRACE_EXIT("gr.out.report_text");
    return 0;
}
