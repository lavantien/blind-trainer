#include "gendata/lib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/sha256.h"
#include "common/strbuf.h"
#include "data/digest.h"
#include "data/paths.h"

const bt_gd_special_count BT_GD_SPECIAL_COUNTS[BT_GD_SPECIAL_COUNTS_LEN] = {
    {"cherry", "flips", 25}, {"cherry", "twists", 20}, {"cherry", "parity", 26},
    {"cherry", "ltct", 8},   {"eggins", "flips", 0},   {"eggins", "twists", 5},
    {"eggins", "parity", 2}, {"eggins", "ltct", 3},
};

const char *bt_gd_crate_id(void) {
    return "bt-gendata";
}

enum {
    GD_SERDE_F64_PRECISION_MAX = 17,
    GD_SERDE_F64_FIXED_CEILING = 15,
    GD_SERDE_F64_FIXED_FLOOR = -5,
    GD_SERDE_DIGITS_MAX = 40,
};

typedef struct {
    const char *key;
    size_t index;
} bt_gd_serde_key;

static int serde_key_cmp(const void *left, const void *right) {
    const bt_gd_serde_key *a = left;
    const bt_gd_serde_key *b = right;
    const unsigned char *pa = (const unsigned char *)a->key;
    const unsigned char *pb = (const unsigned char *)b->key;
    while (*pa != '\0' && *pb != '\0' && *pa == *pb) {
        pa++;
        pb++;
    }
    return (int)*pa - (int)*pb;
}

static int serde_string(const char *text, bt_strbuf *out) {
    if (bt_strbuf_append(out, "\"", 1) != 0) {
        return -1;
    }
    for (const unsigned char *at = (const unsigned char *)text; *at != '\0'; at++) {
        int rc = 0;
        switch (*at) {
        case '"':
            rc = bt_strbuf_append(out, "\\\"", 2);
            break;
        case '\\':
            rc = bt_strbuf_append(out, "\\\\", 2);
            break;
        case '\b':
            rc = bt_strbuf_append(out, "\\b", 2);
            break;
        case '\f':
            rc = bt_strbuf_append(out, "\\f", 2);
            break;
        case '\n':
            rc = bt_strbuf_append(out, "\\n", 2);
            break;
        case '\r':
            rc = bt_strbuf_append(out, "\\r", 2);
            break;
        case '\t':
            rc = bt_strbuf_append(out, "\\t", 2);
            break;
        default:
            if (*at < 0x20) {
                char unicode[7];
                snprintf(unicode, sizeof unicode, "\\u%04x", (unsigned)*at);
                rc = bt_strbuf_append(out, unicode, 6);
            } else {
                rc = bt_strbuf_append(out, (const char *)at, 1);
            }
            break;
        }
        if (rc != 0) {
            return -1;
        }
    }
    return bt_strbuf_append(out, "\"", 1);
}

static int serde_shortest_digits(double value, char digits[GD_SERDE_DIGITS_MAX],
                                 size_t *digits_count, int *exp10) {
    char buffer[64];
    for (int precision = 1; precision <= GD_SERDE_F64_PRECISION_MAX; precision++) {
        snprintf(buffer, sizeof buffer, "%.*g", precision, value);
        if (strtod(buffer, NULL) == value) {
            break;
        }
    }
    char raw[GD_SERDE_DIGITS_MAX];
    size_t raw_count = 0;
    int int_digits = 0;
    bool seen_point = false;
    const char *cursor = buffer;
    if (*cursor == '-') {
        cursor++;
    }
    while (*cursor != '\0' && *cursor != 'e' && *cursor != 'E') {
        if (*cursor >= '0' && *cursor <= '9') {
            if (raw_count < GD_SERDE_DIGITS_MAX) {
                raw[raw_count] = *cursor;
                raw_count++;
            }
            if (!seen_point) {
                int_digits++;
            }
        } else if (*cursor == '.') {
            seen_point = true;
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

static int serde_append_zeros(bt_strbuf *out, int count) {
    for (int i = 0; i < count; i++) {
        if (bt_strbuf_append_str(out, "0") != 0) {
            return -1;
        }
    }
    return 0;
}

static int serde_float(double value, bt_strbuf *out) {
    if (!isfinite(value)) {
        return -1;
    }
    char digits[GD_SERDE_DIGITS_MAX];
    size_t count = 0;
    int exp10 = 0;
    serde_shortest_digits(fabs(value), digits, &count, &exp10);
    if (signbit(value) && bt_strbuf_append_str(out, "-") != 0) {
        return -1;
    }
    int n = (int)count;
    if (exp10 >= n - 1 && exp10 <= GD_SERDE_F64_FIXED_CEILING) {
        if (bt_strbuf_append(out, digits, count) != 0 ||
            serde_append_zeros(out, exp10 - n + 1) != 0 || bt_strbuf_append_str(out, ".0") != 0) {
            return -1;
        }
        return 0;
    }
    if (exp10 >= 0 && exp10 <= GD_SERDE_F64_FIXED_CEILING) {
        if (bt_strbuf_append(out, digits, (size_t)exp10 + 1) != 0 ||
            bt_strbuf_append_str(out, ".") != 0 ||
            bt_strbuf_append(out, digits + exp10 + 1, count - ((size_t)exp10 + 1)) != 0) {
            return -1;
        }
        return 0;
    }
    if (exp10 >= GD_SERDE_F64_FIXED_FLOOR && exp10 <= -1) {
        if (bt_strbuf_append_str(out, "0.") != 0 || serde_append_zeros(out, -exp10 - 1) != 0 ||
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
    snprintf(tail, sizeof tail, "e%+03d", exp10);
    return bt_strbuf_append_str(out, tail) == 0 ? 0 : -1;
}

static int serde_value(const bt_json_value *value, bt_strbuf *out) {
    if (value == NULL) {
        return -1;
    }
    switch (value->type) {
    case BT_JSON_NULL:
        return bt_strbuf_append_str(out, "null");
    case BT_JSON_BOOL:
        return bt_strbuf_append_str(out, value->as.boolean ? "true" : "false");
    case BT_JSON_NUMBER:
        if (value->is_integer) {
            char buffer[32];
            snprintf(buffer, sizeof buffer, "%lld", value->integer);
            return bt_strbuf_append_str(out, buffer);
        }
        return serde_float(value->as.number, out);
    case BT_JSON_STRING:
        return serde_string(value->as.string, out);
    case BT_JSON_ARRAY: {
        if (bt_strbuf_append(out, "[", 1) != 0) {
            return -1;
        }
        int rc = 0;
        for (size_t i = 0; i < value->as.array.count && rc == 0; i++) {
            if (i > 0) {
                rc = bt_strbuf_append(out, ",", 1);
            }
            if (rc == 0) {
                rc = serde_value(bt_json_at(value, i), out);
            }
        }
        if (rc != 0) {
            return -1;
        }
        return bt_strbuf_append(out, "]", 1);
    }
    case BT_JSON_OBJECT: {
        if (bt_strbuf_append(out, "{", 1) != 0) {
            return -1;
        }
        size_t count = value->as.object.count;
        bt_gd_serde_key *slots = count > 0 ? malloc(count * sizeof *slots) : NULL;
        if (count > 0 && slots == NULL) {
            return -1;
        }
        for (size_t i = 0; i < count; i++) {
            slots[i].key = value->as.object.keys[i];
            slots[i].index = i;
        }
        qsort(slots, count, sizeof *slots, serde_key_cmp);
        int rc = 0;
        for (size_t i = 0; i < count && rc == 0; i++) {
            if (i > 0) {
                rc = bt_strbuf_append(out, ",", 1);
            }
            if (rc == 0) {
                rc = serde_string(slots[i].key, out);
            }
            if (rc == 0) {
                rc = bt_strbuf_append(out, ":", 1);
            }
            if (rc == 0) {
                rc = serde_value(value->as.object.values[slots[i].index], out);
            }
        }
        free(slots);
        if (rc != 0) {
            return -1;
        }
        return bt_strbuf_append(out, "}", 1);
    }
    default:
        return -1;
    }
}

int bt_gd_synth_params_sha256(const bt_json_value *config, const char *generator, char out[65],
                              char *err, size_t err_len) {
    BT_TRACE_ENTER("gd.lib.synth_params_sha256");
    const bt_json_value *section = config != NULL ? bt_json_get(config, "synth") : NULL;
    if (section == NULL) {
        snprintf(err, err_len, "%s", "config section missing: synth");
        BT_TRACE_FAIL("gd.lib.synth_params_sha256", err);
        return -1;
    }
    bt_strbuf canonical;
    bt_strbuf_init(&canonical, 0);
    if (serde_value(section, &canonical) != 0) {
        bt_strbuf_free(&canonical);
        snprintf(err, err_len, "%s", "synth section serialization failed");
        BT_TRACE_FAIL("gd.lib.synth_params_sha256", err);
        return -1;
    }
    bt_sha256_ctx ctx;
    bt_sha256_init(&ctx);
    bt_sha256_update(&ctx, canonical.data, canonical.len);
    bt_sha256_update(&ctx, "\0", 1);
    bt_sha256_update(&ctx, generator, strlen(generator));
    unsigned char digest[32];
    bt_sha256_final(&ctx, digest);
    bt_sha256_hex(digest, out);
    bt_strbuf_free(&canonical);
    BT_TRACE_EXIT("gd.lib.synth_params_sha256");
    return 0;
}

static int integer_field(const bt_json_value *section, const char *key, size_t *out, char *err,
                         size_t err_len) {
    const bt_json_value *node = bt_json_get(section, key);
    if (node == NULL || node->type != BT_JSON_NUMBER || !bt_json_is_integer(node) ||
        node->integer < 0) {
        snprintf(err, err_len, "config section missing: synth.%s", key);
        return -1;
    }
    *out = (size_t)node->integer;
    return 0;
}

int bt_gd_limits_from_config(const bt_json_value *config, bt_gd_synth_limits *out, char *err,
                             size_t err_len) {
    BT_TRACE_ENTER("gd.lib.limits_from_config");
    const bt_json_value *section = config != NULL ? bt_json_get(config, "synth") : NULL;
    if (section == NULL) {
        snprintf(err, err_len, "%s", "config section missing: synth");
        BT_TRACE_FAIL("gd.lib.limits_from_config", err);
        return -1;
    }
    if (integer_field(section, "max_setup", &out->max_setup, err, err_len) != 0 ||
        integer_field(section, "max_insert", &out->max_insert, err, err_len) != 0 ||
        integer_field(section, "node_budget", &out->node_budget, err, err_len) != 0) {
        BT_TRACE_FAIL("gd.lib.limits_from_config", err);
        return -1;
    }
    BT_TRACE_EXIT("gd.lib.limits_from_config");
    return 0;
}

int bt_gd_data_vendor_path(const bt_json_value *config, char out[BT_PATH_MAX], char *err,
                           size_t err_len) {
    BT_TRACE_ENTER("gd.lib.data_vendor_path");
    const bt_json_value *node = config != NULL ? bt_json_get(config, "paths") : NULL;
    const bt_json_value *relative = node != NULL ? bt_json_get(node, "data_vendor") : NULL;
    if (relative == NULL || relative->type != BT_JSON_STRING) {
        snprintf(err, err_len, "%s", "config section missing: paths.data_vendor");
        BT_TRACE_FAIL("gd.lib.data_vendor_path", err);
        return -1;
    }
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    bt_paths_join(root, relative->as.string, out);
    BT_TRACE_EXIT("gd.lib.data_vendor_path");
    return 0;
}
