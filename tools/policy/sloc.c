#include "policy/sloc.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "common/bt_trace.h"

static int is_blank(unsigned char byte) {
    return byte == ' ' || byte == '\t' || byte == '\r' || byte == '\v' || byte == '\f';
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
    double value = raw->as.number;
    if (!(value >= 0.0) || value != floor(value)) {
        snprintf(err, err_len, "config value malformed: sloc.max_file_lines");
        BT_TRACE_FAIL("policy.sloc_from_config", "max_file_lines is not an integer");
        BT_TRACE_EXIT("policy.sloc_from_config");
        return -1;
    }
    unsigned long long cap = (unsigned long long)value;
    if (cap < 1) {
        snprintf(err, err_len, "sloc.max_file_lines must be >= 1");
        BT_TRACE_FAIL("policy.sloc_from_config", "max_file_lines below 1");
        BT_TRACE_EXIT("policy.sloc_from_config");
        return -1;
    }
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
        int blank = 1;
        for (size_t at = line_start; at < nl; at++) {
            if (!is_blank((unsigned char)text[at])) {
                blank = 0;
                break;
            }
        }
        if (!blank) {
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
