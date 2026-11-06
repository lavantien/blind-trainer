#include "gendata/model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/strbuf.h"

static int append_indent(bt_strbuf *out, size_t depth) {
    for (size_t i = 0; i < depth; i++) {
        if (bt_strbuf_append_str(out, "  ") != 0) {
            return -1;
        }
    }
    return 0;
}

static int pretty_string(const char *text, bt_strbuf *out) {
    if (bt_strbuf_append(out, "\"", 1) != 0) {
        return -1;
    }
    for (const unsigned char *cursor = (const unsigned char *)text; *cursor != '\0'; cursor++) {
        unsigned char c = *cursor;
        const char *escape = NULL;
        if (c == '"') {
            escape = "\\\"";
        } else if (c == '\\') {
            escape = "\\\\";
        } else if (c == '\b') {
            escape = "\\b";
        } else if (c == '\f') {
            escape = "\\f";
        } else if (c == '\n') {
            escape = "\\n";
        } else if (c == '\r') {
            escape = "\\r";
        } else if (c == '\t') {
            escape = "\\t";
        }
        if (escape != NULL) {
            if (bt_strbuf_append_str(out, escape) != 0) {
                return -1;
            }
            continue;
        }
        if (c < 0x20) {
            char buffer[7];
            snprintf(buffer, sizeof buffer, "\\u%04x", (unsigned)c);
            if (bt_strbuf_append_str(out, buffer) != 0) {
                return -1;
            }
            continue;
        }
        if (bt_strbuf_append(out, (const char *)cursor, 1) != 0) {
            return -1;
        }
    }
    return bt_strbuf_append(out, "\"", 1) == 0 ? 0 : -1;
}

static int pretty_number(const bt_json_value *value, bt_strbuf *out) {
    char buffer[32];
    if (value->is_integer) {
        snprintf(buffer, sizeof buffer, "%lld", value->integer);
        return bt_strbuf_append_str(out, buffer);
    }
    char precise[64];
    for (int precision = 1; precision <= 17; precision++) {
        snprintf(precise, sizeof precise, "%.*g", precision, value->as.number);
        if (strtod(precise, NULL) == value->as.number) {
            break;
        }
    }
    return bt_strbuf_append_str(out, precise);
}

static int pretty_value(const bt_json_value *value, bt_strbuf *out, size_t depth) {
    if (value == NULL) {
        return -1;
    }
    switch (value->type) {
    case BT_JSON_NULL:
        return bt_strbuf_append_str(out, "null");
    case BT_JSON_BOOL:
        return bt_strbuf_append_str(out, value->as.boolean ? "true" : "false");
    case BT_JSON_NUMBER:
        return pretty_number(value, out);
    case BT_JSON_STRING:
        return pretty_string(value->as.string, out);
    case BT_JSON_ARRAY:
        if (value->as.array.count == 0) {
            return bt_strbuf_append_str(out, "[]");
        }
        if (bt_strbuf_append_str(out, "[\n") != 0) {
            return -1;
        }
        for (size_t i = 0; i < value->as.array.count; i++) {
            if (append_indent(out, depth + 1) != 0 ||
                pretty_value(value->as.array.items[i], out, depth + 1) != 0) {
                return -1;
            }
            if (i + 1 < value->as.array.count && bt_strbuf_append_str(out, ",") != 0) {
                return -1;
            }
            if (bt_strbuf_append_str(out, "\n") != 0) {
                return -1;
            }
        }
        if (append_indent(out, depth) != 0 || bt_strbuf_append_str(out, "]") != 0) {
            return -1;
        }
        return 0;
    case BT_JSON_OBJECT:
        if (value->as.object.count == 0) {
            return bt_strbuf_append_str(out, "{}");
        }
        if (bt_strbuf_append_str(out, "{\n") != 0) {
            return -1;
        }
        for (size_t i = 0; i < value->as.object.count; i++) {
            if (append_indent(out, depth + 1) != 0 ||
                pretty_string(value->as.object.keys[i], out) != 0 ||
                bt_strbuf_append_str(out, ": ") != 0 ||
                pretty_value(value->as.object.values[i], out, depth + 1) != 0) {
                return -1;
            }
            if (i + 1 < value->as.object.count && bt_strbuf_append_str(out, ",") != 0) {
                return -1;
            }
            if (bt_strbuf_append_str(out, "\n") != 0) {
                return -1;
            }
        }
        if (append_indent(out, depth) != 0 || bt_strbuf_append_str(out, "}") != 0) {
            return -1;
        }
        return 0;
    }
    return -1;
}

int bt_gd_json_pretty(const bt_json_value *value, bt_strbuf *out) {
    BT_TRACE_ENTER("gd.model.json_pretty");
    int rc = pretty_value(value, out, 0);
    if (rc != 0) {
        BT_TRACE_FAIL("gd.model.json_pretty", "pretty serialization failed");
    } else {
        BT_TRACE_EXIT("gd.model.json_pretty");
    }
    return rc;
}
