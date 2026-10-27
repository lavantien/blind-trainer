#include "policy/sloc.h"

#include <stdio.h>
#include <string.h>

#include "common/bt_trace.h"

static int is_blank(unsigned char byte) {
    return byte == ' ' || byte == '\t' || byte == '\r' || byte == '\v' || byte == '\f';
}

static int is_unicode_space(unsigned int codepoint) {
    return codepoint == 0x85 || codepoint == 0xA0 || codepoint == 0x1680 ||
           (codepoint >= 0x2000 && codepoint <= 0x200A) || codepoint == 0x2028 ||
           codepoint == 0x2029 || codepoint == 0x202F || codepoint == 0x205F || codepoint == 0x3000;
}

static int line_is_blank(const char *text, size_t from, size_t to) {
    size_t at = from;
    while (at < to) {
        unsigned char byte = (unsigned char)text[at];
        if (is_blank(byte)) {
            at++;
            continue;
        }
        if (byte < 0x80) {
            return 0;
        }
        size_t width = byte < 0xE0 ? 2 : (byte < 0xF0 ? 3 : 4);
        if (at + width > to) {
            return 0;
        }
        unsigned int codepoint = byte & (width == 2 ? 0x1F : (width == 3 ? 0x0F : 0x07));
        for (size_t k = 1; k < width; k++) {
            codepoint = (codepoint << 6) | ((unsigned char)text[at + k] & 0x3FU);
        }
        if (!is_unicode_space(codepoint)) {
            return 0;
        }
        at += width;
    }
    return 1;
}

int policy_sloc_from_config(const bt_json_value *config, policy_sloc_config *out, char *err,
                            size_t err_len) {
    BT_TRACE_ENTER("policy.sloc_from_config");
    memset(out, 0, sizeof *out);
    const bt_json_value *section = config == NULL ? NULL : bt_json_get(config, "sloc");
    const bt_json_value *raw = section == NULL ? NULL : bt_json_get(section, "max_file_lines");
    if (raw == NULL) {
        snprintf(err, err_len, "config section missing: sloc.max_file_lines");
        BT_TRACE_FAIL("policy.sloc_from_config", "max_file_lines missing");
        BT_TRACE_EXIT("policy.sloc_from_config");
        return -1;
    }
    if (raw->type != BT_JSON_NUMBER) {
        snprintf(err, err_len, "config value malformed: sloc.max_file_lines");
        BT_TRACE_FAIL("policy.sloc_from_config", "max_file_lines is not a number");
        BT_TRACE_EXIT("policy.sloc_from_config");
        return -1;
    }
    unsigned long long cap = 0;
    if (!bt_json_is_integer(raw)) {
        snprintf(err, err_len, "config value malformed: sloc.max_file_lines");
        BT_TRACE_FAIL("policy.sloc_from_config", "max_file_lines is not an integer");
        BT_TRACE_EXIT("policy.sloc_from_config");
        return -1;
    }
    long long exact = bt_json_integer_or(raw, 0);
    if (exact < 0) {
        snprintf(err, err_len, "config value malformed: sloc.max_file_lines");
        BT_TRACE_FAIL("policy.sloc_from_config", "max_file_lines below zero");
        BT_TRACE_EXIT("policy.sloc_from_config");
        return -1;
    }
    if (exact < 1) {
        snprintf(err, err_len, "sloc.max_file_lines must be >= 1");
        BT_TRACE_FAIL("policy.sloc_from_config", "max_file_lines below 1");
        BT_TRACE_EXIT("policy.sloc_from_config");
        return -1;
    }
    cap = (unsigned long long)exact;
    out->max_file_lines = cap;
    BT_TRACE_EXIT("policy.sloc_from_config");
    return 0;
}

int policy_cap_finding(const char *text, size_t len, const char *rel, unsigned long long cap,
                       policy_finding *out) {
    BT_TRACE_ENTER("policy.cap_finding");
    unsigned long long non_blank = 0;
    size_t crossing = 0;
    size_t line_start = 0;
    size_t index = 0;
    while (line_start < len) {
        size_t nl = line_start;
        while (nl < len && text[nl] != '\n') {
            nl++;
        }
        if (!line_is_blank(text, line_start, nl)) {
            non_blank++;
            if (non_blank == cap + 1) {
                crossing = index + 1;
            }
        }
        if (nl >= len) {
            break;
        }
        line_start = nl + 1;
        index++;
    }
    if (non_blank <= cap) {
        BT_TRACE_EXIT("policy.cap_finding");
        return 0;
    }
    memset(out, 0, sizeof *out);
    out->path = policy_xstrdup(rel);
    out->line = crossing;
    out->kind = "sloc";
    out->detail = policy_xalloc(96);
    snprintf(out->detail, 96, "%llu non-blank lines over cap %llu", non_blank, cap);
    BT_TRACE_EXIT("policy.cap_finding");
    return 1;
}
